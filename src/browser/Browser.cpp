#include "browser/Browser.h"
#include "core/Bookmarks.h"
#include "core/UserCatalog.h"
#include "core/Logging.h"
#include "core/PasswordVault.h"
#include "core/Paths.h"
#include "core/SearchEngines.h"
#include "core/Session.h"
#include "core/Settings.h"
#include "core/UrlUtils.h"
#include "web/BlocklistInterceptor.h"

#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

namespace {

const auto kSessionFileName = "session.json";
const auto kBookmarksFileName = "bookmarks.json";
const auto kVaultFileName = "vault.bin";
const auto kDefaultId = "default";

} // namespace

Browser::Browser()
    : m_settings(std::make_unique<Settings>(
          Paths::configDir() + QStringLiteral("/settings.json")))
{
    m_settings->load();
    m_users = std::make_unique<UserCatalog>(
        Paths::configDir() + QStringLiteral("/users.json"));
    m_users->load();
    m_currentUser = QString::fromLatin1(kDefaultId);
    loadUserSessions();
    m_bookmarks = std::make_unique<Bookmarks>(
        Paths::configDir() + QLatin1Char('/') + kBookmarksFileName);
    m_bookmarks->load();
    QObject::connect(m_bookmarks.get(), &Bookmarks::changed, m_bookmarks.get(),
                     &Bookmarks::save);
    m_vault = std::make_unique<PasswordVault>(
        Paths::configDir() + QLatin1Char('/') + kVaultFileName);
    m_uiConfig = UiLayout::defaultConfiguration();
    loadUiConfiguration();

    m_pagePolicy.httpsOnly = m_settings->httpsOnly();
    m_pagePolicy.setPermission(QStringLiteral("location"),
                               m_settings->permissionPolicy(QStringLiteral("location")));
    m_pagePolicy.setPermission(QStringLiteral("media"),
                               m_settings->permissionPolicy(QStringLiteral("media")));
    m_pagePolicy.setPermission(QStringLiteral("notifications"),
                               m_settings->permissionPolicy(QStringLiteral("notifications")));

    QObject::connect(m_settings.get(), &Settings::httpsOnlyChanged,
                     [this](bool enabled) { m_pagePolicy.httpsOnly = enabled; });
    QObject::connect(m_settings.get(), &Settings::permissionPolicyChanged,
                     [this](const QString &feature, const QString &policy) {
                         m_pagePolicy.setPermission(feature, policy);
                     });
    QObject::connect(m_settings.get(), &Settings::privacyChanged,
                     [this] { applyPrivacyPolicy(); });

    configureDefaultProfile();
}

Browser::~Browser()
{
    // Non-default profiles are heap-allocated here; QtWebEngine requires
    // them gone while QApplication is still alive (main() destroys the
    // Browser before the app, and tests do the same via delete order).
    qDeleteAll(m_profiles);
    m_profiles.clear();
}

void Browser::setWindowFactory(WindowFactory factory)
{
    m_factory = std::move(factory);
}

AppWindow *Browser::newWindow()
{
    if (!m_factory) {
        qCCritical(lcCosmic) << "no window factory set";
        return nullptr;
    }
    AppWindow *window = m_factory();
    qCInfo(lcCosmic) << "window created" << window;
    return window;
}

QWebEngineProfile *Browser::profileForUser(const QString &userId)
{
    const QString id = userId.isEmpty() ? QString::fromLatin1(kDefaultId) : userId;
    if (id == QLatin1String(kDefaultId)) {
        return QWebEngineProfile::defaultProfile();
    }
    if (QWebEngineProfile *existing = m_profiles.value(id, nullptr)) {
        return existing;
    }
    if (!m_users || !m_users->hasUser(id)) {
        qCWarning(lcCosmic) << "unknown user id; falling back to default profile" << id;
        return QWebEngineProfile::defaultProfile();
    }

    // One storage partition per user: cookies, localStorage and
    // cache live under XDG data/cache, never in ~.
    auto *profile = new QWebEngineProfile(QStringLiteral("cosmic.") + id);
    profile->setPersistentStoragePath(Paths::dataDir() + QStringLiteral("/profiles/") + id);
    profile->setCachePath(Paths::cacheDir() + QStringLiteral("/profiles/") + id);
    profile->setPersistentCookiesPolicy(m_settings->sessionCookiesOnly()
                                            ? QWebEngineProfile::NoPersistentCookies
                                            : QWebEngineProfile::ForcePersistentCookies);
    if (m_interceptor) {
        profile->setUrlRequestInterceptor(m_interceptor);
    }
    m_profiles.insert(id, profile);
    qCInfo(lcCosmic).noquote() << "context profile created for" << id << "->"
                               << profile->persistentStoragePath();
    return profile;
}

bool Browser::loadUiConfiguration()
{
    UIConfiguration loaded;
    if (!UiLayout::loadFile(UiLayout::configFilePath(), &loaded)) {
        return false;
    }
    m_uiConfig = loaded;
    return true;
}

bool Browser::saveUiConfiguration(const UIConfiguration &cfg)
{
    if (!UiLayout::saveFile(cfg, UiLayout::configFilePath())) {
        return false;
    }
    m_uiConfig = cfg;
    return true;
}

void Browser::clearBrowsingData()
{    QStringList ids{QString::fromLatin1(kDefaultId)};
    if (m_users) {
        for (const BrowseUser &user : m_users->users()) {
            ids.append(user.id);
        }
    }
    for (const QString &id : ids) {
        QWebEngineProfile *profile = profileForUser(id);
        if (!profile) {
            continue;
        }
        if (QWebEngineCookieStore *cookies = profile->cookieStore()) {
            cookies->deleteAllCookies();
        }
        profile->clearHttpCache();
    }
    qCInfo(lcCosmic).noquote() << "browsing data cleared for" << ids.size()
                               << "profile(s)";
}

QUrl Browser::resolveInput(const QString &text) const
{
    QString templateString;
    const QString engineId = m_settings->defaultSearchEngine();
    if (engineId == QLatin1String("custom")) {
        templateString = m_settings->customSearchTemplate();
    } else if (const SearchEngine *engine = SearchEngines::builtinById(engineId)) {
        templateString = engine->urlTemplate;
    }
    // UrlUtils falls back to the default engine for empty/invalid templates.
    return UrlUtils::toNavigableUrl(text, templateString);
}

SessionWindow Browser::userSession(const QString &userId) const
{
    const QList<SessionWindow> states = m_userSessions.value(userId);
    return states.isEmpty() ? SessionWindow{} : states.first();
}

void Browser::storeUserSession(const QString &userId, const SessionWindow &state)
{
    if (userId.isEmpty()) {
        return;
    }
    m_userSessions[userId] = {state};
    Session::saveUsers(sessionFilePath(), m_currentUser, m_userSessions);
}

void Browser::setCurrentUserId(const QString &userId)
{
    const QString id = userId.isEmpty() ? QString::fromLatin1(kDefaultId) : userId;
    if (m_currentUser == id) {
        return;
    }
    if (id != QLatin1String(kDefaultId) && (!m_users || !m_users->hasUser(id))) {
        qCWarning(lcCosmic) << "unknown user id; keeping" << m_currentUser;
        return;
    }
    m_currentUser = id;
    Session::saveUsers(sessionFilePath(), m_currentUser, m_userSessions);
}

bool Browser::loadUserSessions()
{
    QString current;
    QMap<QString, QList<SessionWindow>> loaded;
    if (!Session::loadUsers(sessionFilePath(), &current, &loaded)) {
        return false;
    }
    m_userSessions = loaded;
    if (!current.isEmpty()
        && (current == QLatin1String(kDefaultId)
            || (m_users && m_users->hasUser(current)))) {
        m_currentUser = current;
    }
    qCInfo(lcCosmic).noquote() << "user sessions loaded for" << m_userSessions.size()
                               << "user(s) from" << sessionFilePath();
    return true;
}

bool Browser::saveUserSessions(const QList<AppWindow *> &windows)
{
    QMap<QString, QList<SessionWindow>> open;
    for (AppWindow *window : windows) {
        if (window) {
            open[window->userId()].append(window->sessionState());
        }
    }
    QMap<QString, QList<SessionWindow>> merged = m_userSessions;
    for (auto it = open.constBegin(); it != open.constEnd(); ++it) {
        merged[it.key()] = it.value();
    }
    m_userSessions = merged;
    const bool ok = Session::saveUsers(sessionFilePath(), m_currentUser, m_userSessions);
    if (ok) {
        qCInfo(lcCosmic).noquote() << "user sessions saved for" << merged.size()
                                   << "user(s) to" << sessionFilePath();
    }
    return ok;
}

QString Browser::sessionFilePath() const
{
    return Paths::configDir() + QLatin1Char('/') + QLatin1String(kSessionFileName);
}

void Browser::configureDefaultProfile()
{
    // Spec §55: all persistent data lives in XDG directories, never in ~.
    Paths::ensureDirs();
    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    profile->setPersistentStoragePath(Paths::dataDir() + QStringLiteral("/profile"));
    profile->setCachePath(Paths::cacheDir() + QStringLiteral("/http-cache"));
    profile->setPersistentCookiesPolicy(m_settings->sessionCookiesOnly()
                                            ? QWebEngineProfile::NoPersistentCookies
                                            : QWebEngineProfile::ForcePersistentCookies);
    qCInfo(lcCosmic).noquote() << "profile storage:" << profile->persistentStoragePath();

    // Request-level blocking. The interceptor is parented to the default
    // profile (outlives Browser safely) and holds only an atomic flag —
    // Settings is never dereferenced from engine threads. Context profiles
    // attach the same instance (identical rules everywhere).
    m_interceptor = new BlocklistInterceptor(m_settings->requestBlockingEnabled(), profile);
    m_interceptor->loadUserRules(
        Paths::configDir() + QStringLiteral("/blocklist.txt"));
    m_interceptor->setDoNotTrack(m_settings->doNotTrack());
    profile->setUrlRequestInterceptor(m_interceptor);
    QObject::connect(m_settings.get(), &Settings::requestBlockingChanged, m_interceptor,
                     &BlocklistInterceptor::setEnabled);
    QObject::connect(m_settings.get(), &Settings::privacyChanged,
                     [this] { m_interceptor->setDoNotTrack(m_settings->doNotTrack()); });

    qCInfo(lcCosmic).noquote()
        << "request blocking:" << (m_interceptor->isEnabled() ? "enabled" : "disabled")
        << "| DNT:" << (m_interceptor->isDoNotTrack() ? "on" : "off")
        << "|" << m_interceptor->ruleCount() << "rules | search engine:"
        << m_settings->defaultSearchEngine();
}

void Browser::applyPrivacyPolicy()
{
    const auto policy = m_settings->sessionCookiesOnly()
        ? QWebEngineProfile::NoPersistentCookies
        : QWebEngineProfile::ForcePersistentCookies;
    QWebEngineProfile::defaultProfile()->setPersistentCookiesPolicy(policy);
    for (QWebEngineProfile *profile : std::as_const(m_profiles)) {
        profile->setPersistentCookiesPolicy(policy);
    }
    if (m_interceptor) {
        m_interceptor->setDoNotTrack(m_settings->doNotTrack());
    }
}
