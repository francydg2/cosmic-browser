#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

/// Pure data and rules of Cosmic Focus: profiles, the list of sites to
/// restrict, and time formatting. No UI, no browser state.
struct FocusProfile
{
    QString id;       // "study", "work" or "custom"
    QString name;
    int minutes = 25;
    QStringList blockedHosts;
    bool hideChrome = true; // hide tab bar, groups, status bar and side panels
    QString workspace;      // saved workspace opened when Focus starts ("" none)
};

namespace Focus {

QList<FocusProfile> builtInProfiles();
/// The user's own profile; an empty one when nothing was saved.
FocusProfile customProfile(const QJsonObject &section);
QJsonObject withCustomProfile(QJsonObject section, const FocusProfile &profile);
QString lastProfileId(const QJsonObject &section);
QJsonObject withLastProfileId(QJsonObject section, const QString &id);

/// One host per line (or separated by commas/spaces). Accepts pasted
/// addresses (scheme, path and "www." are dropped), lowercases, removes
/// duplicates and entries that are not valid host names.
QStringList parseHosts(const QString &text);

/// True when \p host equals a listed host or is a subdomain of it.
bool isBlocked(const QStringList &blockedHosts, const QString &host);

/// "24:09" or "1:05:09".
QString formatRemaining(qint64 milliseconds);

} // namespace Focus
