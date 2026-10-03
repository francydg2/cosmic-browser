#pragma once

#include <QString>
#include <QUrl>

/// Helpers to turn what the user types in the address bar into something
/// navigable: either a URL or a search query.
///
/// Default search engine is DuckDuckGo (privacy-friendly default).
/// It becomes configurable in Phase 2 (see docs/ARCHITECTURE.md).
namespace UrlUtils {

/// Returns true when \p input should be treated as a URL rather than a
/// search query (has a scheme, looks like a host/IP/localhost, etc.).
bool looksLikeUrl(const QString &input);

/// Converts \p input to a navigable URL: a URL if it looks like one,
/// otherwise a search-engine query URL.
QUrl toNavigableUrl(const QString &input);

/// Same, but with an explicit search template (must contain %1; an invalid
/// or empty template falls back to the default engine). This is how the
/// configured engine (Settings) reaches the address bar and New Tab page.
QUrl toNavigableUrl(const QString &input, const QString &searchTemplate);

/// Builds the search URL for \p query using the default engine.
QUrl searchUrl(const QString &query);

/// HTTPS-Only helper: returns the https:// version of \p url when it is
/// an upgradeable http:// URL (scheme http, non-empty host), otherwise
/// \p url unchanged. Pure — used by the page policy and tests.
QUrl upgradedToHttps(const QUrl &url);

} // namespace UrlUtils
