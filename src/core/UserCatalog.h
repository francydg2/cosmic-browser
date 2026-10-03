#pragma once

#include <QList>
#include <QObject>
#include <QString>

struct BrowseUser
{
    QString id;
    QString name;
    QString color;
};

class UserCatalog : public QObject
{
    Q_OBJECT

public:
    explicit UserCatalog(const QString &filePath, QObject *parent = nullptr);

    bool load();
    bool save() const;

    QString filePath() const { return m_path; }

    static BrowseUser defaultUser();

    QList<BrowseUser> users() const;

    BrowseUser userById(const QString &id) const;

    bool hasUser(const QString &id) const;

    QString addUser(const QString &name, const QString &color);

    bool renameUser(const QString &id, const QString &name);
    bool recolorUser(const QString &id, const QString &color);

    bool removeUser(const QString &id);

signals:
    void changed();

private:
    bool importFromContexts(const QString &contextsPath);
    QString makeUniqueId(const QString &name) const;

    QString m_path;
    QList<BrowseUser> m_users;
};
