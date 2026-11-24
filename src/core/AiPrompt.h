#pragma once

#include "core/AiProvider.h"

#include <QList>
#include <QString>
#include <QUrl>

namespace Ai {

struct PageContext
{
    QString title;
    QUrl url;
    QString text; // visible text only; never cookies, forms values or headers
};

/// Characters of page text sent per page / in total.
constexpr int maxCharsPerPage = 12000;
constexpr int maxCharsTotal = 30000;

/// Address safe to send: no credentials, query or fragment (those can
/// carry tokens). Empty when not an http(s) address.
QString safeAddress(const QUrl &url);

/// Clips text on a character boundary and marks the cut.
QString clipText(const QString &text, int limit);

/// System prompt: the assistant must treat page text as untrusted data,
/// not as instructions.
QString systemPrompt(const QString &language);

/// Message list for one turn: system prompt, earlier messages (page
/// context is never replayed), then the new user message with the page
/// contexts attached as quoted data.
QList<Message> buildMessages(const QList<Message> &history, const QString &question,
                             const QList<PageContext> &pages, const QString &language);

} // namespace Ai
