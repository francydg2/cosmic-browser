#pragma once

#include "core/AiConversations.h"
#include "core/AiProvider.h"

#include <QObject>
#include <QSet>

class Settings;
class PasswordVault;

/// Application-level AI configuration: provider settings (from
/// Settings, no secrets), API-key lookup (encrypted vault first, then an
/// environment variable), the saved conversations and the per-session
/// record of which external providers the user agreed to send page
/// content to.
class AiService : public QObject
{
    Q_OBJECT

public:
    enum class KeySource { None, Vault, VaultLocked, Environment };

    AiService(Settings *settings, PasswordVault *vault, const QString &conversationsPath,
              QObject *parent = nullptr);

    /// Current provider settings with the key resolved (may be empty).
    Ai::ProviderConfig config() const;
    /// Settings without any secret, for editing.
    Ai::ProviderConfig configWithoutKey() const;
    QString language() const;
    void setConfig(const Ai::ProviderConfig &config, const QString &language);

    KeySource keySource(Ai::Kind kind) const;
    /// Stores the key in the unlocked vault. Returns false and sets
    /// \p error when the vault is missing or locked.
    bool storeKey(Ai::Kind kind, const QString &key, QString *error);
    bool removeKey(Ai::Kind kind);

    /// Page content may leave the machine for this endpoint only after
    /// consent; loopback endpoints never need it.
    bool needsConsent(const Ai::ProviderConfig &config) const;
    void grantSessionConsent(const Ai::ProviderConfig &config);

    AiConversations *conversations() { return &m_conversations; }

signals:
    void configChanged();

private:
    QString resolveKey(Ai::Kind kind) const;
    static QString vaultSite(Ai::Kind kind);
    static QString endpointKey(const Ai::ProviderConfig &config);

    Settings *m_settings;
    PasswordVault *m_vault;
    AiConversations m_conversations;
    QSet<QString> m_consented;
};
