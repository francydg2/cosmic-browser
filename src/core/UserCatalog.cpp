#include "core/UserCatalog.h"
#include "core/Logging.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace {

const auto kDefaultId = "default";
const auto kDefaultName = "Personal";
const auto kDefaultColor = "#7c6cf0";
constexpr int kFormatVersion = 1;

bool readUsersArray(const QJsonObject &root, const char *key, QList<BrowseUser> *out)
{
    const QJsonArray usersJson = root.value(QString::fromLatin1(key)).toArray();
    for (const QJsonValue &value : usersJson) {
        const QJsonObject object = value.toObject();
        BrowseUser user;
        user.id = object.value(QStringLiteral("id")).toString();
        user.name = object.value(QStringLiteral("name")).toString();
        user.color = object.value(QStringLiteral("color")).toString();
        if (user.id.isEmpty() || user.id == QLatin1String(kDefaultId)
            || user.name.isEmpty()) {
            continue;
        }
        bool duplicate = false;
        for (const BrowseUser &existing : *out) {
            if (existing.id == user.id) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            out->append(user);
        }
    }
    return true;
}

} // namespace

UserCatalog::UserCatalog(const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_path(filePath)
{
}

BrowseUser UserCatalog::defaultUser()
{
    return BrowseUser{QString::fromLatin1(kDefaultId), QString::fromLatin1(kDefaultName),
                      QString::fromLatin1(kDefaultColor)};
}

bool UserCatalog::load()
{
    m_users.clear();
    QFile file(m_path);
    if (!file.exists()) {
        const QString dir = m_path.left(m_path.lastIndexOf(QLatin1Char('/')));
        if (importFromContexts(dir + QStringLiteral("/contexts.json"))) {
            return true;
        }
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcCosmic) << "cannot read user catalog" << m_path;
        return false;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcCosmic).noquote()
            << "user catalog corrupt (" << error.errorString() << "); starting empty";
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt() != kFormatVersion) {
        qCWarning(lcCosmic) << "user catalog version unsupported";
        return false;
    }

    readUsersArray(root, "users", &m_users);
    qCInfo(lcCosmic).noquote() << "user catalog loaded:" << m_users.size()
                               << "user(s) from" << m_path;
    return true;
}

bool UserCatalog::importFromContexts(const QString &contextsPath)
{
    QFile file(contextsPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    QList<BrowseUser> imported;
    readUsersArray(doc.object(), "contexts", &imported);
    if (imported.isEmpty()) {
        return false;
    }
    m_users = imported;
    if (!save()) {
        m_users.clear();
        return false;
    }
    QFile::remove(contextsPath);
    qCInfo(lcCosmic).noquote() << "migrated" << m_users.size()
                               << "context(s) to users from" << contextsPath;
    return true;
}

bool UserCatalog::save() const
{
    QJsonArray usersJson;
    for (const BrowseUser &user : m_users) {
        usersJson.append(QJsonObject{{QStringLiteral("id"), user.id},
                                     {QStringLiteral("name"), user.name},
                                     {QStringLiteral("color"), user.color}});
    }
    const QJsonObject root{{QStringLiteral("version"), kFormatVersion},
                           {QStringLiteral("users"), usersJson}};

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcCosmic) << "cannot write user catalog" << m_path;
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qCWarning(lcCosmic) << "failed to commit user catalog" << m_path;
        return false;
    }
    qCInfo(lcCosmic).noquote() << "user catalog saved to" << m_path;
    return true;
}

QList<BrowseUser> UserCatalog::users() const
{
    return m_users;
}

BrowseUser UserCatalog::userById(const QString &id) const
{
    if (id.isEmpty() || id == QLatin1String(kDefaultId)) {
        return defaultUser();
    }
    for (const BrowseUser &user : m_users) {
        if (user.id == id) {
            return user;
        }
    }
    return BrowseUser{};
}

bool UserCatalog::hasUser(const QString &id) const
{
    if (id == QLatin1String(kDefaultId)) {
        return true;
    }
    for (const BrowseUser &user : m_users) {
        if (user.id == id) {
            return true;
        }
    }
    return false;
}

QString UserCatalog::addUser(const QString &name, const QString &color)
{
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) {
        return QString();
    }
    BrowseUser user;
    user.id = makeUniqueId(cleanName);
    user.name = cleanName;
    user.color = color.isEmpty() ? QString::fromLatin1(kDefaultColor) : color;
    m_users.append(user);
    emit changed();
    return user.id;
}

bool UserCatalog::renameUser(const QString &id, const QString &name)
{
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) {
        return false;
    }
    for (BrowseUser &user : m_users) {
        if (user.id == id) {
            if (user.name == cleanName) {
                return true;
            }
            user.name = cleanName;
            emit changed();
            return true;
        }
    }
    return false;
}

bool UserCatalog::recolorUser(const QString &id, const QString &color)
{
    if (color.isEmpty()) {
        return false;
    }
    for (BrowseUser &user : m_users) {
        if (user.id == id) {
            if (user.color == color) {
                return true;
            }
            user.color = color;
            emit changed();
            return true;
        }
    }
    return false;
}

bool UserCatalog::removeUser(const QString &id)
{
    if (id.isEmpty() || id == QLatin1String(kDefaultId)) {
        return false;
    }
    for (qsizetype i = 0; i < m_users.size(); ++i) {
        if (m_users.at(i).id == id) {
            m_users.removeAt(i);
            emit changed();
            return true;
        }
    }
    return false;
}

QString UserCatalog::makeUniqueId(const QString &name) const
{
    QString slug = name.toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    slug.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    if (slug.isEmpty()) {
        slug = QStringLiteral("user");
    }

    QString candidate = slug;
    int suffix = 2;
    auto exists = [this](const QString &id) {
        for (const BrowseUser &user : m_users) {
            if (user.id == id) {
                return true;
            }
        }
        return false;
    };
    while (candidate == QLatin1String(kDefaultId) || exists(candidate)) {
        candidate = slug + QLatin1Char('-') + QString::number(suffix++);
    }
    return candidate;
}
