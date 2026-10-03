// Unit tests for core/UrlUtils: URL-vs-search detection and resolution.

#include <QtTest>

#include "core/UrlUtils.h"

class TestUrlUtils : public QObject
{
    Q_OBJECT

private slots:
    void urlDetection_data();
    void urlDetection();
    void searchResolution();
    void bareDomainUsesHttps();
    void loopbackUsesHttp();
    void schemePreserved();
    void httpsUpgrade();
};

void TestUrlUtils::urlDetection_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("isUrl");

    QTest::newRow("bare domain") << "example.com" << true;
    QTest::newRow("domain with path") << "docs.rs/book" << true;
    QTest::newRow("https url") << "https://example.com/a?b=c" << true;
    QTest::newRow("http url") << "http://example.com" << true;
    QTest::newRow("localhost") << "localhost:8080" << true;
    QTest::newRow("loopback ip") << "127.0.0.1" << true;
    QTest::newRow("ipv4") << "192.168.1.1" << true;
    QTest::newRow("file scheme") << "file:///tmp/x.html" << true;
    QTest::newRow("about scheme") << "about:blank" << true;
    QTest::newRow("trimmed domain") << "  example.org  " << true;
    QTest::newRow("two words") << "hello world" << false;
    QTest::newRow("single word") << "nebula" << false;
    QTest::newRow("leading dot") << ".hidden.example" << false;
    QTest::newRow("question") << "what is qt webengine" << false;
}

void TestUrlUtils::urlDetection()
{
    QFETCH(QString, input);
    QFETCH(bool, isUrl);
    QCOMPARE(UrlUtils::looksLikeUrl(input), isUrl);
}

void TestUrlUtils::searchResolution()
{
    const QUrl url = UrlUtils::toNavigableUrl(QStringLiteral("qt webengine"));
    QVERIFY(url.isValid());
    QCOMPARE(url.scheme(), QStringLiteral("https"));
    QCOMPARE(url.host(), QStringLiteral("duckduckgo.com"));
    QVERIFY(url.query(QUrl::FullyEncoded).contains(QStringLiteral("qt%20webengine")));
}

void TestUrlUtils::bareDomainUsesHttps()
{
    QCOMPARE(UrlUtils::toNavigableUrl(QStringLiteral("example.com")),
             QUrl(QStringLiteral("https://example.com")));
}

void TestUrlUtils::loopbackUsesHttp()
{
    QCOMPARE(UrlUtils::toNavigableUrl(QStringLiteral("localhost:3000")),
             QUrl(QStringLiteral("http://localhost:3000")));
    QCOMPARE(UrlUtils::toNavigableUrl(QStringLiteral("192.168.1.1")),
             QUrl(QStringLiteral("http://192.168.1.1")));
}

void TestUrlUtils::schemePreserved()
{
    QCOMPARE(UrlUtils::toNavigableUrl(QStringLiteral("https://example.com/x")),
             QUrl(QStringLiteral("https://example.com/x")));
    QCOMPARE(UrlUtils::toNavigableUrl(QStringLiteral("file:///tmp/a.html")),
             QUrl(QStringLiteral("file:///tmp/a.html")));
}

void TestUrlUtils::httpsUpgrade()
{
    QCOMPARE(UrlUtils::upgradedToHttps(QUrl(QStringLiteral("http://example.com/a?b=1"))),
             QUrl(QStringLiteral("https://example.com/a?b=1")));
    QCOMPARE(UrlUtils::upgradedToHttps(QUrl(QStringLiteral("http://example.com:8080/x"))),
             QUrl(QStringLiteral("https://example.com:8080/x")));

    const QUrl httpsUrl(QStringLiteral("https://example.com"));
    QCOMPARE(UrlUtils::upgradedToHttps(httpsUrl), httpsUrl);

    const QUrl fileUrl(QStringLiteral("file:///tmp/a.html"));
    QCOMPARE(UrlUtils::upgradedToHttps(fileUrl), fileUrl);

    const QUrl empty;
    QCOMPARE(UrlUtils::upgradedToHttps(empty), empty);

    QCOMPARE(UrlUtils::upgradedToHttps(QUrl(QStringLiteral("http:///path-only"))),
             QUrl(QStringLiteral("http:///path-only")));
}

QTEST_APPLESS_MAIN(TestUrlUtils)
#include "test_urlutils.moc"
