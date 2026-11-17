#include "core/Bookmarks.h"
#include "core/Logging.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <QRegularExpression>
#include <QTextDocumentFragment>

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

bool Bookmarks::importHtml(const QString &filePath, int *imported, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Could not read the selected bookmarks file.");
        }
        return false;
    }
    if (file.size() > 20 * 1024 * 1024) {
        if (error) {
            *error = QStringLiteral("The selected file is too large to import (20 MB maximum).");
        }
        return false;
    }

    const QString html = QString::fromUtf8(file.readAll());
    const QRegularExpression anchor(
        QStringLiteral("<a\\b[^>]*\\bhref\\s*=\\s*(['\"])(.*?)\\1[^>]*>(.*?)</a\\s*>"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::DotMatchesEverythingOption);
    auto matches = anchor.globalMatch(html);
    int added = 0;
    int examined = 0;
    while (matches.hasNext() && examined < 10000) {
        const QRegularExpressionMatch match = matches.next();
        ++examined;
        const QString rawUrl =
            QTextDocumentFragment::fromHtml(match.captured(2)).toPlainText().trimmed();
        const QUrl url = QUrl::fromUserInput(rawUrl);
        if (!url.isValid()
            || (url.scheme() != QLatin1String("http")
                && url.scheme() != QLatin1String("https"))
            || url.host().isEmpty()) {
            continue;
        }

        QString titleHtml = match.captured(3);
        titleHtml.replace(QRegularExpression(QStringLiteral("<[^>]*>")),
                          QStringLiteral(" "));
        QString title =
            QTextDocumentFragment::fromHtml(titleHtml).toPlainText().trimmed();
        if (title.isEmpty()) {
            title = url.host();
        }
        const bool existed = contains(url);
        if (!add(title, url).isEmpty() && !existed) {
            ++added;
        }
    }
    if (examined == 0) {
        if (error) {
            *error = QStringLiteral("No bookmarks were found in that HTML file.");
        }
        return false;
    }
    if (imported) {
        *imported = added;
    }
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
