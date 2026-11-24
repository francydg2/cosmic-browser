#include "core/TabAnalysis.h"

#include <QtTest>

using namespace TabAnalysis;

namespace {

TabInfo tab(int index, const QString &url, const QString &title = QString())
{
    TabInfo t;
    t.index = index;
    t.url = QUrl(url);
    t.title = title;
    return t;
}

} // namespace

class TestTabAnalysis : public QObject
{
    Q_OBJECT
private slots:
    void normalizesAddresses();
    void sitesAndLabels();
    void findsDuplicates();
    void keepsTheMostValuableDuplicate();
    void suggestsGroupsOnlyForFreeTabs();
    void findsInactiveTabs();
    void searches();
};

void TestTabAnalysis::normalizesAddresses()
{
    QCOMPARE(normalizedAddress(QUrl(QStringLiteral("https://www.Example.com/a/?utm_source=x&b=2&a=1#frag"))),
             normalizedAddress(QUrl(QStringLiteral("http://example.com/a?a=1&b=2"))));
    QVERIFY(normalizedAddress(QUrl(QStringLiteral("https://example.com/a?id=1")))
            != normalizedAddress(QUrl(QStringLiteral("https://example.com/a?id=2"))));
    QCOMPARE(normalizedAddress(QUrl(QStringLiteral("https://example.com/"))), QStringLiteral("example.com"));
    QCOMPARE(normalizedAddress(QUrl()), QString());
}

void TestTabAnalysis::sitesAndLabels()
{
    QCOMPARE(siteKey(QStringLiteral("news.example.com")), QStringLiteral("example.com"));
    QCOMPARE(siteKey(QStringLiteral("www.bbc.co.uk")), QStringLiteral("bbc.co.uk"));
    QCOMPARE(siteKey(QStringLiteral("localhost")), QStringLiteral("localhost"));
    QCOMPARE(siteLabel(QStringLiteral("docs.github.com")), QStringLiteral("github"));
}

void TestTabAnalysis::findsDuplicates()
{
    const QList<TabInfo> tabs{
        tab(0, QStringLiteral("https://example.com/a")),
        tab(1, QStringLiteral("https://example.com/b")),
        tab(2, QStringLiteral("https://www.example.com/a/?utm_campaign=z")),
        tab(3, QString()),                       // new tab
        tab(4, QStringLiteral("about:blank")),
        tab(5, QString())};
    const auto sets = duplicates(tabs);
    QCOMPARE(sets.size(), 1);
    QCOMPARE(sets.first(), (QList<int>{0, 2}));
}

void TestTabAnalysis::keepsTheMostValuableDuplicate()
{
    QList<TabInfo> tabs{tab(0, QStringLiteral("https://x.com/p")), tab(1, QStringLiteral("https://x.com/p")),
                        tab(2, QStringLiteral("https://x.com/p"))};
    QCOMPARE(redundantDuplicates(tabs), (QList<int>{1, 2})); // first kept when equal
    tabs[1].pinned = true;
    QCOMPARE(redundantDuplicates(tabs), (QList<int>{0, 2}));
    tabs[2].current = true;
    QCOMPARE(redundantDuplicates(tabs), (QList<int>{0, 1}));
    tabs[0].audible = true;                           // current still wins
    QCOMPARE(redundantDuplicates(tabs), (QList<int>{0, 1}));
}

void TestTabAnalysis::suggestsGroupsOnlyForFreeTabs()
{
    QList<TabInfo> tabs{
        tab(0, QStringLiteral("https://docs.github.com/a")),
        tab(1, QStringLiteral("https://github.com/b")),
        tab(2, QStringLiteral("https://github.com/c")),
        tab(3, QStringLiteral("https://example.org/")),
        tab(4, QStringLiteral("https://www.example.org/x"))};
    tabs[2].groupId = QStringLiteral("g1");   // already grouped
    tabs[4].pinned = true;                    // pinned
    auto suggestions = suggestGroups(tabs);
    QCOMPARE(suggestions.size(), 1);
    QCOMPARE(suggestions.first().name, QStringLiteral("Github"));
    QCOMPARE(suggestions.first().indexes, (QList<int>{0, 1}));
    QCOMPARE(suggestGroups(tabs, 3).size(), 0);
}

void TestTabAnalysis::findsInactiveTabs()
{
    QList<TabInfo> tabs{tab(0, QStringLiteral("https://a.com")), tab(1, QStringLiteral("https://b.com")),
                        tab(2, QStringLiteral("https://c.com")), tab(3, QStringLiteral("https://d.com")),
                        tab(4, QStringLiteral("https://e.com")), tab(5, QStringLiteral("https://f.com"))};
    const qint64 now = 10'000'000;
    for (TabInfo &t : tabs) {
        t.lastActiveMs = now - 3'600'000;
    }
    tabs[1].current = true;
    tabs[2].pinned = true;
    tabs[3].audible = true;
    tabs[4].suspended = true;
    tabs[5].lastActiveMs = now - 1000;
    QCOMPARE(inactiveTabs(tabs, now, 1'800'000), (QList<int>{0}));
}

void TestTabAnalysis::searches()
{
    const QList<TabInfo> tabs{tab(0, QStringLiteral("https://a.com/Docs"), QStringLiteral("Qt Docs")),
                              tab(1, QStringLiteral("https://b.com"), QStringLiteral("News"))};
    QCOMPARE(search(tabs, QStringLiteral("qt")), (QList<int>{0}));
    QCOMPARE(search(tabs, QStringLiteral("B.COM")), (QList<int>{1}));
    QCOMPARE(search(tabs, QString()).size(), 2);
}

QTEST_APPLESS_MAIN(TestTabAnalysis)
#include "test_tabanalysis.moc"
