// Unit tests for session serialization (core/Session): object format
// with pin/group/user, New Tab round-trip, legacy string tabs.

#include <QtTest>

#include <QTemporaryDir>

#include "core/Session.h"

class TestSession : public QObject
{
    Q_OBJECT

private slots:
    void roundtripWithState();
    void emptyTabUrlsStayInvalid();
    void legacyStringTabsLoad();
    void missingFileLoadsEmpty();
    void corruptFileLoadsEmpty();
    void groupsRoundtrip();
    void usersRoundtripAndV1Migration();
};

void TestSession::roundtripWithState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));

    SessionWindow second;
    second.user = QStringLiteral("work");
    second.tabs = {
        SessionTab{QUrl(QStringLiteral("https://example.com/")), true, QStringLiteral("g1"),
                   QStringLiteral("work")},
        SessionTab{QUrl(), false, QString(), QStringLiteral("default")},
    };

    const QList<SessionWindow> original = {
        SessionWindow{QStringLiteral("default"),
                      {},
                      {SessionTab{QUrl(QStringLiteral("https://docs.rs/serde")), false, QString(),
                                  QStringLiteral("default")}}},
        second,
    };
    QVERIFY(Session::save(path, original));

    const auto loaded = Session::load(path);
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded[0].user, QStringLiteral("default"));
    QCOMPARE(loaded[0].tabs.size(), 1);
    QCOMPARE(loaded[0].tabs[0].url, QUrl(QStringLiteral("https://docs.rs/serde")));
    QVERIFY(!loaded[0].tabs[0].pinned);

    QCOMPARE(loaded[1].user, QStringLiteral("work"));
    QCOMPARE(loaded[1].tabs.size(), 2);
    QCOMPARE(loaded[1].tabs[0].url, QUrl(QStringLiteral("https://example.com/")));
    QVERIFY(loaded[1].tabs[0].pinned);
    QCOMPARE(loaded[1].tabs[0].group, QStringLiteral("g1"));
    QCOMPARE(loaded[1].tabs[0].user, QStringLiteral("work"));
    QVERIFY(!loaded[1].tabs[1].url.isValid());
    QCOMPARE(loaded[1].tabs[1].user, QStringLiteral("default"));
}

void TestSession::emptyTabUrlsStayInvalid()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));

    const QList<SessionWindow> original = {
        SessionWindow{{}, {},
                      {SessionTab{QUrl(), false, QString(), QString()},
                       SessionTab{QUrl(QStringLiteral("https://example.com/")), false, QString(),
                                  QString()},
                       SessionTab{QUrl(), true, QString(), QString()}}},
    };
    QVERIFY(Session::save(path, original));

    const auto loaded = Session::load(path);
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded[0].tabs.size(), 3);
    QVERIFY(!loaded[0].tabs[0].url.isValid());
    QCOMPARE(loaded[0].tabs[1].url, QUrl(QStringLiteral("https://example.com/")));
    QVERIFY(!loaded[0].tabs[2].url.isValid());
    QVERIFY(loaded[0].tabs[2].pinned); // pinned NTP survives
}

void TestSession::legacyStringTabsLoad()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":1,"windows":[{"tabs":["https://a.example/",""]} ]})");
    file.close();

    const auto loaded = Session::load(path);
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded[0].tabs.size(), 2);
    QCOMPARE(loaded[0].tabs[0].url, QUrl(QStringLiteral("https://a.example/")));
    QVERIFY(!loaded[0].tabs[0].pinned);
    QVERIFY(!loaded[0].tabs[1].url.isValid());
}

void TestSession::missingFileLoadsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(Session::load(dir.filePath(QStringLiteral("nope.json"))).isEmpty());
}

void TestSession::corruptFileLoadsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{broken");
    file.close();
    QVERIFY(Session::load(path).isEmpty());
}

void TestSession::groupsRoundtrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));

    SessionWindow window;
    window.user = QStringLiteral("default");
    window.groups = {
        SessionGroup{QStringLiteral("ab12cd34"), QStringLiteral("Research"),
                     QStringLiteral("#2ec4b6"), true},
        SessionGroup{QStringLiteral("zz99"), QStringLiteral("Shopping"),
                     QStringLiteral("#ff9f1c"), false},
    };
    window.tabs = {SessionTab{QUrl(QStringLiteral("https://example.com/")), false,
                              QStringLiteral("ab12cd34"), QString()}};
    QVERIFY(Session::save(path, {window}));

    const auto loaded = Session::load(path);
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded[0].groups.size(), 2);
    QCOMPARE(loaded[0].groups[0].id, QStringLiteral("ab12cd34"));
    QCOMPARE(loaded[0].groups[0].name, QStringLiteral("Research"));
    QCOMPARE(loaded[0].groups[0].color, QStringLiteral("#2ec4b6"));
    QVERIFY(loaded[0].groups[0].collapsed);
    QVERIFY(!loaded[0].groups[1].collapsed);
    QCOMPARE(loaded[0].tabs[0].group, QStringLiteral("ab12cd34"));
}

void TestSession::usersRoundtripAndV1Migration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));

    SessionWindow workWindow;
    workWindow.user = QStringLiteral("work");
    workWindow.tabs = {SessionTab{QUrl(QStringLiteral("https://example.com/")), false,
                                  QString(), QStringLiteral("work")}};
    SessionWindow defaultWindow;
    defaultWindow.user = QStringLiteral("default");
    QMap<QString, QList<SessionWindow>> byUser;
    byUser.insert(QStringLiteral("work"), {workWindow});
    byUser.insert(QStringLiteral("default"), {defaultWindow});
    QVERIFY(Session::saveUsers(path, QStringLiteral("work"), byUser));

    QString current;
    QMap<QString, QList<SessionWindow>> loaded;
    QVERIFY(Session::loadUsers(path, &current, &loaded));
    QCOMPARE(current, QStringLiteral("work"));
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.value(QStringLiteral("work")).size(), 1);
    QCOMPARE(loaded.value(QStringLiteral("work")).at(0).tabs.size(), 1);
    QCOMPARE(loaded.value(QStringLiteral("work")).at(0).tabs.at(0).url,
             QUrl(QStringLiteral("https://example.com/")));

    QFile legacy(path);
    QVERIFY(legacy.open(QIODevice::WriteOnly));
    legacy.write("{\"version\":1,\"windows\":[{\"context\":\"default\",\"tabs\":[{}],"
                 "\"groups\":[]}]}");
    legacy.close();
    QString legacyCurrent;
    QMap<QString, QList<SessionWindow>> legacyLoaded;
    QVERIFY(Session::loadUsers(path, &legacyCurrent, &legacyLoaded));
    QCOMPARE(legacyCurrent, QStringLiteral("default"));
    QCOMPARE(legacyLoaded.size(), 1);
    QVERIFY(legacyLoaded.contains(QStringLiteral("default")));
    QCOMPARE(legacyLoaded.value(QStringLiteral("default")).size(), 1);
}

QTEST_GUILESS_MAIN(TestSession)
#include "test_session.moc"
