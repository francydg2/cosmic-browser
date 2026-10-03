// Unit tests for the user catalog (core/UserCatalog).

#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/UserCatalog.h"

class TestUsers : public QObject
{
    Q_OBJECT

private slots:
    void defaultsWhenFileMissing();
    void addGeneratesUniqueIds();
    void defaultUserIsProtected();
    void renameRecolorRemove();
    void persistenceRoundtrip();
    void corruptFileStartsEmpty();
    void changedSignal();
    void migratesContextsFile();
};

void TestUsers::defaultsWhenFileMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    UserCatalog catalog(dir.filePath(QStringLiteral("users.json")));
    QVERIFY(!catalog.load());
    QVERIFY(catalog.users().isEmpty());

    const BrowseUser fallback = catalog.userById(QStringLiteral("nope"));
    QVERIFY(fallback.id.isEmpty()); // unknown → invalid

    const BrowseUser defaultUser = catalog.userById(QStringLiteral("default"));
    QCOMPARE(defaultUser.id, QStringLiteral("default"));
    QCOMPARE(UserCatalog::defaultUser().id, QStringLiteral("default"));
    QVERIFY(catalog.hasUser(QStringLiteral("default")));
    QVERIFY(!catalog.hasUser(QStringLiteral("work")));
}

void TestUsers::addGeneratesUniqueIds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    UserCatalog catalog(dir.filePath(QStringLiteral("users.json")));
    catalog.load();

    const QString work = catalog.addUser(QStringLiteral("Work"), QStringLiteral("#7c6cf0"));
    QCOMPARE(work, QStringLiteral("work"));
    QVERIFY(catalog.hasUser(work));

    // Same name twice → suffixed id.
    const QString work2 = catalog.addUser(QStringLiteral("Work"), QString());
    QCOMPARE(work2, QStringLiteral("work-2"));
    QVERIFY(catalog.hasUser(work2));

    // Weird characters are slugified; blank names rejected.
    const QString weird = catalog.addUser(QStringLiteral("  Casa / Ufficio!! "), QString());
    QVERIFY(!weird.isEmpty());
    QVERIFY(catalog.hasUser(weird));
    QVERIFY(catalog.addUser(QStringLiteral("   "), QString()).isEmpty());

    // The default id can never be generated (it becomes a suffix).
    QVERIFY(catalog.addUser(QStringLiteral("Default!"), QString()) != QStringLiteral("default"));

    QCOMPARE(catalog.users().size(), 4); // Work, work-2, casa-ufficio, default-2
}

void TestUsers::defaultUserIsProtected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    UserCatalog catalog(dir.filePath(QStringLiteral("users.json")));
    catalog.load();

    QVERIFY(!catalog.removeUser(QStringLiteral("default")));
    QVERIFY(!catalog.removeUser(QString()));
    QVERIFY(!catalog.renameUser(QStringLiteral("default"), QStringLiteral("Hacked")));
    QVERIFY(!catalog.recolorUser(QStringLiteral("default"), QStringLiteral("#000000")));
    QCOMPARE(UserCatalog::defaultUser().name, QStringLiteral("Personal"));
}

void TestUsers::renameRecolorRemove()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    UserCatalog catalog(dir.filePath(QStringLiteral("users.json")));
    catalog.load();

    const QString id = catalog.addUser(QStringLiteral("Work"), QStringLiteral("#7c6cf0"));
    QVERIFY(catalog.renameUser(id, QStringLiteral("Lavoro")));
    QVERIFY(catalog.recolorUser(id, QStringLiteral("#06d6a0")));
    QCOMPARE(catalog.userById(id).name, QStringLiteral("Lavoro"));
    QCOMPARE(catalog.userById(id).color, QStringLiteral("#06d6a0"));

    // Blank rename rejected, unknown ids rejected.
    QVERIFY(!catalog.renameUser(id, QStringLiteral("  ")));
    QVERIFY(!catalog.renameUser(QStringLiteral("missing"), QStringLiteral("x")));
    QVERIFY(!catalog.recolorUser(QStringLiteral("missing"), QStringLiteral("#ffffff")));

    QVERIFY(catalog.removeUser(id));
    QVERIFY(!catalog.hasUser(id));
    QVERIFY(!catalog.removeUser(id)); // second time: gone
}

void TestUsers::persistenceRoundtrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("users.json"));
    {
        UserCatalog catalog(path);
        catalog.load();
        catalog.addUser(QStringLiteral("Work"), QStringLiteral("#7c6cf0"));
        catalog.addUser(QStringLiteral("Casa"), QStringLiteral("#2ec4b6"));
        QVERIFY(catalog.save());
    }
    UserCatalog reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.users().size(), 2);
    QCOMPARE(reloaded.users().at(0).id, QStringLiteral("work"));
    QCOMPARE(reloaded.users().at(0).color, QStringLiteral("#7c6cf0"));
    QCOMPARE(reloaded.users().at(1).id, QStringLiteral("casa"));

    // The default user is never written into the file.
    for (const BrowseUser &user : reloaded.users()) {
        QVERIFY(user.id != QStringLiteral("default"));
    }
}

void TestUsers::corruptFileStartsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("users.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{nope");
    file.close();

    UserCatalog catalog(path);
    QVERIFY(!catalog.load());
    QVERIFY(catalog.users().isEmpty());
}

void TestUsers::changedSignal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    UserCatalog catalog(dir.filePath(QStringLiteral("users.json")));
    catalog.load();

    QSignalSpy spy(&catalog, &UserCatalog::changed);
    const QString id = catalog.addUser(QStringLiteral("Work"), QString());
    QCOMPARE(spy.count(), 1);
    catalog.renameUser(id, QStringLiteral("W"));
    catalog.recolorUser(id, QStringLiteral("#ef476f"));
    catalog.removeUser(id);
    QCOMPARE(spy.count(), 4);

    // No signal for no-op failures.
    catalog.removeUser(QStringLiteral("ghost"));
    catalog.renameUser(QStringLiteral("ghost"), QStringLiteral("x"));
    QCOMPARE(spy.count(), 4);
}

void TestUsers::migratesContextsFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile legacy(dir.filePath(QStringLiteral("contexts.json")));
    QVERIFY(legacy.open(QIODevice::WriteOnly));
    legacy.write("{\"version\":1,\"contexts\":["
                 "{\"id\":\"work\",\"name\":\"Work\",\"color\":\"#7c6cf0\"},"
                 "{\"id\":\"default\",\"name\":\"Hacked\",\"color\":\"#000000\"}]}");
    legacy.close();

    UserCatalog catalog(dir.filePath(QStringLiteral("users.json")));
    QVERIFY(catalog.load());
    QCOMPARE(catalog.users().size(), 1);
    QCOMPARE(catalog.users().at(0).id, QStringLiteral("work"));
    QCOMPARE(catalog.users().at(0).name, QStringLiteral("Work"));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("contexts.json"))));

    UserCatalog again(dir.filePath(QStringLiteral("users.json")));
    QVERIFY(again.load());
    QCOMPARE(again.users().size(), 1);
}

QTEST_GUILESS_MAIN(TestUsers)
#include "test_users.moc"
