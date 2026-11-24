#pragma once

#include <deque>
#include <QList>
#include <QString>
#include <QUrl>

/// A finished download remembered across sessions.
struct DownloadRecord
{
    QString id;
    QUrl url;
    QString fileName;
    QString directory;
    qint64 totalBytes = -1;
    qint64 receivedBytes = 0;
    QString state; // "completed", "cancelled" or "failed"
    QString error;
    qint64 startedMs = 0;
    qint64 finishedMs = 0;
};

/// Persistent list of finished downloads (`downloads.json`). Only
/// metadata is stored; the file contents are never touched. The list is
/// capped, newest first, and written atomically. A corrupt file is left
/// untouched and reported by load().
class DownloadHistory
{
public:
    static constexpr int maxEntries = 500;

    explicit DownloadHistory(const QString &filePath);

    bool load();
    bool save() const;

    QList<DownloadRecord> records() const { return m_records; }
    bool contains(const QString &id) const;
    /// Inserts or replaces by id, keeps newest first and the size cap.
    void upsert(const DownloadRecord &record);
    bool remove(const QString &id);
    void clear();

    /// Case-insensitive match on file name, address and location;
    /// \p state "" matches all.
    static QList<DownloadRecord> filtered(const QList<DownloadRecord> &records,
                                          const QString &query, const QString &state);

private:
    QString m_path;
    QList<DownloadRecord> m_records;
};

/// Rolling transfer-rate estimate over a short window, so a stalled or
/// bursty transfer does not report a stale average.
class SpeedTracker
{
public:
    void sample(qint64 timeMs, qint64 receivedBytes);
    /// Bytes per second, or 0 when there are not enough samples.
    double bytesPerSecond() const;
    /// Remaining seconds, or -1 when unknown (no speed or unknown size).
    qint64 secondsRemaining(qint64 totalBytes, qint64 receivedBytes) const;

private:
    static constexpr qint64 windowMs = 5000;
    struct Sample { qint64 timeMs; qint64 bytes; };
    std::deque<Sample> m_samples;
};
