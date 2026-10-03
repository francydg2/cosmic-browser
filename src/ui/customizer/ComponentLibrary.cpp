#include "ui/customizer/ComponentLibrary.h"

#include <QApplication>
#include <QDrag>
#include <QIcon>
#include <QMimeData>
#include <QMouseEvent>

namespace {

const auto kMimeComponent = "application/x-cosmic-component";

QList<ComponentInfo> buildComponents()
{
    return {
        {QStringLiteral("nav.back"), ComponentLibrary::tr("Back"),
         ComponentLibrary::tr("Go to the previous page"),
         QStringLiteral(":/icons/back.svg"), QStringLiteral("Navigation"), true, true,
         false},
        {QStringLiteral("nav.forward"), ComponentLibrary::tr("Forward"),
         ComponentLibrary::tr("Go to the next page"),
         QStringLiteral(":/icons/forward.svg"), QStringLiteral("Navigation"), true, true,
         false},
        {QStringLiteral("nav.reload"), ComponentLibrary::tr("Reload"),
         ComponentLibrary::tr("Reload the page (Stop while loading)"),
         QStringLiteral(":/icons/reload.svg"), QStringLiteral("Navigation"), true, true,
         false},
        {QStringLiteral("urlbar"), ComponentLibrary::tr("URL Bar"),
         ComponentLibrary::tr("Address and search field. Must appear exactly once."),
         QString(), QStringLiteral("Navigation"), true, true, true},
        {QStringLiteral("bookmarks"), ComponentLibrary::tr("Bookmarks"),
         ComponentLibrary::tr("Menu with the bookmarked sites"),
         QStringLiteral(":/icons/star.svg"), QStringLiteral("Navigation"), true, true,
         false},
        {QString(), ComponentLibrary::tr("Stop"), QString(), QString(),
         QStringLiteral("Navigation"), false, false, false},
        {QString(), ComponentLibrary::tr("Home"), QString(), QString(),
         QStringLiteral("Navigation"), false, false, false},
        {QString(), ComponentLibrary::tr("Search"), QString(), QString(),
         QStringLiteral("Navigation"), false, false, false},
        {QString(), ComponentLibrary::tr("Downloads"), QString(), QString(),
         QStringLiteral("Navigation"), false, false, false},
        {QString(), ComponentLibrary::tr("History"), QString(), QString(),
         QStringLiteral("Navigation"), false, false, false},
        {QStringLiteral("tabs"), ComponentLibrary::tr("Tab Bar"),
         ComponentLibrary::tr("The tab strip. Position and options are configured "
                               "in a dedicated step."),
         QString(), QStringLiteral("Tabs"), true, false, true},
        {QString(), ComponentLibrary::tr("New Tab button"), QString(), QString(),
         QStringLiteral("Tabs"), false, false, false},
        {QString(), ComponentLibrary::tr("Tab close button"), QString(), QString(),
         QStringLiteral("Tabs"), false, false, false},
        {QString(), ComponentLibrary::tr("Tab favicon"), QString(), QString(),
         QStringLiteral("Tabs"), false, false, false},
        {QString(), ComponentLibrary::tr("Tab title"), QString(), QString(),
         QStringLiteral("Tabs"), false, false, false},
        {QString(), ComponentLibrary::tr("Tab progress indicator"), QString(), QString(),
         QStringLiteral("Tabs"), false, false, false},
        {QStringLiteral("toolbar"), ComponentLibrary::tr("Toolbar"),
         ComponentLibrary::tr("The main toolbar row."), QString(),
         QStringLiteral("Browser UI"), true, false, true},
        {QStringLiteral("user"), ComponentLibrary::tr("User"),
         ComponentLibrary::tr("Current user with the switch-users menu"),
         QStringLiteral(":/icons/layers.svg"), QStringLiteral("Browser UI"), true, true,
         false},
        {QStringLiteral("settings"), ComponentLibrary::tr("Settings button"),
         ComponentLibrary::tr("Opens the settings tab"),
         QStringLiteral(":/icons/gear.svg"), QStringLiteral("Browser UI"), true, true,
         false},
        {QStringLiteral("ai"), ComponentLibrary::tr("AI button"),
         ComponentLibrary::tr("Menu with the AI assistants"),
         QStringLiteral(":/icons/ai.svg"), QStringLiteral("Browser UI"), true, true,
         false},
        {QStringLiteral("vault"), ComponentLibrary::tr("Password manager"),
         ComponentLibrary::tr("Opens the password manager section"),
         QStringLiteral(":/icons/key.svg"), QStringLiteral("Browser UI"), true, true,
         false},
        {QStringLiteral("ram"), ComponentLibrary::tr("RAM Saver"),
         ComponentLibrary::tr("Menu with the memory saver options"),
         QStringLiteral(":/icons/ram.svg"), QStringLiteral("Browser UI"), true, true,
         false},
        {QStringLiteral("ai.eye"), ComponentLibrary::tr("AI page lens"),
         ComponentLibrary::tr("Lets the AI read the current page"),
         QStringLiteral(":/icons/eye.svg"), QStringLiteral("Browser UI"), true, true,
         false},
        {QString(), ComponentLibrary::tr("Sidebar"), QString(), QString(),
         QStringLiteral("Browser UI"), false, false, false},
        {QString(), ComponentLibrary::tr("Status bar"), QString(), QString(),
         QStringLiteral("Browser UI"), false, false, false},
        {QString(), ComponentLibrary::tr("Menu button"), QString(), QString(),
         QStringLiteral("Browser UI"), false, false, false},
        {QString(), ComponentLibrary::tr("Extensions area"), QString(), QString(),
         QStringLiteral("Browser UI"), false, false, false},
        {QString(), ComponentLibrary::tr("New Tab page"), QString(), QString(),
         QStringLiteral("Content"), false, false, false},
        {QString(), ComponentLibrary::tr("Search box"), QString(), QString(),
         QStringLiteral("Content"), false, false, false},
        {QString(), ComponentLibrary::tr("Quick links"), QString(), QString(),
         QStringLiteral("Content"), false, false, false},
        {QString(), ComponentLibrary::tr("Widgets"), QString(), QString(),
         QStringLiteral("Content"), false, false, false},
        {QString(), ComponentLibrary::tr("Clock"), QString(), QString(),
         QStringLiteral("Content"), false, false, false},
        {QString(), ComponentLibrary::tr("Weather"), QString(), QString(),
         QStringLiteral("Content"), false, false, false},
        {QString(), ComponentLibrary::tr("Notes"), QString(), QString(),
         QStringLiteral("Content"), false, false, false},
        {QString(), ComponentLibrary::tr("AI sidebar"), QString(), QString(),
         QStringLiteral("Future"), false, false, false},
        {QString(), ComponentLibrary::tr("Workspaces"), QString(), QString(),
         QStringLiteral("Future"), false, false, false},
        {QString(), ComponentLibrary::tr("Split view"), QString(), QString(),
         QStringLiteral("Future"), false, false, false},
        {QString(), ComponentLibrary::tr("Extensions"), QString(), QString(),
         QStringLiteral("Future"), false, false, false},
    };
}

} // namespace

ComponentLibrary::ComponentLibrary(QWidget *parent)
    : QTreeWidget(parent)
{
    setObjectName(QStringLiteral("componentLibrary"));
    setHeaderHidden(true);
    setDragEnabled(true);
    setRootIsDecorated(true);

    QStringList order;
    for (const ComponentInfo &info : buildComponents()) {
        if (!order.contains(info.category)) {
            order.append(info.category);
        }
    }
    for (const QString &category : order) {
        auto *group = new QTreeWidgetItem(this, QStringList(category));
        group->setFlags(group->flags() & ~Qt::ItemIsSelectable);
        group->setExpanded(true);
        for (const ComponentInfo &info : buildComponents()) {
            if (info.category != category) {
                continue;
            }
            QString text = info.label;
            if (!info.available) {
                text += tr(" — planned");
            }
            auto *item = new QTreeWidgetItem(group, QStringList(text));
            item->setData(0, Qt::UserRole, info.id);
            if (!info.iconPath.isEmpty()) {
                item->setIcon(0, QIcon(info.iconPath));
            }
            if (!info.available) {
                item->setFlags(item->flags() & ~Qt::ItemIsSelectable
                               & ~Qt::ItemIsDragEnabled);
                item->setToolTip(0, tr("Planned for a future phase."));
            } else if (!info.id.isEmpty()) {
                item->setToolTip(0, info.description);
            } else {
                item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
            }
        }
    }
    connect(this, &QTreeWidget::itemDoubleClicked, this,
            &ComponentLibrary::onItemDoubleClicked);
}

QList<ComponentInfo> ComponentLibrary::components()
{
    return buildComponents();
}

const ComponentInfo *ComponentLibrary::infoFor(const QString &id)
{
    static const QList<ComponentInfo> all = buildComponents();
    for (const ComponentInfo &info : all) {
        if (!info.id.isEmpty() && info.id == id) {
            return &info;
        }
    }
    return nullptr;
}

QString ComponentLibrary::iconFor(const QString &id)
{
    if (const ComponentInfo *info = infoFor(id)) {
        return info->iconPath;
    }
    return {};
}

QString ComponentLibrary::labelFor(const QString &id)
{
    if (const ComponentInfo *info = infoFor(id)) {
        return info->label;
    }
    return id;
}

bool ComponentLibrary::isToolbarItem(const QString &id)
{
    if (const ComponentInfo *info = infoFor(id)) {
        return info->toolbarItem;
    }
    return false;
}

QStringList ComponentLibrary::mimeTypes() const
{
    return {QString::fromLatin1(kMimeComponent)};
}

QMimeData *ComponentLibrary::mimeData(const QList<QTreeWidgetItem *> &items) const
{
    if (items.size() != 1) {
        return nullptr;
    }
    const QString id = items.first()->data(0, Qt::UserRole).toString();
    if (id.isEmpty() || !isToolbarItem(id)) {
        return nullptr;
    }
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(kMimeComponent), id.toUtf8());
    return mime;
}

void ComponentLibrary::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) {
        return;
    }
    const QString id = item->data(0, Qt::UserRole).toString();
    if (!id.isEmpty() && isToolbarItem(id)) {
        emit addRequested(id);
    }
}
