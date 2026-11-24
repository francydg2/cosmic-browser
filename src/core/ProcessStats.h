#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

/// Measured resource use of Cosmic's own processes (the browser and the
/// Chromium helper processes it started). Values come from the operating
/// system; where the OS offers no source the sampler reports itself
/// unsupported rather than guessing.
namespace ProcessStats {

struct Process
{
    qint64 pid = 0;
    qint64 ppid = 0;
    QString kind;        // "browser", "renderer", "gpu", "utility", "other"
    QString command;     // executable name only (no arguments, no paths)
    qint64 rssBytes = 0;
    double cpuPercent = -1; // -1: needs a second sample; 100 = one full core
};

struct Raw
{
    qint64 pid = 0;
    qint64 ppid = 0;
    QString command;
    QString args;        // full command line, used only for classification
    quint64 cpuTicks = 0;
    qint64 rssBytes = 0;
    double psCpuPercent = -1;
};

/// Linux /proc/<pid>/stat line → pid, parent, command, utime+stime ticks
/// and resident pages. Handles command names containing spaces/parens.
bool parseProcStat(const QByteArray &line, Raw *out, qint64 pageSize);

/// macOS/BSD `ps -axo pid=,ppid=,rss=,pcpu=,command=` output.
QList<Raw> parsePs(const QByteArray &output);

/// Role of a Chromium process from its command line.
QString classify(const QString &args, bool isRoot);

/// Descendants of \p rootPid (and the root itself) from a flat list.
QList<Raw> descendants(const QList<Raw> &all, qint64 rootPid);

/// Builds Process entries; CPU from tick deltas when \p previous holds
/// the same pid and \p wallSeconds > 0.
QList<Process> build(const QList<Raw> &family, const QHash<qint64, quint64> &previousTicks,
                     double wallSeconds, long ticksPerSecond, qint64 rootPid);

bool isSupported();
QString unsupportedReason();

/// Stateful sampler: call sample() periodically (≥ 1 s apart).
class Sampler
{
public:
    explicit Sampler(qint64 rootPid);
    QList<Process> sample();

private:
    qint64 m_root;
    QHash<qint64, quint64> m_ticks;
    qint64 m_lastMs = 0;
};

} // namespace ProcessStats
