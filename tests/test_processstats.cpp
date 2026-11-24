#include "core/ProcessStats.h"

#include <QCoreApplication>
#include <QtTest>
#include <unistd.h>

using namespace ProcessStats;

class TestProcessStats : public QObject
{
    Q_OBJECT
private slots:
    void parsesProcStatWithTrickyName();
    void rejectsGarbage();
    void parsesPsOutput();
    void classifiesChromiumProcesses();
    void findsDescendantsOnly();
    void computesCpuFromTickDeltas();
    void samplesTheRunningTestProcess();
};

void TestProcessStats::parsesProcStatWithTrickyName()
{
    // Field order: state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime ...
    const QByteArray line =
        "4242 (my (weird) app) S 100 4242 4242 0 -1 4194560 1 0 0 0 150 50 0 0 20 0 1 0 5000 "
        "104857600 2560 18446744073709551615";
    Raw raw;
    QVERIFY(parseProcStat(line, &raw, 4096));
    QCOMPARE(raw.pid, qint64(4242));
    QCOMPARE(raw.ppid, qint64(100));
    QCOMPARE(raw.command, QStringLiteral("my (weird) app"));
    QCOMPARE(raw.cpuTicks, quint64(200));
    QCOMPARE(raw.rssBytes, qint64(2560) * 4096);
}

void TestProcessStats::rejectsGarbage()
{
    Raw raw;
    QVERIFY(!parseProcStat("not a stat line", &raw, 4096));
    QVERIFY(!parseProcStat("1 (x) S 0", &raw, 4096));
}

void TestProcessStats::parsesPsOutput()
{
    const QByteArray output =
        "  100     1  204800   3.5 /Applications/Cosmic.app/Contents/MacOS/cosmic\n"
        "  101   100  102400  12,0 /Applications/Cosmic.app/.../QtWebEngineProcess --type=renderer --foo\n"
        "garbage line\n";
    const QList<Raw> list = parsePs(output);
    QCOMPARE(list.size(), 2);
    QCOMPARE(list.at(0).rssBytes, qint64(204800) * 1024);
    QCOMPARE(list.at(0).command, QStringLiteral("cosmic"));
    QCOMPARE(list.at(1).psCpuPercent, 12.0);
    QCOMPARE(classify(list.at(1).args, false), QStringLiteral("renderer"));
}

void TestProcessStats::classifiesChromiumProcesses()
{
    QCOMPARE(classify(QStringLiteral("x --type=gpu-process --y"), false), QStringLiteral("gpu"));
    QCOMPARE(classify(QStringLiteral("x --type=utility"), false), QStringLiteral("utility"));
    QCOMPARE(classify(QStringLiteral("x --type=zygote"), false), QStringLiteral("utility"));
    QCOMPARE(classify(QStringLiteral("x --type=broker"), false), QStringLiteral("other"));
    QCOMPARE(classify(QStringLiteral("plain"), false), QStringLiteral("other"));
    QCOMPARE(classify(QStringLiteral("anything"), true), QStringLiteral("browser"));
}

void TestProcessStats::findsDescendantsOnly()
{
    QList<Raw> all;
    for (auto [pid, ppid] : {qMakePair(1, 0), qMakePair(10, 1), qMakePair(11, 10), qMakePair(12, 11),
                             qMakePair(20, 1), qMakePair(21, 20)}) {
        Raw raw;
        raw.pid = pid;
        raw.ppid = ppid;
        all.append(raw);
    }
    const QList<Raw> family = descendants(all, 10);
    QSet<qint64> pids;
    for (const Raw &raw : family) {
        pids.insert(raw.pid);
    }
    QCOMPARE(pids, (QSet<qint64>{10, 11, 12}));
    QVERIFY(descendants(all, 999).isEmpty());
}

void TestProcessStats::computesCpuFromTickDeltas()
{
    Raw raw;
    raw.pid = 7;
    raw.ppid = 1;
    raw.cpuTicks = 300;
    QHash<qint64, quint64> previous{{7, 100}};
    // 200 ticks at 100 Hz = 2 CPU seconds over 4 wall seconds = 50 %.
    QCOMPARE(build({raw}, previous, 4.0, 100, 7).first().cpuPercent, 50.0);
    // No earlier sample → unknown, never 0 or invented.
    QCOMPARE(build({raw}, {}, 4.0, 100, 7).first().cpuPercent, -1.0);
    // A reused pid whose counter went backwards is not reported.
    previous[7] = 1000;
    QCOMPARE(build({raw}, previous, 4.0, 100, 7).first().cpuPercent, -1.0);
}

void TestProcessStats::samplesTheRunningTestProcess()
{
#if defined(Q_OS_LINUX)
    QVERIFY(isSupported());
    Sampler sampler{qint64(getpid())};
    const QList<Process> first = sampler.sample();
    QVERIFY(!first.isEmpty());
    QCOMPARE(first.first().kind, QStringLiteral("browser"));
    QVERIFY(first.first().rssBytes > 1024 * 1024);
    QCOMPARE(first.first().cpuPercent, -1.0);
    // Burn a little CPU so the second sample has something to measure.
    QElapsedTimer timer;
    timer.start();
    volatile double sink = 0;
    while (timer.elapsed() < 300) {
        sink += 1.0;
    }
    const QList<Process> second = sampler.sample();
    QVERIFY(second.first().cpuPercent >= 0);
#else
    QSKIP("Linux only");
#endif
}

QTEST_APPLESS_MAIN(TestProcessStats)
#include "test_processstats.moc"
