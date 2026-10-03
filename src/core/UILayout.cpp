#include "core/UILayout.h"
#include "core/Logging.h"
#include "core/Paths.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

const auto kKeyVersion = "schemaVersion";
const auto kKeyToolbar = "toolbar";
const auto kKeyTabs = "tabs";
const auto kKeySidebar = "sidebar";
const auto kKeyLeft = "left";
const auto kKeyCenter = "center";
const auto kKeyRight = "right";
const auto kKeyComponent = "component";
const auto kKeyIconSize = "iconSize";
const auto kKeySpacing = "spacing";
const auto kKeyPosition = "position";
const auto kKeySide = "side";
const auto kKeyWidth = "width";
const auto kKeyVisible = "visible";
const auto kFileName = "ui.json";

const auto kIdBack = "nav.back";
const auto kIdForward = "nav.forward";
const auto kIdReload = "nav.reload";
const auto kIdContainer = "container";
const auto kIdUser = "user";
const auto kIdUrlBar = "urlbar";const auto kIdBookmarks = "bookmarks";
const auto kIdAi = "ai";
const auto kIdAiEye = "ai.eye";
const auto kIdRam = "ram";
const auto kIdVault = "vault";
const auto kIdSettings = "settings";

int clampedInt(const QJsonObject &obj, const char *key, int fallback, int lo,
               int hi)
{
    const QJsonValue value = obj.value(QString::fromLatin1(key));
    if (!value.isDouble()) {
        return fallback;
    }
    return qBound(lo, static_cast<int>(value.toDouble()), hi);
}

bool validColor(const QString &color)
{
    return UiLayout::isValidColor(color);
}

QString clampedColor(const QJsonObject &obj, const char *key)
{
    const QString value = obj.value(QString::fromLatin1(key)).toString();
    return validColor(value) ? value : QString();
}

QString clampedString(const QJsonObject &obj, const char *key,
                      const QString &fallback, const QStringList &allowed)
{
    const QString value = obj.value(QString::fromLatin1(key)).toString();
    return allowed.contains(value) ? value : fallback;
}

QList<UiToolbarItem> parseItems(const QJsonArray &array, bool *ok)
{
    QList<UiToolbarItem> items;
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            *ok = false;
            return {};
        }
        const QString id =
            value.toObject().value(QString::fromLatin1(kKeyComponent)).toString();
        if (!UiLayout::isKnownComponent(id)) {
            *ok = false;
            return {};
        }
        items.append({id});
    }
    return items;
}

QJsonArray dumpItems(const QList<UiToolbarItem> &items)
{
    QJsonArray array;
    for (const UiToolbarItem &item : items) {
        QJsonObject obj;
        obj.insert(QString::fromLatin1(kKeyComponent), item.component);
        array.append(obj);
    }
    return array;
}

int countComponent(const UIConfiguration &cfg, const QString &id)
{
    int count = 0;
    const QList<QList<UiToolbarItem>> zones = {cfg.toolbar.left, cfg.toolbar.center,
                                              cfg.toolbar.right};
    for (const QList<UiToolbarItem> &zone : zones) {
        for (const UiToolbarItem &item : zone) {
            if (item.component == id) {
                ++count;
            }
        }
    }
    return count;
}

} // namespace

bool UiLayout::isValidColor(const QString &color){
    if (color.isEmpty()) {
        return true;
    }
    if (color.size() != 7 || !color.startsWith(QLatin1Char('#'))) {
        return false;
    }
    for (int i = 1; i < 7; ++i) {
        const char c = color.at(i).toLatin1();
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
            || (c >= 'A' && c <= 'F');
        if (!hex) {
            return false;
        }
    }
    return true;
}

UiLayout::UiToolbarMetrics UiLayout::toolbarMetrics(const UiToolbarLayout &toolbar)
{
    UiToolbarMetrics metrics;
    metrics.height = qBound(40, toolbar.height, 72);
    metrics.iconSize = qBound(16, qMin(toolbar.iconSize, metrics.height - 20), 32);
    metrics.urlHeight = metrics.height - 16;
    return metrics;
}

UIConfiguration UiLayout::defaultConfiguration()
{
    UIConfiguration cfg;
    cfg.schemaVersion = kSchemaVersion;
    cfg.toolbar.left = {{QString::fromLatin1(kIdBack)},
                        {QString::fromLatin1(kIdForward)},
                        {QString::fromLatin1(kIdReload)}};
    cfg.toolbar.center = {{QString::fromLatin1(kIdUser)},
                          {QString::fromLatin1(kIdUrlBar)}};
    cfg.toolbar.right = {{QString::fromLatin1(kIdBookmarks)},
                         {QString::fromLatin1(kIdAi)},
                         {QString::fromLatin1(kIdAiEye)},
                         {QString::fromLatin1(kIdRam)},
                         {QString::fromLatin1(kIdVault)},
                         {QString::fromLatin1(kIdSettings)}};
    cfg.toolbar.iconSize = 20;
    cfg.toolbar.spacing = 6;
    cfg.toolbar.height = 56;
    cfg.toolbar.color = QString();
    cfg.urlbar.height = 0;
    cfg.urlbar.radius = 0;
    cfg.urlbar.maxWidth = 0;
    cfg.urlbar.color = QString();
    cfg.tabs.position = QStringLiteral("horizontal");
    cfg.tabs.tabHeight = 36;
    cfg.tabs.tabRadius = 9;
    cfg.tabs.tabSpacing = 6;
    cfg.tabs.showPlus = true;
    cfg.sidebar.side = QStringLiteral("left");
    cfg.sidebar.width = 220;
    cfg.sidebar.visible = false;
    return cfg;
}

bool UiLayout::isKnownComponent(const QString &id)
{
    static const QStringList known = {
        QString::fromLatin1(kIdBack),   QString::fromLatin1(kIdForward),
        QString::fromLatin1(kIdReload), QString::fromLatin1(kIdContainer),
        QString::fromLatin1(kIdUser),   QString::fromLatin1(kIdUrlBar),
        QString::fromLatin1(kIdBookmarks), QString::fromLatin1(kIdAi),
        QString::fromLatin1(kIdAiEye),  QString::fromLatin1(kIdRam),
        QString::fromLatin1(kIdVault),  QString::fromLatin1(kIdSettings)};
    return known.contains(id);
}

bool UiLayout::isUniqueComponent(const QString &id)
{
    return id == QLatin1String(kIdUrlBar);
}

bool UiLayout::validate(const UIConfiguration &cfg, QString *error)
{
    if (cfg.schemaVersion != kSchemaVersion) {
        if (error) {
            *error = QStringLiteral("unsupported schemaVersion");
        }
        return false;
    }
    if (countComponent(cfg, QString::fromLatin1(kIdUrlBar)) != 1) {
        if (error) {
            *error = QStringLiteral("the URL bar must appear exactly once");
        }
        return false;
    }
    const QList<QList<UiToolbarItem>> zones = {cfg.toolbar.left, cfg.toolbar.center,
                                              cfg.toolbar.right};
    for (const QList<UiToolbarItem> &zone : zones) {
        for (const UiToolbarItem &item : zone) {
            if (!isKnownComponent(item.component)) {
                if (error) {
                    *error = QStringLiteral("unknown component: ") + item.component;
                }
                return false;
            }
        }
    }
    if (!validColor(cfg.toolbar.color) || !validColor(cfg.urlbar.color)) {
        if (error) {
            *error = QStringLiteral("invalid color value");
        }
        return false;
    }
    return true;
}

QString UiLayout::configFilePath()
{
    return Paths::configDir() + QLatin1Char('/') + QString::fromLatin1(kFileName);
}

bool UiLayout::saveFile(const UIConfiguration &cfg, const QString &path)
{
    QString error;
    if (!validate(cfg, &error)) {
        qCWarning(lcCosmic).noquote() << "refusing to save invalid UI config:" << error;
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcCosmic) << "cannot write UI config file" << path;
        return false;
    }
    file.write(QJsonDocument(cfg.toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qCWarning(lcCosmic) << "failed to commit UI config file" << path;
        return false;
    }
    qCInfo(lcCosmic).noquote() << "UI config saved to" << path;
    return true;
}

bool UiLayout::loadFile(const QString &path, UIConfiguration *out)
{
    QFile file(path);
    if (!file.exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcCosmic) << "cannot read UI config file" << path;
        return false;
    }
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcCosmic).noquote()
            << "UI config file corrupt (" << error.errorString() << "); ignoring";
        return false;
    }
    bool ok = false;
    const UIConfiguration cfg = UIConfiguration::fromJson(doc.object(), &ok);
    if (!ok) {
        qCWarning(lcCosmic) << "UI config file invalid; ignoring";
        return false;
    }
    if (out) {
        *out = cfg;
    }
    return true;
}

QJsonObject UIConfiguration::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), schemaVersion);
    QJsonObject toolbarObj;
    toolbarObj.insert(QStringLiteral("left"), dumpItems(toolbar.left));
    toolbarObj.insert(QStringLiteral("center"), dumpItems(toolbar.center));
    toolbarObj.insert(QStringLiteral("right"), dumpItems(toolbar.right));
    toolbarObj.insert(QStringLiteral("iconSize"), toolbar.iconSize);
    toolbarObj.insert(QStringLiteral("spacing"), toolbar.spacing);
    toolbarObj.insert(QStringLiteral("height"), toolbar.height);
    toolbarObj.insert(QStringLiteral("color"), toolbar.color);
    root.insert(QStringLiteral("toolbar"), toolbarObj);
    QJsonObject urlbarObj;
    urlbarObj.insert(QStringLiteral("height"), urlbar.height);
    urlbarObj.insert(QStringLiteral("radius"), urlbar.radius);
    urlbarObj.insert(QStringLiteral("maxWidth"), urlbar.maxWidth);
    urlbarObj.insert(QStringLiteral("color"), urlbar.color);
    root.insert(QStringLiteral("urlbar"), urlbarObj);
    QJsonObject tabsObj;
    tabsObj.insert(QStringLiteral("position"), tabs.position);
    tabsObj.insert(QStringLiteral("tabHeight"), tabs.tabHeight);
    tabsObj.insert(QStringLiteral("tabRadius"), tabs.tabRadius);
    tabsObj.insert(QStringLiteral("tabSpacing"), tabs.tabSpacing);
    tabsObj.insert(QStringLiteral("showPlus"), tabs.showPlus);
    root.insert(QStringLiteral("tabs"), tabsObj);
    QJsonObject sidebarObj;
    sidebarObj.insert(QStringLiteral("side"), sidebar.side);
    sidebarObj.insert(QStringLiteral("width"), sidebar.width);
    sidebarObj.insert(QStringLiteral("visible"), sidebar.visible);
    root.insert(QStringLiteral("sidebar"), sidebarObj);
    return root;
}

UIConfiguration UIConfiguration::fromJson(const QJsonObject &obj, bool *ok)
{
    UIConfiguration cfg = UiLayout::defaultConfiguration();
    bool valid = true;

    const int version = obj.value(QString::fromLatin1(kKeyVersion)).toInt(-1);
    if (version == -1) {
        cfg.schemaVersion = UiLayout::kSchemaVersion;
    } else if (version != UiLayout::kSchemaVersion) {
        valid = false;
    }
    if (!obj.value(QString::fromLatin1(kKeyToolbar)).isObject()
        || !obj.value(QString::fromLatin1(kKeyTabs)).isObject()
        || !obj.value(QString::fromLatin1(kKeySidebar)).isObject()) {
        valid = false;
    }
    QJsonObject urlbarObj;
    if (valid) {
        urlbarObj = obj.value(QStringLiteral("urlbar")).toObject();
    }

    if (valid) {
        const QJsonObject toolbarObj =
            obj.value(QString::fromLatin1(kKeyToolbar)).toObject();
        const QJsonValue left = toolbarObj.value(QString::fromLatin1(kKeyLeft));
        const QJsonValue center = toolbarObj.value(QString::fromLatin1(kKeyCenter));
        const QJsonValue right = toolbarObj.value(QString::fromLatin1(kKeyRight));
        if (!left.isArray() || !center.isArray() || !right.isArray()) {
            valid = false;
        } else {
            cfg.toolbar.left = parseItems(left.toArray(), &valid);
            cfg.toolbar.center = parseItems(center.toArray(), &valid);
            cfg.toolbar.right = parseItems(right.toArray(), &valid);
            cfg.toolbar.iconSize = clampedInt(toolbarObj, kKeyIconSize, 20, 16, 32);
            cfg.toolbar.spacing = clampedInt(toolbarObj, kKeySpacing, 6, 0, 16);
            cfg.toolbar.height = clampedInt(toolbarObj, "height", 44, 40, 72);
            cfg.toolbar.color = clampedColor(toolbarObj, "color");
        }
    }
    if (valid) {
        cfg.urlbar.height = clampedInt(urlbarObj, "height", 0, 0, 56);
        cfg.urlbar.radius = clampedInt(urlbarObj, "radius", 0, 0, 24);
        cfg.urlbar.maxWidth = clampedInt(urlbarObj, "maxWidth", 0, 0, 2000);
        cfg.urlbar.color = clampedColor(urlbarObj, "color");
        const QJsonObject tabsObj = obj.value(QString::fromLatin1(kKeyTabs)).toObject();
        cfg.tabs.position = clampedString(tabsObj, kKeyPosition,
                                          QStringLiteral("horizontal"),
                                          {QStringLiteral("horizontal"),
                                           QStringLiteral("vertical")});
        cfg.tabs.tabHeight = clampedInt(tabsObj, "tabHeight", 36, 32, 48);
        cfg.tabs.tabRadius = clampedInt(tabsObj, "tabRadius", 9, 0, 16);
        cfg.tabs.tabSpacing = clampedInt(tabsObj, "tabSpacing", 6, 0, 12);
        const QJsonValue showPlus = tabsObj.value(QStringLiteral("showPlus"));
        cfg.tabs.showPlus = showPlus.isBool() ? showPlus.toBool() : true;
        const QJsonObject sidebarObj =
            obj.value(QString::fromLatin1(kKeySidebar)).toObject();
        cfg.sidebar.side = clampedString(sidebarObj, kKeySide, QStringLiteral("left"),
                                         {QStringLiteral("left"),
                                          QStringLiteral("right")});
        cfg.sidebar.width = clampedInt(sidebarObj, kKeyWidth, 220, 120, 400);
        const QJsonValue visible = sidebarObj.value(QString::fromLatin1(kKeyVisible));
        cfg.sidebar.visible = visible.isBool() ? visible.toBool() : false;
    }
    if (valid) {
        QString error;
        valid = UiLayout::validate(cfg, &error);
        if (!valid) {
            qCWarning(lcCosmic).noquote() << "UI config failed validation:" << error;
        }
    }
    if (ok) {
        *ok = valid;
    }
    if (!valid) {
        return UiLayout::defaultConfiguration();
    }
    return cfg;
}
