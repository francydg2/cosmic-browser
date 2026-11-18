#include "browser/UpdateAsset.h"

#include <QJsonArray>
#include <QRegularExpression>

bool Updates::findAsset(const QJsonObject &release, const QString &version,
                        const QString &platform, UpdateAsset *asset, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };
    if (!asset || !QRegularExpression(QStringLiteral("^\\d+\\.\\d+\\.\\d+$"))
                       .match(version).hasMatch()) {
        return fail(QStringLiteral("Invalid release version."));
    }

    QString suffix;
    if (platform == QLatin1String("linux-x64")) {
        suffix = QStringLiteral("linux-x64.AppImage");
    } else if (platform == QLatin1String("windows-x64")) {
        suffix = QStringLiteral("windows-x64.exe");
    } else if (platform == QLatin1String("macos-arm64")) {
        suffix = QStringLiteral("macos-arm64.dmg");
    } else {
        return fail(QStringLiteral("This platform is not supported by the updater."));
    }
    const QString name = QStringLiteral("cosmic-%1-%2").arg(version, suffix);
    const QString expectedPath = QStringLiteral("/francydg2/cosmic-browser/releases/download/v%1/%2")
                                     .arg(version, name);
    const QRegularExpression digestPattern(QStringLiteral("^sha256:([a-fA-F0-9]{64})$"));
    bool found = false;
    UpdateAsset result;
    for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("name")).toString() != name) {
            continue;
        }
        if (found) {
            return fail(QStringLiteral("The release contains duplicate update packages."));
        }
        found = true;
        const QUrl url(entry.value(QStringLiteral("browser_download_url")).toString());
        const auto digest = digestPattern.match(entry.value(QStringLiteral("digest")).toString());
        const double size = entry.value(QStringLiteral("size")).toDouble();
        if (url.scheme() != QLatin1String("https") || url.host() != QLatin1String("github.com")
            || url.port(-1) != -1 || !url.userInfo().isEmpty() || url.hasQuery()
            || url.hasFragment() || url.path() != expectedPath || !digest.hasMatch()
            || size <= 0 || size > 1024.0 * 1024 * 1024
            || size != static_cast<qint64>(size)) {
            return fail(QStringLiteral("The update package has invalid metadata."));
        }
        result.url = url;
        result.sha256 = QByteArray::fromHex(digest.captured(1).toLatin1());
        result.size = static_cast<qint64>(size);
    }
    if (!found) {
        return fail(QStringLiteral("No update package is available for this platform."));
    }
    *asset = result;
    return true;
}
