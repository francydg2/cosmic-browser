#include "web/BlocklistInterceptor.h"
#include "core/Logging.h"

#include <QFile>
#include <QTextStream>
#include <QWebEngineUrlRequestInfo>

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
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        addDomain(line.toLower());
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

void BlocklistInterceptor::addDomain(const QString &domain)
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
        m_domains.insert(clean);
    }
}

bool BlocklistInterceptor::shouldBlock(const QUrl &url) const
{
    if (!isEnabled()) {
        return false;
    }
    const QString host = url.host().toLower();
    if (host.isEmpty()) {
        return false;
    }
    if (m_domains.contains(host)) {
        return true;
    }
    for (const QString &domain : m_domains) {
        if (host.endsWith(QLatin1Char('.') + domain)) {
            return true;
        }
    }
    return false;
}

bool BlocklistInterceptor::tryBlock(const QUrl &url)
{
    if (!shouldBlock(url)) {
        return false;
    }
    const int count = m_blocked.fetchAndAddOrdered(1) + 1;
    qCDebug(lcCosmic).noquote() << "blocked request to" << url.host();
    emit blockedCountChanged(count);
    return true;
}

void BlocklistInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (isDoNotTrack()) {
        info.setHttpHeader(QByteArrayLiteral("DNT"), QByteArrayLiteral("1"));
    }
    if (tryBlock(info.requestUrl())) {
        info.block(true);
    }
}
