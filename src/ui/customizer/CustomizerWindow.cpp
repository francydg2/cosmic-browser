#include "ui/customizer/CustomizerWindow.h"

#include "browser/Browser.h"
#include "core/LayoutEditor.h"
#include "core/Logging.h"
#include "core/Settings.h"
#include "ui/BrowserWindow.h"
#include "ui/customizer/ComponentLibrary.h"
#include "ui/customizer/ComponentProperties.h"
#include "ui/customizer/CustomizerPreview.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

CustomizerWindow::CustomizerWindow(Browser *browser, QWidget *parent)
    : QDialog(parent)
    , m_browser(browser)
    , m_editor(new LayoutEditor(this))
{
    setObjectName(QStringLiteral("customizerWindow"));
    setWindowTitle(tr("Cosmic UI Customizer"));
    setAttribute(Qt::WA_DeleteOnClose);

    if (m_browser) {
        UIConfiguration cfg = m_browser->uiConfiguration();
        cfg.tabs.position = m_browser->settings()->tabPosition();
        m_editor->setConfiguration(cfg);
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *header = new QHBoxLayout;
    header->setSpacing(8);
    auto *title = new QLabel(tr("Cosmic UI Customizer"), this);
    title->setObjectName(QStringLiteral("customizerTitle"));
    m_editModeButton = new QToolButton(this);
    m_editModeButton->setObjectName(QStringLiteral("customizerEditModeButton"));
    m_editModeButton->setText(tr("Edit Mode"));
    m_editModeButton->setCheckable(true);
    m_editModeButton->setChecked(true);
    m_editModeButton->setToolTip(tr("Highlight zones and selectable components."));
    m_dirtyLabel = new QLabel(this);
    m_dirtyLabel->setObjectName(QStringLiteral("customizerDirtyLabel"));
    auto *saveButton = new QPushButton(tr("Save"), this);
    saveButton->setObjectName(QStringLiteral("customizerSaveButton"));
    saveButton->setDefault(true);
    m_saveButton = saveButton;
    auto *discardButton = new QPushButton(tr("Discard"), this);
    discardButton->setObjectName(QStringLiteral("customizerDiscardButton"));
    discardButton->setToolTip(tr("Reload the saved configuration."));
    auto *closeButton = new QPushButton(tr("Close"), this);
    closeButton->setObjectName(QStringLiteral("customizerCloseButton"));
    header->addWidget(title);
    header->addWidget(m_editModeButton);
    header->addWidget(m_dirtyLabel, 1);
    header->addWidget(saveButton);
    header->addWidget(discardButton);
    header->addWidget(closeButton);
    layout->addLayout(header);

    auto *splitter = new QSplitter(this);
    splitter->setObjectName(QStringLiteral("customizerSplitter"));
    m_library = new ComponentLibrary(splitter);
    m_preview = new CustomizerPreview(m_editor, splitter);
    m_properties = new ComponentProperties(splitter);
    m_properties->setEditor(m_editor);
    splitter->addWidget(m_library);
    splitter->addWidget(m_preview);
    splitter->addWidget(m_properties);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({200, 680, 220});
    layout->addWidget(splitter, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("customizerStatusLabel"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_editModeButton, &QToolButton::toggled, m_preview,
            &CustomizerPreview::setEditMode);
    connect(m_preview, &CustomizerPreview::selectionChanged, this,
            &CustomizerWindow::onSelectionChanged);    connect(m_preview, &CustomizerPreview::statusMessage, m_statusLabel,
            &QLabel::setText);
    connect(m_editor, &LayoutEditor::error, m_statusLabel, &QLabel::setText);
    connect(m_editor, &LayoutEditor::changed, this, &CustomizerWindow::updateDirty);
    connect(m_library, &ComponentLibrary::addRequested, this,
            &CustomizerWindow::onLibraryAdd);
    connect(saveButton, &QPushButton::clicked, this, &CustomizerWindow::save);
    connect(discardButton, &QPushButton::clicked, this, &CustomizerWindow::discard);
    connect(closeButton, &QPushButton::clicked, this, &CustomizerWindow::close);

    m_preview->setEditMode(m_editModeButton->isChecked());
    onSelectionChanged();
    updateDirty();
    resize(1100, 680);
}

void CustomizerWindow::closeEvent(QCloseEvent *event)
{
    if (!m_editor->isModified()) {
        event->accept();
        return;
    }
    const auto answer = QMessageBox::question(
        this, tr("Cosmic UI Customizer"),
        tr("Save the layout changes before closing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Cancel) {
        event->ignore();
    } else if (answer == QMessageBox::Save) {
        save();
        event->accept();
    } else {
        event->accept();
    }
}

void CustomizerWindow::onSelectionChanged()
{
    if (m_preview->tabsSelected()) {
        m_properties->showTabs();
    } else if (m_preview->toolbarSelected()) {
        m_properties->showToolbar();
    } else if (m_preview->hasToolbarSelection()) {
        m_properties->showComponent(
            m_preview->selectedComponent(),
            LayoutEditor::zoneName(m_preview->selectedZone()),
            m_preview->selectedIndex());
    } else {
        m_properties->showNothing();
    }
}

void CustomizerWindow::onLibraryAdd(const QString &component)
{
    if (!ComponentLibrary::isToolbarItem(component)) {
        m_statusLabel->setText(
            tr("This component is configured in its own customization step."));
        return;
    }
    const UIConfiguration cfg = m_editor->configuration();
    if (m_editor->addItem(2, cfg.toolbar.right.size(), component)) {
        m_statusLabel->setText(tr("Added. Drag it anywhere in the preview."));
    }
}

void CustomizerWindow::updateDirty()
{
    m_dirtyLabel->setText(m_editor->isModified() ? tr("Unsaved changes") : QString());
    m_saveButton->setEnabled(m_editor->isModified());
}

void CustomizerWindow::save()
{
    if (!m_browser) {
        m_statusLabel->setText(tr("No browser to save to."));
        return;
    }
    const UIConfiguration cfg = m_editor->configuration();
    QString error;
    if (!UiLayout::validate(cfg, &error)) {
        m_statusLabel->setText(tr("Invalid layout: %1").arg(error));
        return;
    }
    if (!m_browser->saveUiConfiguration(cfg)) {
        m_statusLabel->setText(tr("Could not write the configuration file."));
        return;
    }
    if (m_browser->settings()->tabPosition() != cfg.tabs.position) {
        m_browser->settings()->setTabPosition(cfg.tabs.position);
        m_browser->settings()->save();
    }
    for (BrowserWindow *window : BrowserWindow::allWindows()) {
        window->applyUILayout();
    }
    m_editor->setConfiguration(cfg);
    m_statusLabel->setText(tr("Saved — all windows updated."));
    qCInfo(lcCosmic).noquote() << "custom layout saved and applied";
}

void CustomizerWindow::discard()
{
    if (m_browser) {
        m_editor->setConfiguration(m_browser->uiConfiguration());
    }
    m_statusLabel->setText(tr("Reverted to the saved configuration."));
}
