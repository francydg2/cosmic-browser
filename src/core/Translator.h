#pragma once

#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

/// Pure helpers of the page translator: language list and detection,
/// per-site preferences, batching, and parsing of model replies.
namespace Translator {

struct Language
{
    QString code;
    QString name;
};

QList<Language> languages();
QString languageName(const QString &code);

/// Best-effort detection from visible text. Uses Unicode scripts for
/// non-Latin languages and stop-word frequency for Latin ones. Returns
/// an empty string when there is not enough text or no clear winner;
/// callers may then fall back to the page's declared language.
QString detectLanguage(const QString &text);

/// "en-GB" → "en"; empty when not a plausible language tag.
QString primaryTag(const QString &declared);

/// Splits \p texts into consecutive batches no larger than \p maxChars
/// (and \p maxItems items). Returns [start, end) index ranges. A single
/// oversized string forms its own batch.
QList<QPair<int, int>> batches(const QStringList &texts, int maxChars, int maxItems = 60);

/// Extracts a JSON array of exactly \p expected strings from a model
/// reply (tolerates code fences and surrounding prose).
bool parseTranslations(const QString &reply, int expected, QStringList *out);

/// Preferences, stored in the "translation" settings section.
QString targetLanguage(const QJsonObject &section, const QString &fallback);
QJsonObject withTargetLanguage(QJsonObject section, const QString &code);
/// "", "always" or "never".
QString siteRule(const QJsonObject &section, const QString &host);
QJsonObject withSiteRule(QJsonObject section, const QString &host, const QString &rule);
/// Language to auto-translate this host into ("" when no "always" rule).
QString siteTarget(const QJsonObject &section, const QString &host);

} // namespace Translator
