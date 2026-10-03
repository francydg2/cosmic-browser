#include "core/LayoutEditor.h"

#include "core/Logging.h"

LayoutEditor::LayoutEditor(QObject *parent)
    : QObject(parent)
    , m_working(UiLayout::defaultConfiguration())
    , m_base(m_working)
{
}

void LayoutEditor::setConfiguration(const UIConfiguration &cfg)
{
    QString error;
    m_working = UiLayout::validate(cfg, &error) ? cfg
                                                : UiLayout::defaultConfiguration();
    m_base = m_working;
    emit changed();
}

bool LayoutEditor::isModified() const
{
    return m_working.toJson() != m_base.toJson();
}

QString LayoutEditor::zoneName(int zone)
{
    switch (zone) {
    case 0:
        return QStringLiteral("Left");
    case 1:
        return QStringLiteral("Center");
    case 2:
        return QStringLiteral("Right");
    default:
        return {};
    }
}

QList<UiToolbarItem> *LayoutEditor::zoneList(int zone)
{
    switch (zone) {
    case 0:
        return &m_working.toolbar.left;
    case 1:
        return &m_working.toolbar.center;
    case 2:
        return &m_working.toolbar.right;
    default:
        return nullptr;
    }
}

const QList<UiToolbarItem> *LayoutEditor::zoneList(int zone) const
{
    switch (zone) {
    case 0:
        return &m_working.toolbar.left;
    case 1:
        return &m_working.toolbar.center;
    case 2:
        return &m_working.toolbar.right;
    default:
        return nullptr;
    }
}

bool LayoutEditor::moveItem(int fromZone, int fromIndex, int toZone, int toIndex)
{
    QList<UiToolbarItem> *from = zoneList(fromZone);
    QList<UiToolbarItem> *to = zoneList(toZone);
    if (!from || !to || fromIndex < 0 || fromIndex >= from->size()) {
        emit error(tr("Invalid move."));
        return false;
    }
    UiToolbarItem item = from->takeAt(fromIndex);
    int at = qBound(0, toIndex, to->size());
    if (from == to && at > fromIndex) {
        --at;
    }
    to->insert(at, item);
    emit changed();
    return true;
}

bool LayoutEditor::addItem(int zone, int index, const QString &component)
{
    QList<UiToolbarItem> *to = zoneList(zone);
    if (!to) {
        emit error(tr("Invalid zone."));
        return false;
    }
    if (!UiLayout::isKnownComponent(component)) {
        emit error(tr("Unknown component."));
        return false;
    }
    if (UiLayout::isUniqueComponent(component)) {
        int count = 0;
        const QList<const QList<UiToolbarItem> *> zones = {zoneList(0), zoneList(1),
                                                           zoneList(2)};
        for (const QList<UiToolbarItem> *list : zones) {
            for (const UiToolbarItem &item : *list) {
                if (item.component == component) {
                    ++count;
                }
            }
        }
        if (count > 0) {
            emit error(tr("This component can only appear once."));
            return false;
        }
    }
    const int at = (index < 0) ? to->size() : qBound(0, index, to->size());
    to->insert(at, {component});
    emit changed();
    return true;
}

bool LayoutEditor::removeItem(int zone, int index)
{
    QList<UiToolbarItem> *from = zoneList(zone);
    if (!from || index < 0 || index >= from->size()) {
        emit error(tr("Invalid item."));
        return false;
    }
    if (UiLayout::isUniqueComponent(from->at(index).component)) {
        emit error(tr("This component cannot be removed."));
        return false;
    }
    from->removeAt(index);
    emit changed();
    return true;
}

bool LayoutEditor::setIconSize(int size)
{
    const int clamped = qBound(16, size, 32);
    if (m_working.toolbar.iconSize == clamped) {
        return true;
    }
    m_working.toolbar.iconSize = clamped;
    emit changed();
    return true;
}

bool LayoutEditor::setSpacing(int spacing)
{
    const int clamped = qBound(0, spacing, 16);
    if (m_working.toolbar.spacing == clamped) {
        return true;
    }
    m_working.toolbar.spacing = clamped;
    emit changed();
    return true;
}

bool LayoutEditor::setToolbarHeight(int height)
{
    const int clamped = qBound(40, height, 72);
    if (m_working.toolbar.height == clamped) {
        return true;
    }
    m_working.toolbar.height = clamped;
    emit changed();
    return true;
}

bool LayoutEditor::setToolbarColor(const QString &color)
{
    if (!UiLayout::isValidColor(color)) {
        emit error(tr("Color must be #rrggbb or empty."));
        return false;
    }
    if (m_working.toolbar.color == color) {
        return true;
    }
    m_working.toolbar.color = color;
    emit changed();
    return true;
}

bool LayoutEditor::setUrlHeight(int height)
{
    const int clamped = qBound(0, height, 56);
    if (m_working.urlbar.height == clamped) {
        return true;
    }
    m_working.urlbar.height = clamped;
    emit changed();
    return true;
}

bool LayoutEditor::setUrlRadius(int radius)
{
    const int clamped = qBound(0, radius, 24);
    if (m_working.urlbar.radius == clamped) {
        return true;
    }
    m_working.urlbar.radius = clamped;
    emit changed();
    return true;
}

bool LayoutEditor::setUrlMaxWidth(int maxWidth)
{
    const int clamped = qBound(0, maxWidth, 2000);
    if (m_working.urlbar.maxWidth == clamped) {
        return true;
    }
    m_working.urlbar.maxWidth = clamped;
    emit changed();
    return true;
}

bool LayoutEditor::setUrlColor(const QString &color)
{
    if (!UiLayout::isValidColor(color)) {
        emit error(tr("Color must be #rrggbb or empty."));
        return false;
    }
    if (m_working.urlbar.color == color) {
        return true;
    }
    m_working.urlbar.color = color;
    emit changed();
    return true;
}

bool LayoutEditor::setTabPosition(const QString &position)
{
    if (position != QLatin1String("horizontal") && position != QLatin1String("vertical")) {
        emit error(tr("Invalid tab position."));
        return false;
    }
    if (m_working.tabs.position == position) {
        return true;
    }
    m_working.tabs.position = position;
    emit changed();
    return true;
}

bool LayoutEditor::setTabHeight(int height)
{
    const int clamped = qBound(32, height, 48);
    if (m_working.tabs.tabHeight == clamped) {
        return true;
    }
    m_working.tabs.tabHeight = clamped;
    emit changed();
    return true;
}

bool LayoutEditor::setTabRadius(int radius)
{
    const int clamped = qBound(0, radius, 16);
    if (m_working.tabs.tabRadius == clamped) {
        return true;
    }
    m_working.tabs.tabRadius = clamped;
    emit changed();
    return true;
}

bool LayoutEditor::setTabSpacing(int spacing)
{
    const int clamped = qBound(0, spacing, 12);
    if (m_working.tabs.tabSpacing == clamped) {
        return true;
    }
    m_working.tabs.tabSpacing = clamped;
    emit changed();
    return true;
}

bool LayoutEditor::setShowPlus(bool show)
{
    if (m_working.tabs.showPlus == show) {
        return true;
    }
    m_working.tabs.showPlus = show;
    emit changed();
    return true;
}
