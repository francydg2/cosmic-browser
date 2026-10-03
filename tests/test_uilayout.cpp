#include <QtTest>

#include "core/LayoutEditor.h"
#include "core/UILayout.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>

class TestUiLayout : public QObject
{
    Q_OBJECT

private slots:
    void defaultsMatchCurrentUi();
    void roundtrip();
    void validationRejectsBadConfigs();
    void missingFieldsGetSafeDefaults();
    void versionHandling();
    void fileRoundtrip();
    void editorMoveAddRemove();
    void editorProtectsUrlBar();
    void newFieldsRoundtripAndClamp();
    void editorStyleOps();
    void toolbarMetricsMapping();
};

void TestUiLayout::defaultsMatchCurrentUi()
{
    const UIConfiguration cfg = UiLayout::defaultConfiguration();
    QCOMPARE(cfg.schemaVersion, 1);
    QStringList left;
    for (const UiToolbarItem &item : cfg.toolbar.left) {
        left.append(item.component);
    }
    QCOMPARE(left, QStringList({QStringLiteral("nav.back"),
                                QStringLiteral("nav.forward"),
                                QStringLiteral("nav.reload")}));
    QStringList center;
    for (const UiToolbarItem &item : cfg.toolbar.center) {
        center.append(item.component);
    }
    QCOMPARE(center, QStringList({QStringLiteral("user"), QStringLiteral("urlbar")}));
    QStringList right;
    for (const UiToolbarItem &item : cfg.toolbar.right) {
        right.append(item.component);
    }
    QCOMPARE(right, QStringList({QStringLiteral("bookmarks"), QStringLiteral("ai"),
                                 QStringLiteral("ai.eye"), QStringLiteral("ram"),
                                 QStringLiteral("vault"), QStringLiteral("settings")}));
    QCOMPARE(cfg.toolbar.iconSize, 20);
    QCOMPARE(cfg.toolbar.spacing, 6);
    QCOMPARE(cfg.toolbar.height, 56);
    QVERIFY(cfg.toolbar.color.isEmpty());
    QCOMPARE(cfg.urlbar.height, 0);
    QCOMPARE(cfg.urlbar.radius, 0);
    QCOMPARE(cfg.urlbar.maxWidth, 0);
    QVERIFY(cfg.urlbar.color.isEmpty());
    QCOMPARE(cfg.tabs.position, QStringLiteral("horizontal"));
    QCOMPARE(cfg.tabs.tabHeight, 36);
    QCOMPARE(cfg.tabs.tabRadius, 9);
    QCOMPARE(cfg.tabs.tabSpacing, 6);
    QVERIFY(cfg.tabs.showPlus);
    QVERIFY(!cfg.sidebar.visible);
    QString error;
    QVERIFY(UiLayout::validate(cfg, &error));
}

void TestUiLayout::roundtrip()
{
    UIConfiguration cfg = UiLayout::defaultConfiguration();
    cfg.toolbar.iconSize = 28;
    cfg.sidebar.visible = true;
    cfg.sidebar.side = QStringLiteral("right");
    bool ok = false;
    const UIConfiguration back =
        UIConfiguration::fromJson(cfg.toJson(), &ok);
    QVERIFY(ok);
    QCOMPARE(back.toJson(), cfg.toJson());
}

void TestUiLayout::validationRejectsBadConfigs()
{
    UIConfiguration cfg = UiLayout::defaultConfiguration();
    cfg.toolbar.right.append({QStringLiteral("nope.not-real")});
    QString error;
    QVERIFY(!UiLayout::validate(cfg, &error));
    QVERIFY(!error.isEmpty());

    UIConfiguration noUrl = UiLayout::defaultConfiguration();
    noUrl.toolbar.center.clear();
    QVERIFY(!UiLayout::validate(noUrl, nullptr));

    UIConfiguration dupUrl = UiLayout::defaultConfiguration();
    dupUrl.toolbar.left.append({QStringLiteral("urlbar")});
    QVERIFY(!UiLayout::validate(dupUrl, nullptr));

    UIConfiguration future = UiLayout::defaultConfiguration();
    future.schemaVersion = 99;
    QVERIFY(!UiLayout::validate(future, nullptr));

    bool ok = true;
    UIConfiguration::fromJson(QJsonObject(), &ok);
    QVERIFY(!ok);
}

void TestUiLayout::missingFieldsGetSafeDefaults()
{
    QJsonObject root;
    root.insert(QStringLiteral("toolbar"), QJsonObject{
        {QStringLiteral("left"), QJsonArray()},
        {QStringLiteral("center"),
         QJsonArray{QJsonObject{{QStringLiteral("component"), QStringLiteral("urlbar")}}}},
        {QStringLiteral("right"), QJsonArray()},
    });
    root.insert(QStringLiteral("tabs"), QJsonObject{});
    root.insert(QStringLiteral("sidebar"), QJsonObject{});
    bool ok = false;
    const UIConfiguration cfg = UIConfiguration::fromJson(root, &ok);
    QVERIFY(ok);
    QCOMPARE(cfg.toolbar.iconSize, 20);
    QCOMPARE(cfg.toolbar.spacing, 6);
    QCOMPARE(cfg.tabs.position, QStringLiteral("horizontal"));
    QCOMPARE(cfg.sidebar.width, 220);
    QVERIFY(!cfg.sidebar.visible);

    QJsonObject badTypes = root;
    QJsonObject toolbarObj = badTypes.value(QStringLiteral("toolbar")).toObject();
    toolbarObj.insert(QStringLiteral("iconSize"), QStringLiteral("huge"));
    toolbarObj.insert(QStringLiteral("spacing"), -50);
    badTypes.insert(QStringLiteral("toolbar"), toolbarObj);
    const UIConfiguration clamped = UIConfiguration::fromJson(badTypes, &ok);
    QVERIFY(ok);
    QCOMPARE(clamped.toolbar.iconSize, 20);
    QCOMPARE(clamped.toolbar.spacing, 0);
}

void TestUiLayout::versionHandling()
{
    UIConfiguration cfg = UiLayout::defaultConfiguration();
    QJsonObject json = cfg.toJson();
    json.remove(QStringLiteral("schemaVersion"));
    bool ok = false;
    QVERIFY(UIConfiguration::fromJson(json, &ok).schemaVersion == 1);
    QVERIFY(ok);

    json = cfg.toJson();
    json.insert(QStringLiteral("schemaVersion"), 2);
    UIConfiguration::fromJson(json, &ok);
    QVERIFY(!ok);
}

void TestUiLayout::fileRoundtrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("ui.json"));

    UIConfiguration missing;
    QVERIFY(!UiLayout::loadFile(path, &missing));

    UIConfiguration cfg = UiLayout::defaultConfiguration();
    cfg.toolbar.spacing = 10;
    QVERIFY(UiLayout::saveFile(cfg, path));
    UIConfiguration loaded;
    QVERIFY(UiLayout::loadFile(path, &loaded));
    QCOMPARE(loaded.toJson(), cfg.toJson());

    UIConfiguration invalid = UiLayout::defaultConfiguration();
    invalid.toolbar.center.clear();
    QVERIFY(!UiLayout::saveFile(invalid, path));
    UIConfiguration still;
    QVERIFY(UiLayout::loadFile(path, &still));
    QCOMPARE(still.toolbar.spacing, 10);

    QFile corrupt(path);
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    corrupt.write("{not json");
    corrupt.close();
    UIConfiguration fallback;
    QVERIFY(!UiLayout::loadFile(path, &fallback));
}

void TestUiLayout::editorMoveAddRemove()
{
    LayoutEditor editor;
    QSignalSpy changed(&editor, &LayoutEditor::changed);
    QSignalSpy errors(&editor, &LayoutEditor::error);
    QVERIFY(!editor.isModified());

    QVERIFY(editor.moveItem(2, 5, 0, 0));
    QCOMPARE(editor.configuration().toolbar.left.first().component,
             QStringLiteral("settings"));
    QVERIFY(editor.isModified());
    QCOMPARE(changed.count(), 1);

    QVERIFY(editor.addItem(1, -1, QStringLiteral("ram")));
    QVERIFY(!editor.addItem(0, 0, QStringLiteral("nope.not-real")));
    QCOMPARE(errors.count(), 1);

    QStringList center;
    for (const UiToolbarItem &item : editor.configuration().toolbar.center) {
        center.append(item.component);
    }
    QVERIFY(center.contains(QStringLiteral("ram")));

    const int rightCount = editor.configuration().toolbar.right.size();
    QVERIFY(editor.removeItem(2, 0));
    QCOMPARE(editor.configuration().toolbar.right.size(), rightCount - 1);
    QVERIFY(!editor.removeItem(2, 99));
}

void TestUiLayout::newFieldsRoundtripAndClamp()
{
    UIConfiguration cfg = UiLayout::defaultConfiguration();
    cfg.toolbar.height = 60;
    cfg.toolbar.color = QStringLiteral("#123456");
    cfg.urlbar.height = 40;
    cfg.urlbar.radius = 12;
    cfg.urlbar.maxWidth = 600;
    cfg.urlbar.color = QStringLiteral("#abcdef");
    cfg.tabs.tabHeight = 44;
    cfg.tabs.tabRadius = 4;
    cfg.tabs.tabSpacing = 2;
    cfg.tabs.showPlus = false;
    bool ok = false;
    const UIConfiguration back = UIConfiguration::fromJson(cfg.toJson(), &ok);
    QVERIFY(ok);
    QCOMPARE(back.toJson(), cfg.toJson());

    QJsonObject json = cfg.toJson();
    QJsonObject toolbarObj = json.value(QStringLiteral("toolbar")).toObject();
    toolbarObj.insert(QStringLiteral("height"), 500);
    toolbarObj.insert(QStringLiteral("color"), QStringLiteral("red"));
    json.insert(QStringLiteral("toolbar"), toolbarObj);
    QJsonObject tabsObj = json.value(QStringLiteral("tabs")).toObject();
    tabsObj.insert(QStringLiteral("tabHeight"), -5);
    tabsObj.insert(QStringLiteral("showPlus"), QStringLiteral("yes"));
    json.insert(QStringLiteral("tabs"), tabsObj);
    const UIConfiguration clamped = UIConfiguration::fromJson(json, &ok);
    QVERIFY(ok);
    QCOMPARE(clamped.toolbar.height, 72);
    QVERIFY(clamped.toolbar.color.isEmpty());
    QCOMPARE(clamped.tabs.tabHeight, 32);
    QVERIFY(clamped.tabs.showPlus);

    UIConfiguration badColor = UiLayout::defaultConfiguration();
    badColor.toolbar.color = QStringLiteral("#zzzzzz");
    QString error;
    QVERIFY(!UiLayout::validate(badColor, &error));
    QVERIFY(UiLayout::isValidColor(QStringLiteral("#1a2b3c")));
    QVERIFY(!UiLayout::isValidColor(QStringLiteral("#xyz")));
    QVERIFY(UiLayout::isValidColor(QString()));
}

void TestUiLayout::editorStyleOps()
{    LayoutEditor editor;
    QSignalSpy changed(&editor, &LayoutEditor::changed);
    QSignalSpy errors(&editor, &LayoutEditor::error);

    QVERIFY(editor.setToolbarHeight(60));
    QCOMPARE(editor.configuration().toolbar.height, 60);
    QVERIFY(editor.setToolbarHeight(500));
    QCOMPARE(editor.configuration().toolbar.height, 72);
    QVERIFY(editor.setToolbarColor(QStringLiteral("#112233")));
    QVERIFY(!editor.setToolbarColor(QStringLiteral("nope")));
    QCOMPARE(errors.count(), 1);

    QVERIFY(editor.setUrlHeight(40));
    QVERIFY(editor.setUrlRadius(10));
    QVERIFY(editor.setUrlMaxWidth(500));
    QVERIFY(editor.setUrlColor(QStringLiteral("#445566")));
    QCOMPARE(editor.configuration().urlbar.radius, 10);

    QVERIFY(editor.setTabPosition(QStringLiteral("vertical")));
    QVERIFY(!editor.setTabPosition(QStringLiteral("diagonal")));
    QVERIFY(editor.setTabHeight(44));
    QVERIFY(editor.setTabRadius(0));
    QVERIFY(editor.setTabSpacing(12));
    QVERIFY(editor.setShowPlus(false));
    QVERIFY(!editor.configuration().tabs.showPlus);
    QVERIFY(changed.count() > 0);
    QString error;
    QVERIFY(UiLayout::validate(editor.configuration(), &error));
}

void TestUiLayout::editorProtectsUrlBar()
{
    LayoutEditor editor;
    QSignalSpy errors(&editor, &LayoutEditor::error);

    QVERIFY(!editor.addItem(0, 0, QStringLiteral("urlbar")));
    QVERIFY(!editor.removeItem(1, 1));
    QCOMPARE(errors.count(), 2);

    QVERIFY(editor.moveItem(1, 1, 2, 0));
    QString error;
    QVERIFY(UiLayout::validate(editor.configuration(), &error));
    QCOMPARE(editor.configuration().toolbar.right.first().component,
             QStringLiteral("urlbar"));
}

void TestUiLayout::toolbarMetricsMapping()
{
    UiToolbarLayout toolbar;
    toolbar.height = 44;
    toolbar.iconSize = 20;
    UiLayout::UiToolbarMetrics metrics = UiLayout::toolbarMetrics(toolbar);
    QCOMPARE(metrics.height, 44);
    QCOMPARE(metrics.iconSize, 20);
    QCOMPARE(metrics.urlHeight, 28);

    toolbar.height = 10;
    metrics = UiLayout::toolbarMetrics(toolbar);
    QCOMPARE(metrics.height, 40);

    toolbar.height = 72;
    toolbar.iconSize = 32;
    metrics = UiLayout::toolbarMetrics(toolbar);
    QCOMPARE(metrics.height, 72);
    QCOMPARE(metrics.iconSize, 32);
    QCOMPARE(metrics.urlHeight, 56);

    toolbar.height = 44;
    toolbar.iconSize = 32;
    metrics = UiLayout::toolbarMetrics(toolbar);
    QCOMPARE(metrics.iconSize, 24);
}

QTEST_GUILESS_MAIN(TestUiLayout)
#include "test_uilayout.moc"
