#pragma once

#include "browser/UpdateAsset.h"

#include <QCryptographicHash>
#include <QFile>
#include <QObject>
#include <QSaveFile>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class UpdateDownload : public QObject
{
    Q_OBJECT

public:
    explicit UpdateDownload(QObject *parent = nullptr);
    void start(const UpdateAsset &asset, const QString &destination, bool executable);

signals:
    void progress(qint64 received, qint64 total);
    void finished(const QString &path);
    void failed(const QString &message);

private:
    void consume();
    void complete();

    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    QTimer *m_timeout;
    std::unique_ptr<QSaveFile> m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    UpdateAsset m_asset;
    QString m_error;
    QByteArray m_header;
    qint64 m_received = 0;
    bool m_executable = false;
    QFile::Permissions m_permissions;
};
