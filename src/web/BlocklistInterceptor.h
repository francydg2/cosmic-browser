#pragma once

#include <QAtomicInt>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QReadWriteLock>
#include <QSet>
#include <QUrl>
#include <QVector>
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
    enum class Category { Unclassified, Advertising, AnalyticsOrTracking, Social, Other };
    struct Event {
        QDateTime time;
        QString host;
        QString site;
        QString rule;
        Category category = Category::Unclassified;
    };
    struct Statistics {
        int blocked = 0;
        int matchedRules = 0;
        QVector<Event> recent;
    };
    explicit BlocklistInterceptor(bool enabled = true, QObject *parent = nullptr);

    /// Loads `:/blocklists/default.txt` (bundled, one host per line).
    void loadBuiltinRules();
    /// Loads the user file if present (same format, `#` comments).
    void loadUserRules(const QString &filePath);

    /// Pure decision: enabled AND host matches a rule (exact or
    /// subdomain of a ruled domain).
    bool shouldBlock(const QUrl &url) const;

    /// Decision + statistics (used by interceptRequest and tests).
    bool tryBlock(const QUrl &url, const QUrl &firstParty = QUrl());
    Statistics statistics() const;
    void clearStatistics();

    bool isEnabled() const { return m_enabled.loadAcquire(); }
    void setEnabled(bool enabled) { m_enabled.storeRelease(enabled); }

    bool isDoNotTrack() const { return m_dnt.loadAcquire() != 0; }
    void setDoNotTrack(bool enabled) { m_dnt.storeRelease(enabled ? 1 : 0); }

    int blockedCount() const;
    int ruleCount() const;

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

signals:
    void blockedCountChanged(int count);
    void statisticsChanged();

private:
    void addDomain(const QString &domain, Category category = Category::Unclassified);
    QString matchingRule(const QUrl &url, Category *category = nullptr) const;

    QSet<QString> m_domains;
    QHash<QString, Category> m_categories;
    mutable QReadWriteLock m_rulesLock;
    mutable QMutex m_statisticsLock;
    QVector<Event> m_recent;
    QSet<QString> m_matchedRules;
    int m_blocked = 0;
    QAtomicInt m_enabled{1};
    QAtomicInt m_dnt{1};
};
