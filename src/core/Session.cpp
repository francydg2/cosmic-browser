#include "core/Session.h"
#include "core/Logging.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace Session {

namespace {

constexpr int kFormatVersion = 1;
constexpr int kUsersFormatVersion = 2;
const auto kDefaultUserId = "default";

QJsonObject tabToJson(const SessionTab &tab)
{
    return QJsonObject{
        {QStringLiteral("url"),
         tab.url.isValid() && !tab.url.isEmpty() ? tab.url.toString() : QString()},
        {QStringLiteral("pinned"), tab.pinned},
        {QStringLiteral("group"), tab.group},
        {QStringLiteral("user"), tab.user},
    };
}

SessionTab tabFromJson(const QJsonValue &value)
{
    SessionTab tab;
    if (value.isString()) {
        const QString text = value.toString();
        tab.url = text.isEmpty() ? QUrl() : QUrl(text);
        return tab;
    }
    const QJsonObject object = value.toObject();
    const QString text = object.value(QStringLiteral("url")).toString();
    tab.url = text.isEmpty() ? QUrl() : QUrl(text);
    tab.pinned = object.value(QStringLiteral("pinned")).toBool(false);
    tab.group = object.value(QStringLiteral("group")).toString();
    tab.user = object.value(QStringLiteral("user")).toString();
    if (tab.user.isEmpty()) {
        tab.user = object.value(QStringLiteral("context")).toString();
    }
    return tab;
}

QJsonObject windowToJson(const SessionWindow &window)
{
    QJsonArray tabsJson;
    for (const SessionTab &tab : window.tabs) {
        tabsJson.append(tabToJson(tab));
    }
    QJsonArray groupsJson;
    for (const SessionGroup &group : window.groups) {
        groupsJson.append(QJsonObject{{QStringLiteral("id"), group.id},
                                      {QStringLiteral("name"), group.name},
                                      {QStringLiteral("color"), group.color},
                                      {QStringLiteral("collapsed"), group.collapsed}});
    }
    return QJsonObject{{QStringLiteral("user"), window.user},
                       {QStringLiteral("tabs"), tabsJson},
                       {QStringLiteral("groups"), groupsJson}};
}

SessionWindow windowFromJson(const QJsonValue &windowValue)
{
    SessionWindow window;
    const QJsonObject windowObject = windowValue.toObject();
    window.user = windowObject.value(QStringLiteral("user")).toString();
    if (window.user.isEmpty()) {
        window.user = windowObject.value(QStringLiteral("context")).toString();
    }
    const QJsonArray tabsJson = windowObject.value(QStringLiteral("tabs")).toArray();
    for (const QJsonValue &tabValue : tabsJson) {
        window.tabs.append(tabFromJson(tabValue));
    }
    const QJsonArray groupsJson = windowObject.value(QStringLiteral("groups")).toArray();
    for (const QJsonValue &groupValue : groupsJson) {
        const QJsonObject groupObject = groupValue.toObject();
        SessionGroup group;
        group.id = groupObject.value(QStringLiteral("id")).toString();
        if (group.id.isEmpty()) {
            continue;
        }
        group.name = groupObject.value(QStringLiteral("name")).toString();
        group.color = groupObject.value(QStringLiteral("color")).toString();
        group.collapsed = groupObject.value(QStringLiteral("collapsed")).toBool(false);
        window.groups.append(group);
    }
    return window;
}

QList<SessionWindow> windowsFromArray(const QJsonArray &windowsJson)
{
    QList<SessionWindow> windows;
    for (const QJsonValue &windowValue : windowsJson) {
        windows.append(windowFromJson(windowValue));
    }
    return windows;
}

bool writeDocument(const QString &filePath, const QJsonObject &root)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcCosmic) << "cannot write session file" << filePath;
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        qCWarning(lcCosmic) << "failed to commit session file" << filePath;
        return false;
    }
    return true;
}

} // namespace

bool save(const QString &filePath, const QList<SessionWindow> &windows)
{
    QJsonArray windowsJson;
    for (const SessionWindow &window : windows) {
        windowsJson.append(windowToJson(window));
    }

    const QJsonObject root{{QStringLiteral("version"), kFormatVersion},
                           {QStringLiteral("windows"), windowsJson}};
    return writeDocument(filePath, root);
}

QList<SessionWindow> load(const QString &filePath)
{
    QList<SessionWindow> windows;
    QFile file(filePath);
    if (!file.exists()) {
        return windows;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcCosmic) << "cannot read session file" << filePath;
        return windows;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcCosmic).noquote()
            << "session file corrupt (" << error.errorString() << "); not restoring";
        return windows;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt() != kFormatVersion) {
        qCWarning(lcCosmic) << "session file version unsupported; not restoring";
        return windows;
    }

    return windowsFromArray(root.value(QStringLiteral("windows")).toArray());
}

bool saveUsers(const QString &filePath, const QString &currentUser,
               const QMap<QString, QList<SessionWindow>> &byUser)
{
    QJsonObject usersJson;
    for (auto it = byUser.constBegin(); it != byUser.constEnd(); ++it) {
        QJsonArray windowsJson;
        for (const SessionWindow &window : it.value()) {
            windowsJson.append(windowToJson(window));
        }
        usersJson.insert(it.key(), windowsJson);
    }
    const QJsonObject root{{QStringLiteral("version"), kUsersFormatVersion},
                           {QStringLiteral("current"), currentUser},
                           {QStringLiteral("users"), usersJson}};
    return writeDocument(filePath, root);
}

bool loadUsers(const QString &filePath, QString *currentUser,
               QMap<QString, QList<SessionWindow>> *byUser)
{
    QFile file(filePath);
    if (!file.exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcCosmic) << "cannot read session file" << filePath;
        return false;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcCosmic).noquote()
            << "session file corrupt (" << error.errorString() << "); not restoring";
        return false;
    }
    const QJsonObject root = doc.object();
    const int version = root.value(QStringLiteral("version")).toInt();
    if (version != kFormatVersion && version != kUsersFormatVersion) {
        qCWarning(lcCosmic) << "session file version unsupported; not restoring";
        return false;
    }

    QString current = QString::fromLatin1(kDefaultUserId);
    QMap<QString, QList<SessionWindow>> loaded;
    if (version == kFormatVersion) {
        loaded.insert(current, windowsFromArray(root.value(QStringLiteral("windows")).toArray()));
    } else {
        const QString stored = root.value(QStringLiteral("current")).toString();
        if (!stored.isEmpty()) {
            current = stored;
        }
        const QJsonObject usersJson = root.value(QStringLiteral("users")).toObject();
        for (auto it = usersJson.constBegin(); it != usersJson.constEnd(); ++it) {
            if (it.value().isArray()) {
                loaded.insert(it.key(), windowsFromArray(it.value().toArray()));
            }
        }
    }
    if (currentUser) {
        *currentUser = current;
    }
    if (byUser) {
        *byUser = loaded;
    }
    return true;
}

} // namespace Session
