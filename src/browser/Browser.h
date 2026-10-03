#pragma once

#include "browser/AppWindow.h"
#include "core/UILayout.h"
#include "web/PagePolicy.h"

#include <QHash>
#include <QMap>
#include <QUrl>
#include <functional>
#include <memory>

class Settings;
class UserCatalog;
class BlocklistInterceptor;
class Bookmarks;
class PasswordVault;
class QWebEngineProfile;

/// Application-level orchestration: creates windows through an injected
/// factory (so browser/ never depends on ui/), owns Settings, the
/// user catalog and the request-blocking interceptor, resolves
/// address-bar input against the configured search engine, provides one
/// WebEngine profile per user, and persists/restores per-user sessions.
class Browser
{
public:
    using WindowFactory = std::function<AppWindow *()>;

    Browser();
    ~Browser();

    void setWindowFactory(WindowFactory factory);

    /// Creates a window through the factory. Returns nullptr if no factory
    /// is set (logged as critical).
    AppWindow *newWindow();

    /// Never null; loaded from XDG config (defaults if missing/corrupt).
    Settings *settings() const { return m_settings.get(); }

    /// Never null; loaded from XDG config (default: empty catalog).
    UserCatalog *userCatalog() const { return m_users.get(); }

    /// Never null; loaded from XDG config (default: empty list).
    Bookmarks *bookmarks() const { return m_bookmarks.get(); }

    /// Never null; encrypted vault file in the XDG config dir
    /// (created on first use from Settings → Passwords).
    PasswordVault *passwordVault() const { return m_vault.get(); }

    /// Attached to every web profile; never null.
    BlocklistInterceptor *requestInterceptor() const { return m_interceptor; }

    /// Live policy shared with every WebPage (HTTPS-only, permission
    /// rules); kept in sync with Settings by this class.
    const PagePolicy *pagePolicy() const { return &m_pagePolicy; }

    /// The WebEngine profile for a user. "default" (or an unknown id)
    /// is the main XDG profile; other users get their own storage
    /// under the XDG data/cache dirs. Created on first use and
    /// configured with the same request-blocking interceptor.
    QWebEngineProfile *profileForUser(const QString &userId);

    /// Turns address-bar/New-Tab input into a URL using the configured
    /// search engine (URLs pass through UrlUtils rules unchanged).
    QUrl resolveInput(const QString &text) const;

    /// Per-user remembered tab sets (session.json, format version 2:
    /// {"current": id, "users": {id: [windows]}}). Version 1 files
    /// migrate their windows to the default user. Users with no open
    /// window keep their stored state.
    QMap<QString, QList<SessionWindow>> userSessions() const { return m_userSessions; }
    SessionWindow userSession(const QString &userId) const;
    void storeUserSession(const QString &userId, const SessionWindow &state);
    QString currentUserId() const { return m_currentUser; }
    void setCurrentUserId(const QString &userId);
    bool loadUserSessions();
    bool saveUserSessions(const QList<AppWindow *> &windows);
    QString sessionFilePath() const;

    /// UI layout (toolbar zones, tab bar, sidebar) from ui.json in the
    /// XDG config dir. Invalid or missing files fall back to the
    /// built-in default without touching the disk.
    UIConfiguration uiConfiguration() const { return m_uiConfig; }
    bool loadUiConfiguration();
    bool saveUiConfiguration(const UIConfiguration &cfg);

    /// Deletes all cookies and the HTTP cache of every profile
    /// (main + users). Used by Settings → Privacy → Clear now.
    void clearBrowsingData();

private:
    void configureDefaultProfile();
    void applyPrivacyPolicy();

    WindowFactory m_factory;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<UserCatalog> m_users;
    std::unique_ptr<Bookmarks> m_bookmarks;
    std::unique_ptr<PasswordVault> m_vault;
    BlocklistInterceptor *m_interceptor = nullptr; // owned by the default profile
    QHash<QString, QWebEngineProfile *> m_profiles; // user id → profile
    PagePolicy m_pagePolicy;
    UIConfiguration m_uiConfig;
    QMap<QString, QList<SessionWindow>> m_userSessions;
    QString m_currentUser;
};
