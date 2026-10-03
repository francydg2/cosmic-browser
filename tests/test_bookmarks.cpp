// Unit tests for the bookmarks model (core/Bookmarks).

#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/Bookmarks.h"

class TestBookmarks : public QObject
{
    Q_OBJECT

private slots:
    void missingFileStartsEmpty();
    void addRemoveRename();
    void deduplicatesByUrl();
    void rejectsInvalidInput();
    void persistenceRoundtrip();
    void corruptFileIgnored();
    void changedSignal();
    void findById();
};

void TestBookmarks::missingFileStartsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Bookmarks bookmarks(dir.filePath(QStringLiteral("bookmarks.json")));
    QVERIFY(!bookmarks.load());
    QCOMPARE(bookmarks.count(), 0);
}

void TestBookmarks::addRemoveRename()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Bookmarks bookmarks(dir.filePath(QStringLiteral("bookmarks.json")));
    bookmarks.load();

    const QString id = bookmarks.add(QStringLiteral("Example"),
                                     QUrl(QStringLiteral("https://example.com")));
    QVERIFY(!id.isEmpty());
    QCOMPARE(bookmarks.count(), 1);
    QVERIFY(bookmarks.contains(QUrl(QStringLiteral("https://example.com"))));

    QVERIFY(bookmarks.rename(id, QStringLiteral("  Example Org ")));
    QCOMPARE(bookmarks.findById(id).title, QStringLiteral("Example Org"));

    QVERIFY(bookmarks.rename(QStringLiteral("ghost"), QStringLiteral("x")) == false);
    QVERIFY(!bookmarks.rename(id, QStringLiteral("   ")));

    QVERIFY(bookmarks.remove(id));
    QCOMPARE(bookmarks.count(), 0);
    QVERIFY(!bookmarks.remove(id));
}

void TestBookmarks::deduplicatesByUrl()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Bookmarks bookmarks(dir.filePath(QStringLiteral("bookmarks.json")));
    bookmarks.load();

    const QUrl url(QStringLiteral("https://example.com"));
    const QString first = bookmarks.add(QStringLiteral("One"), url);
    const QString second = bookmarks.add(QStringLiteral("Two"), url);
    QCOMPARE(first, second);
    QCOMPARE(bookmarks.count(), 1);
}

void TestBookmarks::rejectsInvalidInput()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Bookmarks bookmarks(dir.filePath(QStringLiteral("bookmarks.json")));
    bookmarks.load();

    QVERIFY(bookmarks.add(QStringLiteral("   "), QUrl(QStringLiteral("https://a.com"))).isEmpty());
    QVERIFY(bookmarks.add(QStringLiteral("A"), QUrl()).isEmpty());
    QVERIFY(bookmarks.add(QStringLiteral("A"), QUrl(QStringLiteral("relative"))).isEmpty());
    QCOMPARE(bookmarks.count(), 0);
}

void TestBookmarks::persistenceRoundtrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("bookmarks.json"));
    {
        Bookmarks bookmarks(path);
        bookmarks.load();
        bookmarks.add(QStringLiteral("Example"), QUrl(QStringLiteral("https://example.com")));
        bookmarks.add(QStringLiteral("Qt"), QUrl(QStringLiteral("https://www.qt.io")));
        QVERIFY(bookmarks.save());
    }
    Bookmarks reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.count(), 2);
    QCOMPARE(reloaded.items().at(0).title, QStringLiteral("Example"));
    QCOMPARE(reloaded.items().at(1).url, QUrl(QStringLiteral("https://www.qt.io")));
    QVERIFY(!reloaded.items().at(0).id.isEmpty());
}

void TestBookmarks::corruptFileIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("bookmarks.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{broken");
    file.close();

    Bookmarks bookmarks(path);
    QVERIFY(!bookmarks.load());
    QCOMPARE(bookmarks.count(), 0);
}

void TestBookmarks::changedSignal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Bookmarks bookmarks(dir.filePath(QStringLiteral("bookmarks.json")));
    bookmarks.load();

    QSignalSpy spy(&bookmarks, &Bookmarks::changed);
    const QString id = bookmarks.add(QStringLiteral("Example"),
                                     QUrl(QStringLiteral("https://example.com")));
    bookmarks.rename(id, QStringLiteral("Renamed"));
    bookmarks.remove(id);
    QCOMPARE(spy.count(), 3);

    bookmarks.remove(QStringLiteral("ghost"));
    QCOMPARE(spy.count(), 3);
}

void TestBookmarks::findById()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Bookmarks bookmarks(dir.filePath(QStringLiteral("bookmarks.json")));
    bookmarks.load();

    const QString id = bookmarks.add(QStringLiteral("Example"),
                                     QUrl(QStringLiteral("https://example.com")));
    QCOMPARE(bookmarks.findById(id).url, QUrl(QStringLiteral("https://example.com")));
    QVERIFY(bookmarks.findById(QStringLiteral("ghost")).id.isEmpty());
}

QTEST_GUILESS_MAIN(TestBookmarks)
#include "test_bookmarks.moc"
