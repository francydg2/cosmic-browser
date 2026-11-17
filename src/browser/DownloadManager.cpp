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
            &DownloadManager::requested);
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
    m_downloads.prepend(request);
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
    request->accept();
    emit changed();
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
