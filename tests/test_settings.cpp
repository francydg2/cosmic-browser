// Unit tests for core/Settings: defaults, persistence, merge, signals.

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/Settings.h"

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaultsWhenFileMissing();
    void saveLoadRoundtrip();
    void corruptFileKeepsDefaults();
    void unknownKeysIgnoredMissingKeysDefaulted();
    void changeSignals();
    void settersDoNotPersistUntilSave();
    void privacyHttpsAppearanceDefaults();
    void privacySignals();
    void validationRejectsBadValues();
    void newFieldsRoundtrip();
};

void TestSettings::defaultsWhenFileMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Settings settings(dir.filePath(QStringLiteral("missing.json")));
    QVERIFY(!settings.load());
    QCOMPARE(settings.defaultSearchEngine(), QStringLiteral("duckduckgo"));
    QCOMPARE(settings.customSearchTemplate(), QString());
    QVERIFY(settings.requestBlockingEnabled());
    QVERIFY(!settings.restoreSessionOnStartup());
}

void TestSettings::saveLoadRoundtrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));

    {
        Settings settings(path);
        settings.load();
        settings.setDefaultSearchEngine(QStringLiteral("startpage"));
        settings.setCustomSearchTemplate(QStringLiteral("https://x.test/?q=%1"));
        settings.setRequestBlockingEnabled(false);
        settings.setRestoreSessionOnStartup(true);
        QVERIFY(settings.save());
        QVERIFY(QFile::exists(path));
    }

    Settings reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.defaultSearchEngine(), QStringLiteral("startpage"));
    QCOMPARE(reloaded.customSearchTemplate(), QStringLiteral("https://x.test/?q=%1"));
    QVERIFY(!reloaded.requestBlockingEnabled());
    QVERIFY(reloaded.restoreSessionOnStartup());
}

void TestSettings::corruptFileKeepsDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{not json!!");
    file.close();

    Settings settings(path);
    QVERIFY(!settings.load());
    QCOMPARE(settings.defaultSearchEngine(), QStringLiteral("duckduckgo"));
    QVERIFY(settings.requestBlockingEnabled());
}

void TestSettings::unknownKeysIgnoredMissingKeysDefaulted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"search.engine": "bing", "future.unknown": 42})");
    file.close();

    Settings settings(path);
    QVERIFY(settings.load());
    QCOMPARE(settings.defaultSearchEngine(), QStringLiteral("bing"));
    QCOMPARE(settings.customSearchTemplate(), QString());       // missing → default
    QVERIFY(settings.requestBlockingEnabled());                 // missing → default
    QVERIFY(!settings.restoreSessionOnStartup());               // missing → default
}

void TestSettings::changeSignals()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Settings settings(dir.filePath(QStringLiteral("settings.json")));

    QSignalSpy blockingSpy(&settings, &Settings::requestBlockingChanged);
    QSignalSpy restoreSpy(&settings, &Settings::restoreSessionChanged);
    QSignalSpy engineSpy(&settings, &Settings::defaultSearchEngineChanged);

    settings.setRequestBlockingEnabled(false);
    settings.setRestoreSessionOnStartup(true);
    settings.setDefaultSearchEngine(QStringLiteral("google"));

    QCOMPARE(blockingSpy.count(), 1);
    QCOMPARE(blockingSpy.first().first().toBool(), false);
    QCOMPARE(restoreSpy.count(), 1);
    QCOMPARE(restoreSpy.first().first().toBool(), true);
    QCOMPARE(engineSpy.count(), 1);
    QCOMPARE(engineSpy.first().first().toString(), QStringLiteral("google"));

    // Setting the same value again must not re-emit.
    settings.setRequestBlockingEnabled(false);
    QCOMPARE(blockingSpy.count(), 1);
}

void TestSettings::settersDoNotPersistUntilSave()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));

    Settings settings(path);
    settings.load();
    settings.setDefaultSearchEngine(QStringLiteral("bing"));
    QVERIFY(!QFile::exists(path));

    QVERIFY(settings.save());
    Settings reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.defaultSearchEngine(), QStringLiteral("bing"));
}

void TestSettings::privacyHttpsAppearanceDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Settings settings(dir.filePath(QStringLiteral("settings.json")));
    QVERIFY(!settings.load());

    QVERIFY(settings.doNotTrack());
    QVERIFY(!settings.sessionCookiesOnly());
    QCOMPARE(settings.permissionPolicy(QStringLiteral("location")), QStringLiteral("ask"));
    QCOMPARE(settings.permissionPolicy(QStringLiteral("media")), QStringLiteral("ask"));
    QCOMPARE(settings.permissionPolicy(QStringLiteral("notifications")), QStringLiteral("ask"));
    QCOMPARE(settings.permissionPolicy(QStringLiteral("unknown")), QStringLiteral("ask"));
    QVERIFY(!settings.httpsOnly());
    QCOMPARE(settings.accentColor(), QStringLiteral("#7c6cf0"));
    QCOMPARE(settings.tabPosition(), QStringLiteral("horizontal"));
}

void TestSettings::privacySignals()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Settings settings(dir.filePath(QStringLiteral("settings.json")));

    QSignalSpy privacySpy(&settings, &Settings::privacyChanged);
    QSignalSpy permissionSpy(&settings, &Settings::permissionPolicyChanged);
    QSignalSpy httpsSpy(&settings, &Settings::httpsOnlyChanged);
    QSignalSpy accentSpy(&settings, &Settings::accentColorChanged);
    QSignalSpy tabSpy(&settings, &Settings::tabPositionChanged);

    settings.setDoNotTrack(false);
    settings.setSessionCookiesOnly(true);
    settings.setPermissionPolicy(QStringLiteral("location"), QStringLiteral("block"));
    settings.setHttpsOnly(true);
    settings.setAccentColor(QStringLiteral("#06d6a0"));
    settings.setTabPosition(QStringLiteral("vertical"));

    QCOMPARE(privacySpy.count(), 2);
    QCOMPARE(permissionSpy.count(), 1);
    QCOMPARE(permissionSpy.first().at(0).toString(), QStringLiteral("location"));
    QCOMPARE(permissionSpy.first().at(1).toString(), QStringLiteral("block"));
    QCOMPARE(httpsSpy.count(), 1);
    QCOMPARE(accentSpy.count(), 1);
    QCOMPARE(tabSpy.count(), 1);

    settings.setDoNotTrack(false);
    settings.setPermissionPolicy(QStringLiteral("location"), QStringLiteral("block"));
    settings.setTabPosition(QStringLiteral("vertical"));
    QCOMPARE(privacySpy.count(), 2);
    QCOMPARE(permissionSpy.count(), 1);
    QCOMPARE(tabSpy.count(), 1);
}

void TestSettings::validationRejectsBadValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Settings settings(dir.filePath(QStringLiteral("settings.json")));

    settings.setAccentColor(QStringLiteral("#7c6cf0"));
    settings.setAccentColor(QStringLiteral("not-a-color"));
    QCOMPARE(settings.accentColor(), QStringLiteral("#7c6cf0"));
    settings.setAccentColor(QStringLiteral("#7c6cf080"));
    QCOMPARE(settings.accentColor(), QStringLiteral("#7c6cf0"));
    settings.setAccentColor(QStringLiteral("red"));
    QCOMPARE(settings.accentColor(), QStringLiteral("#ff0000"));

    settings.setPermissionPolicy(QStringLiteral("location"), QStringLiteral("always"));
    settings.setPermissionPolicy(QStringLiteral("location"), QStringLiteral("allow"));
    QCOMPARE(settings.permissionPolicy(QStringLiteral("location")), QStringLiteral("allow"));

    settings.setTabPosition(QStringLiteral("north"));
    QCOMPARE(settings.tabPosition(), QStringLiteral("horizontal"));
    settings.setTabPosition(QStringLiteral("vertical"));
    QCOMPARE(settings.tabPosition(), QStringLiteral("vertical"));
}

void TestSettings::newFieldsRoundtrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    {
        Settings settings(path);
        settings.load();
        settings.setDoNotTrack(false);
        settings.setSessionCookiesOnly(true);
        settings.setPermissionPolicy(QStringLiteral("media"), QStringLiteral("allow"));
        settings.setPermissionPolicy(QStringLiteral("notifications"), QStringLiteral("block"));
        settings.setHttpsOnly(true);
        settings.setAccentColor(QStringLiteral("#06d6a0"));
        settings.setTabPosition(QStringLiteral("vertical"));
        QVERIFY(settings.save());
    }
    Settings reloaded(path);
    QVERIFY(reloaded.load());
    QVERIFY(!reloaded.doNotTrack());
    QVERIFY(reloaded.sessionCookiesOnly());
    QCOMPARE(reloaded.permissionPolicy(QStringLiteral("media")), QStringLiteral("allow"));
    QCOMPARE(reloaded.permissionPolicy(QStringLiteral("notifications")), QStringLiteral("block"));
    QCOMPARE(reloaded.permissionPolicy(QStringLiteral("location")), QStringLiteral("ask"));
    QVERIFY(reloaded.httpsOnly());
    QCOMPARE(reloaded.accentColor(), QStringLiteral("#06d6a0"));
    QCOMPARE(reloaded.tabPosition(), QStringLiteral("vertical"));
}

QTEST_GUILESS_MAIN(TestSettings)
#include "test_settings.moc"
