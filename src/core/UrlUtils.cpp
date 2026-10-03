#include "core/UrlUtils.h"

#include <QRegularExpression>

namespace UrlUtils {

namespace {

// DuckDuckGo as privacy-friendly default; replaced by a configurable
// SearchEngineRegistry in Phase 2.
constexpr auto kDefaultSearchTemplate = "https://duckduckgo.com/?q=%1";

bool hasScheme(const QString &input)
{
    static const QRegularExpression schemeRe(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*://"));
    return schemeRe.match(input).hasMatch();
}

bool isLoopbackHost(const QString &s)
{
    static const QRegularExpression localRe(
        QStringLiteral("^localhost(:\\d+)?([/?#].*)?$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression ipv6Re(QStringLiteral("^\\[::1\\](:\\d+)?([/?#].*)?$"));
    return localRe.match(s).hasMatch() || ipv6Re.match(s).hasMatch();
}

bool isBareIPv4(const QString &s)
{
    static const QRegularExpression ipv4Re(QStringLiteral("^\\d{1,3}(\\.\\d{1,3}){3}(:\\d+)?([/?#].*)?$"));
    return ipv4Re.match(s).hasMatch();
}

} // namespace

bool looksLikeUrl(const QString &input)
{
    const QString s = input.trimmed();
    if (s.isEmpty()) {
        return false;
    }

    if (hasScheme(s)) {
        return true;
    }

    // Anything with a space is a search query, not a URL.
    if (s.contains(QLatin1Char(' '))) {
        return false;
    }

    if (isLoopbackHost(s) || isBareIPv4(s)) {
        return true;
    }

    // "example.com", "docs.rs" — a dot usually means a host.
    if (s.contains(QLatin1Char('.')) && !s.startsWith(QLatin1Char('.'))
        && !s.endsWith(QLatin1Char('.'))) {
        return true;
    }

    // Scheme-like prefixes without "//" (e.g. "file:/tmp", "about:blank").
    if (s.contains(QLatin1Char(':'))) {
        const auto scheme = s.left(s.indexOf(QLatin1Char(':'))).toLower();
        static const QRegularExpression schemeNameRe(QStringLiteral("^[a-z][a-z0-9+.-]*$"));
        if (schemeNameRe.match(scheme).hasMatch()) {
            return true;
        }
    }

    return false;
}

QUrl searchUrl(const QString &query)
{
    return QUrl(QString::fromLatin1(kDefaultSearchTemplate)
                    .arg(QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed()))));
}

QUrl toNavigableUrl(const QString &input, const QString &searchTemplate)
{
    const QString s = input.trimmed();
    if (s.isEmpty()) {
        return {};
    }
    if (!looksLikeUrl(s)) {
        // Invalid/empty template → default engine (never a broken URL).
        const QString use = (searchTemplate.startsWith(QLatin1String("https://"))
                             || searchTemplate.startsWith(QLatin1String("http://")))
                && searchTemplate.contains(QLatin1String("%1"))
            ? searchTemplate
            : QString::fromLatin1(kDefaultSearchTemplate);
        return QUrl(use.arg(QString::fromUtf8(QUrl::toPercentEncoding(s))));
    }
    if (hasScheme(s)) {
        const QUrl u(s);
        return u.isValid() && !u.scheme().isEmpty() ? u : searchUrl(s);
    }
    const QString prefix = (isLoopbackHost(s) || isBareIPv4(s)) ? QStringLiteral("http://")
                                                                : QStringLiteral("https://");
    const QUrl u(prefix + s);
    return u.isValid() ? u : searchUrl(s);
}

QUrl toNavigableUrl(const QString &input)
{
    return toNavigableUrl(input, QString());
}

QUrl upgradedToHttps(const QUrl &url)
{
    if (!url.isValid() || url.scheme() != QLatin1String("http") || url.host().isEmpty()) {
        return url;
    }
    QUrl result = url;
    result.setScheme(QStringLiteral("https"));
    return result;
}

} // namespace UrlUtils
