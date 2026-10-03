#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

/// Persistent application settings (Phase 2), stored as JSON in
/// `~/.config/nebula/settings.json` (XDG config dir).
///
/// Pure data + change signals: no UI, no WebEngine. Load/Save are explicit
/// so tests can use any temporary path. Setters update the in-memory value
/// and emit a signal immediately (live application); persistence happens
/// when the Settings dialog (or any caller) invokes save().
class Settings : public QObject
{
    Q_OBJECT

public:
    explicit Settings(const QString &filePath, QObject *parent = nullptr);

    /// Reads the file. Returns false when missing or corrupt (defaults
    /// stay in effect; a corrupt file is logged and left untouched).
    bool load();
    /// Writes the file atomically (QSaveFile). Returns false on I/O error.
    bool save() const;

    QString filePath() const { return m_path; }

    QString defaultSearchEngine() const;
    void setDefaultSearchEngine(const QString &id);

    /// User-defined search template (contains %1), used when the default
    /// engine is "custom".
    QString customSearchTemplate() const;
    void setCustomSearchTemplate(const QString &urlTemplate);

    bool requestBlockingEnabled() const;
    void setRequestBlockingEnabled(bool enabled);

    bool restoreSessionOnStartup() const;
    void setRestoreSessionOnStartup(bool restore);

    /// Do Not Track header on every request (privacy.dnt, default true).
    bool doNotTrack() const;
    void setDoNotTrack(bool enabled);

    /// Keep cookies only for the session: no persistent cookie jar
    /// (privacy.sessionCookiesOnly, default false).
    bool sessionCookiesOnly() const;
    void setSessionCookiesOnly(bool enabled);

    /// Feature permission policy, Chromium style. feature: "location",
    /// "media" (camera+mic) or "notifications"; policy: "ask" (default),
    /// "allow" or "block".
    QString permissionPolicy(const QString &feature) const;
    void setPermissionPolicy(const QString &feature, const QString &policy);

    /// HTTPS-Only: upgrade top-level http:// navigations to https://
    /// (https.only, default false).
    bool httpsOnly() const;
    void setHttpsOnly(bool enabled);

    /// Accent color for the stylesheet, "#rrggbb"
    /// (appearance.accent, default violet #7c6cf0).
    QString accentColor() const;
    void setAccentColor(const QString &hex);

    /// Tab strip orientation: "horizontal" (Dia style, default) or
    /// "vertical" (Zen style sidebar).
    QString tabPosition() const;
    void setTabPosition(const QString &position);

    /// RAM Saver: discard background tabs (they reload on activation)
    /// to keep renderer memory down (performance.ramSaver, default false).
    bool ramSaverEnabled() const;
    void setRamSaverEnabled(bool enabled);

signals:
    void defaultSearchEngineChanged(const QString &id);
    void requestBlockingChanged(bool enabled);
    void restoreSessionChanged(bool restore);
    void privacyChanged();
    void permissionPolicyChanged(const QString &feature, const QString &policy);
    void httpsOnlyChanged(bool enabled);
    void accentColorChanged(const QString &hex);
    void tabPositionChanged(const QString &position);
    void ramSaverChanged(bool enabled);

private:
    QString m_path;
    QJsonObject m_data;
};
