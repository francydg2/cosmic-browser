#include "ui/customizer/ComponentProperties.h"

#include "core/LayoutEditor.h"
#include "ui/customizer/ComponentLibrary.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QSpinBox *makeSpin(QWidget *parent, const QString &objectName, int lo, int hi,
                   int step = 1)
{
    auto *spin = new QSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setRange(lo, hi);
    spin->setSingleStep(step);
    spin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    return spin;
}

} // namespace

ComponentProperties::ComponentProperties(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("componentProperties"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setObjectName(QStringLiteral("propIcon"));
    layout->addWidget(m_iconLabel, 0, Qt::AlignHCenter);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName(QStringLiteral("propName"));
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_nameLabel);

    auto *form = new QFormLayout;
    form->setSpacing(6);
    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setWordWrap(true);
    m_stateLabel = new QLabel(this);
    m_locationLabel = new QLabel(this);
    form->addRow(QObject::tr("Details:"), m_descriptionLabel);
    form->addRow(QObject::tr("State:"), m_stateLabel);
    form->addRow(QObject::tr("Location:"), m_locationLabel);
    layout->addLayout(form);

    m_editArea = new QWidget(this);
    m_editArea->setObjectName(QStringLiteral("propEditArea"));
    m_editForm = new QFormLayout(m_editArea);
    m_editForm->setSpacing(6);
    m_editForm->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_editArea);
    layout->addStretch(1);
    showNothing();
}

void ComponentProperties::setEditor(LayoutEditor *editor)
{
    m_editor = editor;
    refresh();
}

void ComponentProperties::clearEditors()
{
    while (m_editForm->count() > 0) {
        QLayoutItem *taken = m_editForm->takeAt(0);
        if (!taken) {
            break;
        }
        delete taken->widget();
        delete taken;
    }
}

void ComponentProperties::showNothing()
{
    m_kind.clear();
    m_component.clear();
    clearEditors();
    m_iconLabel->clear();
    m_nameLabel->setText(tr("Nothing selected"));
    m_descriptionLabel->setText(tr("Select a component in the preview."));
    m_stateLabel->setText(QStringLiteral("—"));
    m_locationLabel->setText(QStringLiteral("—"));
    clearEditors();
}

void ComponentProperties::showToolbar()
{
    if (!m_editor) {
        return;
    }
    m_kind = QStringLiteral("toolbar");
    m_component.clear();
    m_iconLabel->clear();
    m_nameLabel->setText(tr("Toolbar"));
    m_descriptionLabel->setText(tr("The main toolbar row."));
    m_stateLabel->setText(tr("Available"));
    m_locationLabel->setText(tr("Top"));
    clearEditors();
    const UIConfiguration cfg = m_editor->configuration();
    auto *height = makeSpin(m_editArea, QStringLiteral("propHeightSpin"), 40, 72);
    height->setValue(cfg.toolbar.height);
    height->setToolTip(tr("Toolbar height in pixels."));
    connect(height, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_editor) {
            m_editor->setToolbarHeight(value);
        }
    });
    auto *spacing = makeSpin(m_editArea, QStringLiteral("propSpacingSpin"), 0, 16);
    spacing->setValue(cfg.toolbar.spacing);
    spacing->setToolTip(tr("Space between toolbar items."));
    connect(spacing, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_editor) {
            m_editor->setSpacing(value);
        }
    });
    auto *icons = makeSpin(m_editArea, QStringLiteral("propIconSpin"), 16, 32);
    icons->setValue(cfg.toolbar.iconSize);
    icons->setToolTip(tr("Toolbar icon size."));
    connect(icons, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_editor) {
            m_editor->setIconSize(value);
        }
    });
    m_editForm->addRow(tr("Height:"), height);
    m_editForm->addRow(tr("Spacing:"), spacing);
    m_editForm->addRow(tr("Icon size:"), icons);
    m_editForm->addRow(tr("Color:"), colorRow(cfg.toolbar.color, "propColorButton",
                                              &ComponentProperties::applyToolbarColor));
}

void ComponentProperties::showComponent(const QString &id, const QString &zone,
                                        int index)
{
    if (!m_editor) {
        return;
    }
    m_kind = QStringLiteral("item");
    m_component = id;
    m_zone = zone;
    m_index = index;
    const ComponentInfo *info = ComponentLibrary::infoFor(id);
    m_iconLabel->clear();
    if (info && !info->iconPath.isEmpty()) {
        const QPixmap pixmap(info->iconPath);
        if (!pixmap.isNull()) {
            m_iconLabel->setPixmap(
                pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    m_nameLabel->setText(info ? info->label : id);
    m_descriptionLabel->setText(info && !info->description.isEmpty()
                                    ? info->description
                                    : tr("Toolbar component."));
    m_stateLabel->setText(tr("Available"));
    m_locationLabel->setText(
        index >= 0 ? tr("%1 zone, position %2").arg(zone).arg(index + 1) : zone);
    clearEditors();
    const UIConfiguration cfg = m_editor->configuration();
    if (id == QLatin1String("urlbar")) {
        auto *height = makeSpin(m_editArea, QStringLiteral("propUrlHeightSpin"), 0, 56);
        height->setValue(cfg.urlbar.height);
        height->setSpecialValueText(tr("Auto"));
        height->setToolTip(tr("URL bar height. 0 follows the toolbar."));
        connect(height, &QSpinBox::valueChanged, this, [this](int value) {
            if (m_editor) {
                m_editor->setUrlHeight(value);
            }
        });
        auto *radius = makeSpin(m_editArea, QStringLiteral("propUrlRadiusSpin"), 0, 24);
        radius->setValue(cfg.urlbar.radius);
        radius->setToolTip(tr("Corner radius."));
        connect(radius, &QSpinBox::valueChanged, this, [this](int value) {
            if (m_editor) {
                m_editor->setUrlRadius(value);
            }
        });
        auto *width = makeSpin(m_editArea, QStringLiteral("propUrlWidthSpin"), 0, 2000,
                               20);
        width->setValue(cfg.urlbar.maxWidth);
        width->setSpecialValueText(tr("Unlimited"));
        width->setToolTip(tr("Maximum width. 0 grows with the window."));
        connect(width, &QSpinBox::valueChanged, this, [this](int value) {
            if (m_editor) {
                m_editor->setUrlMaxWidth(value);
            }
        });
        m_editForm->addRow(tr("Height:"), height);
        m_editForm->addRow(tr("Radius:"), radius);
        m_editForm->addRow(tr("Max width:"), width);
        m_editForm->addRow(tr("Color:"),
                            colorRow(cfg.urlbar.color, "propUrlColorButton",
                                     &ComponentProperties::applyUrlColor));
    }
    auto *zoneCombo = new QComboBox(m_editArea);
    zoneCombo->setObjectName(QStringLiteral("propZoneCombo"));
    zoneCombo->addItem(tr("Left"), 0);
    zoneCombo->addItem(tr("Center"), 1);
    zoneCombo->addItem(tr("Right"), 2);
    int zoneIndex = 0;
    if (zone == QLatin1String("Center")) {
        zoneIndex = 1;
    } else if (zone == QLatin1String("Right")) {
        zoneIndex = 2;
    }
    zoneCombo->setCurrentIndex(zoneIndex);
    connect(
        zoneCombo, &QComboBox::currentIndexChanged, this,
        [this, id](int target) {
            if (!m_editor) {
                return;
            }
            const UIConfiguration live = m_editor->configuration();
            const QList<QList<UiToolbarItem>> zones = {
                live.toolbar.left, live.toolbar.center, live.toolbar.right};
            for (int z = 0; z < 3; ++z) {
                for (int i = 0; i < zones.at(z).size(); ++i) {
                    if (zones.at(z).at(i).component == id) {
                        int at = 0;
                        if (target == 0) {
                            at = live.toolbar.left.size();
                        } else if (target == 1) {
                            at = live.toolbar.center.size();
                        } else {
                            at = live.toolbar.right.size();
                        }
                        m_editor->moveItem(z, i, target, at);
                        return;
                    }
                }
            }
        });
    m_editForm->addRow(tr("Zone:"), zoneCombo);
    auto *removeButton = new QPushButton(tr("Remove from toolbar"), m_editArea);
    removeButton->setObjectName(QStringLiteral("propRemoveButton"));
    connect(removeButton, &QPushButton::clicked, this, [this] {
        if (!m_editor) {
            return;
        }
        const UIConfiguration live = m_editor->configuration();
        const QList<QList<UiToolbarItem>> zones = {
            live.toolbar.left, live.toolbar.center, live.toolbar.right};
        for (int z = 0; z < 3; ++z) {
            for (int i = 0; i < zones.at(z).size(); ++i) {
                if (zones.at(z).at(i).component == m_component) {
                    m_editor->removeItem(z, i);
                    return;
                }
            }
        }
    });
    m_editForm->addRow(QString(), removeButton);
}

void ComponentProperties::showTabs()
{
    if (!m_editor) {
        return;
    }
    m_kind = QStringLiteral("tabs");
    m_component.clear();
    m_iconLabel->clear();
    m_nameLabel->setText(tr("Tab Bar"));
    m_descriptionLabel->setText(tr("The tab strip."));
    m_stateLabel->setText(tr("Available"));
    m_locationLabel->setText(tr("Above or beside the page"));
    clearEditors();
    const UIConfiguration cfg = m_editor->configuration();
    auto *position = new QComboBox(m_editArea);
    position->setObjectName(QStringLiteral("propTabPositionCombo"));
    position->addItem(tr("Horizontal — top"), QStringLiteral("horizontal"));
    position->addItem(tr("Vertical — sidebar"), QStringLiteral("vertical"));
    position->setCurrentIndex(cfg.tabs.position == QLatin1String("vertical") ? 1 : 0);
    connect(position, &QComboBox::currentIndexChanged, this, [this, position](int row) {
        if (m_editor) {
            m_editor->setTabPosition(position->itemData(row).toString());
        }
    });
    auto *height = makeSpin(m_editArea, QStringLiteral("propTabHeightSpin"), 32, 48);
    height->setValue(cfg.tabs.tabHeight);
    height->setToolTip(tr("Tab height."));
    connect(height, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_editor) {
            m_editor->setTabHeight(value);
        }
    });
    auto *radius = makeSpin(m_editArea, QStringLiteral("propTabRadiusSpin"), 0, 16);
    radius->setValue(cfg.tabs.tabRadius);
    radius->setToolTip(tr("Tab corner radius."));
    connect(radius, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_editor) {
            m_editor->setTabRadius(value);
        }
    });
    auto *spacing = makeSpin(m_editArea, QStringLiteral("propTabSpacingSpin"), 0, 12);
    spacing->setValue(cfg.tabs.tabSpacing);
    spacing->setToolTip(tr("Space between tabs."));
    connect(spacing, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_editor) {
            m_editor->setTabSpacing(value);
        }
    });
    auto *plus = new QCheckBox(tr("Show the + button"), m_editArea);
    plus->setObjectName(QStringLiteral("propShowPlusCheck"));
    plus->setChecked(cfg.tabs.showPlus);
    connect(plus, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_editor) {
            m_editor->setShowPlus(checked);
        }
    });
    m_editForm->addRow(tr("Position:"), position);
    m_editForm->addRow(tr("Tab height:"), height);
    m_editForm->addRow(tr("Tab radius:"), radius);
    m_editForm->addRow(tr("Tab spacing:"), spacing);
    m_editForm->addRow(QString(), plus);
}

void ComponentProperties::refresh()
{
    if (!m_editor) {
        return;
    }
    if (m_kind == QLatin1String("toolbar")) {
        showToolbar();
    } else if (m_kind == QLatin1String("tabs")) {
        showTabs();
    } else if (m_kind == QLatin1String("item") && !m_component.isEmpty()) {
        showComponent(m_component, m_zone, m_index);
    } else {
        showNothing();
    }
}

QWidget *ComponentProperties::colorRow(const QString &current,
                                       const char *buttonName,
                                       void (ComponentProperties::*apply)(const QString &))
{
    auto *row = new QWidget(m_editArea);
    auto *box = new QHBoxLayout(row);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(6);
    auto *pick = new QPushButton(row);
    pick->setObjectName(QString::fromLatin1(buttonName));
    pick->setText(current.isEmpty() ? tr("Theme") : current);
    pick->setStyleSheet(current.isEmpty()
                            ? QString()
                            : QStringLiteral("QPushButton { background: %1; }").arg(current));
    pick->setToolTip(tr("Pick a custom color."));
    connect(pick, &QPushButton::clicked, this, [this, apply, current] {
        const QColor color =
            QColorDialog::getColor(QColor(current), this, tr("Choose color"));
        if (color.isValid()) {
            (this->*apply)(color.name(QColor::HexRgb));
        }
    });
    auto *reset = new QPushButton(tr("Theme"), row);
    reset->setObjectName(QString::fromLatin1(QByteArray(buttonName) + "Default"));
    reset->setToolTip(tr("Back to the theme color."));
    connect(reset, &QPushButton::clicked, this, [this, apply] {
        (this->*apply)(QString());
    });
    box->addWidget(pick, 1);
    box->addWidget(reset);
    return row;
}

void ComponentProperties::applyToolbarColor(const QString &color)
{
    if (m_editor && m_editor->setToolbarColor(color)) {
        refresh();
    }
}

void ComponentProperties::applyUrlColor(const QString &color)
{
    if (m_editor && m_editor->setUrlColor(color)) {
        refresh();
    }
}
