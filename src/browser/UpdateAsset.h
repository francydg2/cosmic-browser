#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QUrl>

struct UpdateAsset
{
    QUrl url;
    QByteArray sha256;
    qint64 size = 0;
};

namespace Updates {

bool findAsset(const QJsonObject &release, const QString &version,
               const QString &platform, UpdateAsset *asset, QString *error);

}
