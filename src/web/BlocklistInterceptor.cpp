#include "web/BlocklistInterceptor.h"
#include "core/Logging.h"

#include <QFile>
#include <QTextStream>
#include <QWebEngineUrlRequestInfo>
#include <QMutexLocker>
#include <QReadLocker>
#include <QWriteLocker>

BlocklistInterceptor::BlocklistInterceptor(bool enabled, QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
    m_enabled.storeRelease(enabled ? 1 : 0);
    loadBuiltinRules();
}

void BlocklistInterceptor::loadBuiltinRules()
{
    QFile file(QStringLiteral(":/blocklists/default.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcCosmic) << "builtin blocklist resource missing";
        return;
    }
    QTextStream stream(&file);
    int added = 0;
    Category category = Category::Unclassified;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.startsWith(QLatin1String("# ---"))) {
            if (line.contains(QLatin1String("advertising"), Qt::CaseInsensitive)) {
                category = Category::Advertising;
            } else if (line.contains(QLatin1String("analytics / tracking"), Qt::CaseInsensitive)) {
                category = Category::AnalyticsOrTracking;
            } else if (line.contains(QLatin1String("social pixels"), Qt::CaseInsensitive)) {
                category = Category::Social;
            } else if (line.contains(QLatin1String("fingerprinting"), Qt::CaseInsensitive)) {
                category = Category::Other;
            } else {
                category = Category::Unclassified;
            }
        }
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        addDomain(line.toLower(), category);
        ++added;
    }
    qCInfo(lcCosmic) << "builtin blocklist rules loaded:" << added;
}

void BlocklistInterceptor::loadUserRules(const QString &filePath)
{
    QFile file(filePath);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcCosmic) << "cannot read user blocklist" << filePath;
        return;
    }
    QTextStream stream(&file);
    int added = 0;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        addDomain(line.toLower());
        ++added;
    }
    qCInfo(lcCosmic) << "user blocklist rules loaded:" << added << "from" << filePath;
}

void BlocklistInterceptor::addDomain(const QString &domain, Category category)
{
    // Strip accidental scheme/path so "https://foo.com/bar" still works.
    QString clean = domain;
    if (clean.contains(QLatin1String("://"))) {
        clean = clean.mid(clean.indexOf(QLatin1String("://")) + 3);
    }
    const int cut = clean.indexOf(QLatin1Char('/'));
    if (cut >= 0) {
        clean = clean.left(cut);
    }
    if (clean.startsWith(QLatin1Char('*'))) {
        clean = clean.mid(1);
    }
    if (clean.startsWith(QLatin1Char('.'))) {
        clean = clean.mid(1);
    }
    if (!clean.isEmpty()) {
        QWriteLocker locker(&m_rulesLock);
        m_domains.insert(clean);
        if (category != Category::Unclassified) {
            m_categories.insert(clean, category);
        }
    }
}

bool BlocklistInterceptor::shouldBlock(const QUrl &url) const
{
    return isEnabled() && !matchingRule(url).isEmpty();
}

QString BlocklistInterceptor::matchingRule(const QUrl &url, Category *category) const
{
    QString host = url.host().toLower();
    QReadLocker locker(&m_rulesLock);
    while (!host.isEmpty()) {
        if (m_domains.contains(host)) {
            if (category) {
                *category = m_categories.value(host, Category::Unclassified);
            }
            return host;
        }
        const qsizetype dot = host.indexOf(QLatin1Char('.'));
        if (dot < 0) {
            break;
        }
        host = host.mid(dot + 1);
    }
    return {};
}

bool BlocklistInterceptor::tryBlock(const QUrl &url, const QUrl &firstParty)
{
    if (!isEnabled()) {
        return false;
    }
    Category category = Category::Unclassified;
    const QString rule = matchingRule(url, &category);
    if (rule.isEmpty()) {
        return false;
    }
    int count;
    {
        QMutexLocker locker(&m_statisticsLock);
        count = ++m_blocked;
        m_matchedRules.insert(rule);
        if (m_recent.size() == 500) {
            m_recent.removeFirst();
        }
        m_recent.append({QDateTime::currentDateTimeUtc(), url.host().toLower(),
                         firstParty.host().toLower(), rule, category});
    }
    qCDebug(lcCosmic).noquote() << "blocked request to" << url.host();
    emit blockedCountChanged(count);
    emit statisticsChanged();
    return true;
}

int BlocklistInterceptor::blockedCount() const
{
    QMutexLocker locker(&m_statisticsLock);
    return m_blocked;
}

int BlocklistInterceptor::ruleCount() const
{
    QReadLocker locker(&m_rulesLock);
    return m_domains.size();
}

BlocklistInterceptor::Statistics BlocklistInterceptor::statistics() const
{
    QMutexLocker locker(&m_statisticsLock);
    return {m_blocked, static_cast<int>(m_matchedRules.size()), m_recent};
}

void BlocklistInterceptor::clearStatistics()
{
    {
        QMutexLocker locker(&m_statisticsLock);
        m_blocked = 0;
        m_recent.clear();
        m_matchedRules.clear();
    }
    emit blockedCountChanged(0);
    emit statisticsChanged();
}

void BlocklistInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (isDoNotTrack()) {
        info.setHttpHeader(QByteArrayLiteral("DNT"), QByteArrayLiteral("1"));
    }
    if (tryBlock(info.requestUrl(), info.firstPartyUrl())) {
        info.block(true);
    }
}
