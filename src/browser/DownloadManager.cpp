#include "browser/DownloadManager.h"
#include "core/Logging.h"

#include <QFileInfo>
#include <QTimer>
#include <QWebEngineDownloadRequest>
#include <QWebEngineProfile>

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
{
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
    } else {
        m_downloads.prepend(request);
    }
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
        if (!guard || state != QWebEngineDownloadRequest::DownloadInterrupted) {
            return;
        }
        qCWarning(lcCosmic) << "download interrupted, reason code:"
                            << static_cast<int>(guard->interruptReason());
        int &attempts = m_retryCounts[guard.data()];
        if (attempts >= 2) {
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
    emit changed();
}
