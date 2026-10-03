#include "ui/SettingsForm.h"

#include "browser/Browser.h"
#include "core/UserCatalog.h"
#include "core/Logging.h"
#include "core/PasswordVault.h"
#include "core/SearchEngines.h"
#include "core/Settings.h"
#include "ui/NameColorDialog.h"
#include "ui/customizer/CustomizerWindow.h"
#include "web/BlocklistInterceptor.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

const auto kCustomId = "custom";

const QStringList accentPresets()
{
    return {QStringLiteral("#7c6cf0"), QStringLiteral("#5b8def"),
            QStringLiteral("#22c55e"), QStringLiteral("#f59e0b"),
            QStringLiteral("#ef4444"), QStringLiteral("#ec4899"),
            QStringLiteral("#14b8a6"), QStringLiteral("#94a3b8")};
}

QComboBox *makePermissionCombo(QWidget *parent, const QString &objectName)
{
    auto *combo = new QComboBox(parent);
    combo->setObjectName(objectName);
    combo->addItem(SettingsForm::tr("Ask"), QStringLiteral("ask"));
    combo->addItem(SettingsForm::tr("Always allow"), QStringLiteral("allow"));
    combo->addItem(SettingsForm::tr("Always block"), QStringLiteral("block"));
    return combo;
}

void selectPermissionCombo(QComboBox *combo, const QString &policy)
{
    const int index = combo->findData(policy);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

} // namespace

SettingsForm::SettingsForm(Settings *settings, BlocklistInterceptor *interceptor,
                           UserCatalog *catalog, PasswordVault *vault,
                           Browser *browser, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_interceptor(interceptor)
    , m_userCatalog(catalog)
    , m_vault(vault)
    , m_browser(browser)
{
    setObjectName(QStringLiteral("settingsForm"));

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("settingsTabs"));
    m_sections = tabs;

    // --- General ----------------------------------------------------------
    auto *generalPage = new QWidget(tabs);
    generalPage->setObjectName(QStringLiteral("settingsTabGeneral"));
    auto *generalForm = new QVBoxLayout(generalPage);
    generalForm->setSpacing(10);
    generalForm->setContentsMargins(12, 12, 12, 12);

    m_sessionCheck = new QCheckBox(tr("Restore previous session on startup"), generalPage);
    m_sessionCheck->setObjectName(QStringLiteral("sessionRestoreCheck"));

    generalForm->addWidget(m_sessionCheck);
    generalForm->addStretch(1);
    tabs->addTab(generalPage, QIcon(QStringLiteral(":/icons/gear.svg")),
                 tr("General"));

    // --- Search engine ----------------------------------------------------
    auto *searchPage = new QWidget(tabs);
    searchPage->setObjectName(QStringLiteral("settingsTabSearch"));
    auto *searchForm = new QFormLayout(searchPage);
    searchForm->setSpacing(10);
    searchForm->setContentsMargins(12, 12, 12, 12);

    m_engineCombo = new QComboBox(searchPage);
    m_engineCombo->setObjectName(QStringLiteral("engineCombo"));
    const auto engines = SearchEngines::builtins();
    for (const SearchEngine &engine : engines) {
        m_engineCombo->addItem(engine.name, engine.id);
    }
    m_engineCombo->addItem(tr("Custom…"), QString::fromLatin1(kCustomId));

    m_customEdit = new QLineEdit(searchPage);
    m_customEdit->setObjectName(QStringLiteral("customTemplateEdit"));
    m_customEdit->setPlaceholderText(tr("https://example.com/search?q=%1"));
    m_customEdit->setClearButtonEnabled(true);

    searchForm->addRow(tr("Search engine:"), m_engineCombo);
    searchForm->addRow(tr("Custom template:"), m_customEdit);
    tabs->addTab(searchPage, QIcon(QStringLiteral(":/icons/search.svg")),
                 tr("Search engine"));

    // --- Privacy ----------------------------------------------------------
    auto *privacyPage = new QWidget(tabs);
    privacyPage->setObjectName(QStringLiteral("settingsTabPrivacy"));
    auto *privacyForm = new QVBoxLayout(privacyPage);
    privacyForm->setSpacing(10);
    privacyForm->setContentsMargins(12, 12, 12, 12);

    m_blockingCheck = new QCheckBox(tr("Block ad/tracker requests (domain level)"), privacyPage);
    m_blockingCheck->setObjectName(QStringLiteral("blockingCheck"));
    m_blockingCheck->setToolTip(tr(
        "Cancels network requests to known tracker domains. Some sites may "
        "break; add rules to ~/.config/nebula/blocklist.txt or disable here."));

    m_blockedLabel = new QLabel(privacyPage);
    m_blockedLabel->setObjectName(QStringLiteral("blockedLabel"));
    m_blockedLabel->setStyleSheet(QStringLiteral("color: #7d8290;"));

    m_dntCheck = new QCheckBox(tr("Send a “Do Not Track” header with every request"),
                               privacyPage);
    m_dntCheck->setObjectName(QStringLiteral("dntCheck"));

    m_sessionCookiesCheck =
        new QCheckBox(tr("Keep cookies only for this session (delete on exit)"), privacyPage);
    m_sessionCookiesCheck->setObjectName(QStringLiteral("sessionCookiesCheck"));

    auto *permLabel = new QLabel(
        tr("Site permissions — what Cosmic does when a page asks:"), privacyPage);
    permLabel->setObjectName(QStringLiteral("permissionsLabel"));
    permLabel->setWordWrap(true);
    permLabel->setStyleSheet(QStringLiteral("color: #7d8290;"));

    auto *permForm = new QFormLayout;
    permForm->setSpacing(8);
    m_locationPermCombo =
        makePermissionCombo(privacyPage, QStringLiteral("locationPermCombo"));
    m_mediaPermCombo = makePermissionCombo(privacyPage, QStringLiteral("mediaPermCombo"));
    m_notificationsPermCombo =
        makePermissionCombo(privacyPage, QStringLiteral("notificationsPermCombo"));
    permForm->addRow(tr("Location:"), m_locationPermCombo);
    permForm->addRow(tr("Camera && microphone:"), m_mediaPermCombo);
    permForm->addRow(tr("Notifications:"), m_notificationsPermCombo);

    privacyForm->addWidget(m_blockingCheck);
    privacyForm->addWidget(m_blockedLabel);
    privacyForm->addWidget(m_dntCheck);
    privacyForm->addWidget(m_sessionCookiesCheck);
    privacyForm->addWidget(permLabel);
    privacyForm->addLayout(permForm);
    privacyForm->addStretch(1);
    tabs->addTab(privacyPage, QIcon(QStringLiteral(":/icons/lock.svg")),
                 tr("Privacy"));

    // --- Appearance -------------------------------------------------------
    tabs->addTab(buildAppearancePage(tabs),
                 QIcon(QStringLiteral(":/icons/palette.svg")), tr("Appearance"));

    // --- Passwords --------------------------------------------------------
    tabs->addTab(buildPasswordsPage(tabs), QIcon(QStringLiteral(":/icons/key.svg")),
                 tr("Passwords"));

    // --- Users ------------------------------------------------------------
    auto *usersPage = new QWidget(tabs);
    usersPage->setObjectName(QStringLiteral("settingsTabUsers"));
    auto *usersLayout = new QVBoxLayout(usersPage);
    usersLayout->setSpacing(10);
    usersLayout->setContentsMargins(12, 12, 12, 12);

    auto *usersHint = new QLabel(
        tr("Users keep separate tabs, cookies and logins. Switching user "
           "in the toolbar swaps the whole tab set — every user remembers "
           "their own tabs."),
        usersPage);
    usersHint->setObjectName(QStringLiteral("usersHint"));
    usersHint->setWordWrap(true);
    usersHint->setStyleSheet(QStringLiteral("color: #7d8290;"));
    usersLayout->addWidget(usersHint);

    m_usersList = new QListWidget(usersPage);
    m_usersList->setObjectName(QStringLiteral("usersList"));
    usersLayout->addWidget(m_usersList, 1);

    auto *userButtons = new QHBoxLayout;
    m_addUserButton = new QPushButton(tr("Add…"), usersPage);
    m_addUserButton->setObjectName(QStringLiteral("addUserButton"));
    m_renameUserButton = new QPushButton(tr("Rename…"), usersPage);
    m_renameUserButton->setObjectName(QStringLiteral("renameUserButton"));
    m_removeUserButton = new QPushButton(tr("Remove"), usersPage);
    m_removeUserButton->setObjectName(QStringLiteral("removeUserButton"));
    userButtons->addWidget(m_addUserButton);
    userButtons->addWidget(m_renameUserButton);
    userButtons->addWidget(m_removeUserButton);
    userButtons->addStretch(1);
    usersLayout->addLayout(userButtons);
    tabs->addTab(usersPage, QIcon(QStringLiteral(":/icons/user.svg")), tr("Users"));

    // --- Advanced ---------------------------------------------------------
    tabs->addTab(buildAdvancedPage(tabs), QIcon(QStringLiteral(":/icons/wrench.svg")),
                 tr("Advanced"));

    layout->addWidget(tabs, 1);

    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(12, 0, 12, 12);
    actionRow->setSpacing(8);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("settingsStatusLabel"));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #7d8290;"));
    m_resetButton = new QPushButton(tr("Reset"), this);
    m_resetButton->setObjectName(QStringLiteral("settingsResetButton"));
    m_resetButton->setToolTip(tr("Reload all fields from the saved settings."));
    connect(m_resetButton, &QPushButton::clicked, this, &SettingsForm::load);
    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setObjectName(QStringLiteral("settingsSaveButton"));
    m_saveButton->setDefault(true);
    connect(m_saveButton, &QPushButton::clicked, this, &SettingsForm::apply);
    actionRow->addWidget(m_statusLabel, 1);
    actionRow->addWidget(m_resetButton);
    actionRow->addWidget(m_saveButton);
    layout->addLayout(actionRow);

    load();

    if (m_interceptor) {
        const int count = m_interceptor->blockedCount();
        m_blockedLabel->setText(tr("Blocked requests this session: %1").arg(count));
        connect(m_interceptor, &BlocklistInterceptor::blockedCountChanged, m_blockedLabel,
                [label = m_blockedLabel](int total) {
                    label->setText(
                        SettingsForm::tr("Blocked requests this session: %1").arg(total));
                });
    } else {
        m_blockedLabel->hide();
    }

    connect(m_engineCombo, &QComboBox::currentIndexChanged, this,
            &SettingsForm::syncCustomTemplateState);
    syncCustomTemplateState();

    if (m_userCatalog) {
        refreshUserWidgets();
        connect(m_userCatalog, &UserCatalog::changed, this,
                &SettingsForm::refreshUserWidgets);
        connect(m_usersList, &QListWidget::currentRowChanged, this, [this] {
            const bool hasSelection = m_usersList->currentItem() != nullptr;
            m_renameUserButton->setEnabled(hasSelection);
            m_removeUserButton->setEnabled(hasSelection);
        });
        connect(m_addUserButton, &QPushButton::clicked, this,
                &SettingsForm::addUser);
        connect(m_renameUserButton, &QPushButton::clicked, this,
                &SettingsForm::renameUser);
        connect(m_removeUserButton, &QPushButton::clicked, this,
                &SettingsForm::removeUser);
    } else {
        m_usersList->setEnabled(false);
        m_addUserButton->setEnabled(false);
        m_renameUserButton->setEnabled(false);
        m_removeUserButton->setEnabled(false);
    }
}

void SettingsForm::load()
{
    const QString engineId = m_settings->defaultSearchEngine();
    int index = m_engineCombo->findData(engineId);
    if (index < 0) {
        index = m_engineCombo->findData(QString::fromLatin1(kCustomId));
    }
    m_engineCombo->setCurrentIndex(index);
    m_customEdit->setText(m_settings->customSearchTemplate());
    m_blockingCheck->setChecked(m_settings->requestBlockingEnabled());
    m_sessionCheck->setChecked(m_settings->restoreSessionOnStartup());
    m_dntCheck->setChecked(m_settings->doNotTrack());
    m_sessionCookiesCheck->setChecked(m_settings->sessionCookiesOnly());
    selectPermissionCombo(m_locationPermCombo,
                          m_settings->permissionPolicy(QStringLiteral("location")));
    selectPermissionCombo(m_mediaPermCombo,
                          m_settings->permissionPolicy(QStringLiteral("media")));
    selectPermissionCombo(m_notificationsPermCombo,
                          m_settings->permissionPolicy(QStringLiteral("notifications")));
    m_httpsOnlyCheck->setChecked(m_settings->httpsOnly());
    selectAccent(m_settings->accentColor());
    index = m_tabPositionCombo->findData(m_settings->tabPosition());
    m_tabPositionCombo->setCurrentIndex(index >= 0 ? index : 0);
    syncCustomTemplateState();
    refreshUserWidgets();
    refreshVaultWidgets();
    m_statusLabel->setText(QString());
}

void SettingsForm::showSection(const QString &name)
{
    if (!m_sections || name.isEmpty()) {
        return;
    }
    const QString objectName =
        QStringLiteral("settingsTab") + name.at(0).toUpper() + name.mid(1);
    if (QWidget *page = findChild<QWidget *>(objectName)) {
        m_sections->setCurrentIndex(m_sections->indexOf(page));
    }
}

QWidget *SettingsForm::buildAdvancedPage(QTabWidget *tabs)
{
    auto *page = new QWidget(tabs);
    page->setObjectName(QStringLiteral("settingsTabAdvanced"));
    auto *form = new QVBoxLayout(page);
    form->setSpacing(10);
    form->setContentsMargins(12, 12, 12, 12);

    m_httpsOnlyCheck = new QCheckBox(tr("Upgrade http:// pages to https://"), page);
    m_httpsOnlyCheck->setObjectName(QStringLiteral("httpsOnlyCheck"));
    m_httpsOnlyCheck->setToolTip(tr(
        "Top-level navigations that start with http:// are retried with "
        "https://. Sub-resources are not rewritten."));

    auto *httpsHint = new QLabel(
        tr("Only top-level navigations are upgraded: embedded http:// "
           "images, frames and scripts stay as the page requested them, and "
           "a site with an invalid certificate still shows the engine's "
           "error page (there is no bypass button)."), page);
    httpsHint->setObjectName(QStringLiteral("httpsHint"));
    httpsHint->setWordWrap(true);
    httpsHint->setStyleSheet(QStringLiteral("color: #7d8290;"));

    m_clearDataButton = new QPushButton(tr("Clear cookies and cache now…"), page);
    m_clearDataButton->setObjectName(QStringLiteral("clearDataButton"));
    connect(m_clearDataButton, &QPushButton::clicked, this,
            &SettingsForm::clearBrowsingDataRequested);

    auto *resetButton = new QPushButton(tr("Restore default settings…"), page);
    resetButton->setObjectName(QStringLiteral("resetDefaultsButton"));
    resetButton->setToolTip(tr("Reset every setting on this page to its built-in default."));
    connect(resetButton, &QPushButton::clicked, this, &SettingsForm::resetDefaults);

    form->addWidget(m_httpsOnlyCheck);
    form->addWidget(httpsHint);
    form->addWidget(m_clearDataButton);
    form->addWidget(resetButton);
    form->addStretch(1);
    return page;
}

QWidget *SettingsForm::buildAppearancePage(QTabWidget *tabs)
{
    auto *page = new QWidget(tabs);
    page->setObjectName(QStringLiteral("settingsTabAppearance"));
    auto *form = new QVBoxLayout(page);
    form->setSpacing(10);
    form->setContentsMargins(12, 12, 12, 12);

    auto *accentLabel = new QLabel(tr("Accent color:"), page);
    accentLabel->setObjectName(QStringLiteral("accentLabel"));

    auto *swatchRow = new QHBoxLayout;
    swatchRow->setSpacing(6);
    auto *group = new QButtonGroup(page);
    group->setExclusive(true);
    const QStringList presets = accentPresets();
    for (int i = 0; i < presets.size(); ++i) {
        const QString color = presets.at(i);
        auto *swatch = new QToolButton(page);
        swatch->setObjectName(QStringLiteral("accentSwatch"));
        swatch->setCheckable(true);
        swatch->setFixedSize(22, 22);
        swatch->setToolTip(color);
        swatch->setStyleSheet(QStringLiteral(
            "QToolButton { background: %1; border: 2px solid transparent;"
            " border-radius: 11px; }"
            " QToolButton:checked { border-color: #e8e9ee; }"
            " QToolButton:hover { border-color: #9ba1ad; }")
                                  .arg(color));
        group->addButton(swatch, i);
        m_accentSwatches.append(qMakePair(swatch, color));
        connect(swatch, &QToolButton::clicked, this, [this, color] {
            m_accent = color;
        });
        swatchRow->addWidget(swatch);
    }
    auto *customButton = new QPushButton(tr("Custom…"), page);
    customButton->setObjectName(QStringLiteral("accentCustomButton"));
    connect(customButton, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(QColor(m_accent), this,
                                                    tr("Choose accent color"));
        if (color.isValid()) {
            selectAccent(color.name(QColor::HexRgb));
        }
    });
    swatchRow->addWidget(customButton);
    swatchRow->addStretch(1);

    auto *tabsForm = new QFormLayout;
    tabsForm->setSpacing(8);
    m_tabPositionCombo = new QComboBox(page);
    m_tabPositionCombo->setObjectName(QStringLiteral("tabPositionCombo"));
    m_tabPositionCombo->addItem(tr("Horizontal — top strip"),
                                QStringLiteral("horizontal"));
    m_tabPositionCombo->addItem(tr("Vertical — sidebar"),
                                QStringLiteral("vertical"));
    tabsForm->addRow(tr("Tab position:"), m_tabPositionCombo);

    auto *customizeButton = new QPushButton(tr("Customize Cosmic…"), page);
    customizeButton->setObjectName(QStringLiteral("customizeButton"));
    customizeButton->setToolTip(
        tr("Open the UI customizer: rearrange the toolbar in a live preview."));
    customizeButton->setEnabled(m_browser != nullptr);
    connect(customizeButton, &QPushButton::clicked, this, [this] {
        if (!m_browser) {
            return;
        }
        auto *customizer = new CustomizerWindow(m_browser, this);
        customizer->show();
    });

    form->addWidget(accentLabel);
    form->addLayout(swatchRow);
    form->addLayout(tabsForm);
    form->addWidget(customizeButton);
    form->addStretch(1);
    return page;
}

QWidget *SettingsForm::buildPasswordsPage(QTabWidget *tabs)
{
    auto *page = new QWidget(tabs);
    page->setObjectName(QStringLiteral("settingsTabPasswords"));
    auto *form = new QVBoxLayout(page);
    form->setSpacing(10);
    form->setContentsMargins(12, 12, 12, 12);

    m_vaultStatusLabel = new QLabel(page);
    m_vaultStatusLabel->setObjectName(QStringLiteral("vaultStatusLabel"));
    m_vaultStatusLabel->setWordWrap(true);

    auto *masterRow = new QHBoxLayout;
    masterRow->setSpacing(6);
    m_vaultMasterEdit = new QLineEdit(page);
    m_vaultMasterEdit->setObjectName(QStringLiteral("vaultMasterEdit"));
    m_vaultMasterEdit->setEchoMode(QLineEdit::Password);
    m_vaultMasterEdit->setPlaceholderText(tr("Master password"));
    m_vaultMasterEdit->setClearButtonEnabled(true);
    m_vaultMasterButton = new QPushButton(page);
    m_vaultMasterButton->setObjectName(QStringLiteral("vaultMasterButton"));
    connect(m_vaultMasterButton, &QPushButton::clicked, this,
            &SettingsForm::vaultMasterButtonClicked);
    connect(m_vaultMasterEdit, &QLineEdit::returnPressed, this,
            &SettingsForm::vaultMasterButtonClicked);
    masterRow->addWidget(m_vaultMasterEdit, 1);
    masterRow->addWidget(m_vaultMasterButton);

    m_vaultPanel = new QWidget(page);
    m_vaultPanel->setObjectName(QStringLiteral("vaultPanel"));
    auto *panelForm = new QVBoxLayout(m_vaultPanel);
    panelForm->setSpacing(8);
    panelForm->setContentsMargins(0, 0, 0, 0);

    m_vaultList = new QListWidget(m_vaultPanel);
    m_vaultList->setObjectName(QStringLiteral("vaultList"));
    panelForm->addWidget(m_vaultList, 1);

    auto *entryForm = new QFormLayout;
    entryForm->setSpacing(8);
    m_vaultSiteEdit = new QLineEdit(m_vaultPanel);
    m_vaultSiteEdit->setObjectName(QStringLiteral("vaultSiteEdit"));
    m_vaultSiteEdit->setPlaceholderText(tr("example.com"));
    m_vaultSiteEdit->setClearButtonEnabled(true);
    m_vaultUserEdit = new QLineEdit(m_vaultPanel);
    m_vaultUserEdit->setObjectName(QStringLiteral("vaultUserEdit"));
    m_vaultUserEdit->setClearButtonEnabled(true);
    m_vaultPassEdit = new QLineEdit(m_vaultPanel);
    m_vaultPassEdit->setObjectName(QStringLiteral("vaultPassEdit"));
    m_vaultPassEdit->setEchoMode(QLineEdit::Password);
    m_vaultRevealCheck = new QCheckBox(tr("Show"), m_vaultPanel);
    m_vaultRevealCheck->setObjectName(QStringLiteral("vaultRevealCheck"));
    connect(m_vaultRevealCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_vaultPassEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    auto *passRow = new QHBoxLayout;
    passRow->setSpacing(6);
    passRow->addWidget(m_vaultPassEdit, 1);
    passRow->addWidget(m_vaultRevealCheck);
    m_vaultTypeCombo = new QComboBox(m_vaultPanel);
    m_vaultTypeCombo->setObjectName(QStringLiteral("vaultTypeCombo"));
    m_vaultTypeCombo->addItem(tr("Password"), QStringLiteral("password"));
    m_vaultTypeCombo->addItem(tr("Passkey (metadata only)"), QStringLiteral("passkey"));
    m_vaultNoteEdit = new QLineEdit(m_vaultPanel);
    m_vaultNoteEdit->setObjectName(QStringLiteral("vaultNoteEdit"));
    m_vaultNoteEdit->setPlaceholderText(tr("Note (optional)"));
    m_vaultNoteEdit->setClearButtonEnabled(true);
    entryForm->addRow(tr("Site:"), m_vaultSiteEdit);
    entryForm->addRow(tr("Username:"), m_vaultUserEdit);
    entryForm->addRow(tr("Secret:"), passRow);
    entryForm->addRow(tr("Type:"), m_vaultTypeCombo);
    entryForm->addRow(tr("Note:"), m_vaultNoteEdit);
    panelForm->addLayout(entryForm);

    auto *entryButtons = new QHBoxLayout;
    entryButtons->setSpacing(6);
    m_vaultAddButton = new QPushButton(tr("Add entry"), m_vaultPanel);
    m_vaultAddButton->setObjectName(QStringLiteral("vaultAddButton"));
    m_vaultSaveButton = new QPushButton(tr("Save entry"), m_vaultPanel);
    m_vaultSaveButton->setObjectName(QStringLiteral("vaultSaveButton"));
    m_vaultRemoveButton = new QPushButton(tr("Remove"), m_vaultPanel);
    m_vaultRemoveButton->setObjectName(QStringLiteral("vaultRemoveButton"));
    m_vaultLockButton = new QPushButton(tr("Lock now"), m_vaultPanel);
    m_vaultLockButton->setObjectName(QStringLiteral("vaultLockButton"));
    entryButtons->addWidget(m_vaultAddButton);
    entryButtons->addWidget(m_vaultSaveButton);
    entryButtons->addWidget(m_vaultRemoveButton);
    entryButtons->addStretch(1);
    entryButtons->addWidget(m_vaultLockButton);
    panelForm->addLayout(entryButtons);

    auto *hint = new QLabel(
        tr("The vault (~/.config/cosmic/vault.bin, mode 0600) is encrypted "
           "with AES-256-GCM; the master key is derived with "
           "PBKDF2-SHA256 × 210,000 iterations. Secrets are never written "
           "to cookies, settings or the page. Passkeys are stored as "
           "metadata only: Qt WebEngine exposes no WebAuthn device APIs, "
           "so Cosmic cannot create or use real passkeys."), page);
    hint->setObjectName(QStringLiteral("vaultHint"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #7d8290;"));

    form->addWidget(m_vaultStatusLabel);
    form->addLayout(masterRow);
    form->addWidget(m_vaultPanel, 1);
    form->addWidget(hint);

    if (!m_vault) {
        m_vaultStatusLabel->setText(tr("Password vault unavailable in this context."));
        m_vaultMasterEdit->setEnabled(false);
        m_vaultMasterButton->setEnabled(false);
        m_vaultPanel->setEnabled(false);
        return page;
    }

    connect(m_vaultList, &QListWidget::currentRowChanged, this,
            &SettingsForm::vaultSelectionChanged);
    connect(m_vaultAddButton, &QPushButton::clicked, this, &SettingsForm::vaultAddEntry);
    connect(m_vaultSaveButton, &QPushButton::clicked, this, &SettingsForm::vaultSaveEntry);
    connect(m_vaultRemoveButton, &QPushButton::clicked, this,
            &SettingsForm::vaultRemoveEntry);
    connect(m_vaultLockButton, &QPushButton::clicked, this, [this] {
        if (m_vault) {
            m_vault->lock();
        }
    });
    connect(m_vault, &PasswordVault::entriesChanged, this,
            &SettingsForm::refreshVaultWidgets);
    connect(m_vault, &PasswordVault::lockStateChanged, this,
            &SettingsForm::refreshVaultWidgets);
    refreshVaultWidgets();
    return page;
}

void SettingsForm::selectAccent(const QString &hex)
{
    const QString normalized = QColor(hex).isValid()
        ? QColor(hex).name(QColor::HexRgb)
        : QStringLiteral("#7c6cf0");
    m_accent = normalized;
    for (const auto &pair : m_accentSwatches) {
        const bool match = pair.second == normalized;
        if (match) {
            pair.first->setChecked(true);
        } else {
            const QSignalBlocker blocker(pair.first);
            pair.first->setChecked(false);
        }
    }
}

void SettingsForm::refreshVaultWidgets()
{
    if (!m_vault) {
        return;
    }
    if (!m_vault->fileExists()) {
        m_vaultStatusLabel->setText(
            tr("No vault yet — choose a master password (4+ characters) "
               "and press “Create vault”."));
        m_vaultMasterButton->setText(tr("Create vault"));
        m_vaultMasterButton->setEnabled(true);
        m_vaultPanel->setEnabled(false);
        m_vaultList->clear();
        return;
    }
    if (!m_vault->isUnlocked()) {
        m_vaultStatusLabel->setText(
            tr("Vault locked — enter the master password and press Unlock."));
        m_vaultMasterButton->setText(tr("Unlock"));
        m_vaultMasterButton->setEnabled(true);
        m_vaultPanel->setEnabled(false);
        m_vaultList->clear();
        return;
    }

    const QVector<VaultEntry> entries = m_vault->entries();
    m_vaultStatusLabel->setText(
        tr("Unlocked — %1 entr%2 in %3.")
            .arg(entries.size())
            .arg(entries.size() == 1 ? QStringLiteral("y") : QStringLiteral("ies"))
            .arg(m_vault->filePath()));
    m_vaultMasterButton->setText(tr("Unlocked"));
    m_vaultMasterButton->setEnabled(false);
    m_vaultPanel->setEnabled(true);

    const QString previousId =
        m_vaultList->currentItem() ? m_vaultList->currentItem()->data(Qt::UserRole).toString()
                                   : QString();
    int selectRow = -1;
    {
        const QSignalBlocker blocker(m_vaultList);
        m_vaultList->clear();
        for (const VaultEntry &entryData : entries) {
            QString text = entryData.site;
            if (!entryData.username.isEmpty()) {
                text += QStringLiteral(" — ") + entryData.username;
            }
            if (entryData.type == QLatin1String("passkey")) {
                text += tr(" (passkey)");
            }
            auto *item = new QListWidgetItem(text, m_vaultList);
            item->setData(Qt::UserRole, entryData.id);
            item->setToolTip(entryData.site);
            if (entryData.id == previousId) {
                selectRow = m_vaultList->count() - 1;
            }
        }
        if (selectRow >= 0) {
            m_vaultList->setCurrentRow(selectRow);
        }
    }
    if (selectRow < 0 && !previousId.isEmpty()) {
        m_vaultSiteEdit->clear();
        m_vaultUserEdit->clear();
        m_vaultPassEdit->clear();
        m_vaultNoteEdit->clear();
        m_vaultTypeCombo->setCurrentIndex(0);
        m_vaultRevealCheck->setChecked(false);
    }
}

void SettingsForm::vaultMasterButtonClicked()
{
    if (!m_vault) {
        return;
    }
    const QString master = m_vaultMasterEdit->text();
    bool ok = false;
    if (!m_vault->fileExists()) {
        if (master.size() < 4) {
            QMessageBox::warning(this, tr("Password vault"),
                                 tr("The master password needs at least 4 characters."));
            return;
        }
        ok = m_vault->initialize(master);
    } else {
        ok = m_vault->unlock(master);
    }
    if (!ok) {
        QMessageBox::warning(this, tr("Password vault"), m_vault->lastError());
        return;
    }
    m_vaultMasterEdit->clear();
    refreshVaultWidgets();
    qCInfo(lcCosmic).noquote() << "vault unlocked";
}

void SettingsForm::vaultSelectionChanged()
{
    if (!m_vault || !m_vault->isUnlocked()) {
        return;
    }
    auto *item = m_vaultList->currentItem();
    if (!item) {
        return;
    }
    const VaultEntry entryData = m_vault->entry(item->data(Qt::UserRole).toString());
    if (entryData.id.isEmpty()) {
        return;
    }
    m_vaultSiteEdit->setText(entryData.site);
    m_vaultUserEdit->setText(entryData.username);
    m_vaultPassEdit->setText(entryData.password);
    m_vaultNoteEdit->setText(entryData.note);
    const int typeIndex = m_vaultTypeCombo->findData(entryData.type);
    m_vaultTypeCombo->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
    m_vaultRevealCheck->setChecked(false);
}

void SettingsForm::vaultAddEntry()
{
    if (!m_vault || !m_vault->isUnlocked()) {
        return;
    }
    VaultEntry entryData;
    entryData.site = m_vaultSiteEdit->text().trimmed();
    entryData.username = m_vaultUserEdit->text().trimmed();
    entryData.password = m_vaultPassEdit->text();
    entryData.type = m_vaultTypeCombo->currentData().toString();
    entryData.note = m_vaultNoteEdit->text().trimmed();
    if (entryData.site.isEmpty()) {
        QMessageBox::warning(this, tr("Password vault"), tr("The site field is required."));
        return;
    }
    if (entryData.type == QLatin1String("password") && entryData.password.isEmpty()) {
        QMessageBox::warning(this, tr("Password vault"),
                             tr("The secret field cannot be empty for a password entry."));
        return;
    }
    const QString id = m_vault->addEntry(entryData);
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("Password vault"), m_vault->lastError());
        return;
    }
    for (int row = 0; row < m_vaultList->count(); ++row) {
        if (m_vaultList->item(row)->data(Qt::UserRole).toString() == id) {
            m_vaultList->setCurrentRow(row);
            break;
        }
    }
    qCInfo(lcCosmic).noquote() << "vault entry added for" << entryData.site;
}

void SettingsForm::vaultSaveEntry()
{
    if (!m_vault || !m_vault->isUnlocked()) {
        return;
    }
    auto *item = m_vaultList->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("Password vault"),
                             tr("Select an entry in the list first."));
        return;
    }
    VaultEntry entryData = m_vault->entry(item->data(Qt::UserRole).toString());
    if (entryData.id.isEmpty()) {
        return;
    }
    entryData.site = m_vaultSiteEdit->text().trimmed();
    entryData.username = m_vaultUserEdit->text().trimmed();
    entryData.password = m_vaultPassEdit->text();
    entryData.type = m_vaultTypeCombo->currentData().toString();
    entryData.note = m_vaultNoteEdit->text().trimmed();
    if (entryData.site.isEmpty()) {
        QMessageBox::warning(this, tr("Password vault"), tr("The site field is required."));
        return;
    }
    if (!m_vault->updateEntry(entryData)) {
        QMessageBox::warning(this, tr("Password vault"), m_vault->lastError());
        return;
    }
    qCInfo(lcCosmic).noquote() << "vault entry updated for" << entryData.site;
}

void SettingsForm::vaultRemoveEntry()
{
    if (!m_vault || !m_vault->isUnlocked()) {
        return;
    }
    auto *item = m_vaultList->currentItem();
    if (!item) {
        return;
    }
    const QString id = item->data(Qt::UserRole).toString();
    if (!m_vault->removeEntry(id)) {
        QMessageBox::warning(this, tr("Password vault"), m_vault->lastError());
        return;
    }
    qCInfo(lcCosmic).noquote() << "vault entry removed";
}

void SettingsForm::syncCustomTemplateState()
{
    const bool custom =
        m_engineCombo->currentData().toString() == QLatin1String(kCustomId);
    m_customEdit->setEnabled(custom);
    m_customEdit->setToolTip(custom ? tr("Must contain %1 where the query goes.")
                                    : tr("Select “Custom…” to use a custom template."));
}

void SettingsForm::refreshUserWidgets()
{
    if (!m_userCatalog) {
        return;
    }

    const QString previousSelection =
        m_usersList->currentItem() ? m_usersList->currentItem()->data(Qt::UserRole).toString()
                                   : QString();
    m_usersList->clear();
    const BrowseUser builtIn = UserCatalog::defaultUser();
    auto *mainItem = new QListWidgetItem(
        tr("%1 (main profile)").arg(builtIn.name), m_usersList);
    mainItem->setData(Qt::UserRole, builtIn.id);
    mainItem->setForeground(QColor(builtIn.color));
    mainItem->setToolTip(tr("id: %1").arg(builtIn.id));
    mainItem->setFlags(mainItem->flags() & ~Qt::ItemIsSelectable);
    for (const BrowseUser &user : m_userCatalog->users()) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1   %2").arg(user.name, user.color), m_usersList);
        item->setData(Qt::UserRole, user.id);
        item->setForeground(QColor(user.color));
        item->setToolTip(tr("id: %1").arg(user.id));
        if (user.id == previousSelection) {
            m_usersList->setCurrentItem(item);
        }
    }
    const bool hasSelection = m_usersList->currentItem() != nullptr;
    m_renameUserButton->setEnabled(hasSelection);
    m_removeUserButton->setEnabled(hasSelection);
}

void SettingsForm::addUser()
{
    if (!m_userCatalog) {
        return;
    }
    NameColorDialog dialog(tr("Add user"), QString(), QString(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString id = m_userCatalog->addUser(dialog.nameValue(), dialog.colorValue());
    if (!id.isEmpty()) {
        m_userCatalog->save();
        qCInfo(lcCosmic).noquote() << "user added:" << id << dialog.nameValue();
        // Select the new entry (the changed() signal already refreshed the list).
        for (int i = 0; i < m_usersList->count(); ++i) {
            if (m_usersList->item(i)->data(Qt::UserRole).toString() == id) {
                m_usersList->setCurrentRow(i);
                break;
            }
        }
    }
}

void SettingsForm::renameUser()
{
    if (!m_userCatalog || !m_usersList->currentItem()) {
        return;
    }
    const QString id = m_usersList->currentItem()->data(Qt::UserRole).toString();
    const BrowseUser user = m_userCatalog->userById(id);
    NameColorDialog dialog(tr("Rename user"), user.name, user.color,
                           this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (m_userCatalog->renameUser(id, dialog.nameValue())) {
        m_userCatalog->recolorUser(id, dialog.colorValue());
        m_userCatalog->save();
        qCInfo(lcCosmic).noquote() << "user renamed:" << id << dialog.nameValue();
    }
}

void SettingsForm::removeUser()
{
    if (!m_userCatalog || !m_usersList->currentItem()) {
        return;
    }
    const QString id = m_usersList->currentItem()->data(Qt::UserRole).toString();
    if (m_userCatalog->removeUser(id)) {
        m_userCatalog->save();
        qCInfo(lcCosmic).noquote() << "user removed:" << id;
    }
}

void SettingsForm::apply()
{
    m_settings->setDefaultSearchEngine(m_engineCombo->currentData().toString());
    m_settings->setCustomSearchTemplate(m_customEdit->text().trimmed());
    m_settings->setRequestBlockingEnabled(m_blockingCheck->isChecked());
    m_settings->setRestoreSessionOnStartup(m_sessionCheck->isChecked());
    m_settings->setDoNotTrack(m_dntCheck->isChecked());
    m_settings->setSessionCookiesOnly(m_sessionCookiesCheck->isChecked());
    m_settings->setPermissionPolicy(QStringLiteral("location"),
                                    m_locationPermCombo->currentData().toString());
    m_settings->setPermissionPolicy(QStringLiteral("media"),
                                    m_mediaPermCombo->currentData().toString());
    m_settings->setPermissionPolicy(QStringLiteral("notifications"),
                                    m_notificationsPermCombo->currentData().toString());
    m_settings->setHttpsOnly(m_httpsOnlyCheck->isChecked());
    m_settings->setAccentColor(m_accent);
    m_settings->setTabPosition(m_tabPositionCombo->currentData().toString());
    m_settings->save();
    m_statusLabel->setText(tr("All changes saved."));
    qCInfo(lcCosmic).noquote()
        << "settings applied: engine =" << m_settings->defaultSearchEngine()
        << "| blocking =" << m_settings->requestBlockingEnabled()
        << "| restore =" << m_settings->restoreSessionOnStartup()
        << "| dnt =" << m_settings->doNotTrack()
        << "| httpsOnly =" << m_settings->httpsOnly()
        << "| accent =" << m_settings->accentColor()
        << "| tabPosition =" << m_settings->tabPosition();
}

void SettingsForm::resetDefaults()
{
    const auto answer = QMessageBox::question(
        this, tr("Restore default settings"),
        tr("Reset every setting to its built-in default? The password vault "
           "and the users are left untouched."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    m_settings->setDefaultSearchEngine(QStringLiteral("duckduckgo"));
    m_settings->setCustomSearchTemplate(QString());
    m_settings->setRequestBlockingEnabled(true);
    m_settings->setRestoreSessionOnStartup(false);
    m_settings->setDoNotTrack(true);
    m_settings->setSessionCookiesOnly(false);
    m_settings->setPermissionPolicy(QStringLiteral("location"), QStringLiteral("ask"));
    m_settings->setPermissionPolicy(QStringLiteral("media"), QStringLiteral("ask"));
    m_settings->setPermissionPolicy(QStringLiteral("notifications"), QStringLiteral("ask"));
    m_settings->setHttpsOnly(false);
    m_settings->setAccentColor(QStringLiteral("#7c6cf0"));
    m_settings->setTabPosition(QStringLiteral("horizontal"));
    m_settings->save();
    load();
    m_statusLabel->setText(tr("Defaults restored."));
    qCInfo(lcCosmic).noquote() << "settings reset to defaults";
}
