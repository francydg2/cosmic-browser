#include "core/Focus.h"

#include <QtTest>

class TestFocus : public QObject
{
    Q_OBJECT
private slots:
    void parsesPastedHosts();
    void blocksSubdomainsOnly();
    void formatsTime();
    void customProfileRoundTrip();
    void builtInProfilesAreSane();
};

void TestFocus::parsesPastedHosts()
{
    const QStringList hosts = Focus::parseHosts(
        QStringLiteral("https://www.YouTube.com/watch?v=1\nreddit.com, reddit.com;  x.com/home\n"
                       "not a host\nlocalhost\n-bad-.com\nok-site.co.uk"));
    QCOMPARE(hosts, (QStringList{QStringLiteral("youtube.com"), QStringLiteral("reddit.com"),
                                 QStringLiteral("x.com"), QStringLiteral("ok-site.co.uk")}));
    QVERIFY(Focus::parseHosts(QString()).isEmpty());
}

void TestFocus::blocksSubdomainsOnly()
{
    const QStringList blocked{QStringLiteral("x.com"), QStringLiteral("youtube.com")};
    QVERIFY(Focus::isBlocked(blocked, QStringLiteral("x.com")));
    QVERIFY(Focus::isBlocked(blocked, QStringLiteral("m.YouTube.com")));
    QVERIFY(Focus::isBlocked(blocked, QStringLiteral("x.com.")));
    QVERIFY(!Focus::isBlocked(blocked, QStringLiteral("netflix.com")));
    QVERIFY(!Focus::isBlocked(blocked, QStringLiteral("max.com")));   // suffix, not subdomain
    QVERIFY(!Focus::isBlocked(blocked, QStringLiteral("notyoutube.com")));
    QVERIFY(!Focus::isBlocked({}, QStringLiteral("x.com")));
}

void TestFocus::formatsTime()
{
    QCOMPARE(Focus::formatRemaining(0), QStringLiteral("0:00"));
    QCOMPARE(Focus::formatRemaining(1), QStringLiteral("0:01"));
    QCOMPARE(Focus::formatRemaining(24 * 60'000 + 9'500), QStringLiteral("24:10"));
    QCOMPARE(Focus::formatRemaining(3'725'000), QStringLiteral("1:02:05"));
    QCOMPARE(Focus::formatRemaining(-5), QStringLiteral("0:00"));
}

void TestFocus::customProfileRoundTrip()
{
    QCOMPARE(Focus::customProfile({}).blockedHosts, QStringList());
    FocusProfile profile;
    profile.minutes = 90;
    profile.hideChrome = false;
    profile.workspace = QStringLiteral("Thesis");
    profile.blockedHosts = {QStringLiteral("news.ycombinator.com"), QStringLiteral("garbage entry")};
    const QJsonObject section = Focus::withCustomProfile({}, profile);
    const FocusProfile loaded = Focus::customProfile(section);
    QCOMPARE(loaded.minutes, 90);
    QCOMPARE(loaded.hideChrome, false);
    QCOMPARE(loaded.workspace, QStringLiteral("Thesis"));
    QCOMPARE(loaded.blockedHosts, QStringList{QStringLiteral("news.ycombinator.com")});
    QCOMPARE(Focus::lastProfileId(Focus::withLastProfileId(section, QStringLiteral("work"))),
             QStringLiteral("work"));
    QCOMPARE(Focus::lastProfileId(Focus::withLastProfileId(section, QStringLiteral("evil"))),
             QStringLiteral("study"));
}

void TestFocus::builtInProfilesAreSane()
{
    const auto profiles = Focus::builtInProfiles();
    QCOMPARE(profiles.size(), 2);
    for (const FocusProfile &profile : profiles) {
        QVERIFY(profile.minutes > 0);
        QVERIFY(!profile.blockedHosts.isEmpty());
        QCOMPARE(Focus::parseHosts(profile.blockedHosts.join(QLatin1Char('\n'))), profile.blockedHosts);
    }
}

QTEST_APPLESS_MAIN(TestFocus)
#include "test_focus.moc"
