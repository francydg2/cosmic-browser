#include "core/AiConversations.h"

#include "core/Logging.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

namespace {

bool validRole(const QString &role)
{
    return role == QLatin1String("user") || role == QLatin1String("assistant");
}

QString titleFrom(const QString &text)
{
    QString title = text.simplified();
    if (title.size() > 60) {
        title = title.left(57) + QStringLiteral("…");
    }
    return title;
}

} // namespace

AiConversations::AiConversations(const QString &filePath)
    : m_path(filePath)
{
}

bool AiConversations::load()
{
    m_items.clear();
    QFile file(m_path);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcCosmic) << "AI conversations file is corrupt, left untouched" << m_path;
        return false;
    }
    for (const QJsonValue &value : doc.object().value(QStringLiteral("conversations")).toArray()) {
        const QJsonObject o = value.toObject();
        AiConversation c;
        c.id = o.value(QStringLiteral("id")).toString();
        if (c.id.isEmpty() || !find(c.id).id.isEmpty()) {
            continue;
        }
        c.title = o.value(QStringLiteral("title")).toString();
        c.createdMs = static_cast<qint64>(o.value(QStringLiteral("created")).toDouble());
        c.updatedMs = static_cast<qint64>(o.value(QStringLiteral("updated")).toDouble());
        for (const QJsonValue &m : o.value(QStringLiteral("messages")).toArray()) {
            const QJsonObject mo = m.toObject();
            const QString role = mo.value(QStringLiteral("role")).toString();
            if (validRole(role) && c.messages.size() < maxMessages) {
                c.messages.append({role, mo.value(QStringLiteral("content")).toString()});
            }
        }
        m_items.append(c);
        if (m_items.size() >= maxConversations) {
            break;
        }
    }
    return true;
}

bool AiConversations::save() const
{
    QJsonArray items;
    for (const AiConversation &c : m_items) {
        QJsonArray messages;
        for (const Ai::Message &m : c.messages) {
            messages.append(QJsonObject{{QStringLiteral("role"), m.role},
                                        {QStringLiteral("content"), m.content}});
        }
        items.append(QJsonObject{{QStringLiteral("id"), c.id}, {QStringLiteral("title"), c.title},
                                 {QStringLiteral("created"), static_cast<double>(c.createdMs)},
                                 {QStringLiteral("updated"), static_cast<double>(c.updatedMs)},
                                 {QStringLiteral("messages"), messages}});
    }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                         {QStringLiteral("conversations"), items}}).toJson());
    return file.commit();
}

AiConversation AiConversations::find(const QString &id) const
{
    for (const AiConversation &c : m_items) {
        if (c.id == id) {
            return c;
        }
    }
    return {};
}

QString AiConversations::create(const QString &title)
{
    AiConversation c;
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.title = titleFrom(title);
    c.createdMs = c.updatedMs = QDateTime::currentMSecsSinceEpoch();
    m_items.prepend(c);
    while (m_items.size() > maxConversations) {
        m_items.removeLast();
    }
    return c.id;
}

bool AiConversations::append(const QString &id, const Ai::Message &message)
{
    if (!validRole(message.role)) {
        return false;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id != id) {
            continue;
        }
        AiConversation c = m_items.takeAt(i);
        if (c.title.isEmpty() && message.role == QLatin1String("user")) {
            c.title = titleFrom(message.content);
        }
        c.messages.append(message);
        while (c.messages.size() > maxMessages) {
            c.messages.removeFirst();
        }
        c.updatedMs = QDateTime::currentMSecsSinceEpoch();
        m_items.prepend(c);
        return true;
    }
    return false;
}

bool AiConversations::updateLastAssistant(const QString &id, const QString &content)
{
    for (AiConversation &c : m_items) {
        if (c.id == id && !c.messages.isEmpty()
            && c.messages.last().role == QLatin1String("assistant")) {
            c.messages.last().content = content;
            return true;
        }
    }
    return false;
}

bool AiConversations::rename(const QString &id, const QString &title)
{
    for (AiConversation &c : m_items) {
        if (c.id == id) {
            c.title = titleFrom(title);
            return !c.title.isEmpty();
        }
    }
    return false;
}

bool AiConversations::remove(const QString &id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            m_items.removeAt(i);
            return true;
        }
    }
    return false;
}

void AiConversations::clear()
{
    m_items.clear();
}
