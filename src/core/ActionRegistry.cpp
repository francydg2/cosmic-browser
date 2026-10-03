#include "core/ActionRegistry.h"
#include "core/Logging.h"

#include <QAction>

ActionRegistry::ActionRegistry(QObject *parent)
    : QObject(parent)
{
}

QAction *ActionRegistry::define(const QString &id, const QString &text,
                                const QKeySequence &shortcut, std::function<void()> handler)
{
    if (m_actions.contains(id)) {
        qCWarning(lcCosmic) << "duplicate action id" << id;
        return m_actions.value(id);
    }

    auto *action = new QAction(text, this);
    action->setObjectName(id);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
    }
    if (handler) {
        connect(action, &QAction::triggered, this, [handler = std::move(handler)] {
            handler();
        });
    }

    m_actions.insert(id, action);
    return action;
}

QAction *ActionRegistry::action(const QString &id) const
{
    return m_actions.value(id, nullptr);
}

QStringList ActionRegistry::ids() const
{
    return m_actions.keys();
}

QList<QAction *> ActionRegistry::allActions() const
{
    return m_actions.values();
}

void ActionRegistry::remove(const QString &id)
{
    QAction *action = m_actions.take(id);
    if (action) {
        action->deleteLater(); // QAction unregisters itself from all widgets
    }
}
