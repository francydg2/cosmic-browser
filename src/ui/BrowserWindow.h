#pragma once

#include "browser/AppWindow.h"
#include "core/Session.h"
#include "web/WebPageDelegate.h"

#include <QMainWindow>
#include <QStringList>
#include <QVector>
#include <vector>

class Browser;
class ActionRegistry;
class UrlBar;
class TabContent;
class SettingsForm;
class QTabWidget;
class QProgressBar;
class QAction;
class QMenu;
class QToolButton;
class QHBoxLayout;
class QWidget;
class QUrl;
struct UIConfiguration;

/// Main browser window: assembles toolbar, tabs, group strip and status
/// bar from the core/web/ui modules. Implements AppWindow (browser/)
/// and WebPageDelegate (web/) — ui/ may depend on everything else.
///
/// Window-scoped state: the owning user (tabs, groups and pins all
/// belong to it), tab groups (chip strip above the tab bar, collapse +
/// color) and pinned tabs (kept at the front, close-button removed).
class BrowserWindow : public QMainWindow, public AppWindow, public WebPageDelegate
{
    Q_OBJECT

public:
    explicit BrowserWindow(Browser *browser, QWidget *parent = nullptr);
    ~BrowserWindow() override;

    static QList<BrowserWindow *> allWindows();

    // AppWindow
    void present() override;
    void openUrl(const QUrl &url, bool background = false) override;
    WebPage *activeWebPage() const override;
    SessionWindow sessionState() const override;
    void restoreSessionState(const SessionWindow &state) override;

    // WebPageDelegate
    WebPage *createPageForWindowType(QWebEnginePage::WebWindowType type) override;
    const PagePolicy *pagePolicy() const override;

    // Introspection (integration tests and menus).
    QTabWidget *tabWidget() const { return m_tabs; }
    TabContent *tabAt(int index) const;
    TabContent *currentTab() const;
    int tabCount() const;
    QString userId() const override { return m_userId; }

    /// Rebuilds the toolbar from Browser::uiConfiguration() (customizer).
    void applyUILayout();

    /// Points this window at another user without swapping tabs (used
    /// for fresh windows before any state is restored into them).
    void setUserId(const QString &userId);

    /// Swaps the whole tab set to another user's remembered tabs,
    /// persisting the current set first (Chrome-style profile switch).
    void switchToUser(const QString &userId);

    // --- pinned tabs -----------------------------------------------------
    void setTabPinned(int index, bool pinned);

    // --- tab groups (window-scoped) --------------------------------------
    QList<SessionGroup> groups() const { return m_groups; }
    SessionGroup groupById(const QString &id) const;
    /// Creates a group; returns its id (empty on invalid name).
    QString createGroup(const QString &name, const QString &color);
    /// Assigns tab \p index to a group; "" removes it from its group.
    /// A group that loses its last tab is deleted.
    bool assignTabToGroup(int index, const QString &groupId);
    void renameGroup(const QString &groupId, const QString &name);
    void setGroupColor(const QString &groupId, const QString &color);
    void setGroupCollapsed(const QString &groupId, bool collapsed);
    void deleteGroup(const QString &groupId);
    int groupTabCount(const QString &groupId) const;

private:
    TabContent *addTab(const QUrl &url, bool background, int insertAt = -1,
                       const QString &userId = QString(), bool pinned = false,
                       const QString &groupId = QString());
    void closeTab(int index);
    void reopenClosedTab();
    void pruneGroupIfEmpty(const QString &groupId);
    void navigateCurrent(const QUrl &url);
    void handleInput(const QString &text);
    void openSettingsTab(const QString &section = QString());
    void confirmClearBrowsingData();
    void handleFeaturePermissionAsk(WebPage *page, const QUrl &origin,
                                    QWebEnginePage::Feature feature);
    void updateChrome();
    void wireTab(TabContent *content);

    void handleBookmarkOpen(const QUrl &url, bool newTab);
    void addToolbarComponent(const QString &component);
    QString urlBarStyleSheet(const UIConfiguration &cfg) const;
    QString tabWidgetStyleSheet(const UIConfiguration &cfg) const;
    void buildBookmarksMenu();
    void buildAiMenu();
    void buildRamMenu();
    void setAiPageContext(bool enabled);
    void openAiService(const QUrl &url);
    void applyTabPosition();
    void discardBackgroundTabs();
    void tryAutofill(WebPage *page);
    void savePasswordForCurrentPage();
    void promptSavePassword(const QUrl &url, const QString &username,
                            const QString &password);

    // groups / pin presentation
    void removeDynamicActions(const QString &idPrefix);
    void rebuildGroupActions();
    void updateGroupStrip();
    void applyGroupAppearance();
    void refreshTabColors();
    void showTabContextMenu(const QPoint &pos);
    void buildUserMenu();
    bool newGroupFromCurrentTab();

    Browser *m_browser; // not owned (application scope)
    ActionRegistry *m_actions;
    QStringList m_dynamicActionIds;
    QTabWidget *m_tabs = nullptr;
    QToolBar *m_toolbar = nullptr;
    QList<QAction *> m_toolbarActions;   // registry actions, removed (never deleted)
    QList<QWidget *> m_toolbarWidgets;  // owned items, deleted on rebuild
    QToolButton *m_bookmarksButton = nullptr;
    QToolButton *m_aiButton = nullptr;
    QToolButton *m_aiEyeButton = nullptr;
    QToolButton *m_ramButton = nullptr;
    QToolButton *m_keyButton = nullptr;
    bool m_aiPageContext = false;
    QWidget *m_groupStrip = nullptr;
    QHBoxLayout *m_groupStripLayout = nullptr;
    UrlBar *m_urlBar = nullptr;
    QProgressBar *m_progress = nullptr;
    QAction *m_reloadAction = nullptr;
    QToolButton *m_userButton = nullptr;
    QMenu *m_userMenu = nullptr;

    QString m_userId; // owning user of this window's tabs
    QList<SessionGroup> m_groups;

    struct ClosedTab {
        QUrl url;
        QString title;
        int index = 0;
        QString userId;
        bool pinned = false;
        QString groupId;
    };
    std::vector<ClosedTab> m_closedTabs;
};
