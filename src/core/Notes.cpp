#include "core/Notes.h"
#include "core/Logging.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace {
bool fail(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    qCWarning(lcCosmic).noquote() << message;
    return false;
}

bool validPageUrl(const QUrl &url)
{
    return url.isEmpty()
        || (url.isValid() && !url.host().isEmpty() && url.userInfo().isEmpty()
            && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")));
}
}

Notes::Notes(const QString &path, QObject *parent)
    : QObject(parent), m_path(path)
{
}

bool Notes::load(QString *error)
{
    m_available = false;
    QFile file(m_path);
    if (!file.exists()) {
        m_items.clear();
        m_available = true;
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error, tr("Could not read the notes file: %1").arg(file.errorString()));
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    const auto root = doc.object();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()
        || root.value(QStringLiteral("version")).toInt() != 1
        || !root.value(QStringLiteral("notes")).isArray()) {
        return fail(error, tr("The notes file is invalid. It has not been overwritten."));
    }
    QVector<Note> loaded;
    QSet<QString> ids;
    for (const auto &value : root.value(QStringLiteral("notes")).toArray()) {
        if (!value.isObject()) {
            return fail(error, tr("The notes file contains invalid entries."));
        }
        const auto obj = value.toObject();
        Note note;
        note.id = obj.value(QStringLiteral("id")).toString();
        note.title = obj.value(QStringLiteral("title")).toString();
        note.content = obj.value(QStringLiteral("content")).toString();
        note.category = obj.value(QStringLiteral("category")).toString();
        note.pageUrl = QUrl(obj.value(QStringLiteral("url")).toString());
        note.created = QDateTime::fromString(obj.value(QStringLiteral("created")).toString(),
                                             Qt::ISODateWithMs);
        note.modified = QDateTime::fromString(obj.value(QStringLiteral("modified")).toString(),
                                              Qt::ISODateWithMs);
        if (note.id.isEmpty() || ids.contains(note.id) || !note.created.isValid()
            || !note.modified.isValid()
            || !obj.value(QStringLiteral("title")).isString()
            || !obj.value(QStringLiteral("content")).isString()
            || !obj.value(QStringLiteral("category")).isString()
            || !obj.value(QStringLiteral("url")).isString()
            || !validPageUrl(note.pageUrl)) {
            return fail(error, tr("The notes file contains invalid entries."));
        }
        ids.insert(note.id);
        loaded.append(note);
    }
    m_items = loaded;
    m_available = true;
    emit changed();
    return true;
}

bool Notes::write(const QVector<Note> &items, QString *error)
{
    if (!m_available) {
        return fail(error, tr("Notes are unavailable; the existing file was not changed."));
    }
    QJsonArray array;
    for (const Note &note : items) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), note.id);
        obj.insert(QStringLiteral("title"), note.title);
        obj.insert(QStringLiteral("content"), note.content);
        obj.insert(QStringLiteral("category"), note.category);
        obj.insert(QStringLiteral("url"), note.pageUrl.toString());
        obj.insert(QStringLiteral("created"), note.created.toUTC().toString(Qt::ISODateWithMs));
        obj.insert(QStringLiteral("modified"), note.modified.toUTC().toString(Qt::ISODateWithMs));
        array.append(obj);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("notes"), array);
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(error, tr("Could not save notes: %1").arg(file.errorString()));
    }
    const QByteArray data = QJsonDocument(root).toJson();
    if (file.write(data) != data.size() || !file.commit()) {
        return fail(error, tr("Could not save notes: %1").arg(file.errorString()));
    }
    return true;
}

bool Notes::create(Note note, QString *id, QString *error)
{
    if (!validPageUrl(note.pageUrl)) {
        return fail(error, tr("The note's page address is invalid."));
    }
    note.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    note.created = note.modified = QDateTime::currentDateTimeUtc();
    QVector<Note> next = m_items;
    next.prepend(note);
    if (!write(next, error)) {
        return false;
    }
    m_items = next;
    if (id) {
        *id = note.id;
    }
    emit changed();
    return true;
}

bool Notes::update(Note note, QString *error)
{
    if (!validPageUrl(note.pageUrl)) {
        return fail(error, tr("The note's page address is invalid."));
    }
    QVector<Note> next = m_items;
    for (Note &existing : next) {
        if (existing.id != note.id) {
            continue;
        }
        if (existing.title == note.title && existing.content == note.content
            && existing.category == note.category && existing.pageUrl == note.pageUrl) {
            return true;
        }
        note.created = existing.created;
        note.modified = QDateTime::currentDateTimeUtc();
        existing = note;
        if (!write(next, error)) {
            return false;
        }
        m_items = next;
        emit changed();
        return true;
    }
    return fail(error, tr("The selected note no longer exists."));
}

bool Notes::remove(const QString &id, QString *error)
{
    QVector<Note> next = m_items;
    for (int i = 0; i < next.size(); ++i) {
        if (next.at(i).id != id) {
            continue;
        }
        next.removeAt(i);
        if (!write(next, error)) {
            return false;
        }
        m_items = next;
        emit changed();
        return true;
    }
    return fail(error, tr("The selected note no longer exists."));
}

Note Notes::find(const QString &id) const
{
    for (const Note &note : m_items) {
        if (note.id == id) {
            return note;
        }
    }
    return {};
}
