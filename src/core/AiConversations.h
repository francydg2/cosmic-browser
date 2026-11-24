#pragma once

#include "core/AiProvider.h"

#include <QList>
#include <QString>

struct AiConversation
{
    QString id;
    QString title;
    qint64 createdMs = 0;
    qint64 updatedMs = 0;
    QList<Ai::Message> messages; // only what the user typed and the replies
};

/// Local conversation history (`ai-conversations.json`). Page contents
/// sent as context are never stored, only the visible messages. Capped,
/// newest first, atomic writes; a corrupt file is left untouched.
class AiConversations
{
public:
    static constexpr int maxConversations = 100;
    static constexpr int maxMessages = 200;

    explicit AiConversations(const QString &filePath);

    bool load();
    bool save() const;

    QList<AiConversation> conversations() const { return m_items; }
    AiConversation find(const QString &id) const;
    QString create(const QString &title = QString());
    bool append(const QString &id, const Ai::Message &message);
    /// Replaces the text of the last assistant message (used while streaming).
    bool updateLastAssistant(const QString &id, const QString &content);
    bool rename(const QString &id, const QString &title);
    bool remove(const QString &id);
    void clear();

private:
    QString m_path;
    QList<AiConversation> m_items;
};
