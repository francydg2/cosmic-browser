#pragma once

#include "core/ProcessStats.h"

#include <QDateTime>
#include <QObject>

class QTimer;
class Settings;

/// Measures Cosmic's own processes on demand and, in the background,
/// watches total memory use. The background check runs rarely (every 30
/// seconds) and only when the user left the warning enabled, so it costs
/// next to nothing; the detailed view samples faster only while open.
class PerformanceMonitor : public QObject
{
    Q_OBJECT

public:
    struct Snapshot
    {
        bool supported = false;
        QString reason; // why not supported
        QList<ProcessStats::Process> processes;
        qint64 totalRssBytes = 0;
        double totalCpuPercent = -1; // -1 until two samples exist
        QDateTime time;
    };

    explicit PerformanceMonitor(Settings *settings, QObject *parent = nullptr);

    Snapshot sampleNow();

    bool warningEnabled() const;
    int warningMegabytes() const;
    void setWarning(bool enabled, int megabytes);

    /// Starts the periodic memory check (no-op where unsupported).
    void start();

signals:
    void memoryWarning(qint64 usedBytes, qint64 limitBytes);

private:
    void check();

    Settings *m_settings;
    ProcessStats::Sampler m_sampler;
    QTimer *m_timer;
    QDateTime m_lastWarning;
    bool m_armed = true;
};
