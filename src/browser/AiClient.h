#pragma once

#include "core/AiProvider.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <memory>

class QNetworkReply;
class QTimer;

/// Sends one streaming chat request to the configured provider. One
/// request at a time; start a new one only after finished()/failed().
/// The API key is passed per call, kept only in memory and never logged.
class AiClient : public QObject
{
    Q_OBJECT

public:
    explicit AiClient(QObject *parent = nullptr);
    ~AiClient() override;

    bool isBusy() const { return !m_reply.isNull(); }

    /// Starts a request. Emits failed() immediately (asynchronously) when
    /// the configuration is invalid.
    void send(const Ai::ProviderConfig &config, const QList<Ai::Message> &messages);
    void cancel();

signals:
    void delta(const QString &text);
    void finished();
    void failed(const QString &message);

private:
    void finish(const QString &error);
    void readyRead();
    void replyFinished();

    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<Ai::StreamParser> m_parser;
    QTimer *m_timeout;
    QByteArray m_errorBody;
    bool m_cancelled = false;
    bool m_done = false;
    bool m_gotData = false;
};
