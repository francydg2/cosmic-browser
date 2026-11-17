#pragma once

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
    explicit DownloadManager(QObject *parent = nullptr);
    void attachProfile(QWebEngineProfile *profile);
    void start(QWebEngineDownloadRequest *request, const QString &path);
    void resume(QWebEngineDownloadRequest *request);
    QVector<QPointer<QWebEngineDownloadRequest>> downloads() const { return m_downloads; }
    void clearFinished();

signals:
    void requested(QWebEngineDownloadRequest *request);
    void changed();

private:
    QVector<QPointer<QWebEngineDownloadRequest>> m_downloads;
    QHash<QWebEngineDownloadRequest *, int> m_retryCounts;
};
