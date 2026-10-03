#pragma once

#include <QString>
#include <QWebEnginePage>

struct PagePolicy
{
    bool httpsOnly = false;
    QString locationPolicy = QStringLiteral("ask");
    QString mediaPolicy = QStringLiteral("ask");
    QString notificationsPolicy = QStringLiteral("ask");

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
