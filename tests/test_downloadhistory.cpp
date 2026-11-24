#include "core/DownloadHistory.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestDownloadHistory : public QObject
{
    Q_OBJECT
private slots:
    void persistsAndReloads();
    void upsertReplacesAndCaps();
    void corruptFileIsKept();
    void filtersByQueryAndState();
    void speedAndEta();
};

static DownloadRecord record(const QString &id, const QString &name, const QString &state)
{
    DownloadRecord r;
    r.id = id;
    r.fileName = name;
    r.directory = QStringLiteral("/home/user/Downloads");
    r.url = QUrl(QStringLiteral("https://example.com/") + name);
    r.state = state;
    r.totalBytes = 100;
    r.receivedBytes = 100;
    return r;
}

void TestDownloadHistory::persistsAndReloads()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("downloads.json"));
    DownloadHistory history(path);
    QVERIFY(history.load());
    history.upsert(record(QStringLiteral("a"), QStringLiteral("a.zip"), QStringLiteral("completed")));
    DownloadRecord failed = record(QStringLiteral("b"), QStringLiteral("b.iso"), QStringLiteral("failed"));
    failed.error = QStringLiteral("Network failed");
    history.upsert(failed);
    QVERIFY(history.save());

    DownloadHistory reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.records().size(), 2);
    QCOMPARE(reloaded.records().first().id, QStringLiteral("b"));
    QCOMPARE(reloaded.records().first().error, QStringLiteral("Network failed"));
    QCOMPARE(reloaded.records().last().url.toString(), QStringLiteral("https://example.com/a.zip"));
}

void TestDownloadHistory::upsertReplacesAndCaps()
{
    DownloadHistory history{QString()};
    history.upsert(record(QStringLiteral("a"), QStringLiteral("a"), QStringLiteral("failed")));
    history.upsert(record(QStringLiteral("a"), QStringLiteral("a"), QStringLiteral("completed")));
    QCOMPARE(history.records().size(), 1);
    QCOMPARE(history.records().first().state, QStringLiteral("completed"));
    for (int i = 0; i < DownloadHistory::maxEntries + 10; ++i) {
        history.upsert(record(QString::number(i + 100), QStringLiteral("f"), QStringLiteral("completed")));
    }
    QCOMPARE(history.records().size(), DownloadHistory::maxEntries);
    QVERIFY(!history.contains(QStringLiteral("a")));
    QVERIFY(history.remove(QStringLiteral("300")));
    QVERIFY(!history.remove(QStringLiteral("300")));
}

void TestDownloadHistory::corruptFileIsKept()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("downloads.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{not json");
    file.close();
    DownloadHistory history(path);
    QVERIFY(!history.load());
    QVERIFY(history.records().isEmpty());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("{not json"));
}

void TestDownloadHistory::filtersByQueryAndState()
{
    const QList<DownloadRecord> all{
        record(QStringLiteral("1"), QStringLiteral("Report.PDF"), QStringLiteral("completed")),
        record(QStringLiteral("2"), QStringLiteral("movie.mkv"), QStringLiteral("failed")),
        record(QStringLiteral("3"), QStringLiteral("song.mp3"), QStringLiteral("cancelled"))};
    QCOMPARE(DownloadHistory::filtered(all, QStringLiteral("report"), {}).size(), 1);
    QCOMPARE(DownloadHistory::filtered(all, {}, QStringLiteral("failed")).size(), 1);
    QCOMPARE(DownloadHistory::filtered(all, QStringLiteral("example.com"), {}).size(), 3);
    QCOMPARE(DownloadHistory::filtered(all, QStringLiteral("report"), QStringLiteral("failed")).size(), 0);
}

void TestDownloadHistory::speedAndEta()
{
    SpeedTracker tracker;
    QCOMPARE(tracker.bytesPerSecond(), 0.0);
    tracker.sample(0, 0);
    tracker.sample(2000, 2000);
    QCOMPARE(tracker.bytesPerSecond(), 1000.0);
    QCOMPARE(tracker.secondsRemaining(10000, 2000), qint64(8));
    QCOMPARE(tracker.secondsRemaining(-1, 2000), qint64(-1));
    // Old samples fall out of the window so a stall is visible.
    tracker.sample(20000, 2000);
    QCOMPARE(tracker.bytesPerSecond(), 0.0);
    // A restarted transfer resets the estimate instead of going negative.
    tracker.sample(21000, 100);
    QVERIFY(tracker.bytesPerSecond() >= 0.0);
}

QTEST_APPLESS_MAIN(TestDownloadHistory)
#include "test_downloadhistory.moc"
