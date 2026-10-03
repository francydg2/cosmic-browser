#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

struct Bookmark
{
    QString id;
    QString title;
    QUrl url;
};

class Bookmarks : public QObject
{
    Q_OBJECT

public:
    explicit Bookmarks(const QString &filePath, QObject *parent = nullptr);

    bool load();
    bool save() const;

    QString filePath() const { return m_path; }

    const QVector<Bookmark> &items() const { return m_items; }
    int count() const { return m_items.size(); }

    QString add(const QString &title, const QUrl &url);
    bool remove(const QString &id);
    bool rename(const QString &id, const QString &title);
    bool retarget(const QString &id, const QUrl &url);

    bool contains(const QUrl &url) const;
    Bookmark findById(const QString &id) const;

signals:
    void changed();

private:
    QString m_path;
    QVector<Bookmark> m_items;
};
