#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>

class BlocklistInterceptor;
class Browser;
class UserCatalog;
class PasswordVault;
class Settings;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTabWidget;
class QToolButton;

/// Settings page content (Chrome-style: hosted in a tab, not a dialog):
/// General (session restore), Search engine, Appearance
/// (accent, tab position), Privacy (blocking, cookies, permissions),
/// Passwords (encrypted vault), Users, Advanced
/// (HTTPS-only, clear data, restore defaults). load() reads Settings into
/// the widgets; apply() writes back and saves. Vault and users mutate
/// their stores immediately (other windows live-update via signals).
class SettingsForm : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsForm(Settings *settings, BlocklistInterceptor *interceptor,
                          UserCatalog *catalog, PasswordVault *vault = nullptr,
                          Browser *browser = nullptr, QWidget *parent = nullptr);

    void load();
    void apply();
    void resetDefaults();

    void showSection(const QString &name);

signals:
    void clearBrowsingDataRequested();

private:
    void syncCustomTemplateState();
    void refreshUserWidgets();
    void addUser();
    void renameUser();
    void removeUser();

    QWidget *buildHttpsPage(QTabWidget *tabs);
    QWidget *buildAppearancePage(QTabWidget *tabs);
    QWidget *buildAdvancedPage(QTabWidget *tabs);
    QWidget *buildPasswordsPage(QTabWidget *tabs);

    void selectAccent(const QString &hex);
    void refreshVaultWidgets();
    void vaultMasterButtonClicked();
    void vaultSelectionChanged();
    void vaultAddEntry();
    void vaultSaveEntry();
    void vaultRemoveEntry();

    Settings *m_settings;
    BlocklistInterceptor *m_interceptor;
    UserCatalog *m_userCatalog;
    PasswordVault *m_vault = nullptr;
    Browser *m_browser = nullptr;

    QTabWidget *m_sections = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_resetButton = nullptr;

    QComboBox *m_engineCombo = nullptr;
    QLineEdit *m_customEdit = nullptr;
    QCheckBox *m_blockingCheck = nullptr;
    QLabel *m_blockedLabel = nullptr;
    QCheckBox *m_sessionCheck = nullptr;
    QCheckBox *m_dntCheck = nullptr;
    QCheckBox *m_sessionCookiesCheck = nullptr;
    QComboBox *m_locationPermCombo = nullptr;
    QComboBox *m_mediaPermCombo = nullptr;
    QComboBox *m_notificationsPermCombo = nullptr;
    QPushButton *m_clearDataButton = nullptr;
    QCheckBox *m_httpsOnlyCheck = nullptr;
    QString m_accent;
    QVector<QPair<QToolButton *, QString>> m_accentSwatches;
    QComboBox *m_tabPositionCombo = nullptr;

    QLabel *m_vaultStatusLabel = nullptr;
    QLineEdit *m_vaultMasterEdit = nullptr;
    QPushButton *m_vaultMasterButton = nullptr;
    QWidget *m_vaultPanel = nullptr;
    QListWidget *m_vaultList = nullptr;
    QLineEdit *m_vaultSiteEdit = nullptr;
    QLineEdit *m_vaultUserEdit = nullptr;
    QLineEdit *m_vaultPassEdit = nullptr;
    QCheckBox *m_vaultRevealCheck = nullptr;
    QComboBox *m_vaultTypeCombo = nullptr;
    QLineEdit *m_vaultNoteEdit = nullptr;
    QPushButton *m_vaultAddButton = nullptr;
    QPushButton *m_vaultSaveButton = nullptr;
    QPushButton *m_vaultRemoveButton = nullptr;
    QPushButton *m_vaultLockButton = nullptr;

    QListWidget *m_usersList = nullptr;
    QPushButton *m_addUserButton = nullptr;
    QPushButton *m_renameUserButton = nullptr;
    QPushButton *m_removeUserButton = nullptr;
};
