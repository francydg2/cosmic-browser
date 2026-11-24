#include "browser/AiService.h"

#include "core/PasswordVault.h"
#include "core/Settings.h"

#include <QJsonObject>
#include <QUrl>
#include <QtGlobal>

namespace {

const auto kProvider = QStringLiteral("provider");
const auto kBaseUrl = QStringLiteral("baseUrl");
const auto kModel = QStringLiteral("model");
const auto kTimeout = QStringLiteral("timeout");
const auto kLanguage = QStringLiteral("language");

} // namespace

AiService::AiService(Settings *settings, PasswordVault *vault, const QString &conversationsPath,
                     QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_vault(vault)
    , m_conversations(conversationsPath)
{
    m_conversations.load();
}

Ai::ProviderConfig AiService::configWithoutKey() const
{
    const QJsonObject section = m_settings->section(QStringLiteral("ai"));
    Ai::ProviderConfig config;
    Ai::kindFromId(section.value(kProvider).toString(), &config.kind);
    config.baseUrl = section.contains(kBaseUrl) ? section.value(kBaseUrl).toString()
                                                : Ai::defaultBaseUrl(config.kind);
    config.model = section.contains(kModel) ? section.value(kModel).toString()
                                            : Ai::defaultModel(config.kind);
    config.timeoutSeconds = qBound(5, section.value(kTimeout).toInt(60), 600);
    return config;
}

Ai::ProviderConfig AiService::config() const
{
    Ai::ProviderConfig config = configWithoutKey();
    config.apiKey = resolveKey(config.kind);
    return config;
}

QString AiService::language() const
{
    const QString value = m_settings->section(QStringLiteral("ai")).value(kLanguage).toString();
    return value.isEmpty() ? QStringLiteral("the user's language") : value;
}

void AiService::setConfig(const Ai::ProviderConfig &config, const QString &language)
{
    QJsonObject section;
    section.insert(kProvider, Ai::kindId(config.kind));
    section.insert(kBaseUrl, config.baseUrl.trimmed());
    section.insert(kModel, config.model.trimmed());
    section.insert(kTimeout, qBound(5, config.timeoutSeconds, 600));
    section.insert(kLanguage, language.trimmed());
    m_settings->setSection(QStringLiteral("ai"), section);
    m_settings->save();
    emit configChanged();
}

QString AiService::vaultSite(Ai::Kind kind)
{
    return QStringLiteral("cosmic-ai:") + Ai::kindId(kind);
}

QString AiService::resolveKey(Ai::Kind kind) const
{
    if (m_vault && m_vault->isUnlocked()) {
        for (const VaultEntry &entry : m_vault->entries()) {
            if (entry.type == QLatin1String("ai-key") && entry.site == vaultSite(kind)) {
                return entry.password;
            }
        }
    }
    return qEnvironmentVariable(Ai::keyEnvironmentVariable(kind).toLatin1().constData());
}

AiService::KeySource AiService::keySource(Ai::Kind kind) const
{
    if (m_vault && m_vault->isUnlocked()) {
        for (const VaultEntry &entry : m_vault->entries()) {
            if (entry.type == QLatin1String("ai-key") && entry.site == vaultSite(kind)) {
                return KeySource::Vault;
            }
        }
    }
    if (!qEnvironmentVariable(Ai::keyEnvironmentVariable(kind).toLatin1().constData()).isEmpty()) {
        return KeySource::Environment;
    }
    return m_vault && m_vault->fileExists() && !m_vault->isUnlocked() ? KeySource::VaultLocked
                                                                       : KeySource::None;
}

bool AiService::storeKey(Ai::Kind kind, const QString &key, QString *error)
{
    const QString trimmed = key.trimmed();
    if (!m_vault || !m_vault->fileExists()) {
        if (error) {
            *error = tr("Create the password vault first (Settings → Passwords): API keys are "
                        "stored encrypted there.");
        }
        return false;
    }
    if (!m_vault->isUnlocked()) {
        if (error) {
            *error = tr("Unlock the password vault first (Settings → Passwords).");
        }
        return false;
    }
    removeKey(kind);
    if (trimmed.isEmpty()) {
        return true;
    }
    VaultEntry entry;
    entry.site = vaultSite(kind);
    entry.username = QStringLiteral("api-key");
    entry.password = trimmed;
    entry.type = QStringLiteral("ai-key");
    if (m_vault->addEntry(entry).isEmpty()) {
        if (error) {
            *error = m_vault->lastError();
        }
        return false;
    }
    emit configChanged();
    return true;
}

bool AiService::removeKey(Ai::Kind kind)
{
    if (!m_vault || !m_vault->isUnlocked()) {
        return false;
    }
    bool removed = false;
    for (const VaultEntry &entry : m_vault->entries()) {
        if (entry.type == QLatin1String("ai-key") && entry.site == vaultSite(kind)) {
            removed = m_vault->removeEntry(entry.id) || removed;
        }
    }
    return removed;
}

QString AiService::endpointKey(const Ai::ProviderConfig &config)
{
    const QUrl url(config.baseUrl.trimmed());
    return Ai::kindId(config.kind) + QLatin1Char('|') + url.host().toLower();
}

bool AiService::needsConsent(const Ai::ProviderConfig &config) const
{
    const QUrl url(config.baseUrl.trimmed());
    return !Ai::isLocalEndpoint(url) && !m_consented.contains(endpointKey(config));
}

void AiService::grantSessionConsent(const Ai::ProviderConfig &config)
{
    m_consented.insert(endpointKey(config));
}
