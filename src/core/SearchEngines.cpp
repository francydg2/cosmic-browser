#include "core/SearchEngines.h"

namespace SearchEngines {

namespace {

const auto kFallbackTemplate = "https://duckduckgo.com/?q=%1";

} // namespace

QList<SearchEngine> builtins()
{
    return {
        {QStringLiteral("duckduckgo"), QStringLiteral("DuckDuckGo"),
         QStringLiteral("https://duckduckgo.com/?q=%1")},
        {QStringLiteral("google"), QStringLiteral("Google"),
         QStringLiteral("https://www.google.com/search?q=%1")},
        {QStringLiteral("bing"), QStringLiteral("Bing"),
         QStringLiteral("https://www.bing.com/search?q=%1")},
        {QStringLiteral("startpage"), QStringLiteral("Startpage"),
         QStringLiteral("https://www.startpage.com/sp/search?query=%1")},
        {QStringLiteral("brave"), QStringLiteral("Brave Search"),
         QStringLiteral("https://search.brave.com/search?q=%1")},
        {QStringLiteral("mojeek"), QStringLiteral("Mojeek"),
         QStringLiteral("https://www.mojeek.com/search?q=%1")},
    };
}

const SearchEngine *builtinById(const QString &id)
{
    static const QList<SearchEngine> engines = builtins();
    for (const SearchEngine &engine : engines) {
        if (engine.id == id) {
            return &engine;
        }
    }
    return nullptr;
}

bool isValidTemplate(const QString &urlTemplate)
{
    const bool hasScheme = urlTemplate.startsWith(QLatin1String("https://"))
        || urlTemplate.startsWith(QLatin1String("http://"));
    return hasScheme && urlTemplate.contains(QLatin1String("%1"));
}

QUrl buildSearchUrl(const QString &urlTemplate, const QString &query)
{
    const QString use = isValidTemplate(urlTemplate) ? urlTemplate
                                                      : QString::fromLatin1(kFallbackTemplate);
    const QString encoded =
        QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed()));
    return QUrl(use.arg(encoded));
}

QString defaultTemplate()
{
    return QString::fromLatin1(kFallbackTemplate);
}

} // namespace SearchEngines
