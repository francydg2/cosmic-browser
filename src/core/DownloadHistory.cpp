#include "core/DownloadHistory.h"

#include "core/Logging.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QJsonObject toJson(const DownloadRecord &r)
{
    return {
        {QStringLiteral("id"), r.id},
        {QStringLiteral("url"), r.url.toString(QUrl::FullyEncoded)},
        {QStringLiteral("fileName"), r.fileName},
        {QStringLiteral("directory"), r.directory},
        {QStringLiteral("totalBytes"), static_cast<double>(r.totalBytes)},
        {QStringLiteral("receivedBytes"), static_cast<double>(r.receivedBytes)},
        {QStringLiteral("state"), r.state},
        {QStringLiteral("error"), r.error},
        {QStringLiteral("startedMs"), static_cast<double>(r.startedMs)},
        {QStringLiteral("finishedMs"), static_cast<double>(r.finishedMs)},
    };
}

bool fromJson(const QJsonObject &o, DownloadRecord *out)
{
    DownloadRecord r;
    r.id = o.value(QStringLiteral("id")).toString();
    r.fileName = o.value(QStringLiteral("fileName")).toString();
    r.state = o.value(QStringLiteral("state")).toString();
    if (r.id.isEmpty() || r.fileName.isEmpty()
        || (r.state != QLatin1String("completed") && r.state != QLatin1String("cancelled")
            && r.state != QLatin1String("failed"))) {
        return false;
    }
    r.url = QUrl(o.value(QStringLiteral("url")).toString());
    r.directory = o.value(QStringLiteral("directory")).toString();
    r.totalBytes = static_cast<qint64>(o.value(QStringLiteral("totalBytes")).toDouble(-1));
    r.receivedBytes = static_cast<qint64>(o.value(QStringLiteral("receivedBytes")).toDouble());
    r.error = o.value(QStringLiteral("error")).toString();
    r.startedMs = static_cast<qint64>(o.value(QStringLiteral("startedMs")).toDouble());
    r.finishedMs = static_cast<qint64>(o.value(QStringLiteral("finishedMs")).toDouble());
    *out = r;
    return true;
}

} // namespace

DownloadHistory::DownloadHistory(const QString &filePath)
    : m_path(filePath)
{
}

bool DownloadHistory::load()
{
    m_records.clear();
    QFile file(m_path);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcCosmic) << "cannot read download history" << m_path;
        return false;
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcCosmic) << "download history is corrupt, left untouched" << m_path;
        return false;
    }
    const QJsonArray array = doc.object().value(QStringLiteral("downloads")).toArray();
    for (const QJsonValue &value : array) {
        DownloadRecord record;
        if (value.isObject() && fromJson(value.toObject(), &record) && !contains(record.id)) {
            m_records.append(record);
        }
        if (m_records.size() >= maxEntries) {
            break;
        }
    }
    return true;
}

bool DownloadHistory::save() const
{
    QJsonArray array;
    for (const DownloadRecord &record : m_records) {
        array.append(toJson(record));
    }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1}, {QStringLiteral("downloads"), array}}).toJson());
    return file.commit();
}

bool DownloadHistory::contains(const QString &id) const
{
    for (const DownloadRecord &record : m_records) {
        if (record.id == id) {
            return true;
        }
    }
    return false;
}

void DownloadHistory::upsert(const DownloadRecord &record)
{
    for (int i = 0; i < m_records.size(); ++i) {
        if (m_records.at(i).id == record.id) {
            m_records.removeAt(i);
            break;
        }
    }
    m_records.prepend(record);
    while (m_records.size() > maxEntries) {
        m_records.removeLast();
    }
}

bool DownloadHistory::remove(const QString &id)
{
    for (int i = 0; i < m_records.size(); ++i) {
        if (m_records.at(i).id == id) {
            m_records.removeAt(i);
            return true;
        }
    }
    return false;
}

void DownloadHistory::clear()
{
    m_records.clear();
}

QList<DownloadRecord> DownloadHistory::filtered(const QList<DownloadRecord> &records,
                                                const QString &query, const QString &state)
{
    const QString needle = query.trimmed();
    QList<DownloadRecord> result;
    for (const DownloadRecord &record : records) {
        if (!state.isEmpty() && record.state != state) {
            continue;
        }
        if (!needle.isEmpty()
            && !record.fileName.contains(needle, Qt::CaseInsensitive)
            && !record.url.toString().contains(needle, Qt::CaseInsensitive)
            && !record.directory.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        result.append(record);
    }
    return result;
}

void SpeedTracker::sample(qint64 timeMs, qint64 receivedBytes)
{
    if (!m_samples.empty() && (timeMs < m_samples.back().timeMs
                                 || receivedBytes < m_samples.back().bytes)) {
        m_samples.clear(); // restarted or resumed transfer
    }
    m_samples.push_back({timeMs, receivedBytes});
    while (m_samples.size() > 2 && timeMs - m_samples.front().timeMs > windowMs) {
        m_samples.pop_front();
    }
}

double SpeedTracker::bytesPerSecond() const
{
    if (m_samples.size() < 2) {
        return 0;
    }
    const Sample &first = m_samples.front();
    const Sample &last = m_samples.back();
    const qint64 elapsed = last.timeMs - first.timeMs;
    return elapsed > 0 ? (last.bytes - first.bytes) * 1000.0 / elapsed : 0;
}

qint64 SpeedTracker::secondsRemaining(qint64 totalBytes, qint64 receivedBytes) const
{
    const double speed = bytesPerSecond();
    if (totalBytes <= 0 || speed < 1 || receivedBytes > totalBytes) {
        return -1;
    }
    return static_cast<qint64>((totalBytes - receivedBytes) / speed + 0.5);
}
