#pragma once

#include <QAtomicInt>
#include <QSet>
#include <QUrl>
#include <QWebEngineUrlRequestInterceptor>

/// Request-level ad/tracker blocking (Phase 2) + Do Not Track header.
///
/// Honest scope (Qt WebEngine limits): this cancels NETWORK REQUESTS to
/// known tracker/ad domains (exact host or subdomain). It does NOT remove
/// already-rendered DOM elements (cosmetic filtering) — engine APIs for
/// that do not exist. Users can disable it in Settings and add their own
/// rules in `~/.config/nebula/blocklist.txt`.
///
/// The enabled/DNT flags are atomic copies (updated by browser/ from
/// Settings): interceptRequest may run off the GUI thread, so the
/// interceptor never dereferences Settings directly.
class BlocklistInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

public:
    explicit BlocklistInterceptor(bool enabled = true, QObject *parent = nullptr);

    /// Loads `:/blocklists/default.txt` (bundled, one host per line).
    void loadBuiltinRules();
    /// Loads the user file if present (same format, `#` comments).
    void loadUserRules(const QString &filePath);

    /// Pure decision: enabled AND host matches a rule (exact or
    /// subdomain of a ruled domain).
    bool shouldBlock(const QUrl &url) const;

    /// Decision + statistics (used by interceptRequest and tests).
    bool tryBlock(const QUrl &url);

    bool isEnabled() const { return m_enabled.loadAcquire(); }
    void setEnabled(bool enabled) { m_enabled.storeRelease(enabled); }

    bool isDoNotTrack() const { return m_dnt.loadAcquire() != 0; }
    void setDoNotTrack(bool enabled) { m_dnt.storeRelease(enabled ? 1 : 0); }

    int blockedCount() const { return m_blocked.loadAcquire(); }
    int ruleCount() const { return m_domains.size(); }

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

signals:
    void blockedCountChanged(int count);

private:
    void addDomain(const QString &domain);

    QSet<QString> m_domains;
    QAtomicInt m_enabled{1};
    QAtomicInt m_dnt{1};
    QAtomicInt m_blocked{0};
};
