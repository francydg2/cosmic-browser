#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QUrl>

struct SessionTab
{
    QUrl url;        // invalid/empty = New Tab page
    bool pinned = false;
    QString group;   // group id, empty = ungrouped
    QString user;    // owning user id, empty = window default
};

struct SessionGroup
{
    QString id;
    QString name;
    QString color;
    bool collapsed = false;
};

struct SessionWindow
{
    QString user; // owning user id of the window's tabs
    QList<SessionGroup> groups;
    QList<SessionTab> tabs;
};

namespace Session {

bool save(const QString &filePath, const QList<SessionWindow> &windows);

QList<SessionWindow> load(const QString &filePath);

bool saveUsers(const QString &filePath, const QString &currentUser,
               const QMap<QString, QList<SessionWindow>> &byUser);

bool loadUsers(const QString &filePath, QString *currentUser,
               QMap<QString, QList<SessionWindow>> *byUser);

} // namespace Session
