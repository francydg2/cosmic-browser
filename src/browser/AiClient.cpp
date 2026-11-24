#include "browser/AiClient.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

AiClient::AiClient(QObject *parent)
    : QObject(parent)
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        if (m_reply) {
            finish(tr("The provider did not respond in time."));
        }
    });
}

AiClient::~AiClient()
{
    cancel();
}

void AiClient::send(const Ai::ProviderConfig &config, const QList<Ai::Message> &messages)
{
    if (m_reply) {
        cancel();
    }
    const QString problem = Ai::validate(config);
    const Ai::HttpRequest http = Ai::buildRequest(config, messages);
    if (!problem.isEmpty() || http.url.isEmpty()) {
        QTimer::singleShot(0, this, [this, problem] {
            emit failed(problem.isEmpty() ? tr("Invalid provider configuration.") : problem);
        });
        return;
    }
    QNetworkRequest request(http.url);
    for (const auto &header : http.headers) {
        request.setRawHeader(header.first, header.second);
    }
    // Never follow redirects: they could carry the key to another host.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(0);
    m_parser = std::make_unique<Ai::StreamParser>(config.kind);
    m_errorBody.clear();
    m_cancelled = false;
    m_done = false;
    m_gotData = false;
    m_reply = m_network.post(request, http.body);
    m_timeout->setInterval(qBound(5, config.timeoutSeconds, 600) * 1000);
    m_timeout->start();
    connect(m_reply, &QNetworkReply::readyRead, this, &AiClient::readyRead);
    connect(m_reply, &QNetworkReply::finished, this, &AiClient::replyFinished);
}

void AiClient::cancel()
{
    m_timeout->stop();
    if (m_reply) {
        m_cancelled = true;
        QNetworkReply *reply = m_reply;
        m_reply.clear();
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_parser.reset();
}

void AiClient::readyRead()
{
    if (!m_reply || m_done) {
        return;
    }
    const QByteArray data = m_reply->readAll();
    m_timeout->start(); // inactivity timeout: restarted by every chunk
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300) {
        m_errorBody += data.left(16 * 1024);
        return;
    }
    m_gotData = true;
    const QStringList parts = m_parser->feed(data);
    for (const QString &part : parts) {
        emit delta(part);
    }
    if (!m_parser->error().isEmpty()) {
        finish(m_parser->error());
    } else if (m_parser->isFinished()) {
        finish(QString());
    }
}

void AiClient::replyFinished()
{
    if (!m_reply || m_done) {
        return;
    }
    readyRead();
    if (m_done || !m_reply) {
        return;
    }
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 300) {
        QString detail = Ai::errorFromBody(m_errorBody);
        if (status == 401 || status == 403) {
            detail = tr("Authentication failed. Check the API key.");
        } else if (status == 429) {
            detail = tr("Rate limit or quota exceeded.");
        } else if (detail.isEmpty()) {
            detail = m_reply->errorString();
        }
        finish(tr("HTTP %1: %2").arg(status).arg(detail));
    } else if (m_reply->error() != QNetworkReply::NoError) {
        finish(tr("Connection failed: %1").arg(m_reply->errorString()));
    } else {
        finish(QString());
    }
}

void AiClient::finish(const QString &error)
{
    if (m_done) {
        return;
    }
    m_done = true;
    m_timeout->stop();
    QNetworkReply *reply = m_reply;
    m_reply.clear();
    if (reply) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_parser.reset();
    if (error.isEmpty()) {
        emit finished();
    } else {
        emit failed(error);
    }
}
