#include "browser/PerformanceMonitor.h"

#include "core/Settings.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QTimer>

namespace {

constexpr int kDefaultWarnMb = 3072;
constexpr int kCheckIntervalMs = 30'000;
constexpr int kMinWarnGapSeconds = 600;

} // namespace

PerformanceMonitor::PerformanceMonitor(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_sampler(QCoreApplication::applicationPid())
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(kCheckIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &PerformanceMonitor::check);
}

void PerformanceMonitor::start()
{
    if (ProcessStats::isSupported()) {
        m_timer->start();
    }
}

PerformanceMonitor::Snapshot PerformanceMonitor::sampleNow()
{
    Snapshot snapshot;
    snapshot.supported = ProcessStats::isSupported();
    snapshot.reason = ProcessStats::unsupportedReason();
    snapshot.time = QDateTime::currentDateTime();
    if (!snapshot.supported) {
        return snapshot;
    }
    snapshot.processes = m_sampler.sample();
    bool allKnown = !snapshot.processes.isEmpty();
    double cpu = 0;
    for (const ProcessStats::Process &process : std::as_const(snapshot.processes)) {
        snapshot.totalRssBytes += process.rssBytes;
        if (process.cpuPercent < 0) {
            allKnown = false;
        } else {
            cpu += process.cpuPercent;
        }
    }
    snapshot.totalCpuPercent = allKnown ? cpu : -1;
    return snapshot;
}

bool PerformanceMonitor::warningEnabled() const
{
    return m_settings->section(QStringLiteral("performance"))
        .value(QStringLiteral("warnEnabled")).toBool(true);
}

int PerformanceMonitor::warningMegabytes() const
{
    const int value = m_settings->section(QStringLiteral("performance"))
                          .value(QStringLiteral("warnMb")).toInt(kDefaultWarnMb);
    return qBound(256, value, 262144);
}

void PerformanceMonitor::setWarning(bool enabled, int megabytes)
{
    QJsonObject section = m_settings->section(QStringLiteral("performance"));
    section.insert(QStringLiteral("warnEnabled"), enabled);
    section.insert(QStringLiteral("warnMb"), qBound(256, megabytes, 262144));
    m_settings->setSection(QStringLiteral("performance"), section);
    m_settings->save();
    m_armed = true;
}

void PerformanceMonitor::check()
{
    if (!warningEnabled()) {
        return;
    }
    const Snapshot snapshot = sampleNow();
    const qint64 limit = qint64(warningMegabytes()) * 1024 * 1024;
    if (snapshot.totalRssBytes < limit * 9 / 10) {
        m_armed = true;
        return;
    }
    if (snapshot.totalRssBytes < limit || !m_armed) {
        return;
    }
    if (m_lastWarning.isValid() && m_lastWarning.secsTo(QDateTime::currentDateTime()) < kMinWarnGapSeconds) {
        return;
    }
    m_armed = false;
    m_lastWarning = QDateTime::currentDateTime();
    emit memoryWarning(snapshot.totalRssBytes, limit);
}
