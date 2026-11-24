#pragma once

#include "core/DownloadHistory.h"

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QVector>

class QWebEngineDownloadRequest;
class QWebEngineProfile;

class DownloadManager : public QObject
{
    Q_OBJECT

public:
    /// \p historyPath: where finished downloads are remembered; empty
    /// keeps the history in memory only (tests).
    explicit DownloadManager(const QString &historyPath = QString(),
                             QObject *parent = nullptr);
    void attachProfile(QWebEngineProfile *profile);
    void start(QWebEngineDownloadRequest *request, const QString &path);
    void resume(QWebEngineDownloadRequest *request);
    QVector<QPointer<QWebEngineDownloadRequest>> downloads() const { return m_downloads; }
    /// Removes finished transfers from the list and from the saved history.
    void clearFinished();

    /// Stable id shared by the live request and its saved record.
    QString recordId(const QWebEngineDownloadRequest *request) const;
    double bytesPerSecond(const QWebEngineDownloadRequest *request) const;
    qint64 secondsRemaining(const QWebEngineDownloadRequest *request) const;
    QList<DownloadRecord> history() const { return m_history.records(); }
    int historyCount() const { return m_history.count(); }
    void removeRecord(const QString &id);
    void pause(QWebEngineDownloadRequest *request);

signals:
    void requested(QWebEngineDownloadRequest *request);
    void changed();
    /// A transfer reached a final state (completed, cancelled or failed).
    void finished(const DownloadRecord &record);

private:
    struct Meta {
        QString id;
        qint64 startedMs = 0;
        SpeedTracker speed;
    };
    void finalize(QWebEngineDownloadRequest *request, const QString &state);
    DownloadHistory m_history;
    QHash<const QWebEngineDownloadRequest *, Meta> m_meta;
    void track(QWebEngineDownloadRequest *request, int index = -1);
    QVector<QPointer<QWebEngineDownloadRequest>> m_downloads;
    QHash<QWebEngineDownloadRequest *, int> m_retryCounts;
};
