#pragma once

#include <QList>
#include <QString>
#include <QUrl>

/// Pure analysis used by the Smart Tab Manager: nothing here touches a
/// tab, a page or the network.
struct TabInfo
{
    int index = -1;
    QString title;
    QUrl url;
    QString groupId;
    bool pinned = false;
    bool current = false;
    bool audible = false;
    bool suspended = false;
    qint64 lastActiveMs = 0;
};

namespace TabAnalysis {

/// True for http(s) tabs with a host (new-tab, settings and crash pages
/// are never analysed).
bool isWebTab(const TabInfo &tab);

/// Address used to compare tabs: scheme-insensitive, no "www.", no
/// fragment, no tracking parameters, no trailing slash.
QString normalizedAddress(const QUrl &url);

/// Site label of a host: "news.example.co.uk" → "example".
QString siteLabel(const QString &host);
/// Registrable-domain approximation used to group tabs by site.
QString siteKey(const QString &host);

/// Sets of tab indexes showing the same normalized address (≥ 2 each).
QList<QList<int>> duplicates(const QList<TabInfo> &tabs);

/// Indexes that can be closed to keep one tab per duplicate set: never
/// the current, pinned or audible one when another candidate exists.
QList<int> redundantDuplicates(const QList<TabInfo> &tabs);

struct GroupSuggestion
{
    QString name;
    QString site;
    QList<int> indexes;
};
/// Ungrouped, unpinned web tabs of the same site (at least \p minTabs).
QList<GroupSuggestion> suggestGroups(const QList<TabInfo> &tabs, int minTabs = 2);

/// Background web tabs not used for \p thresholdMs: not current, pinned,
/// audible or already suspended.
QList<int> inactiveTabs(const QList<TabInfo> &tabs, qint64 nowMs, qint64 thresholdMs);

/// Case-insensitive match on title and address; empty query matches all.
QList<int> search(const QList<TabInfo> &tabs, const QString &query);

} // namespace TabAnalysis
