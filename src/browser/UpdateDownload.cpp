#include "browser/UpdateDownload.h"

#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

UpdateDownload::UpdateDownload(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        m_error = tr("The update download timed out.");
        if (m_reply) {
            m_reply->abort();
        }
    });
}

void UpdateDownload::start(const UpdateAsset &asset, const QString &destination,
                           bool executable)
{
    if (m_reply) {
        emit failed(tr("An update download is already running."));
        return;
    }
    m_asset = asset;
    m_executable = executable;
    m_error.clear();
    m_received = 0;
    m_header.clear();
    m_hash.reset();
    m_permissions = QFileInfo(destination).permissions() | QFile::ExeOwner;
    m_file = std::make_unique<QSaveFile>(destination);
    if (!m_file->open(QIODevice::WriteOnly)) {
        const QString message = m_file->errorString();
        m_file.reset();
        emit failed(tr("Cannot write the update: %1").arg(message));
        return;
    }

    QNetworkRequest request(asset.url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Cosmic Browser"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &UpdateDownload::consume);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateDownload::complete);
    m_timeout->start(60000);
}

void UpdateDownload::consume()
{
    const QByteArray bytes = m_reply->readAll();
    if (bytes.isEmpty()) {
        return;
    }
    m_timeout->start(60000);
    if (m_header.size() < 12) {
        m_header.append(bytes.left(12 - m_header.size()));
    }
    m_received += bytes.size();
    if (m_received > m_asset.size || m_file->write(bytes) != bytes.size()) {
        m_error = tr("The update could not be saved or exceeds its expected size.");
        QMetaObject::invokeMethod(m_reply, "abort", Qt::QueuedConnection);
        return;
    }
    m_hash.addData(bytes);
    emit progress(m_received, m_asset.size);
}

void UpdateDownload::complete()
{
    m_timeout->stop();
    consume();
    const auto error = m_reply->error();
    const QString networkError = m_reply->errorString();
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_reply->deleteLater();
    m_reply = nullptr;

    if (m_error.isEmpty() && (error != QNetworkReply::NoError || status != 200)) {
        m_error = tr("Update download failed: %1").arg(networkError);
    }
    if (m_error.isEmpty() && (m_received != m_asset.size || m_hash.result() != m_asset.sha256)) {
        m_error = tr("The update failed its integrity check.");
    }
    if (m_error.isEmpty() && m_executable
        && (m_header.size() < 11 || m_header.left(4) != QByteArray("\x7f" "ELF", 4)
            || m_header.mid(8, 3) != QByteArray("AI\x02", 3))) {
        m_error = tr("The downloaded file is not a valid AppImage.");
    }
    if (m_error.isEmpty() && m_executable && !m_file->setPermissions(m_permissions)) {
        m_error = tr("Could not make the updated AppImage executable.");
    }
    const QString path = m_file->fileName();
    if (m_error.isEmpty() && !m_file->commit()) {
        m_error = tr("Could not install the update: %1").arg(m_file->errorString());
    }
    if (!m_error.isEmpty()) {
        m_file->cancelWriting();
        m_file.reset();
        emit failed(m_error);
        return;
    }
    m_file.reset();
    emit finished(path);
}
