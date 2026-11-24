#include "browser/DownloadManager.h"
#include "core/Logging.h"

#include <QDateTime>
#include <QFileInfo>
#include <QTimer>
#include <QUuid>
#include <QWebEngineDownloadRequest>
#include <QWebEngineProfile>

DownloadManager::DownloadManager(const QString &historyPath, QObject *parent)
    : QObject(parent)
    , m_history(historyPath)
{
    if (!historyPath.isEmpty() && !m_history.load()) {
        qCWarning(lcCosmic) << "download history unavailable";
    }
}

void DownloadManager::attachProfile(QWebEngineProfile *profile)
{
    connect(profile, &QWebEngineProfile::downloadRequested, this,
            [this](QWebEngineDownloadRequest *request) {
        // Qt 6.7 may emit a fresh request with the same profile and ID on resume.
        for (int i = 0; i < m_downloads.size(); ++i) {
            const auto &existing = m_downloads.at(i);
            if (existing && existing->parent() == request->parent()
                && existing->id() == request->id()) {
                if (request->state() == QWebEngineDownloadRequest::DownloadRequested) {
                    request->setDownloadDirectory(existing->downloadDirectory());
                    request->setDownloadFileName(existing->downloadFileName());
                }
                track(request, i);
                if (request->state() == QWebEngineDownloadRequest::DownloadRequested) {
                    request->accept();
                }
                emit changed();
                return;
            }
        }
        if (request->state() == QWebEngineDownloadRequest::DownloadRequested) {
            emit requested(request);
            return;
        }
        qCWarning(lcCosmic) << "tracking an already accepted download" << request->id();
        track(request);
        emit changed();
    });
}

void DownloadManager::start(QWebEngineDownloadRequest *request, const QString &path)
{
    if (!request || path.isEmpty()
        || request->state() != QWebEngineDownloadRequest::DownloadRequested) {
        return;
    }

    const QFileInfo target(path);
    request->setDownloadDirectory(target.absolutePath());
    request->setDownloadFileName(target.fileName());
    track(request);
    request->accept();
    emit changed();
}

void DownloadManager::track(QWebEngineDownloadRequest *request, int index)
{
    if (index >= 0) {
        auto *previous = m_downloads.at(index).data();
        const int attempts = m_retryCounts.take(previous);
        disconnect(previous, nullptr, this, nullptr);
        m_downloads[index] = request;
        m_retryCounts.insert(request, attempts);
        Meta meta = m_meta.take(previous);
        meta.speed = SpeedTracker();
        m_meta.insert(request, meta);
    } else {
        m_downloads.prepend(request);
        Meta meta;
        meta.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        meta.startedMs = QDateTime::currentMSecsSinceEpoch();
        m_meta.insert(request, meta);
    }
    connect(request, &QWebEngineDownloadRequest::receivedBytesChanged, this,
            [this, request] {
        auto it = m_meta.find(request);
        if (it != m_meta.end()) {
            it->speed.sample(QDateTime::currentMSecsSinceEpoch(), request->receivedBytes());
        }
    });
    connect(request, &QWebEngineDownloadRequest::receivedBytesChanged,
            this, &DownloadManager::changed);
    connect(request, &QWebEngineDownloadRequest::totalBytesChanged,
            this, &DownloadManager::changed);
    connect(request, &QWebEngineDownloadRequest::stateChanged,
            this, &DownloadManager::changed);
    connect(request, &QWebEngineDownloadRequest::isFinishedChanged,
            this, &DownloadManager::changed);
    connect(request, &QWebEngineDownloadRequest::isPausedChanged,
            this, &DownloadManager::changed);
    connect(request, &QWebEngineDownloadRequest::interruptReasonChanged,
            this, &DownloadManager::changed);
    connect(request, &QWebEngineDownloadRequest::stateChanged, this,
            [this, guard = QPointer<QWebEngineDownloadRequest>(request)]
            (QWebEngineDownloadRequest::DownloadState state) {
        if (!guard) {
            return;
        }
        if (state == QWebEngineDownloadRequest::DownloadCompleted) {
            finalize(guard, QStringLiteral("completed"));
            return;
        }
        if (state == QWebEngineDownloadRequest::DownloadCancelled) {
            finalize(guard, QStringLiteral("cancelled"));
            return;
        }
        if (state != QWebEngineDownloadRequest::DownloadInterrupted) {
            return;
        }
        qCWarning(lcCosmic) << "download interrupted, reason code:"
                            << static_cast<int>(guard->interruptReason());
        int &attempts = m_retryCounts[guard.data()];
        if (attempts >= 2) {
            finalize(guard, QStringLiteral("failed"));
            return;
        }
        const int delay = (++attempts) * 2000;
        QTimer::singleShot(delay, this, [this, guard] {
            if (guard && guard->state() == QWebEngineDownloadRequest::DownloadInterrupted
                && !guard->isFinished() && m_downloads.contains(guard)) {
                guard->resume();
            }
        });
    });
    connect(request, &QObject::destroyed, this, [this, request] {
        m_retryCounts.remove(request);
        m_meta.remove(request);
        m_downloads.removeIf([](const auto &item) { return item.isNull(); });
        emit changed();
    });
}

void DownloadManager::resume(QWebEngineDownloadRequest *request)
{
    if (!request || !m_downloads.contains(request) || request->isFinished()) {
        return;
    }
    if (request->state() == QWebEngineDownloadRequest::DownloadInterrupted
        || request->isPaused()) {
        m_retryCounts[request] = 0;
        request->resume();
    }
}

void DownloadManager::clearFinished()
{
    m_downloads.removeIf([](const auto &item) {
        return item.isNull() || item->isFinished();
    });
    m_history.clear();
    m_history.save();
    emit changed();
}

QString DownloadManager::recordId(const QWebEngineDownloadRequest *request) const
{
    return m_meta.value(request).id;
}

double DownloadManager::bytesPerSecond(const QWebEngineDownloadRequest *request) const
{
    return m_meta.value(request).speed.bytesPerSecond();
}

qint64 DownloadManager::secondsRemaining(const QWebEngineDownloadRequest *request) const
{
    return request ? m_meta.value(request).speed.secondsRemaining(
        request->totalBytes(), request->receivedBytes()) : -1;
}

void DownloadManager::pause(QWebEngineDownloadRequest *request)
{
    if (request && m_downloads.contains(request)
        && request->state() == QWebEngineDownloadRequest::DownloadInProgress) {
        request->pause();
    }
}

void DownloadManager::removeRecord(const QString &id)
{
    if (m_history.remove(id)) {
        m_history.save();
    }
    m_downloads.removeIf([this, &id](const auto &item) {
        return item && item->isFinished() && m_meta.value(item.data()).id == id;
    });
    emit changed();
}

void DownloadManager::finalize(QWebEngineDownloadRequest *request, const QString &state)
{
    const Meta meta = m_meta.value(request);
    if (meta.id.isEmpty()) {
        return;
    }
    DownloadRecord record;
    record.id = meta.id;
    record.url = request->url();
    record.fileName = request->downloadFileName();
    record.directory = request->downloadDirectory();
    record.totalBytes = request->totalBytes();
    record.receivedBytes = request->receivedBytes();
    record.state = state;
    if (state == QLatin1String("failed")) {
        record.error = request->interruptReasonString();
    }
    record.startedMs = meta.startedMs;
    record.finishedMs = QDateTime::currentMSecsSinceEpoch();
    m_history.upsert(record);
    m_history.save();
    emit finished(record);
    emit changed();
}
