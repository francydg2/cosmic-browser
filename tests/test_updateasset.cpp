#include <QtTest>

#include "browser/UpdateAsset.h"
#include "browser/UpdateDownload.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

class TestUpdateAsset : public QObject
{
    Q_OBJECT

private slots:
    void acceptsOfficialAsset();
    void rejectsUntrustedMetadata();
    void replacesOnlyAfterVerifiedDownload();
    void downloadsOfficialAppImage();
    void macUpdateKeepsLocationAndBackup();
};

static QJsonObject entry()
{
    return {
        {QStringLiteral("name"), QStringLiteral("cosmic-0.2.0-linux-x64.AppImage")},
        {QStringLiteral("browser_download_url"), QStringLiteral(
             "https://github.com/francydg2/cosmic-browser/releases/download/v0.2.0/cosmic-0.2.0-linux-x64.AppImage")},
        {QStringLiteral("digest"), QStringLiteral("sha256:") + QString(64, QLatin1Char('a'))},
        {QStringLiteral("size"), 100}
    };
}

void TestUpdateAsset::acceptsOfficialAsset()
{
    const QJsonObject release{{QStringLiteral("assets"), QJsonArray{entry()}}};
    UpdateAsset asset;
    QString error;
    QVERIFY2(Updates::findAsset(release, QStringLiteral("0.2.0"),
                                QStringLiteral("linux-x64"), &asset, &error),
             qPrintable(error));
    QCOMPARE(asset.sha256, QByteArray(32, static_cast<char>(0xaa)));
    QCOMPARE(asset.size, 100);
}

void TestUpdateAsset::rejectsUntrustedMetadata()
{
    UpdateAsset asset;
    QString error;
    QJsonObject bad = entry();
    bad.insert(QStringLiteral("browser_download_url"),
               QStringLiteral("https://example.com/cosmic.AppImage"));
    QVERIFY(!Updates::findAsset({{QStringLiteral("assets"), QJsonArray{bad}}},
                                QStringLiteral("0.2.0"), QStringLiteral("linux-x64"),
                                &asset, &error));
    bad = entry();
    bad.remove(QStringLiteral("digest"));
    QVERIFY(!Updates::findAsset({{QStringLiteral("assets"), QJsonArray{bad}}},
                                QStringLiteral("0.2.0"), QStringLiteral("linux-x64"),
                                &asset, &error));
    QVERIFY(!Updates::findAsset({{QStringLiteral("assets"), QJsonArray{entry(), entry()}}},
                                QStringLiteral("0.2.0"), QStringLiteral("linux-x64"),
                                &asset, &error));
}

void TestUpdateAsset::replacesOnlyAfterVerifiedDownload()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString target = directory.filePath(QStringLiteral("Cosmic.AppImage"));
    QFile original(target);
    QVERIFY(original.open(QIODevice::WriteOnly));
    QCOMPARE(original.write("old"), 3);
    original.close();

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QByteArray payload = QByteArray("\x7f" "ELF", 4) + QByteArray(4, '\0')
        + QByteArray("AI\x02", 3) + QByteArray("test");
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        while (QTcpSocket *socket = server.nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, socket, [socket, payload] {
                socket->readAll();
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: "
                              + QByteArray::number(payload.size())
                              + "\r\nConnection: close\r\n\r\n" + payload);
                socket->disconnectFromHost();
            });
        }
    });
    UpdateAsset asset;
    asset.url = QUrl(QStringLiteral("http://127.0.0.1:%1/update").arg(server.serverPort()));
    asset.size = payload.size();
    asset.sha256 = QByteArray(32, '\0');
    UpdateDownload download;
    QSignalSpy failed(&download, &UpdateDownload::failed);
    QSignalSpy finished(&download, &UpdateDownload::finished);
#if defined(Q_OS_LINUX)
    constexpr bool executable = true;
#else
    constexpr bool executable = false;
#endif
    download.start(asset, target, executable);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 5000);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), QByteArray("old"));
    original.close();

    asset.size = payload.size() - 1;
    download.start(asset, target, executable);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 2, 5000);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), QByteArray("old"));
    original.close();

    asset.size = payload.size();
    asset.sha256 = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
    download.start(asset, target, executable);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), payload);
#if defined(Q_OS_LINUX)
    QVERIFY(original.permissions().testFlag(QFile::ExeOwner));
#endif
}

void TestUpdateAsset::downloadsOfficialAppImage()
{
#if !defined(Q_OS_LINUX)
    QSKIP("The AppImage smoke test requires Linux.");
#else
    if (!qEnvironmentVariableIsSet("COSMIC_UPDATE_ONLINE_TEST")) {
        QSKIP("Set COSMIC_UPDATE_ONLINE_TEST to run the real GitHub download.");
    }
    QNetworkAccessManager network;
    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.github.com/repos/francydg2/cosmic-browser/releases/latest")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Cosmic update test"));
    QNetworkReply *reply = network.get(request);
    QSignalSpy checked(reply, &QNetworkReply::finished);
    QTRY_COMPARE_WITH_TIMEOUT(checked.size(), 1, 30000);
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    QString version = release.value(QStringLiteral("tag_name")).toString();
    QVERIFY(version.startsWith(QLatin1Char('v')));
    version.remove(0, 1);
    UpdateAsset asset;
    QString error;
    QVERIFY2(Updates::findAsset(release, version, QStringLiteral("linux-x64"), &asset, &error),
             qPrintable(error));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString target = directory.filePath(QStringLiteral("Cosmic.AppImage"));
    QFile old(target);
    QVERIFY(old.open(QIODevice::WriteOnly));
    old.write("old");
    old.close();
    UpdateDownload download;
    QSignalSpy success(&download, &UpdateDownload::finished);
    QSignalSpy failure(&download, &UpdateDownload::failed);
    download.start(asset, target, true);
    QTRY_VERIFY_WITH_TIMEOUT(!success.isEmpty() || !failure.isEmpty(), 180000);
    QVERIFY2(failure.isEmpty(), failure.isEmpty() ? "" : qPrintable(failure.first().first().toString()));
    QCOMPARE(QFileInfo(target).size(), asset.size);
    QProcess image;
    image.start(target, {QStringLiteral("--appimage-version")});
    QVERIFY(image.waitForFinished(10000));
    QCOMPARE(image.exitCode(), 0);
#endif
}

void TestUpdateAsset::macUpdateKeepsLocationAndBackup()
{
#if !defined(Q_OS_UNIX)
    QSKIP("The macOS helper uses a POSIX shell.");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QDir root(directory.path());
    QVERIFY(root.mkpath(QStringLiteral("source/Cosmic.app/Contents/MacOS")));
    QVERIFY(root.mkpath(QStringLiteral("install location/Cosmic.app/Contents")));
    QVERIFY(root.mkpath(QStringLiteral("stubs")));
    auto write = [](const QString &path, const QByteArray &contents, bool executable = false) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()) {
            return false;
        }
        file.close();
        return !executable || file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    };
    const QString payload = root.filePath(QStringLiteral("source/Cosmic.app"));
    const QString target = root.filePath(QStringLiteral("install location/Cosmic.app"));
    QVERIFY(write(payload + QStringLiteral("/Contents/MacOS/cosmic"), "#!/bin/sh\nexit 0\n", true));
    QVERIFY(write(payload + QStringLiteral("/Contents/version"), "new"));
    QVERIFY(write(target + QStringLiteral("/Contents/version"), "old"));
    QVERIFY(write(root.filePath(QStringLiteral("stubs/hdiutil")),
        "#!/bin/sh\nif [ \"$1\" = attach ]; then /bin/cp -R \"$COSMIC_TEST_PAYLOAD\" \"$6/Cosmic.app\"; fi\n", true));
    QVERIFY(write(root.filePath(QStringLiteral("stubs/ditto")),
        "#!/bin/sh\n/bin/cp -R \"$1\" \"$2\"\n", true));
    QVERIFY(write(root.filePath(QStringLiteral("stubs/plist")),
        "#!/bin/sh\nprintf '%s\\n' \"$COSMIC_TEST_IDENTIFIER\"\n", true));
    QVERIFY(write(root.filePath(QStringLiteral("stubs/open")), "#!/bin/sh\nexit 0\n", true));
    QVERIFY(write(root.filePath(QStringLiteral("stubs/osascript")), "#!/bin/sh\nexit 0\n", true));
    QFile source(QStringLiteral(COSMIC_SOURCE_DIR "/resources/update-macos.sh"));
    QVERIFY(source.open(QIODevice::ReadOnly));
    QByteArray script = source.readAll();
    script.replace("/usr/bin/hdiutil", root.filePath(QStringLiteral("stubs/hdiutil")).toUtf8());
    script.replace("/usr/bin/ditto", root.filePath(QStringLiteral("stubs/ditto")).toUtf8());
    script.replace("/usr/libexec/PlistBuddy", root.filePath(QStringLiteral("stubs/plist")).toUtf8());
    script.replace("/usr/bin/open", root.filePath(QStringLiteral("stubs/open")).toUtf8());
    script.replace("/usr/bin/osascript", root.filePath(QStringLiteral("stubs/osascript")).toUtf8());

    const auto runHelper = [&](const QString &identifier) {
        const QString helper = root.filePath(QStringLiteral("helper.sh"));
        const QString image = root.filePath(QStringLiteral("update.dmg"));
        if (!write(helper, script) || !write(image, "image")) {
            return -1;
        }
        QProcess process;
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("COSMIC_TEST_PAYLOAD"), payload);
        environment.insert(QStringLiteral("COSMIC_TEST_IDENTIFIER"), identifier);
        process.setProcessEnvironment(environment);
        process.start(QStringLiteral("/bin/sh"),
                      {helper, target, image, QStringLiteral("99999999"), helper});
        if (!process.waitForFinished(15000) || process.exitStatus() != QProcess::NormalExit) {
            return -1;
        }
        return process.exitCode();
    };
    QCOMPARE(runHelper(QStringLiteral("wrong.identifier")), 1);
    QFile marker(target + QStringLiteral("/Contents/version"));
    QVERIFY(marker.open(QIODevice::ReadOnly));
    QCOMPARE(marker.readAll(), QByteArray("old"));
    marker.close();

    QCOMPARE(runHelper(QStringLiteral("app.cosmic.browser")), 0);
    QVERIFY(marker.open(QIODevice::ReadOnly));
    QCOMPARE(marker.readAll(), QByteArray("new"));
    const QStringList backups = QDir(root.filePath(QStringLiteral("install location")))
        .entryList({QStringLiteral(".cosmic-update.*")},
                   QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    bool foundBackup = false;
    for (const QString &backup : backups) {
        QFile previous(root.filePath(QStringLiteral("install location/") + backup
                                     + QStringLiteral("/previous.app/Contents/version")));
        if (previous.open(QIODevice::ReadOnly) && previous.readAll() == QByteArray("old")) {
            foundBackup = true;
        }
    }
    QVERIFY(foundBackup);
#endif
}

QTEST_GUILESS_MAIN(TestUpdateAsset)
#include "test_updateasset.moc"
