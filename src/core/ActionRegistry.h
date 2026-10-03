#pragma once

#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

class QAction;

/// Central registration point for every user-facing action and its shortcut
/// (spec §8, §38). Widgets never define shortcuts themselves; they either
/// trigger an action registered here or expose a toolbar button bound to one.
///
/// One registry instance is owned by each BrowserWindow: handlers are
/// window-scoped (they act on that window's tabs). The action ids are stable
/// and global — the Phase 3 command palette will enumerate them.
class ActionRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ActionRegistry(QObject *parent = nullptr);

    /// Creates a QAction with \p id, \p text and \p shortcut; \p handler runs
    /// on trigger. Returns the existing action (and warns) if \p id is taken.
    QAction *define(const QString &id, const QString &text,
                    const QKeySequence &shortcut, std::function<void()> handler);

    QAction *action(const QString &id) const;
    QStringList ids() const;
    QList<QAction *> allActions() const;

    /// Removes and destroys the action with \p id (used for dynamic,
    /// state-driven actions such as group assignment entries).
    void remove(const QString &id);

private:
    QHash<QString, QAction *> m_actions;
};
