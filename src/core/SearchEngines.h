#pragma once

#include <QList>
#include <QString>
#include <QUrl>

/// A search engine definition. `urlTemplate` must contain %1 where the
/// percent-encoded query goes (e.g. "https://duckduckgo.com/?q=%1").
struct SearchEngine
{
    QString id;
    QString name;
    QString urlTemplate;
};

/// Registry of built-in search engines (Phase 2, spec: configurable
/// engines). Users pick one in Settings (or define a custom template);
/// the default engine id lives in Settings, not here.
namespace SearchEngines {

/// Built-in engines, first one is the product default.
QList<SearchEngine> builtins();

/// Returns the engine with \p id, or nullptr.
const SearchEngine *builtinById(const QString &id);

/// True when \p urlTemplate is usable (http(s) scheme + a %1 placeholder).
bool isValidTemplate(const QString &urlTemplate);

/// Builds the final search URL for \p query. An invalid template falls
/// back to the default engine's template (never produces a broken URL).
QUrl buildSearchUrl(const QString &urlTemplate, const QString &query);

/// Template of the product default engine (DuckDuckGo).
QString defaultTemplate();

} // namespace SearchEngines
