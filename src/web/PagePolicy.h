#pragma once

#include "core/Focus.h"

#include <QHash>
#include <QStringList>
#include <QString>
#include <QWebEnginePage>

struct PagePolicy
{
    bool httpsOnly = false;
    /// Cosmic Focus: main-frame navigation to these sites is refused.
    bool focusActive = false;
    QStringList focusBlocked;
    bool isFocusBlocked(const QString &host) const
    {
        return focusActive && Focus::isBlocked(focusBlocked, host);
    }
    QString locationPolicy = QStringLiteral("ask");
    QString mediaPolicy = QStringLiteral("ask");
    QString notificationsPolicy = QStringLiteral("ask");

    /// Per-site decisions the user chose to remember ("host|feature" →
    /// "allow" or "block"); they take precedence over the global policy.
    QHash<QString, QString> sites;

    static QString siteKey(const QString &host, const QString &feature)
    {
        return host.toLower() + QLatin1Char('|') + feature;
    }

    QString permission(const QString &feature, const QString &host) const
    {
        const QString remembered = sites.value(siteKey(host, feature));
        if (remembered == QLatin1String("allow") || remembered == QLatin1String("block")) {
            return remembered;
        }
        return permission(feature);
    }

    QString permission(const QString &feature) const
    {
        const QString policy = feature == QLatin1String("location") ? locationPolicy
            : feature == QLatin1String("media")   ? mediaPolicy
            : feature == QLatin1String("notifications") ? notificationsPolicy
                                                        : QString();
        if (policy == QLatin1String("allow") || policy == QLatin1String("block")) {
            return policy;
        }
        return QStringLiteral("ask");
    }

    void setPermission(const QString &feature, const QString &policy)
    {
        if (policy != QLatin1String("ask") && policy != QLatin1String("allow")
            && policy != QLatin1String("block")) {
            return;
        }
        if (feature == QLatin1String("location")) {
            locationPolicy = policy;
        } else if (feature == QLatin1String("media")) {
            mediaPolicy = policy;
        } else if (feature == QLatin1String("notifications")) {
            notificationsPolicy = policy;
        }
    }

    static QString featureKey(QWebEnginePage::Feature feature)
    {
        switch (feature) {
        case QWebEnginePage::Geolocation:
            return QStringLiteral("location");
        case QWebEnginePage::MediaAudioCapture:
        case QWebEnginePage::MediaVideoCapture:
        case QWebEnginePage::MediaAudioVideoCapture:
            return QStringLiteral("media");
        case QWebEnginePage::Notifications:
            return QStringLiteral("notifications");
        default:
            return {};
        }
    }
};
