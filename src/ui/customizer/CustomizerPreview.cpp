#include "ui/customizer/CustomizerPreview.h"

#include "core/LayoutEditor.h"
#include "ui/Style.h"
#include "ui/TabWidget.h"
#include "ui/SaturnIllustration.h"
#include "ui/customizer/ComponentLibrary.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDropEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

const auto kMimeComponent = "application/x-cosmic-component";

QJsonObject decodeDrop(const QMimeData *mime)
{
    if (!mime || !mime->hasFormat(QString::fromLatin1(kMimeComponent))) {
        return {};
    }
    const QByteArray raw = mime->data(QString::fromLatin1(kMimeComponent));
    if (raw.contains(':')) {
        const QList<QByteArray> parts = raw.split(':');
        if (parts.size() == 3) {
            QJsonObject obj;
            obj.insert(QStringLiteral("move"), true);
            obj.insert(QStringLiteral("zone"), QString::fromUtf8(parts.at(0)).toInt());
            obj.insert(QStringLiteral("index"), QString::fromUtf8(parts.at(1)).toInt());
            obj.insert(QStringLiteral("component"), QString::fromUtf8(parts.at(2)));
            return obj;
        }
        return {};
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("move"), false);
    obj.insert(QStringLiteral("component"), QString::fromUtf8(raw));
    return obj;
}

} // namespace

CustomizerPreview::CustomizerPreview(LayoutEditor *editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
{
    setObjectName(QStringLiteral("customizerPreview"));
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(12, 12, 12, 12);
    m_rootLayout->setSpacing(8);
    connect(m_editor, &LayoutEditor::changed, this, &CustomizerPreview::refresh);
    connect(m_editor, &LayoutEditor::error, this, &CustomizerPreview::statusMessage);
    refresh();
}

void CustomizerPreview::setEditMode(bool on)
{
    if (m_editMode == on) {
        return;
    }
    m_editMode = on;
    updateStyleSheet();
    for (int zone = 0; zone < 3; ++zone) {
        if (m_zoneFrames[zone]) {
            m_zoneFrames[zone]->setAcceptDrops(on);
        }
    }
}

bool CustomizerPreview::hasToolbarSelection() const
{
    return m_selZone >= 0 && m_selIndex >= 0 && !m_selTabs;
}

QString CustomizerPreview::selectedComponent() const
{
    if (m_selTabs) {
        return QStringLiteral("tabs");
    }
    if (!hasToolbarSelection()) {
        return {};
    }
    const UIConfiguration cfg = m_editor->configuration();
    const QList<QList<UiToolbarItem>> zones = {cfg.toolbar.left, cfg.toolbar.center,
                                              cfg.toolbar.right};
    if (m_selZone < 0 || m_selZone > 2 || m_selIndex < 0
        || m_selIndex >= zones.at(m_selZone).size()) {
        return {};
    }
    return zones.at(m_selZone).at(m_selIndex).component;
}

void CustomizerPreview::clearSelection()
{
    m_selZone = -1;
    m_selIndex = -1;
    m_selTabs = false;
    m_selToolbar = false;
    updateStyleSheet();
    emit selectionChanged();
}

void CustomizerPreview::selectToolbarItem(int zone, int index)
{
    m_selZone = zone;
    m_selIndex = index;
    m_selTabs = false;
    m_selToolbar = false;
    updateStyleSheet();
    emit selectionChanged();
}

void CustomizerPreview::selectTabs()
{
    m_selZone = -1;
    m_selIndex = -1;
    m_selTabs = true;
    m_selToolbar = false;
    updateStyleSheet();
    emit selectionChanged();
}

void CustomizerPreview::selectToolbar()
{
    m_selZone = -1;
    m_selIndex = -1;
    m_selTabs = false;
    m_selToolbar = true;
    updateStyleSheet();
    emit selectionChanged();
}

void CustomizerPreview::refresh()
{
    const QString keepId = selectedComponent();
    const int keepZone = m_selZone;
    const int keepIndex = m_selIndex;
    const bool keepTabs = m_selTabs;
    m_pressWidget = nullptr;
    rebuildChrome();
    m_selZone = -1;
    m_selIndex = -1;
    m_selTabs = false;
    if (!keepId.isEmpty()) {
        if (keepTabs && keepId == QLatin1String("tabs")) {
            m_selTabs = true;
        } else if (keepZone >= 0) {
            const UIConfiguration cfg = m_editor->configuration();
            const QList<QList<UiToolbarItem>> zones = {
                cfg.toolbar.left, cfg.toolbar.center, cfg.toolbar.right};
            if (keepZone <= 2 && keepIndex >= 0
                && keepIndex < zones.at(keepZone).size()
                && zones.at(keepZone).at(keepIndex).component == keepId) {
                m_selZone = keepZone;
                m_selIndex = keepIndex;
            }
        }
    }
    updateStyleSheet();
    emit selectionChanged();
}

void CustomizerPreview::rebuildChrome()
{
    delete m_toolbarRow;
    m_toolbarRow = nullptr;
    delete m_tabBar;
    m_tabBar = nullptr;
    delete m_contentArea;
    m_contentArea = nullptr;
    QLayoutItem *taken = nullptr;
    while ((taken = m_rootLayout->takeAt(0)) != nullptr) {
        delete taken;
    }
    m_toolbarRow = nullptr;
    m_tabBar = nullptr;
    m_contentArea = nullptr;
    for (int zone = 0; zone < 3; ++zone) {
        m_zoneFrames[zone] = nullptr;
        m_zoneLayouts[zone] = nullptr;
    }

    const UIConfiguration cfg = m_editor->configuration();

    m_toolbarRow = new QWidget(this);
    m_toolbarRow->setObjectName(QStringLiteral("previewToolbarRow"));
    m_toolbarRow->setFixedHeight(UiLayout::toolbarMetrics(cfg.toolbar).height);
    auto *toolbarLayout = new QHBoxLayout(m_toolbarRow);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(4);

    const QList<QList<UiToolbarItem>> zones = {cfg.toolbar.left, cfg.toolbar.center,
                                              cfg.toolbar.right};
    const QStringList zoneNames = {tr("LEFT"), tr("CENTER"), tr("RIGHT")};
    for (int zone = 0; zone < 3; ++zone) {
        auto *frame = new QWidget(m_toolbarRow);
        frame->setObjectName(QStringLiteral("previewZone"));
        frame->setAcceptDrops(m_editMode);
        frame->setMinimumWidth(56);
        frame->installEventFilter(this);
        auto *box = new QHBoxLayout(frame);
        box->setContentsMargins(4, 4, 4, 4);
        box->setSpacing(cfg.toolbar.spacing);
        if (zone == 0) {
            auto *caption = new QLabel(zoneNames.at(zone), frame);
            caption->setObjectName(QStringLiteral("previewZoneCaption"));
            box->addWidget(caption);
        }
        int index = 0;
        for (const UiToolbarItem &item : zones.at(zone)) {
            box->addWidget(makeItemWidget(item.component, zone, index));
            ++index;
        }
        if (zone == 2) {
            auto *caption = new QLabel(zoneNames.at(zone), frame);
            caption->setObjectName(QStringLiteral("previewZoneCaption"));
            box->addWidget(caption);
        }
        m_zoneFrames[zone] = frame;
        m_zoneLayouts[zone] = box;
        toolbarLayout->addWidget(frame, zone == 1 ? 1 : 0);
    }
    m_rootLayout->addWidget(m_toolbarRow);

    const bool vertical = cfg.tabs.position == QLatin1String("vertical");
    m_tabBar = new NebulaTabBar(this);
    m_tabBar->setObjectName(QStringLiteral("previewTabBar"));
    m_tabBar->setShape(vertical ? QTabBar::RoundedWest : QTabBar::RoundedNorth);
    m_tabBar->setTabHeight(cfg.tabs.tabHeight);
    m_tabBar->setTabRadius(cfg.tabs.tabRadius);
    m_tabBar->setTabSpacing(cfg.tabs.tabSpacing);
    m_tabBar->setShowPlus(cfg.tabs.showPlus);
    m_tabBar->setStyleSheet(
        QStringLiteral("QTabBar::tab { border-radius: %1px; margin: 4px %2px 4px 0; }")
            .arg(cfg.tabs.tabRadius)
            .arg(cfg.tabs.tabSpacing / 2));
    m_tabBar->addTab(QIcon(QStringLiteral(":/icons/globe.svg")), tr("Example"));
    m_tabBar->addTab(tr("New Tab"));
    m_tabBar->setCurrentIndex(0);
    m_tabBar->installEventFilter(this);

    m_contentArea = new QWidget(this);
    m_contentArea->setObjectName(QStringLiteral("previewContent"));
    auto *contentLayout = new QVBoxLayout(m_contentArea);
    contentLayout->setContentsMargins(8, 8, 8, 8);
    auto *saturn = new SaturnIllustration(m_contentArea);
    saturn->setFixedSize(96, 96);
    contentLayout->addStretch(1);
    contentLayout->addWidget(saturn, 0, Qt::AlignHCenter);
    auto *hint = new QLabel(
        tr("Page content lives here in the real browser.\nThis preview shows the chrome only."),
        m_contentArea);
    hint->setObjectName(QStringLiteral("previewContentHint"));
    hint->setAlignment(Qt::AlignHCenter);
    hint->setWordWrap(true);
    contentLayout->addWidget(hint);
    contentLayout->addStretch(2);

    if (vertical) {
        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);
        row->addWidget(m_tabBar);
        row->addWidget(m_contentArea, 1);
        m_rootLayout->addLayout(row, 1);
    } else {
        m_rootLayout->addWidget(m_tabBar);
        m_rootLayout->addWidget(m_contentArea, 1);
    }
}

QWidget *CustomizerPreview::makeItemWidget(const QString &component, int zone, int index)
{
    const UIConfiguration cfg = m_editor->configuration();
    const UiLayout::UiToolbarMetrics metrics = UiLayout::toolbarMetrics(cfg.toolbar);
    const QString iconPath = ComponentLibrary::iconFor(component);
    const QString label = ComponentLibrary::labelFor(component);
    QWidget *widget = nullptr;
    if (component == QLatin1String("urlbar")) {
        const QString bg = cfg.urlbar.color.isEmpty() ? QStringLiteral("#1b1d24")
                                                      : cfg.urlbar.color;
        auto *edit = new QLineEdit(this);
        edit->setReadOnly(true);
        edit->setText(QStringLiteral("example.com"));
        edit->setCursorPosition(0);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        edit->setFocusPolicy(Qt::NoFocus);
        const int urlH = cfg.urlbar.height > 0 ? qBound(24, cfg.urlbar.height, 56)
                                               : metrics.urlHeight;
        edit->setMinimumHeight(urlH);
        edit->setMaximumHeight(qMax(urlH, metrics.urlHeight + 8));
        edit->setStyleSheet(
            QStringLiteral("QLineEdit { background: %1;"
                           " border: 1px solid #2a2e39; border-radius: %2px;"
                           " padding: 4px 10px; color: #e8e9ee; }")
                .arg(bg, QString::number(cfg.urlbar.radius)));
        widget = edit;
    } else {
        auto *button = new QToolButton(this);
        button->setAutoRaise(true);
        button->setIconSize(QSize(metrics.iconSize, metrics.iconSize));
        if (!iconPath.isEmpty()) {
            button->setIcon(QIcon(iconPath));
        } else {
            button->setText(label);
        }
        widget = button;
    }
    widget->setObjectName(QStringLiteral("previewItem"));
    widget->setToolTip(label);
    widget->setProperty("pz", zone);
    widget->setProperty("pi", index);
    widget->setProperty("cc", component);
    widget->setProperty("selected", false);
    widget->setContextMenuPolicy(Qt::CustomContextMenu);
    widget->installEventFilter(this);
    return widget;
}

int CustomizerPreview::dropIndex(QWidget *zoneFrame, int zone, const QPoint &pos) const
{
    Q_UNUSED(zoneFrame);
    QHBoxLayout *box = m_zoneLayouts[zone];
    if (!box) {
        return 0;
    }
    int at = 0;
    for (int i = 0; i < box->count(); ++i) {
        QLayoutItem *item = box->itemAt(i);
        QWidget *child = item ? item->widget() : nullptr;
        if (!child || child->property("cc").toString().isEmpty()) {
            continue;
        }
        const int middle = child->x() + child->width() / 2;
        if (pos.x() < middle) {
            break;
        }
        ++at;
    }
    return at;
}

void CustomizerPreview::showItemMenu(const QPoint &globalPos, int zone, int index)
{
    QMenu *menu = itemMenu(zone, index);
    if (!menu) {
        return;
    }
    menu->exec(globalPos);
    menu->deleteLater();
}

QMenu *CustomizerPreview::itemMenu(int zone, int index)
{
    const UIConfiguration cfg = m_editor->configuration();
    const QList<QList<UiToolbarItem>> zones = {cfg.toolbar.left, cfg.toolbar.center,
                                              cfg.toolbar.right};
    if (zone < 0 || zone > 2 || index < 0 || index >= zones.at(zone).size()) {
        return nullptr;
    }
    auto *menu = new QMenu(this);
    auto *toLeft = menu->addAction(tr("Move to Left zone"));
    auto *toCenter = menu->addAction(tr("Move to Center zone"));
    auto *toRight = menu->addAction(tr("Move to Right zone"));
    menu->addSeparator();
    auto *moveBack = menu->addAction(tr("Move one step left"));
    auto *moveFwd = menu->addAction(tr("Move one step right"));
    menu->addSeparator();
    auto *remove = menu->addAction(tr("Remove"));
    connect(toLeft, &QAction::triggered, this, [this, zone, index] {
        m_editor->moveItem(zone, index, 0,
                           m_editor->configuration().toolbar.left.size());
    });
    connect(toCenter, &QAction::triggered, this, [this, zone, index] {
        m_editor->moveItem(zone, index, 1,
                           m_editor->configuration().toolbar.center.size());
    });
    connect(toRight, &QAction::triggered, this, [this, zone, index] {
        m_editor->moveItem(zone, index, 2,
                           m_editor->configuration().toolbar.right.size());
    });
    connect(moveBack, &QAction::triggered, this, [this, zone, index] {
        m_editor->moveItem(zone, index, zone, index - 1);
    });
    connect(moveFwd, &QAction::triggered, this, [this, zone, index] {
        m_editor->moveItem(zone, index, zone, index + 2);
    });
    connect(remove, &QAction::triggered, this, [this, zone, index] {
        m_editor->removeItem(zone, index);
    });
    return menu;
}

bool CustomizerPreview::eventFilter(QObject *watched, QEvent *event)
{
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (!widget) {
        return QWidget::eventFilter(watched, event);
    }
    const QEvent::Type type = event->type();

    if (widget == m_tabBar) {
        if (type == QEvent::MouseButtonPress) {
            selectTabs();
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    for (int zone = 0; zone < 3; ++zone) {
        if (widget == m_zoneFrames[zone]) {
            if (type == QEvent::MouseButtonPress) {
                auto *mouse = static_cast<QMouseEvent *>(event);
                if (mouse->button() == Qt::LeftButton) {
                    m_zonePressPos = mouse->globalPosition().toPoint();
                    m_zonePressZone = zone;
                }
                return QWidget::eventFilter(watched, event);
            }
            if (type == QEvent::MouseButtonRelease) {
                auto *mouse = static_cast<QMouseEvent *>(event);
                if (m_zonePressZone == zone && mouse->button() == Qt::LeftButton
                    && (mouse->globalPosition().toPoint() - m_zonePressPos)
                            .manhattanLength()
                        < QApplication::startDragDistance()) {
                    m_zonePressZone = -1;
                    selectToolbar();
                    return true;
                }
                m_zonePressZone = -1;
                return QWidget::eventFilter(watched, event);
            }
            if (type == QEvent::DragEnter || type == QEvent::DragMove) {
                auto *dragEvent = static_cast<QDropEvent *>(event);
                if (!decodeDrop(dragEvent->mimeData()).isEmpty()) {
                    dragEvent->acceptProposedAction();
                    setDropActive(zone);
                    return true;
                }
                return false;
            }
            if (type == QEvent::DragLeave) {
                setDropActive(-1);
                return true;
            }
            if (type == QEvent::Drop) {
                auto *dropEvent = static_cast<QDropEvent *>(event);
                const QJsonObject data = decodeDrop(dropEvent->mimeData());
                setDropActive(-1);
                m_zonePressZone = -1;
                if (data.isEmpty()) {
                    return false;
                }
                const int at = dropIndex(widget, zone,
                                           dropEvent->position().toPoint());
                bool done = false;
                if (data.value(QStringLiteral("move")).toBool()) {
                    done = m_editor->moveItem(data.value(QStringLiteral("zone")).toInt(),
                                              data.value(QStringLiteral("index")).toInt(),
                                              zone, at);
                } else {
                    done = m_editor->addItem(
                        zone, at, data.value(QStringLiteral("component")).toString());
                }
                if (done) {
                    dropEvent->acceptProposedAction();
                    selectToolbarItem(zone, at);
                    return true;
                }
                return false;
            }
            return QWidget::eventFilter(watched, event);
        }
    }

    const QString component = widget->property("cc").toString();
    if (component.isEmpty()) {
        return QWidget::eventFilter(watched, event);
    }
    const int zone = widget->property("pz").toInt();
    const int index = widget->property("pi").toInt();

    if (type == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            m_pressPos = mouse->globalPosition().toPoint();
            m_pressWidget = widget;
            return true;
        }
    } else if (type == QEvent::MouseMove) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (m_pressWidget == widget && (mouse->buttons() & Qt::LeftButton)
            && (mouse->globalPosition().toPoint() - m_pressPos).manhattanLength()
                >= QApplication::startDragDistance()) {
            m_pressWidget = nullptr;
            m_zonePressZone = -1;
            auto *drag = new QDrag(this);
            auto *mime = new QMimeData;
            const QString payload = QStringLiteral("%1:%2:%3").arg(zone).arg(index).arg(
                component);
            mime->setData(QStringLiteral("application/x-cosmic-component"),
                          payload.toUtf8());
            drag->setMimeData(mime);
            drag->exec(Qt::MoveAction);
            return true;
        }
    } else if (type == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (m_pressWidget == widget && mouse->button() == Qt::LeftButton) {
            m_pressWidget = nullptr;
            selectToolbarItem(zone, index);
            return true;
        }
        m_pressWidget = nullptr;
    } else if (type == QEvent::ContextMenu) {
        auto *menuEvent = static_cast<QContextMenuEvent *>(event);
        showItemMenu(menuEvent->globalPos(), zone, index);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void CustomizerPreview::setDropActive(int zone)
{
    if (m_dropZone == zone) {
        return;
    }
    m_dropZone = zone;
    for (int i = 0; i < 3; ++i) {
        if (m_zoneFrames[i]) {
            m_zoneFrames[i]->setProperty("dropActive", i == zone);
            m_zoneFrames[i]->style()->unpolish(m_zoneFrames[i]);
            m_zoneFrames[i]->style()->polish(m_zoneFrames[i]);
        }
    }
}

void CustomizerPreview::updateStyleSheet()
{
    const QString accent = Style::currentAccent();
    QString sheet = QStringLiteral(
        "QWidget#previewZone { border: 1px dashed #3a3f4d; border-radius: 8px;"
        " background: rgba(255,255,255,3); min-height: 40px; }"
        "QWidget#previewZone[dropActive=\"true\"] { border: 1px solid %1; }"
        "QLabel#previewZoneCaption { color: #5c616d; font-size: 10px; background: transparent; border: none; }"
        "QToolButton#previewItem[selected=\"true\"], QLineEdit#previewItem[selected=\"true\"] { border: 1px solid %2; border-radius: 8px; }"
        "QLabel#previewContentHint { color: #7d8290; background: transparent; }"
        "QWidget#previewContent { background: #0f1013; border: 1px solid #1e212a; border-radius: 10px; }"
        "QWidget#previewToolbarRow { background: #141519; border: 1px solid #1e212a; border-radius: 10px; }")
                        .arg(accent, accent);
    if (!m_editMode) {
        sheet += QStringLiteral(
            "QWidget#previewZone { border-color: transparent; background: transparent; }"
            "QLabel#previewZoneCaption { color: transparent; }");
    }
    setStyleSheet(sheet);

    for (int zone = 0; zone < 3; ++zone) {
        QHBoxLayout *box = m_zoneLayouts[zone];
        if (!box) {
            continue;
        }
        for (int i = 0; i < box->count(); ++i) {
            QWidget *child = box->itemAt(i) ? box->itemAt(i)->widget() : nullptr;
            if (!child || child->property("cc").toString().isEmpty()) {
                continue;
            }
            const bool selected = hasToolbarSelection() && m_selZone == zone
                && m_selIndex == child->property("pi").toInt();
            child->setProperty("selected", selected);
            child->style()->unpolish(child);
            child->style()->polish(child);
        }
    }
    if (m_tabBar) {
        m_tabBar->setProperty("selected", m_selTabs);
        m_tabBar->style()->unpolish(m_tabBar);
        m_tabBar->style()->polish(m_tabBar);
    }
}
