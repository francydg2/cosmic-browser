// Unit tests for request-level blocking rules (web/BlocklistInterceptor).
// Decision logic only — no engine involved, no network.

#include <QtTest>

#include <QTemporaryDir>

#include "web/BlocklistInterceptor.h"

class TestBlocklist : public QObject
{
    Q_OBJECT

private slots:
    void builtinRulesLoaded();
    void exactAndSubdomainMatching();
    void nearMissDomainsNotBlocked();
    void disableStopsBlocking();
    void statisticsCountOnlyBlocks();
    void userRulesParsing();
    void messyRuleNormalization();
};

void TestBlocklist::builtinRulesLoaded()
{
    BlocklistInterceptor interceptor;
    QVERIFY(interceptor.ruleCount() >= 30);
    QVERIFY(interceptor.isEnabled());
}

void TestBlocklist::exactAndSubdomainMatching()
{
    BlocklistInterceptor interceptor;
    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://doubleclick.net/ad"))));
    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://ad.doubleclick.net/x"))));
    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://page.google-analytics.com/ga"))));
    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://connect.facebook.net/en_US/fbevents.js"))));
}

void TestBlocklist::nearMissDomainsNotBlocked()
{
    BlocklistInterceptor interceptor;
    // Suffix must match on a domain boundary, not as a substring.
    QVERIFY(!interceptor.shouldBlock(QUrl(QStringLiteral("https://notdoubleclick.net/"))));
    QVERIFY(!interceptor.shouldBlock(QUrl(QStringLiteral("https://doubleclick.net.evil.io/"))));
    QVERIFY(!interceptor.shouldBlock(QUrl(QStringLiteral("https://example.com/"))));
    QVERIFY(!interceptor.shouldBlock(QUrl(QStringLiteral("https://google.com/"))));
    // Host-less URLs (data:, about:) are never blocked.
    QVERIFY(!interceptor.shouldBlock(QUrl(QStringLiteral("data:text/html,hi"))));
}

void TestBlocklist::disableStopsBlocking()
{
    BlocklistInterceptor interceptor;
    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://doubleclick.net/"))));
    interceptor.setEnabled(false);
    QVERIFY(!interceptor.isEnabled());
    QVERIFY(!interceptor.shouldBlock(QUrl(QStringLiteral("https://doubleclick.net/"))));
    interceptor.setEnabled(true);
    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://doubleclick.net/"))));
}

void TestBlocklist::statisticsCountOnlyBlocks()
{
    BlocklistInterceptor interceptor;
    QCOMPARE(interceptor.blockedCount(), 0);

    QVERIFY(interceptor.tryBlock(QUrl(QStringLiteral("https://doubleclick.net/x"))));
    QVERIFY(interceptor.tryBlock(QUrl(QStringLiteral("https://a.pubmatic.com/y"))));
    QVERIFY(!interceptor.tryBlock(QUrl(QStringLiteral("https://example.com/"))));
    QCOMPARE(interceptor.blockedCount(), 2);

    QSignalSpy spy(&interceptor, &BlocklistInterceptor::blockedCountChanged);
    QVERIFY(interceptor.tryBlock(QUrl(QStringLiteral("https://taboola.com/"))));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toInt(), 3);
}

void TestBlocklist::userRulesParsing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("blocklist.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("# comment line\n"
               "\n"
               "  MyTracker.test  \n"
               "SUB.mine.example\n");
    file.close();

    BlocklistInterceptor interceptor;
    const int before = interceptor.ruleCount();
    interceptor.loadUserRules(path);
    QCOMPARE(interceptor.ruleCount(), before + 2);

    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://mytracker.test/pixel"))));
    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://sub.mine.example/x"))));
}

void TestBlocklist::messyRuleNormalization()
{
    BlocklistInterceptor interceptor;
    const int before = interceptor.ruleCount();
    // Scheme/path/star/dot noise and uppercase are normalized.
    QVERIFY(!interceptor.shouldBlock(QUrl(QStringLiteral("https://weird.example/"))));

    // Simulate normalization through a temp user file.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("u.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("HTTPS://Odd.Example/path\n"
               "*.starred.example\n"
               ".dotprefixed.example\n");
    file.close();
    interceptor.loadUserRules(path);
    QCOMPARE(interceptor.ruleCount(), before + 3);

    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://odd.example/anything"))));
    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://x.starred.example/"))));
    QVERIFY(interceptor.shouldBlock(QUrl(QStringLiteral("https://a.dotprefixed.example/"))));
}

QTEST_GUILESS_MAIN(TestBlocklist)
#include "test_blocklist.moc"
