#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>

/// Provider-independent description of an AI chat request, and the pure
/// (network-free) parts of talking to each provider: request building,
/// validation and streaming-response parsing.
namespace Ai {

enum class Kind { OpenAi, Anthropic, Ollama, OpenAiCompatible };

struct Message
{
    QString role; // "system", "user" or "assistant"
    QString content;
};

struct ProviderConfig
{
    Kind kind = Kind::OpenAi;
    QString baseUrl;
    QString model;
    QString apiKey; // never logged, never serialized by this module
    int timeoutSeconds = 60;
    int maxTokens = 2048;
};

QStringList kindIds();
QString kindId(Kind kind);
bool kindFromId(const QString &id, Kind *kind);
QString kindLabel(Kind kind);
QString defaultBaseUrl(Kind kind);
QString defaultModel(Kind kind);
bool requiresKey(Kind kind);
/// Name of the environment variable consulted when no key is stored.
QString keyEnvironmentVariable(Kind kind);

/// True for loopback hosts: content sent there does not leave the machine.
bool isLocalEndpoint(const QUrl &url);

/// Empty when the configuration can be used; otherwise a user-facing
/// reason. Remote endpoints must use HTTPS so keys and page content are
/// never sent in clear text.
QString validate(const ProviderConfig &config);

struct HttpRequest
{
    QUrl url;
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
};

/// Builds a streaming chat request. Returns an empty url when the
/// configuration is invalid.
HttpRequest buildRequest(const ProviderConfig &config, const QList<Message> &messages);

/// Extracts a readable error from a JSON error body (any provider).
QString errorFromBody(const QByteArray &body);

/// Incremental parser for streamed responses (SSE for OpenAI/Anthropic,
/// newline-delimited JSON for Ollama). Feed raw chunks as they arrive.
class StreamParser
{
public:
    explicit StreamParser(Kind kind) : m_kind(kind) {}
    /// Returns the text fragments contained in the new data.
    QStringList feed(const QByteArray &chunk);
    bool isFinished() const { return m_finished; }
    /// Set when the stream carried an in-band error.
    QString error() const { return m_error; }

private:
    void handleLine(const QByteArray &line, QStringList *out);

    Kind m_kind;
    QByteArray m_buffer;
    QString m_eventType;
    bool m_finished = false;
    QString m_error;
};

} // namespace Ai
