#pragma once

#include "browser/AppWindow.h"
#include "core/AiPrompt.h"
#include "core/Session.h"
#include "core/TabAnalysis.h"
#include "ui/TabContent.h"
#include "web/WebPage.h"
#include "web/WebPageDelegate.h"

#include <QMainWindow>
#include <QHash>
#include <QPointer>
#include <QStringList>
#include <QVector>
#include <functional>
#include <vector>

class Browser;
class ActionRegistry;
class UrlBar;
class TabContent;
class SettingsForm;
class DownloadsDialog;
class NotesDialog;
class TrackerRadarDialog;
class Toast;
class TabManagerDialog;
class PrivacyDashboardDialog;
class PerformanceDialog;
class FocusDialog;
class FocusPill;
class AiSidebar;
class PageTranslator;
class QDockWidget;
class QWebEngineView;
class QTabWidget;
class QProgressBar;
class QDialog;
class QAction;
class QMenu;
class QToolButton;
class QHBoxLayout;
class QShortcut;
class QSplitter;
class QLineEdit;
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
    friend class TabManagerDialog;
    friend class PerformanceDialog;
    friend class FocusDialog;

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
    Toast *toast() const { return m_toast; }

    /// Shows a transient notification in this window (kind: info,
    /// success, warning, error).
    void notify(const QString &message, const QString &kind = QStringLiteral("info"));

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
    void goHome();
    void handleInput(const QString &text);
    void toggleBookmarkForCurrentPage();
    void showSiteInfo();
    void openSettingsTab(const QString &section = QString());
    void showDownloads();
    void showNotes();
    void showTrackerRadar();
    void showTabSearch();

    // Cosmic 0.2.0 tools (implemented in BrowserWindowTools.cpp)
    void showTabManager();
    void showPrivacyDashboard();
    void showPerformanceCenter();
    void showFocusDialog();
    void applyFocusChrome(bool active);
    void applyFocusWorkspace(const QString &name);
    QStringList workspaceNames() const;
    void toggleWebInspector();
    void bindInspector();
    QList<TabInfo> tabInfos() const;
    /// Discards a background tab's page to free memory (it reloads when
    /// shown again). False when the tab cannot be suspended safely.
    bool suspendTab(int index);
    void toggleAiSidebar();
    void fetchPageContext(TabContent *tab, std::function<void(Ai::PageContext)> done);
    void buildToolsMenuEntries(QMenu *menu);
    void showWorkspaces();
    void openWorkspace(const SessionWindow &state);
    void showReader();
    void translateCurrentPage();
    void translateWithGoogle();
    void translateSelection();
    void maybeAutoTranslate(WebPage *page);
    void updateTranslationIndicator();
    PageTranslator *translator();
    void showPageQrCode();
    void toggleSplitView();
    void closeSplitView();
    TabContent *focusedTab() const;
    void confirmClearBrowsingData();
    void handleFeaturePermissionAsk(WebPage *page, const QUrl &origin,
                                    QWebEnginePage::Feature feature);
    void updateChrome();
    void updateNavigationActions();
    void wireTab(TabContent *content);
    void leaveWebFullScreen();
    void restoreAfterWebFullScreen();

    void handleBookmarkOpen(const QUrl &url, bool newTab);
    void addToolbarComponent(const QString &component);
    QString urlBarStyleSheet(const UIConfiguration &cfg) const;
    QString tabWidgetStyleSheet(const UIConfiguration &cfg) const;
    void buildBookmarksMenu();
    void buildAiMenu();
    void buildRamMenu();
    void applyTabPosition();
    void discardBackgroundTabs();
    void tryAutofill(WebPage *page);
    void savePasswordForCurrentPage();
    void savePasswordForPage(WebPage *page);
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
    QSplitter *m_contentSplitter = nullptr;
    QPointer<QWidget> m_splitPanel;
    QPointer<QWidget> m_splitControls;
    QPointer<TabContent> m_splitContent;
    QPointer<QLineEdit> m_splitAddress;
    bool m_splitFocused = false;
    QToolBar *m_toolbar = nullptr;
    QList<QAction *> m_toolbarActions;   // registry actions, removed (never deleted)
    QList<QWidget *> m_toolbarWidgets;  // owned items, deleted on rebuild
    QToolButton *m_bookmarksButton = nullptr;
    QToolButton *m_aiButton = nullptr;
    QToolButton *m_ramButton = nullptr;
    QToolButton *m_keyButton = nullptr;
    QWidget *m_groupStrip = nullptr;
    QHBoxLayout *m_groupStripLayout = nullptr;
    UrlBar *m_urlBar = nullptr;
    QProgressBar *m_progress = nullptr;
    QAction *m_reloadAction = nullptr;
    QToolButton *m_userButton = nullptr;
    QMenu *m_userMenu = nullptr;
    QPointer<DownloadsDialog> m_downloadsDialog;
    QPointer<NotesDialog> m_notesDialog;
    QPointer<TrackerRadarDialog> m_trackerRadarDialog;
    QPointer<QDialog> m_tabSearchDialog;
    QPointer<TabManagerDialog> m_tabManager;
    QPointer<PrivacyDashboardDialog> m_privacyDashboard;
    QPointer<PerformanceDialog> m_performanceDialog;
    QPointer<FocusDialog> m_focusDialog;
    FocusPill *m_focusPill = nullptr;
    QHash<QWidget *, bool> m_focusHidden;
    QPointer<QDockWidget> m_inspectorDock;
    QPointer<QWebEngineView> m_inspectorView;
    QPointer<WebPage> m_inspectedPage;
    Toast *m_toast = nullptr;
    QPointer<QDockWidget> m_aiDock;
    AiSidebar *m_aiSidebar = nullptr;
    PageTranslator *m_translator = nullptr;
    QToolButton *m_translateIndicator = nullptr;
    QPointer<WebPage> m_fullScreenPage;
    QShortcut *m_exitFullScreenShortcut = nullptr;
    Qt::WindowStates m_beforeFullScreenState;
    bool m_toolbarWasHidden = false;
    bool m_tabBarWasHidden = false;
    bool m_statusBarWasHidden = false;
    bool m_splitPanelWasHidden = false;
    bool m_tabsWereHidden = false;
    bool m_splitControlsWereHidden = false;

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
