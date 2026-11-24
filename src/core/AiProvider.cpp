#include "core/AiProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Ai {

namespace {

QString trimmedBase(const QString &baseUrl)
{
    QString base = baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return base;
}

} // namespace

QStringList kindIds()
{
    return {QStringLiteral("openai"), QStringLiteral("anthropic"), QStringLiteral("ollama"),
            QStringLiteral("compatible")};
}

QString kindId(Kind kind)
{
    switch (kind) {
    case Kind::OpenAi: return QStringLiteral("openai");
    case Kind::Anthropic: return QStringLiteral("anthropic");
    case Kind::Ollama: return QStringLiteral("ollama");
    case Kind::OpenAiCompatible: return QStringLiteral("compatible");
    }
    return {};
}

bool kindFromId(const QString &id, Kind *kind)
{
    for (Kind candidate : {Kind::OpenAi, Kind::Anthropic, Kind::Ollama, Kind::OpenAiCompatible}) {
        if (kindId(candidate) == id) {
            if (kind) {
                *kind = candidate;
            }
            return true;
        }
    }
    return false;
}

QString kindLabel(Kind kind)
{
    switch (kind) {
    case Kind::OpenAi: return QStringLiteral("OpenAI");
    case Kind::Anthropic: return QStringLiteral("Anthropic");
    case Kind::Ollama: return QStringLiteral("Ollama (local)");
    case Kind::OpenAiCompatible: return QStringLiteral("OpenAI-compatible endpoint");
    }
    return {};
}

QString defaultBaseUrl(Kind kind)
{
    switch (kind) {
    case Kind::OpenAi: return QStringLiteral("https://api.openai.com/v1");
    case Kind::Anthropic: return QStringLiteral("https://api.anthropic.com/v1");
    case Kind::Ollama: return QStringLiteral("http://localhost:11434");
    case Kind::OpenAiCompatible: return QString();
    }
    return {};
}

QString defaultModel(Kind kind)
{
    switch (kind) {
    case Kind::OpenAi: return QStringLiteral("gpt-4o-mini");
    case Kind::Anthropic: return QStringLiteral("claude-3-5-haiku-latest");
    case Kind::Ollama: return QStringLiteral("llama3.2");
    case Kind::OpenAiCompatible: return QString();
    }
    return {};
}

bool requiresKey(Kind kind)
{
    return kind == Kind::OpenAi || kind == Kind::Anthropic;
}

QString keyEnvironmentVariable(Kind kind)
{
    return QStringLiteral("COSMIC_AI_%1_KEY").arg(kindId(kind).toUpper());
}

bool isLocalEndpoint(const QUrl &url)
{
    const QString host = url.host().toLower();
    return host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1")
        || host == QLatin1String("::1") || host == QLatin1String("[::1]");
}

QString validate(const ProviderConfig &config)
{
    const QUrl url(trimmedBase(config.baseUrl));
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http"))) {
        return QStringLiteral("Enter a valid provider address (https://…).");
    }
    if (!url.userInfo().isEmpty()) {
        return QStringLiteral("The provider address must not contain credentials.");
    }
    if (url.scheme() == QLatin1String("http") && !isLocalEndpoint(url)) {
        return QStringLiteral("Remote providers must use HTTPS so keys and page content are "
                              "not sent in clear text.");
    }
    if (config.model.trimmed().isEmpty()) {
        return QStringLiteral("Choose a model.");
    }
    if (requiresKey(config.kind) && config.apiKey.trimmed().isEmpty()) {
        return QStringLiteral("An API key is required for this provider.");
    }
    return {};
}

HttpRequest buildRequest(const ProviderConfig &config, const QList<Message> &messages)
{
    HttpRequest request;
    if (!validate(config).isEmpty()) {
        return request;
    }
    const QString base = trimmedBase(config.baseUrl);
    QJsonObject body;
    body.insert(QStringLiteral("model"), config.model.trimmed());
    body.insert(QStringLiteral("stream"), true);

    QJsonArray array;
    if (config.kind == Kind::Anthropic) {
        QStringList system;
        for (const Message &message : messages) {
            if (message.role == QLatin1String("system")) {
                system << message.content;
            } else {
                array.append(QJsonObject{{QStringLiteral("role"), message.role},
                                         {QStringLiteral("content"), message.content}});
            }
        }
        if (!system.isEmpty()) {
            body.insert(QStringLiteral("system"), system.join(QStringLiteral("\n\n")));
        }
        body.insert(QStringLiteral("max_tokens"), config.maxTokens);
        request.url = QUrl(base + QStringLiteral("/messages"));
        request.headers.append(qMakePair(QByteArray("x-api-key"), config.apiKey.trimmed().toUtf8()));
        request.headers.append(qMakePair(QByteArray("anthropic-version"), QByteArray("2023-06-01")));
    } else {
        for (const Message &message : messages) {
            array.append(QJsonObject{{QStringLiteral("role"), message.role},
                                     {QStringLiteral("content"), message.content}});
        }
        if (config.kind == Kind::Ollama) {
            request.url = QUrl(base + QStringLiteral("/api/chat"));
        } else {
            request.url = QUrl(base + QStringLiteral("/chat/completions"));
            if (!config.apiKey.trimmed().isEmpty()) {
                request.headers.append(qMakePair(
                    QByteArray("Authorization"), "Bearer " + config.apiKey.trimmed().toUtf8()));
            }
        }
    }
    body.insert(QStringLiteral("messages"), array);
    request.headers.append(qMakePair(QByteArray("Content-Type"), QByteArray("application/json")));
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return request;
}

QString errorFromBody(const QByteArray &body)
{
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonValue error = root.value(QStringLiteral("error"));
    if (error.isObject()) {
        return error.toObject().value(QStringLiteral("message")).toString();
    }
    if (error.isString()) {
        return error.toString();
    }
    return root.value(QStringLiteral("message")).toString();
}

QStringList StreamParser::feed(const QByteArray &chunk)
{
    QStringList out;
    m_buffer += chunk;
    int newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        handleLine(line, &out);
    }
    return out;
}

void StreamParser::handleLine(const QByteArray &line, QStringList *out)
{
    if (m_finished) {
        return;
    }
    QByteArray payload = line;
    if (m_kind == Kind::Ollama) {
        if (payload.trimmed().isEmpty()) {
            return;
        }
    } else {
        if (payload.isEmpty()) {
            m_eventType.clear();
            return;
        }
        if (payload.startsWith("event:")) {
            m_eventType = QString::fromUtf8(payload.mid(6)).trimmed();
            return;
        }
        if (!payload.startsWith("data:")) {
            return;
        }
        payload = payload.mid(5).trimmed();
        if (payload == "[DONE]") {
            m_finished = true;
            return;
        }
    }
    const QJsonObject root = QJsonDocument::fromJson(payload).object();
    if (root.isEmpty()) {
        return;
    }
    const QString inband = errorFromBody(payload);
    if (!inband.isEmpty()) {
        m_error = inband;
        m_finished = true;
        return;
    }
    QString text;
    switch (m_kind) {
    case Kind::Anthropic: {
        const QString type = root.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("content_block_delta")) {
            text = root.value(QStringLiteral("delta")).toObject()
                       .value(QStringLiteral("text")).toString();
        } else if (type == QLatin1String("message_stop")) {
            m_finished = true;
        }
        break;
    }
    case Kind::Ollama:
        text = root.value(QStringLiteral("message")).toObject()
                   .value(QStringLiteral("content")).toString();
        if (root.value(QStringLiteral("done")).toBool()) {
            m_finished = true;
        }
        break;
    default: {
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty()) {
            const QJsonObject choice = choices.first().toObject();
            text = choice.value(QStringLiteral("delta")).toObject()
                       .value(QStringLiteral("content")).toString();
            if (!choice.value(QStringLiteral("finish_reason")).isNull()
                && !choice.value(QStringLiteral("finish_reason")).isUndefined()
                && text.isEmpty()) {
                m_finished = true;
            }
        }
        break;
    }
    }
    if (!text.isEmpty()) {
        out->append(text);
    }
}

} // namespace Ai
