// Unit tests for the encrypted password vault (core/PasswordVault).

#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/PasswordVault.h"

class TestVault : public QObject
{
    Q_OBJECT

private slots:
    void initializeAndUnlock();
    void wrongPasswordRejected();
    void crudRoundtripPersists();
    void lockClearsMemory();
    void changeMasterPassword();
    void entryForUrlMatching();
    void rejectsBadInput();
    void passkeyEntriesSupported();
};

void TestVault::initializeAndUnlock()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("vault.bin"));
    {
        PasswordVault vault(path);
        QVERIFY(!vault.fileExists());
        QVERIFY(vault.initialize(QStringLiteral("hunter2")));
        QVERIFY(vault.fileExists());
        QVERIFY(vault.isUnlocked());
        QCOMPARE(vault.entries().size(), 0);
    }
    {
        PasswordVault vault(path);
        QVERIFY(vault.fileExists());
        QVERIFY(!vault.isUnlocked());
        QVERIFY(vault.entries().isEmpty());
        QVERIFY(vault.unlock(QStringLiteral("hunter2")));
        QVERIFY(vault.isUnlocked());
    }
}

void TestVault::wrongPasswordRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("vault.bin"));
    PasswordVault writer(path);
    QVERIFY(writer.initialize(QStringLiteral("correct")));

    PasswordVault reader(path);
    QVERIFY(!reader.unlock(QStringLiteral("wrong-password")));
    QVERIFY(!reader.isUnlocked());
    QCOMPARE(reader.lastError(), QStringLiteral("wrong master password"));
    QVERIFY(reader.entries().isEmpty());

    QVERIFY(reader.unlock(QStringLiteral("correct")));
    QVERIFY(reader.isUnlocked());
}

void TestVault::crudRoundtripPersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("vault.bin"));
    QString entryId;
    {
        PasswordVault vault(path);
        QVERIFY(vault.initialize(QStringLiteral("secret")));
        entryId = vault.addEntry([] {
            VaultEntry e;
            e.site = QStringLiteral("example.com");
            e.username = QStringLiteral("alice");
            e.password = QStringLiteral("s3cret-pw");
            e.note = QStringLiteral("note");
            return e;
        }());
        QVERIFY(!entryId.isEmpty());
        QCOMPARE(vault.entries().size(), 1);
    }
    {
        PasswordVault vault(path);
        QVERIFY(vault.unlock(QStringLiteral("secret")));
        QCOMPARE(vault.entries().size(), 1);
        const VaultEntry loaded = vault.entry(entryId);
        QCOMPARE(loaded.site, QStringLiteral("example.com"));
        QCOMPARE(loaded.username, QStringLiteral("alice"));
        QCOMPARE(loaded.password, QStringLiteral("s3cret-pw"));
        QCOMPARE(loaded.type, QStringLiteral("password"));
        QVERIFY(loaded.createdMs > 0);
        QVERIFY(loaded.updatedMs >= loaded.createdMs);

        VaultEntry updated = loaded;
        updated.password = QStringLiteral("changed");
        QVERIFY(vault.updateEntry(updated));
        QCOMPARE(vault.entry(entryId).password, QStringLiteral("changed"));

        QVERIFY(vault.removeEntry(entryId));
        QCOMPARE(vault.entries().size(), 0);
        QVERIFY(!vault.removeEntry(entryId));
    }
    {
        PasswordVault vault(path);
        QVERIFY(vault.unlock(QStringLiteral("secret")));
        QCOMPARE(vault.entries().size(), 0);
    }
}

void TestVault::lockClearsMemory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PasswordVault vault(dir.filePath(QStringLiteral("vault.bin")));
    QVERIFY(vault.initialize(QStringLiteral("secret")));
    const QString id = vault.addEntry([] {
        VaultEntry e;
        e.site = QStringLiteral("example.com");
        e.password = QStringLiteral("pw");
        return e;
    }());
    QVERIFY(!id.isEmpty());

    QSignalSpy spy(&vault, &PasswordVault::lockStateChanged);
    vault.lock();
    QVERIFY(!vault.isUnlocked());
    QVERIFY(vault.entries().isEmpty());
    QVERIFY(vault.entry(id).id.isEmpty());
    QVERIFY(vault.entryForUrl(QUrl(QStringLiteral("https://example.com"))).id.isEmpty());
    QVERIFY(vault.addEntry(VaultEntry{QString(), QStringLiteral("x"),
                                      QString(), QString(), QString(),
                                      QString(), 0, 0})
                .isEmpty());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);

    QVERIFY(vault.unlock(QStringLiteral("secret")));
    QCOMPARE(vault.entries().size(), 1);
}

void TestVault::changeMasterPassword()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("vault.bin"));
    {
        PasswordVault vault(path);
        QVERIFY(vault.initialize(QStringLiteral("old-pass")));
        vault.addEntry([] {
            VaultEntry e;
            e.site = QStringLiteral("example.com");
            e.password = QStringLiteral("pw");
            return e;
        }());
        QVERIFY(!vault.changeMasterPassword(QStringLiteral("wrong"), QStringLiteral("new-pass")));
        QVERIFY(vault.changeMasterPassword(QStringLiteral("old-pass"), QStringLiteral("new-pass")));
        QVERIFY(vault.isUnlocked());
        QCOMPARE(vault.entries().size(), 1);
    }
    {
        PasswordVault vault(path);
        QVERIFY(!vault.unlock(QStringLiteral("old-pass")));
        QVERIFY(vault.unlock(QStringLiteral("new-pass")));
        QCOMPARE(vault.entries().size(), 1);
        QCOMPARE(vault.entries().at(0).password, QStringLiteral("pw"));
    }
}

void TestVault::entryForUrlMatching()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PasswordVault vault(dir.filePath(QStringLiteral("vault.bin")));
    QVERIFY(vault.initialize(QStringLiteral("secret")));

    VaultEntry direct;
    direct.site = QStringLiteral("example.com");
    direct.password = QStringLiteral("pw1");
    const QString directId = vault.addEntry(direct);

    VaultEntry sub;
    sub.site = QStringLiteral("https://shop.example.com/login");
    sub.password = QStringLiteral("pw2");
    const QString subId = vault.addEntry(sub);

    VaultEntry passkey;
    passkey.site = QStringLiteral("example.com");
    passkey.type = QStringLiteral("passkey");
    passkey.note = QStringLiteral("credential-id: abc");
    const QString passkeyId = vault.addEntry(passkey);

    QCOMPARE(vault.entryForUrl(QUrl(QStringLiteral("https://example.com/a"))).id, directId);
    QCOMPARE(vault.entryForUrl(QUrl(QStringLiteral("https://www.example.com"))).id, directId);
    QCOMPARE(vault.entryForUrl(QUrl(QStringLiteral("https://shop.example.com/x"))).id, subId);
    QVERIFY(vault.entryForUrl(QUrl(QStringLiteral("https://other.test"))).id.isEmpty());
    QVERIFY(vault.entryForUrl(QUrl()).id.isEmpty());
    QVERIFY(!passkeyId.isEmpty());
}

void TestVault::rejectsBadInput()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PasswordVault vault(dir.filePath(QStringLiteral("vault.bin")));
    QVERIFY(!vault.initialize(QStringLiteral("ab")));
    QVERIFY(vault.initialize(QStringLiteral("good-pass")));
    QVERIFY(!vault.initialize(QStringLiteral("good-pass"))); // already initialized
    QCOMPARE(vault.lastError(), QStringLiteral("vault already initialized"));

    QVERIFY(vault.addEntry(VaultEntry{QString(), QStringLiteral("   "),
                                      QString(), QString(), QString(),
                                      QString(), 0, 0})
                .isEmpty());
    QCOMPARE(vault.lastError(), QStringLiteral("site is required"));
    QCOMPARE(vault.entries().size(), 0);
}

void TestVault::passkeyEntriesSupported()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    PasswordVault vault(dir.filePath(QStringLiteral("vault.bin")));
    QVERIFY(vault.initialize(QStringLiteral("secret")));

    VaultEntry passkey;
    passkey.site = QStringLiteral("example.com");
    passkey.type = QStringLiteral("passkey");
    passkey.username = QStringLiteral("alice");
    passkey.note = QStringLiteral("credential-id: xyz; transport: nfc");
    const QString id = vault.addEntry(passkey);
    QVERIFY(!id.isEmpty());
    QCOMPARE(vault.entry(id).type, QStringLiteral("passkey"));
    QVERIFY(vault.entryForUrl(QUrl(QStringLiteral("https://example.com"))).id.isEmpty());
}

QTEST_GUILESS_MAIN(TestVault)
#include "test_vault.moc"
