#include "core/Translator.h"

#include <QtTest>

using namespace Translator;

class TestTranslator : public QObject
{
    Q_OBJECT
private slots:
    void detectsLanguages_data();
    void detectsLanguages();
    void undecidedForShortOrAmbiguousText();
    void primaryTags();
    void batching();
    void parsesModelReplies();
    void siteRules();
};

void TestTranslator::detectsLanguages_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("language");
    QTest::newRow("en") << QStringLiteral("The quick brown fox is not here and that is the problem with this page for you")
                        << QStringLiteral("en");
    QTest::newRow("it") << QStringLiteral("Il gatto non è sul tavolo e la casa della nonna è più bella come sempre con una finestra")
                        << QStringLiteral("it");
    QTest::newRow("fr") << QStringLiteral("Le chat est dans la maison et les enfants ne sont pas là pour le dîner avec vous")
                        << QStringLiteral("fr");
    QTest::newRow("de") << QStringLiteral("Der Hund ist nicht im Haus und das Kind spielt mit dem Ball für die ganze Familie")
                        << QStringLiteral("de");
    QTest::newRow("es") << QStringLiteral("El gato que vive en la casa de los abuelos es muy grande y se come todo el pescado")
                        << QStringLiteral("es");
    QTest::newRow("ja") << QStringLiteral("これは日本語のテキストです。ひらがなとカタカナと漢字が含まれています。")
                        << QStringLiteral("ja");
    QTest::newRow("ru") << QStringLiteral("Это очень длинный русский текст для проверки определения языка страницы")
                        << QStringLiteral("ru");
    QTest::newRow("zh") << QStringLiteral("这是一个用于测试语言检测的中文句子，里面没有日文假名字符。")
                        << QStringLiteral("zh");
}

void TestTranslator::detectsLanguages()
{
    QFETCH(QString, text);
    QFETCH(QString, language);
    QCOMPARE(detectLanguage(text), language);
}

void TestTranslator::undecidedForShortOrAmbiguousText()
{
    QCOMPARE(detectLanguage(QStringLiteral("Hello")), QString());
    QCOMPARE(detectLanguage(QString()), QString());
    QCOMPARE(detectLanguage(QStringLiteral("1234 5678 9012 3456 7890 1234 5678 9012")), QString());
}

void TestTranslator::primaryTags()
{
    QCOMPARE(primaryTag(QStringLiteral("en-GB")), QStringLiteral("en"));
    QCOMPARE(primaryTag(QStringLiteral("PT_br")), QStringLiteral("pt"));
    QCOMPARE(primaryTag(QStringLiteral("x")), QString());
    QCOMPARE(primaryTag(QStringLiteral("<script>")), QString());
    QCOMPARE(languageName(QStringLiteral("it")), QStringLiteral("Italiano"));
}

void TestTranslator::batching()
{
    const QStringList texts{QStringLiteral("aaaa"), QStringLiteral("bbbb"), QStringLiteral("cccc"),
                            QString(50, QLatin1Char('x')), QStringLiteral("d")};
    const auto result = batches(texts, 9);
    QCOMPARE(result.size(), 4);
    QCOMPARE(result.at(0), qMakePair(0, 2));
    QCOMPARE(result.at(1), qMakePair(2, 3));
    QCOMPARE(result.at(2), qMakePair(3, 4)); // oversized item alone
    QCOMPARE(result.at(3), qMakePair(4, 5));
    QVERIFY(batches({}, 10).isEmpty());
    QCOMPARE(batches(QStringList(5, QStringLiteral("a")), 100, 2).size(), 3);
}

void TestTranslator::parsesModelReplies()
{
    QStringList out;
    QVERIFY(parseTranslations(QStringLiteral("[\"uno\", \"due\"]"), 2, &out));
    QCOMPARE(out, (QStringList{QStringLiteral("uno"), QStringLiteral("due")}));
    QVERIFY(parseTranslations(QStringLiteral("Sure!\n```json\n[\"a\"]\n```"), 1, &out));
    QVERIFY(!parseTranslations(QStringLiteral("[\"a\"]"), 2, &out));      // wrong length
    QVERIFY(!parseTranslations(QStringLiteral("[1, 2]"), 2, &out));        // not strings
    QVERIFY(!parseTranslations(QStringLiteral("no array here"), 1, &out));
}

void TestTranslator::siteRules()
{
    QJsonObject prefs;
    QCOMPARE(targetLanguage(prefs, QStringLiteral("it")), QStringLiteral("it"));
    prefs = withTargetLanguage(prefs, QStringLiteral("fr"));
    QCOMPARE(targetLanguage(prefs, QStringLiteral("it")), QStringLiteral("fr"));

    prefs = withSiteRule(prefs, QStringLiteral("WWW.Example.com"), QStringLiteral("always:de"));
    QCOMPARE(siteRule(prefs, QStringLiteral("example.com")), QStringLiteral("always"));
    QCOMPARE(siteTarget(prefs, QStringLiteral("www.example.com")), QStringLiteral("de"));
    QCOMPARE(siteTarget(prefs, QStringLiteral("other.com")), QString());

    prefs = withSiteRule(prefs, QStringLiteral("example.com"), QStringLiteral("never"));
    QCOMPARE(siteRule(prefs, QStringLiteral("example.com")), QStringLiteral("never"));
    QCOMPARE(siteTarget(prefs, QStringLiteral("example.com")), QString());

    prefs = withSiteRule(prefs, QStringLiteral("example.com"), QStringLiteral("always:<bad>"));
    QCOMPARE(siteRule(prefs, QStringLiteral("example.com")), QString()); // invalid language ignored → removed
    prefs = withSiteRule(prefs, QStringLiteral("example.com"), QString());
    QVERIFY(prefs.value(QStringLiteral("sites")).toObject().isEmpty());
}

QTEST_APPLESS_MAIN(TestTranslator)
#include "test_translator.moc"
