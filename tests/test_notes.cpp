#include "core/Notes.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestNotes : public QObject
{
    Q_OBJECT
private slots:
    void persistenceAndDeletion();
    void corruptFileIsPreserved();
    void writeFailureDoesNotChangeMemory();
    void invalidPageIsRejected();
};

void TestNotes::persistenceAndDeletion()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("notes.json"));
    Notes notes(path);
    QVERIFY(notes.load());
    Note note;
    note.title = QStringLiteral("Title");
    note.content = QStringLiteral("Selected text");
    note.category = QStringLiteral("Work");
    note.pageUrl = QUrl(QStringLiteral("https://example.com/path?q=1"));
    QString id;
    QVERIFY(notes.create(note, &id));
    QVERIFY(!id.isEmpty());
    note = notes.find(id);
    QVERIFY(note.created.isValid());
    QVERIFY(note.modified.isValid());
    const auto created = note.created;
    note.content = QStringLiteral("Changed");
    QVERIFY(notes.update(note));
    Notes loaded(path);
    QVERIFY(loaded.load());
    QCOMPARE(loaded.items().size(), 1);
    QCOMPARE(loaded.find(id).content, QStringLiteral("Changed"));
    QCOMPARE(loaded.find(id).category, QStringLiteral("Work"));
    QCOMPARE(loaded.find(id).pageUrl, note.pageUrl);
    QCOMPARE(loaded.find(id).created, created);
    QVERIFY(loaded.remove(id));
    Notes empty(path);
    QVERIFY(empty.load());
    QVERIFY(empty.items().isEmpty());
}

void TestNotes::corruptFileIsPreserved()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("notes.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("{broken"), 7);
    file.close();
    Notes notes(path);
    QString error;
    QVERIFY(!notes.load(&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!notes.create(Note{}, nullptr, &error));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("{broken"));
}

void TestNotes::writeFailureDoesNotChangeMemory()
{
    QTemporaryDir dir;
    Notes notes(dir.filePath(QStringLiteral("missing/notes.json")));
    QVERIFY(notes.load());
    QString error;
    QVERIFY(!notes.create(Note{}, nullptr, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(notes.items().isEmpty());
}

void TestNotes::invalidPageIsRejected()
{
    QTemporaryDir dir;
    Notes notes(dir.filePath(QStringLiteral("notes.json")));
    QVERIFY(notes.load());
    Note note;
    note.pageUrl = QUrl(QStringLiteral("https://user:password@example.com/"));
    QString error;
    QVERIFY(!notes.create(note, nullptr, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(notes.items().isEmpty());
}

QTEST_GUILESS_MAIN(TestNotes)
#include "test_notes.moc"
