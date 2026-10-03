#include "core/Settings.h"
#include "core/Logging.h"

#include <QColor>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

const auto kKeyEngine = "search.engine";
const auto kKeyCustomTemplate = "search.customTemplate";
const auto kKeyBlocking = "blocking.enabled";
const auto kKeyRestore = "session.restore";
const auto kKeyDnt = "privacy.dnt";
const auto kKeySessionCookies = "privacy.sessionCookiesOnly";
const auto kKeyPermLocation = "privacy.perm.location";
const auto kKeyPermMedia = "privacy.perm.media";
const auto kKeyPermNotifications = "privacy.perm.notifications";
const auto kKeyHttpsOnly = "https.only";
const auto kKeyAccent = "appearance.accent";
const auto kKeyTabPosition = "tabs.position";
const auto kKeyRamSaver = "performance.ramSaver";

const auto kDefaultEngine = "duckduckgo";
const auto kDefaultAccent = "#7c6cf0";
const auto kPolicyAsk = "ask";
const auto kPositionHorizontal = "horizontal";
const auto kPositionVertical = "vertical";

} // namespace

Settings::Settings(const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_path(filePath)
{
    // Built-in defaults; a loaded file overrides key-by-key below.
    m_data.insert(QString::fromLatin1(kKeyEngine), QString::fromLatin1(kDefaultEngine));
    m_data.insert(QString::fromLatin1(kKeyCustomTemplate), QString());
    m_data.insert(QString::fromLatin1(kKeyBlocking), true);
    m_data.insert(QString::fromLatin1(kKeyRestore), false);
    m_data.insert(QString::fromLatin1(kKeyDnt), true);
    m_data.insert(QString::fromLatin1(kKeySessionCookies), false);
    m_data.insert(QString::fromLatin1(kKeyPermLocation), QString::fromLatin1(kPolicyAsk));
    m_data.insert(QString::fromLatin1(kKeyPermMedia), QString::fromLatin1(kPolicyAsk));
    m_data.insert(QString::fromLatin1(kKeyPermNotifications), QString::fromLatin1(kPolicyAsk));
    m_data.insert(QString::fromLatin1(kKeyHttpsOnly), false);
    m_data.insert(QString::fromLatin1(kKeyAccent), QString::fromLatin1(kDefaultAccent));
    m_data.insert(QString::fromLatin1(kKeyTabPosition), QString::fromLatin1(kPositionHorizontal));
    m_data.insert(QString::fromLatin1(kKeyRamSaver), false);
}

bool Settings::load()
{
    QFile file(m_path);
    if (!file.exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcCosmic) << "cannot read settings file" << m_path;
        return false;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcCosmic).noquote()
            << "settings file corrupt (" << error.errorString() << "); using defaults";
        return false;
    }

    // Merge: known keys from file, missing keys keep the defaults.
    const QJsonObject loaded = doc.object();
    for (auto it = m_data.constBegin(); it != m_data.constEnd(); ++it) {
        if (loaded.contains(it.key()) && loaded.value(it.key()).type() == it.value().type()) {
            m_data.insert(it.key(), loaded.value(it.key()));
        }
    }
    return true;
}

bool Settings::save() const
{
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcCosmic) << "cannot write settings file" << m_path;
        return false;
    }
    file.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qCWarning(lcCosmic) << "failed to commit settings file" << m_path;
        return false;
    }
    qCInfo(lcCosmic).noquote() << "settings saved to" << m_path;
    return true;
}

QString Settings::defaultSearchEngine() const
{
    return m_data.value(QString::fromLatin1(kKeyEngine)).toString();
}

void Settings::setDefaultSearchEngine(const QString &id)
{
    const QString value = id.isEmpty() ? QString::fromLatin1(kDefaultEngine) : id;
    if (defaultSearchEngine() == value) {
        return;
    }
    m_data.insert(QString::fromLatin1(kKeyEngine), value);
    emit defaultSearchEngineChanged(value);
}

QString Settings::customSearchTemplate() const
{
    return m_data.value(QString::fromLatin1(kKeyCustomTemplate)).toString();
}

void Settings::setCustomSearchTemplate(const QString &urlTemplate)
{
    if (customSearchTemplate() == urlTemplate) {
        return;
    }
    m_data.insert(QString::fromLatin1(kKeyCustomTemplate), urlTemplate);
}

bool Settings::requestBlockingEnabled() const
{
    return m_data.value(QString::fromLatin1(kKeyBlocking)).toBool(true);
}

void Settings::setRequestBlockingEnabled(bool enabled)
{
    if (requestBlockingEnabled() == enabled) {
        return;
    }
    m_data.insert(QString::fromLatin1(kKeyBlocking), enabled);
    emit requestBlockingChanged(enabled);
}

bool Settings::restoreSessionOnStartup() const
{
    return m_data.value(QString::fromLatin1(kKeyRestore)).toBool(false);
}

void Settings::setRestoreSessionOnStartup(bool restore)
{
    if (restoreSessionOnStartup() == restore) {
        return;
    }
    m_data.insert(QString::fromLatin1(kKeyRestore), restore);
    emit restoreSessionChanged(restore);
}

bool Settings::doNotTrack() const
{
    return m_data.value(QString::fromLatin1(kKeyDnt)).toBool(true);
}

void Settings::setDoNotTrack(bool enabled)
{
    if (doNotTrack() == enabled) {
        return;
    }
    m_data.insert(QString::fromLatin1(kKeyDnt), enabled);
    emit privacyChanged();
}

bool Settings::sessionCookiesOnly() const
{
    return m_data.value(QString::fromLatin1(kKeySessionCookies)).toBool(false);
}

void Settings::setSessionCookiesOnly(bool enabled)
{
    if (sessionCookiesOnly() == enabled) {
        return;
    }
    m_data.insert(QString::fromLatin1(kKeySessionCookies), enabled);
    emit privacyChanged();
}

QString Settings::permissionPolicy(const QString &feature) const
{
    const QString key = feature == QLatin1String("location")
        ? QString::fromLatin1(kKeyPermLocation)
        : feature == QLatin1String("media")
            ? QString::fromLatin1(kKeyPermMedia)
            : QString::fromLatin1(kKeyPermNotifications);
    const QString policy = m_data.value(key).toString();
    if (policy == QLatin1String("allow") || policy == QLatin1String("block")) {
        return policy;
    }
    return QString::fromLatin1(kPolicyAsk);
}

void Settings::setPermissionPolicy(const QString &feature, const QString &policy)
{
    if (policy != QLatin1String("ask") && policy != QLatin1String("allow")
        && policy != QLatin1String("block")) {
        return;
    }
    const QString key = feature == QLatin1String("location")
        ? QString::fromLatin1(kKeyPermLocation)
        : feature == QLatin1String("media")
            ? QString::fromLatin1(kKeyPermMedia)
            : QString::fromLatin1(kKeyPermNotifications);
    if (m_data.value(key).toString() == policy) {
        return;
    }
    m_data.insert(key, policy);
    emit permissionPolicyChanged(feature, policy);
}

bool Settings::httpsOnly() const
{
    return m_data.value(QString::fromLatin1(kKeyHttpsOnly)).toBool(false);
}

void Settings::setHttpsOnly(bool enabled)
{
    if (httpsOnly() == enabled) {
        return;
    }
    m_data.insert(QString::fromLatin1(kKeyHttpsOnly), enabled);
    emit httpsOnlyChanged(enabled);
}

QString Settings::accentColor() const
{
    const QString hex = m_data.value(QString::fromLatin1(kKeyAccent)).toString();
    return hex.isEmpty() ? QString::fromLatin1(kDefaultAccent) : hex;
}

void Settings::setAccentColor(const QString &hex)
{
    const QColor color(hex);
    if (!color.isValid() || color.alpha() != 255) {
        return;
    }
    const QString value = color.name();
    if (accentColor() == value) {
        return;
    }
    m_data.insert(QString::fromLatin1(kKeyAccent), value);
    emit accentColorChanged(value);
}

QString Settings::tabPosition() const
{
    const QString position = m_data.value(QString::fromLatin1(kKeyTabPosition)).toString();
    return position == QString::fromLatin1(kPositionVertical)
        ? QString::fromLatin1(kPositionVertical)
        : QString::fromLatin1(kPositionHorizontal);
}

void Settings::setTabPosition(const QString &position)
{
    if (position != QLatin1String("horizontal") && position != QLatin1String("vertical")) {
        return;
    }
    if (tabPosition() == position) {
        return;
    }
    m_data.insert(QString::fromLatin1(kKeyTabPosition), position);
    emit tabPositionChanged(position);
}

bool Settings::ramSaverEnabled() const
{
    return m_data.value(QString::fromLatin1(kKeyRamSaver)).toBool(false);
}

void Settings::setRamSaverEnabled(bool enabled)
{
    if (ramSaverEnabled() == enabled) {
        return;
    }
    m_data.insert(QString::fromLatin1(kKeyRamSaver), enabled);
    emit ramSaverChanged(enabled);
}
