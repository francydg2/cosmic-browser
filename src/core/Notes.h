#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

struct Note
{
    QString id;
    QString title;
    QString content;
    QString category;
    QUrl pageUrl;
    QDateTime created;
    QDateTime modified;
};

class Notes : public QObject
{
    Q_OBJECT

public:
    explicit Notes(const QString &path, QObject *parent = nullptr);
    bool load(QString *error = nullptr);
    bool create(Note note, QString *id, QString *error = nullptr);
    bool update(Note note, QString *error = nullptr);
    bool remove(const QString &id, QString *error = nullptr);
    const QVector<Note> &items() const { return m_items; }
    Note find(const QString &id) const;
    bool isAvailable() const { return m_available; }

signals:
    void changed();

private:
    bool write(const QVector<Note> &items, QString *error);
    QString m_path;
    QVector<Note> m_items;
    bool m_available = false;
};
