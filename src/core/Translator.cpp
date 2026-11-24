#include "core/Translator.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

namespace Translator {

namespace {

const QHash<QString, QStringList> &stopWords()
{
    static const QHash<QString, QStringList> words{
        {QStringLiteral("en"), {"the", "and", "of", "to", "is", "that", "with", "for", "this", "are", "you", "not", "have", "from", "was"}},
        {QStringLiteral("it"), {"il", "di", "che", "è", "non", "per", "una", "con", "sono", "della", "come", "più", "gli", "alla", "anche"}},
        {QStringLiteral("fr"), {"le", "la", "les", "des", "est", "et", "pour", "pas", "dans", "que", "une", "qui", "sur", "avec", "vous"}},
        {QStringLiteral("de"), {"der", "die", "und", "ist", "nicht", "das", "mit", "für", "ein", "auf", "den", "sie", "von", "auch", "zu"}},
        {QStringLiteral("es"), {"el", "la", "de", "que", "y", "en", "los", "por", "con", "para", "una", "las", "del", "es", "se"}},
        {QStringLiteral("pt"), {"o", "de", "que", "e", "do", "da", "em", "um", "para", "com", "não", "uma", "os", "no", "se"}},
        {QStringLiteral("nl"), {"de", "het", "een", "en", "van", "is", "dat", "niet", "op", "te", "voor", "met", "zijn", "ook", "aan"}},
    };
    return words;
}

} // namespace

QList<Language> languages()
{
    return {
        {QStringLiteral("it"), QStringLiteral("Italiano")},
        {QStringLiteral("en"), QStringLiteral("English")},
        {QStringLiteral("fr"), QStringLiteral("Français")},
        {QStringLiteral("de"), QStringLiteral("Deutsch")},
        {QStringLiteral("es"), QStringLiteral("Español")},
        {QStringLiteral("pt"), QStringLiteral("Português")},
        {QStringLiteral("nl"), QStringLiteral("Nederlands")},
        {QStringLiteral("pl"), QStringLiteral("Polski")},
        {QStringLiteral("ru"), QStringLiteral("Русский")},
        {QStringLiteral("tr"), QStringLiteral("Türkçe")},
        {QStringLiteral("ar"), QStringLiteral("العربية")},
        {QStringLiteral("zh"), QStringLiteral("中文")},
        {QStringLiteral("ja"), QStringLiteral("日本語")},
        {QStringLiteral("ko"), QStringLiteral("한국어")},
    };
}

QString languageName(const QString &code)
{
    for (const Language &language : languages()) {
        if (language.code == code) {
            return language.name;
        }
    }
    return code;
}

QString primaryTag(const QString &declared)
{
    static const QRegularExpression pattern(QStringLiteral("^([a-zA-Z]{2,3})(?:[-_].*)?$"));
    const auto match = pattern.match(declared.trimmed());
    return match.hasMatch() ? match.captured(1).toLower() : QString();
}

QString detectLanguage(const QString &text)
{
    int letters = 0;
    int kana = 0, hangul = 0, han = 0, cyrillic = 0, arabic = 0;
    for (const QChar c : text) {
        if (!c.isLetter()) {
            continue;
        }
        ++letters;
        const ushort u = c.unicode();
        if ((u >= 0x3040 && u <= 0x30FF)) {
            ++kana;
        } else if (u >= 0xAC00 && u <= 0xD7AF) {
            ++hangul;
        } else if (u >= 0x4E00 && u <= 0x9FFF) {
            ++han;
        } else if (u >= 0x0400 && u <= 0x04FF) {
            ++cyrillic;
        } else if (u >= 0x0600 && u <= 0x06FF) {
            ++arabic;
        }
    }
    if (letters < 20) {
        return {};
    }
    const auto share = [letters](int n) { return n * 100 / letters; };
    if (share(kana) >= 10) {
        return QStringLiteral("ja");
    }
    if (share(hangul) >= 30) {
        return QStringLiteral("ko");
    }
    if (share(han) >= 30) {
        return QStringLiteral("zh");
    }
    if (share(cyrillic) >= 30) {
        return QStringLiteral("ru");
    }
    if (share(arabic) >= 30) {
        return QStringLiteral("ar");
    }

    static const QRegularExpression splitter(QStringLiteral("[^\\p{L}']+"));
    const QStringList words = text.toLower().split(splitter, Qt::SkipEmptyParts);
    if (words.size() < 8) {
        return {};
    }
    QHash<QString, int> counts;
    for (const QString &word : words) {
        counts[word]++;
    }
    QString best;
    int bestScore = 0;
    int secondScore = 0;
    for (auto it = stopWords().cbegin(); it != stopWords().cend(); ++it) {
        int score = 0;
        for (const QString &stop : it.value()) {
            score += counts.value(stop);
        }
        if (score > bestScore) {
            secondScore = bestScore;
            bestScore = score;
            best = it.key();
        } else if (score > secondScore) {
            secondScore = score;
        }
    }
    // Require a clear winner (short pages and mixed pages stay undecided).
    return bestScore >= 3 && bestScore * 10 >= secondScore * 13 ? best : QString();
}

QList<QPair<int, int>> batches(const QStringList &texts, int maxChars, int maxItems)
{
    QList<QPair<int, int>> result;
    int start = 0;
    int chars = 0;
    for (int i = 0; i < texts.size(); ++i) {
        const int length = texts.at(i).size();
        if (i > start && (chars + length > maxChars || i - start >= maxItems)) {
            result.append({start, i});
            start = i;
            chars = 0;
        }
        chars += length;
    }
    if (start < texts.size()) {
        result.append({start, int(texts.size())});
    }
    return result;
}

bool parseTranslations(const QString &reply, int expected, QStringList *out)
{
    const int first = reply.indexOf(QLatin1Char('['));
    const int last = reply.lastIndexOf(QLatin1Char(']'));
    if (first < 0 || last <= first) {
        return false;
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(reply.mid(first, last - first + 1).toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        return false;
    }
    const QJsonArray array = doc.array();
    if (array.size() != expected) {
        return false;
    }
    QStringList result;
    for (const QJsonValue &value : array) {
        if (!value.isString()) {
            return false;
        }
        result.append(value.toString());
    }
    if (out) {
        *out = result;
    }
    return true;
}

QString targetLanguage(const QJsonObject &section, const QString &fallback)
{
    const QString code = section.value(QStringLiteral("target")).toString();
    return code.isEmpty() ? fallback : code;
}

QJsonObject withTargetLanguage(QJsonObject section, const QString &code)
{
    section.insert(QStringLiteral("target"), code);
    return section;
}

static QString siteKey(const QString &host)
{
    QString key = host.toLower();
    if (key.startsWith(QLatin1String("www."))) {
        key.remove(0, 4);
    }
    return key;
}

QString siteRule(const QJsonObject &section, const QString &host)
{
    const QString value = section.value(QStringLiteral("sites")).toObject()
                              .value(siteKey(host)).toString();
    if (value == QLatin1String("never")) {
        return QStringLiteral("never");
    }
    return value.startsWith(QLatin1String("always:")) ? QStringLiteral("always") : QString();
}

QJsonObject withSiteRule(QJsonObject section, const QString &host, const QString &rule)
{
    QJsonObject sites = section.value(QStringLiteral("sites")).toObject();
    const QString key = siteKey(host);
    if (key.isEmpty()) {
        return section;
    }
    if (rule == QLatin1String("never")) {
        sites.insert(key, QStringLiteral("never"));
    } else if (rule.startsWith(QLatin1String("always:")) && !primaryTag(rule.mid(7)).isEmpty()) {
        sites.insert(key, rule);
    } else {
        sites.remove(key);
    }
    section.insert(QStringLiteral("sites"), sites);
    return section;
}

QString siteTarget(const QJsonObject &section, const QString &host)
{
    const QString value = section.value(QStringLiteral("sites")).toObject()
                              .value(siteKey(host)).toString();
    return value.startsWith(QLatin1String("always:")) ? value.mid(7) : QString();
}

} // namespace Translator
