#include "core/Bookmarks.h"
#include "core/Logging.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

namespace {

const auto kKeyId = "id";
const auto kKeyTitle = "title";
const auto kKeyUrl = "url";

} // namespace

Bookmarks::Bookmarks(const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_path(filePath)
{
}

bool Bookmarks::load()
{
    QFile file(m_path);
    if (!file.exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcCosmic) << "cannot read bookmarks file" << m_path;
        return false;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcCosmic).noquote()
            << "bookmarks file corrupt (" << error.errorString() << "); ignoring";
        return false;
    }

    QVector<Bookmark> loaded;
    const QJsonArray array = doc.object().value(QStringLiteral("bookmarks")).toArray();
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        Bookmark bm;
        bm.id = obj.value(QString::fromLatin1(kKeyId)).toString();
        bm.title = obj.value(QString::fromLatin1(kKeyTitle)).toString().trimmed();
        bm.url = QUrl(obj.value(QString::fromLatin1(kKeyUrl)).toString());
        if (bm.id.isEmpty() || bm.title.isEmpty() || !bm.url.isValid()
            || bm.url.isRelative()) {
            continue;
        }
        if (contains(bm.url)) {
            continue;
        }
        loaded.append(bm);
    }
    m_items = loaded;
    emit changed();
    return true;
}

bool Bookmarks::save() const
{
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcCosmic) << "cannot write bookmarks file" << m_path;
        return false;
    }
    QJsonArray array;
    for (const Bookmark &bm : m_items) {
        QJsonObject obj;
        obj.insert(QString::fromLatin1(kKeyId), bm.id);
        obj.insert(QString::fromLatin1(kKeyTitle), bm.title);
        obj.insert(QString::fromLatin1(kKeyUrl), bm.url.toString());
        array.append(obj);
    }
    QJsonObject root;
    root.insert(QStringLiteral("bookmarks"), array);
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qCWarning(lcCosmic) << "failed to commit bookmarks file" << m_path;
        return false;
    }
    qCInfo(lcCosmic).noquote() << "bookmarks saved to" << m_path;
    return true;
}

QString Bookmarks::add(const QString &title, const QUrl &url)
{
    const QString cleanTitle = title.trimmed();
    if (cleanTitle.isEmpty() || !url.isValid() || url.isRelative()) {
        return {};
    }
    for (const Bookmark &bm : m_items) {
        if (bm.url == url) {
            return bm.id;
        }
    }
    Bookmark bm;
    bm.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
    bm.title = cleanTitle;
    bm.url = url;
    m_items.append(bm);
    emit changed();
    return bm.id;
}

bool Bookmarks::remove(const QString &id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == id) {
            m_items.removeAt(i);
            emit changed();
            return true;
        }
    }
    return false;
}

bool Bookmarks::rename(const QString &id, const QString &title)
{
    const QString cleanTitle = title.trimmed();
    if (cleanTitle.isEmpty()) {
        return false;
    }
    for (Bookmark &bm : m_items) {
        if (bm.id == id) {
            if (bm.title == cleanTitle) {
                return true;
            }
            bm.title = cleanTitle;
            emit changed();
            return true;
        }
    }
    return false;
}

bool Bookmarks::retarget(const QString &id, const QUrl &url)
{
    if (!url.isValid() || url.isRelative()) {
        return false;
    }
    for (Bookmark &bm : m_items) {
        if (bm.id == id) {
            if (bm.url == url) {
                return true;
            }
            bm.url = url;
            emit changed();
            return true;
        }
    }
    return false;
}

bool Bookmarks::contains(const QUrl &url) const
{
    for (const Bookmark &bm : m_items) {
        if (bm.url == url) {
            return true;
        }
    }
    return false;
}

Bookmark Bookmarks::findById(const QString &id) const
{
    for (const Bookmark &bm : m_items) {
        if (bm.id == id) {
            return bm;
        }
    }
    return {};
}
