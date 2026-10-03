// Unit tests for the search engine registry and configured-engine
// resolution (core/SearchEngines + UrlUtils overload).

#include <QtTest>

#include "core/SearchEngines.h"
#include "core/UrlUtils.h"

class TestSearchEngines : public QObject
{
    Q_OBJECT

private slots:
    void builtinsAreValid();
    void lookupById();
    void buildSearchUrlEncodesQuery();
    void invalidTemplateFallsBack();
    void urlDetectionIgnoresTemplate();
    void configuredTemplateUsedForSearch();
};

void TestSearchEngines::builtinsAreValid()
{
    const auto engines = SearchEngines::builtins();
    QVERIFY(engines.size() >= 5);

    QSet<QString> ids;
    for (const SearchEngine &engine : engines) {
        QVERIFY(!engine.id.isEmpty());
        QVERIFY(!engine.name.isEmpty());
        QVERIFY(SearchEngines::isValidTemplate(engine.urlTemplate));
        QVERIFY(!ids.contains(engine.id));
        ids.insert(engine.id);
    }
    QVERIFY(ids.contains(QStringLiteral("duckduckgo")));
}

void TestSearchEngines::lookupById()
{
    const SearchEngine *ddg = SearchEngines::builtinById(QStringLiteral("duckduckgo"));
    QVERIFY(ddg);
    QCOMPARE(ddg->name, QStringLiteral("DuckDuckGo"));

    QVERIFY(SearchEngines::builtinById(QStringLiteral("does-not-exist")) == nullptr);
}

void TestSearchEngines::buildSearchUrlEncodesQuery()
{
    const QUrl url = SearchEngines::buildSearchUrl(
        QStringLiteral("https://www.google.com/search?q=%1"), QStringLiteral("c++ qt web"));
    QCOMPARE(url.host(), QStringLiteral("www.google.com"));
    QVERIFY(url.query(QUrl::FullyEncoded).contains(QStringLiteral("c%2B%2B%20qt%20web")));
}

void TestSearchEngines::invalidTemplateFallsBack()
{
    const QUrl url =
        SearchEngines::buildSearchUrl(QStringLiteral("ftp://no-placeholder"), QStringLiteral("x"));
    QCOMPARE(url.host(), QStringLiteral("duckduckgo.com"));

    const QUrl empty = SearchEngines::buildSearchUrl(QString(), QStringLiteral("x"));
    QCOMPARE(empty.host(), QStringLiteral("duckduckgo.com"));
}

void TestSearchEngines::urlDetectionIgnoresTemplate()
{
    // A URL stays a URL no matter which search template is configured.
    const QUrl url = UrlUtils::toNavigableUrl(
        QStringLiteral("example.com/path"), QStringLiteral("https://x.test/?q=%1"));
    QCOMPARE(url, QUrl(QStringLiteral("https://example.com/path")));

    const QUrl schemeUrl = UrlUtils::toNavigableUrl(
        QStringLiteral("file:///tmp/a.html"), QStringLiteral("https://x.test/?q=%1"));
    QCOMPARE(schemeUrl, QUrl(QStringLiteral("file:///tmp/a.html")));
}

void TestSearchEngines::configuredTemplateUsedForSearch()
{
    const QUrl url = UrlUtils::toNavigableUrl(
        QStringLiteral("hello world"), QStringLiteral("https://www.bing.com/search?q=%1"));
    QCOMPARE(url.host(), QStringLiteral("www.bing.com"));
    QVERIFY(url.query(QUrl::FullyEncoded).contains(QStringLiteral("hello%20world")));

    // Empty/invalid template → default engine (DuckDuckGo).
    const QUrl fallback = UrlUtils::toNavigableUrl(QStringLiteral("hello world"), QString());
    QCOMPARE(fallback.host(), QStringLiteral("duckduckgo.com"));
}

QTEST_GUILESS_MAIN(TestSearchEngines)
#include "test_searchengines.moc"
