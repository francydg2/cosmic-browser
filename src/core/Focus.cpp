#include "core/Focus.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QUrl>

namespace Focus {

namespace {

const QStringList &distractions()
{
    static const QStringList hosts{
        QStringLiteral("facebook.com"), QStringLiteral("instagram.com"), QStringLiteral("tiktok.com"),
        QStringLiteral("x.com"), QStringLiteral("twitter.com"), QStringLiteral("reddit.com"),
        QStringLiteral("netflix.com"), QStringLiteral("twitch.tv")};
    return hosts;
}

} // namespace

QList<FocusProfile> builtInProfiles()
{
    FocusProfile study;
    study.id = QStringLiteral("study");
    study.name = QStringLiteral("Study");
    study.minutes = 25;
    study.blockedHosts = distractions() + QStringList{QStringLiteral("youtube.com")};
    FocusProfile work;
    work.id = QStringLiteral("work");
    work.name = QStringLiteral("Work");
    work.minutes = 50;
    work.blockedHosts = distractions();
    return {study, work};
}

FocusProfile customProfile(const QJsonObject &section)
{
    const QJsonObject o = section.value(QStringLiteral("custom")).toObject();
    FocusProfile profile;
    profile.id = QStringLiteral("custom");
    profile.name = QStringLiteral("Custom");
    profile.minutes = qBound(1, o.value(QStringLiteral("minutes")).toInt(30), 480);
    profile.hideChrome = o.value(QStringLiteral("hideChrome")).toBool(true);
    profile.workspace = o.value(QStringLiteral("workspace")).toString();
    for (const QJsonValue &host : o.value(QStringLiteral("blocked")).toArray()) {
        profile.blockedHosts.append(host.toString());
    }
    profile.blockedHosts = parseHosts(profile.blockedHosts.join(QLatin1Char('\n')));
    return profile;
}

QJsonObject withCustomProfile(QJsonObject section, const FocusProfile &profile)
{
    QJsonArray blocked;
    for (const QString &host : parseHosts(profile.blockedHosts.join(QLatin1Char('\n')))) {
        blocked.append(host);
    }
    section.insert(QStringLiteral("custom"), QJsonObject{
        {QStringLiteral("minutes"), qBound(1, profile.minutes, 480)},
        {QStringLiteral("hideChrome"), profile.hideChrome},
        {QStringLiteral("workspace"), profile.workspace},
        {QStringLiteral("blocked"), blocked}});
    return section;
}

QString lastProfileId(const QJsonObject &section)
{
    const QString id = section.value(QStringLiteral("last")).toString();
    return id == QLatin1String("study") || id == QLatin1String("work") || id == QLatin1String("custom")
        ? id : QStringLiteral("study");
}

QJsonObject withLastProfileId(QJsonObject section, const QString &id)
{
    section.insert(QStringLiteral("last"), id);
    return section;
}

QStringList parseHosts(const QString &text)
{
    static const QRegularExpression separators(QStringLiteral("[\\s,;]+"));
    static const QRegularExpression valid(
        QStringLiteral("^(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\\.)+[a-z]{2,63}$"));
    QStringList result;
    for (QString token : text.split(separators, Qt::SkipEmptyParts)) {
        token = token.trimmed().toLower();
        if (token.contains(QStringLiteral("://"))) {
            token = QUrl(token).host();
        } else {
            const int slash = token.indexOf(QLatin1Char('/'));
            if (slash >= 0) {
                token = token.left(slash);
            }
        }
        if (token.startsWith(QLatin1String("www."))) {
            token.remove(0, 4);
        }
        if (valid.match(token).hasMatch() && !result.contains(token)) {
            result.append(token);
        }
    }
    return result;
}

bool isBlocked(const QStringList &blockedHosts, const QString &hostIn)
{
    QString host = hostIn.toLower();
    while (host.endsWith(QLatin1Char('.'))) {
        host.chop(1);
    }
    for (const QString &blocked : blockedHosts) {
        if (host == blocked || host.endsWith(QLatin1Char('.') + blocked)) {
            return true;
        }
    }
    return false;
}

QString formatRemaining(qint64 milliseconds)
{
    const qint64 total = qMax<qint64>(0, (milliseconds + 999) / 1000);
    const qint64 hours = total / 3600;
    const qint64 minutes = (total % 3600) / 60;
    const qint64 seconds = total % 60;
    return hours > 0 ? QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0'))
                           .arg(seconds, 2, 10, QLatin1Char('0'))
                     : QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}

} // namespace Focus
