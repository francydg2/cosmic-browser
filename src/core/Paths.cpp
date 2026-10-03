#include "core/Paths.h"
#include "core/Logging.h"

#include <QDir>
#include <QStandardPaths>
#include <QStringList>

namespace Paths {

namespace {

bool ensurePath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    QDir dir(path);
    if (dir.exists()) {
        return true;
    }
    if (dir.mkpath(QStringLiteral("."))) {
        return true;
    }
    qCCritical(lcCosmic) << "failed to create directory" << path;
    return false;
}

} // namespace

QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString dataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString cacheDir()
{
    // CacheLocation = XDG cache dir + app name → ~/.cache/cosmic
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

bool ensureDirs()
{
    bool ok = true;
    ok &= ensurePath(configDir());
    ok &= ensurePath(dataDir());
    ok &= ensurePath(cacheDir());
    if (ok) {
        qCInfo(lcCosmic).noquote() << "XDG dirs:" << configDir() << dataDir() << cacheDir();
    }
    return ok;
}

namespace {

bool copyMissingFiles(const QString &srcDir, const QString &dstDir,
                      const QStringList &names)
{
    bool ok = true;
    QDir dst(dstDir);
    for (const QString &name : names) {
        const QString dstPath = dst.filePath(name);
        if (QFile::exists(dstPath)) {
            continue;
        }
        const QString srcPath = QDir(srcDir).filePath(name);
        if (!QFile::exists(srcPath)) {
            continue;
        }
        if (!QFile::copy(srcPath, dstPath)) {
            qCWarning(lcCosmic).noquote() << "profile migration: cannot copy" << srcPath;
            ok = false;
        }
    }
    return ok;
}

bool copyMissingTree(const QString &srcDir, const QString &dstDir)
{
    QDir src(srcDir);
    if (!src.exists()) {
        return true;
    }
    QDir().mkpath(dstDir);
    bool ok = true;
    const auto entries = src.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        const QString srcPath = src.filePath(entry);
        const QString dstPath = QDir(dstDir).filePath(entry);
        QFileInfo info(srcPath);
        if (info.isDir()) {
            if (QDir(dstPath).exists() || QDir().mkpath(dstPath)) {
                ok &= copyMissingTree(srcPath, dstPath);
            } else {
                ok = false;
            }
        } else if (!QFile::exists(dstPath) && !QFile::copy(srcPath, dstPath)) {
            qCWarning(lcCosmic).noquote() << "profile migration: cannot copy" << srcPath;
            ok = false;
        }
    }
    return ok;
}

} // namespace

bool migrateLegacyProfile()
{
    const QString legacyConfig =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/nebula");
    const QString legacyData =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/nebula");
    if (!QDir(legacyConfig).exists() && !QDir(legacyData).exists()) {
        return true;
    }
    bool ok = true;
    ok &= copyMissingFiles(legacyConfig, configDir(),
                           {QStringLiteral("settings.json"),
                            QStringLiteral("session.json"),
                            QStringLiteral("users.json"),
                            QStringLiteral("contexts.json"),
                            QStringLiteral("bookmarks.json"),
                            QStringLiteral("ui.json"),
                            QStringLiteral("vault.bin"),
                            QStringLiteral("blocklist.txt")});
    ok &= copyMissingTree(legacyData + QStringLiteral("/profiles"),
                          dataDir() + QStringLiteral("/profiles"));
    ok &= copyMissingTree(legacyData + QStringLiteral("/profile"),
                          dataDir() + QStringLiteral("/profile"));
    if (ok) {
        qCInfo(lcCosmic).noquote() << "profile migrated from Nebula directories";
    }
    return ok;
}

} // namespace Paths
