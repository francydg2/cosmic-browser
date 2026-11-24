#include "core/ProcessStats.h"

#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

#if defined(Q_OS_LINUX)
#include <unistd.h>
#endif

namespace ProcessStats {

bool parseProcStat(const QByteArray &line, Raw *out, qint64 pageSize)
{
    // "pid (comm) state ppid ... utime(14) stime(15) ... rss(24)"
    const int open = line.indexOf('(');
    const int close = line.lastIndexOf(')');
    if (open < 0 || close < open) {
        return false;
    }
    bool ok = false;
    const qint64 pid = line.left(open).trimmed().toLongLong(&ok);
    if (!ok) {
        return false;
    }
    const QList<QByteArray> fields = line.mid(close + 1).trimmed().split(' ');
    // fields[0] = state (field 3), so field N is fields[N - 3].
    if (fields.size() < 22) {
        return false;
    }
    const qint64 ppid = fields.at(1).toLongLong(&ok);
    if (!ok) {
        return false;
    }
    const quint64 utime = fields.at(11).toULongLong(&ok);
    const quint64 stime = ok ? fields.at(12).toULongLong(&ok) : 0;
    const qint64 rssPages = ok ? fields.at(21).toLongLong(&ok) : 0;
    if (!ok) {
        return false;
    }
    out->pid = pid;
    out->ppid = ppid;
    out->command = QString::fromUtf8(line.mid(open + 1, close - open - 1));
    out->cpuTicks = utime + stime;
    out->rssBytes = rssPages * pageSize;
    return true;
}

QList<Raw> parsePs(const QByteArray &output)
{
    QList<Raw> result;
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+([0-9.,]+)\\s+(.*)$"));
    for (const QByteArray &line : output.split('\n')) {
        const auto match = pattern.match(QString::fromUtf8(line));
        if (!match.hasMatch()) {
            continue;
        }
        Raw raw;
        raw.pid = match.captured(1).toLongLong();
        raw.ppid = match.captured(2).toLongLong();
        raw.rssBytes = match.captured(3).toLongLong() * 1024; // ps reports KiB
        QString cpu = match.captured(4);
        cpu.replace(QLatin1Char(','), QLatin1Char('.'));
        raw.psCpuPercent = cpu.toDouble();
        raw.args = match.captured(5);
        const int space = raw.args.indexOf(QLatin1Char(' '));
        raw.command = QFileInfo(space < 0 ? raw.args : raw.args.left(space)).fileName();
        result.append(raw);
    }
    return result;
}

QString classify(const QString &args, bool isRoot)
{
    if (isRoot) {
        return QStringLiteral("browser");
    }
    static const QRegularExpression type(QStringLiteral("--type=([a-zA-Z-]+)"));
    const auto match = type.match(args);
    if (!match.hasMatch()) {
        return QStringLiteral("other");
    }
    const QString value = match.captured(1);
    if (value == QLatin1String("renderer")) {
        return QStringLiteral("renderer");
    }
    if (value == QLatin1String("gpu-process")) {
        return QStringLiteral("gpu");
    }
    if (value == QLatin1String("utility") || value == QLatin1String("zygote")) {
        return QStringLiteral("utility");
    }
    return QStringLiteral("other");
}

QList<Raw> descendants(const QList<Raw> &all, qint64 rootPid)
{
    QHash<qint64, QList<const Raw *>> children;
    const Raw *root = nullptr;
    for (const Raw &raw : all) {
        children[raw.ppid].append(&raw);
        if (raw.pid == rootPid) {
            root = &raw;
        }
    }
    QList<Raw> result;
    if (!root) {
        return result;
    }
    QSet<qint64> seen;
    QList<const Raw *> stack{root};
    while (!stack.isEmpty()) {
        const Raw *current = stack.takeLast();
        if (seen.contains(current->pid)) {
            continue;
        }
        seen.insert(current->pid);
        result.append(*current);
        stack.append(children.value(current->pid));
    }
    return result;
}

QList<Process> build(const QList<Raw> &family, const QHash<qint64, quint64> &previousTicks,
                     double wallSeconds, long ticksPerSecond, qint64 rootPid)
{
    QList<Process> result;
    for (const Raw &raw : family) {
        Process process;
        process.pid = raw.pid;
        process.ppid = raw.ppid;
        process.kind = classify(raw.args, raw.pid == rootPid);
        process.command = raw.command;
        process.rssBytes = raw.rssBytes;
        if (raw.psCpuPercent >= 0) {
            process.cpuPercent = raw.psCpuPercent;
        } else if (previousTicks.contains(raw.pid) && wallSeconds > 0 && ticksPerSecond > 0
                   && raw.cpuTicks >= previousTicks.value(raw.pid)) {
            const double cpuSeconds = double(raw.cpuTicks - previousTicks.value(raw.pid)) / ticksPerSecond;
            process.cpuPercent = cpuSeconds / wallSeconds * 100.0;
        }
        result.append(process);
    }
    std::sort(result.begin(), result.end(), [](const Process &a, const Process &b) {
        return a.rssBytes > b.rssBytes;
    });
    return result;
}

bool isSupported()
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

QString unsupportedReason()
{
    return isSupported() ? QString()
        : QStringLiteral("Per-process measurements are not available on this operating system "
                         "in this version.");
}

Sampler::Sampler(qint64 rootPid)
    : m_root(rootPid)
{
}

QList<Process> Sampler::sample()
{
    QList<Raw> all;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    long ticks = 100;
#if defined(Q_OS_LINUX)
    const qint64 pageSize = sysconf(_SC_PAGESIZE);
    ticks = sysconf(_SC_CLK_TCK);
    const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool numeric = false;
        entry.toLongLong(&numeric);
        if (!numeric) {
            continue;
        }
        QFile stat(QStringLiteral("/proc/") + entry + QStringLiteral("/stat"));
        if (!stat.open(QIODevice::ReadOnly)) {
            continue; // the process ended meanwhile
        }
        Raw raw;
        if (!parseProcStat(stat.readAll(), &raw, pageSize)) {
            continue;
        }
        all.append(raw);
    }
    const QList<Raw> family = descendants(all, m_root);
    QList<Raw> detailed;
    for (Raw raw : family) {
        QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(raw.pid));
        if (cmdline.open(QIODevice::ReadOnly)) {
            raw.args = QString::fromUtf8(cmdline.read(4096)).replace(QChar(0), QLatin1Char(' '));
        }
        detailed.append(raw);
    }
    const double wall = m_lastMs > 0 ? (now - m_lastMs) / 1000.0 : 0;
    const QList<Process> result = build(detailed, m_ticks, wall, ticks, m_root);
    m_ticks.clear();
    for (const Raw &raw : detailed) {
        m_ticks.insert(raw.pid, raw.cpuTicks);
    }
    m_lastMs = now;
    return result;
#elif defined(Q_OS_MACOS)
    QProcess ps;
    ps.start(QStringLiteral("/bin/ps"), {QStringLiteral("-axo"),
                                         QStringLiteral("pid=,ppid=,rss=,pcpu=,command=")});
    if (!ps.waitForFinished(3000) || ps.exitStatus() != QProcess::NormalExit) {
        return {};
    }
    all = parsePs(ps.readAllStandardOutput());
    m_lastMs = now;
    return build(descendants(all, m_root), {}, 0, ticks, m_root);
#else
    Q_UNUSED(now)
    Q_UNUSED(ticks)
    return {};
#endif
}

} // namespace ProcessStats
