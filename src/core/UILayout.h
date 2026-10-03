#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

struct UiToolbarItem {
    QString component;
};

struct UiToolbarLayout {
    QList<UiToolbarItem> left;
    QList<UiToolbarItem> center;
    QList<UiToolbarItem> right;
    int iconSize = 20;
    int spacing = 6;
    int height = 56;
    QString color;
};

struct UiUrlBarLayout {
    int height = 0;
    int radius = 0;
    int maxWidth = 0;
    QString color;
};

struct UiTabsLayout {
    QString position;
    int tabHeight = 36;
    int tabRadius = 9;
    int tabSpacing = 6;
    bool showPlus = true;
};

struct UiSidebarLayout {
    QString side;
    int width = 220;
    bool visible = false;
};

struct UIConfiguration {
    int schemaVersion = 1;
    UiToolbarLayout toolbar;
    UiUrlBarLayout urlbar;
    UiTabsLayout tabs;
    UiSidebarLayout sidebar;

    QJsonObject toJson() const;
    static UIConfiguration fromJson(const QJsonObject &obj, bool *ok = nullptr);
};

namespace UiLayout
{
constexpr int kSchemaVersion = 1;

struct UiToolbarMetrics {
    int height = 44;
    int iconSize = 20;
    int urlHeight = 28;
};

UiToolbarMetrics toolbarMetrics(const UiToolbarLayout &toolbar);

UIConfiguration defaultConfiguration();
bool isKnownComponent(const QString &id);
bool isUniqueComponent(const QString &id);
bool isValidColor(const QString &color);
bool validate(const UIConfiguration &cfg, QString *error = nullptr);
QString configFilePath();
bool saveFile(const UIConfiguration &cfg, const QString &path);
bool loadFile(const QString &path, UIConfiguration *out);
} // namespace UiLayout
