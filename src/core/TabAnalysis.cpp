#include "core/TabAnalysis.h"

#include <algorithm>
#include <QHash>
#include <QSet>
#include <QUrlQuery>

namespace TabAnalysis {

namespace {

bool isTrackingParameter(const QString &name)
{
    const QString n = name.toLower();
    return n.startsWith(QLatin1String("utm_")) || n == QLatin1String("fbclid")
        || n == QLatin1String("gclid") || n == QLatin1String("msclkid")
        || n == QLatin1String("mc_eid") || n == QLatin1String("igshid");
}

QString stripWww(QString host)
{
    host = host.toLower();
    return host.startsWith(QLatin1String("www.")) ? host.mid(4) : host;
}

} // namespace

bool isWebTab(const TabInfo &tab)
{
    return (tab.url.scheme() == QLatin1String("http") || tab.url.scheme() == QLatin1String("https"))
        && !tab.url.host().isEmpty();
}

QString normalizedAddress(const QUrl &url)
{
    if (url.host().isEmpty()) {
        return {};
    }
    QString path = url.path();
    while (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (path == QLatin1String("/")) {
        path.clear();
    }
    QUrlQuery query(url);
    QList<QPair<QString, QString>> kept;
    for (const auto &item : query.queryItems(QUrl::FullyDecoded)) {
        if (!isTrackingParameter(item.first)) {
            kept.append(item);
        }
    }
    std::sort(kept.begin(), kept.end());
    QString result = stripWww(url.host());
    if (url.port() > 0) {
        result += QLatin1Char(':') + QString::number(url.port());
    }
    result += path;
    if (!kept.isEmpty()) {
        QStringList parts;
        for (const auto &item : std::as_const(kept)) {
            parts << item.first + QLatin1Char('=') + item.second;
        }
        result += QLatin1Char('?') + parts.join(QLatin1Char('&'));
    }
    return result;
}

QString siteKey(const QString &hostIn)
{
    const QString host = stripWww(hostIn);
    const QStringList labels = host.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (labels.size() <= 2) {
        return host;
    }
    static const QSet<QString> secondLevel{QStringLiteral("co"), QStringLiteral("com"),
        QStringLiteral("org"), QStringLiteral("net"), QStringLiteral("gov"),
        QStringLiteral("ac"), QStringLiteral("edu")};
    const bool compound = labels.last().size() == 2 && secondLevel.contains(labels.at(labels.size() - 2));
    const int keep = compound ? 3 : 2;
    return labels.mid(labels.size() - keep).join(QLatin1Char('.'));
}

QString siteLabel(const QString &host)
{
    const QStringList labels = siteKey(host).split(QLatin1Char('.'), Qt::SkipEmptyParts);
    return labels.isEmpty() ? QString() : labels.first();
}

QList<QList<int>> duplicates(const QList<TabInfo> &tabs)
{
    QHash<QString, QList<int>> byAddress;
    QStringList order;
    for (const TabInfo &tab : tabs) {
        if (!isWebTab(tab)) {
            continue;
        }
        const QString key = normalizedAddress(tab.url);
        if (!byAddress.contains(key)) {
            order << key;
        }
        byAddress[key].append(tab.index);
    }
    QList<QList<int>> result;
    for (const QString &key : std::as_const(order)) {
        if (byAddress.value(key).size() >= 2) {
            result.append(byAddress.value(key));
        }
    }
    return result;
}

QList<int> redundantDuplicates(const QList<TabInfo> &tabs)
{
    QHash<int, const TabInfo *> byIndex;
    for (const TabInfo &tab : tabs) {
        byIndex.insert(tab.index, &tab);
    }
    QList<int> redundant;
    for (const QList<int> &set : duplicates(tabs)) {
        // Keep the most valuable one: current > audible > pinned > grouped > oldest.
        int keep = set.first();
        const auto score = [&](int index) {
            const TabInfo *t = byIndex.value(index);
            return (t->current ? 8 : 0) + (t->audible ? 4 : 0) + (t->pinned ? 2 : 0)
                + (!t->groupId.isEmpty() ? 1 : 0);
        };
        for (int index : set) {
            if (score(index) > score(keep)) {
                keep = index;
            }
        }
        for (int index : set) {
            if (index != keep) {
                redundant.append(index);
            }
        }
    }
    std::sort(redundant.begin(), redundant.end());
    return redundant;
}

QList<GroupSuggestion> suggestGroups(const QList<TabInfo> &tabs, int minTabs)
{
    QHash<QString, GroupSuggestion> bySite;
    QStringList order;
    for (const TabInfo &tab : tabs) {
        if (!isWebTab(tab) || tab.pinned || !tab.groupId.isEmpty()) {
            continue;
        }
        const QString key = siteKey(tab.url.host());
        if (!bySite.contains(key)) {
            order << key;
            GroupSuggestion suggestion;
            suggestion.site = key;
            QString label = siteLabel(tab.url.host());
            if (!label.isEmpty()) {
                label[0] = label.at(0).toUpper();
            }
            suggestion.name = label;
            bySite.insert(key, suggestion);
        }
        bySite[key].indexes.append(tab.index);
    }
    QList<GroupSuggestion> result;
    for (const QString &key : std::as_const(order)) {
        if (bySite.value(key).indexes.size() >= minTabs) {
            result.append(bySite.value(key));
        }
    }
    return result;
}

QList<int> inactiveTabs(const QList<TabInfo> &tabs, qint64 nowMs, qint64 thresholdMs)
{
    QList<int> result;
    for (const TabInfo &tab : tabs) {
        if (isWebTab(tab) && !tab.current && !tab.pinned && !tab.audible && !tab.suspended
            && tab.lastActiveMs > 0 && nowMs - tab.lastActiveMs >= thresholdMs) {
            result.append(tab.index);
        }
    }
    return result;
}

QList<int> search(const QList<TabInfo> &tabs, const QString &query)
{
    const QString needle = query.trimmed();
    QList<int> result;
    for (const TabInfo &tab : tabs) {
        if (needle.isEmpty() || tab.title.contains(needle, Qt::CaseInsensitive)
            || tab.url.toString().contains(needle, Qt::CaseInsensitive)) {
            result.append(tab.index);
        }
    }
    return result;
}

} // namespace TabAnalysis
