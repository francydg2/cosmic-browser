#include "ui/BrowserWindow.h"

#include "browser/Browser.h"
#include "core/ActionRegistry.h"
#include "core/Bookmarks.h"
#include "core/UserCatalog.h"
#include "core/Logging.h"
#include "core/PasswordVault.h"
#include "core/Settings.h"
#include "ui/NameColorDialog.h"
#include "ui/NewTabPage.h"
#include "ui/SettingsForm.h"
#include "ui/Style.h"
#include "ui/TabContent.h"
#include "ui/TabWidget.h"
#include "ui/UrlBar.h"
#include "web/BlocklistInterceptor.h"
#include "web/WebPage.h"
#include "web/WebView.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QResizeEvent>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWebEngineProfile>

namespace {

const auto kDefaultId = "default";
const auto kSettingsUrl = "cosmic://settings";

const QString kExtractPasswordScript = QStringLiteral(R"JS(
(function () {
  var out = { username: "", password: "", url: location.href, hasField: false };
  var pws = document.querySelectorAll('input[type="password"]');
  if (!pws.length) { return JSON.stringify(out); }
  out.hasField = true;
  var pw = null;
  for (var i = 0; i < pws.length; i++) {
    if (!pws[i].disabled && !pws[i].readOnly) { pw = pws[i]; break; }
  }
  if (!pw) { return JSON.stringify(out); }
  out.password = pw.value;
  var scope = pw.form || document;
  var cands = scope.querySelectorAll(
      'input[type="text"],input[type="email"],input[type="tel"],input:not([type])');
  for (var j = 0; j < cands.length; j++) {
    var c = cands[j];
    if (c !== pw && !c.disabled) {
      out.username = c.value || out.username;
      if (out.username) { break; }
    }
  }
  return JSON.stringify(out);
})()
)JS");

const QString kAutofillScript = QStringLiteral(R"JS(
(function (a) {
  var pws = document.querySelectorAll('input[type="password"]');
  if (!pws.length) { return 'no-field'; }
  var pw = null;
  for (var i = 0; i < pws.length; i++) {
    if (!pws[i].disabled && !pws[i].readOnly) { pw = pws[i]; break; }
  }
  if (!pw) { return 'no-field'; }
  var scope = pw.form || document;
  var cands = scope.querySelectorAll(
      'input[type="text"],input[type="email"],input[type="tel"],input:not([type])');
  var uf = null;
  for (var j = 0; j < cands.length; j++) {
    if (cands[j] !== pw && !cands[j].disabled) { uf = cands[j]; break; }
  }
  if (uf && !uf.value && a[0]) {
    uf.value = a[0];
    uf.dispatchEvent(new Event('input', { bubbles: true }));
    uf.dispatchEvent(new Event('change', { bubbles: true }));
  }
  if (!pw.value && a[1]) {
    pw.value = a[1];
    pw.dispatchEvent(new Event('input', { bubbles: true }));
    pw.dispatchEvent(new Event('change', { bubbles: true }));
  }
  return 'filled';
})JS");

const QString kExtractPageContextScript = QStringLiteral(R"JS(
(function () {
  var b = document.body ? document.body.innerText : '';
  b = b.replace(/\s+/g, ' ').slice(0, 2000);
  return JSON.stringify({ url: location.href, excerpt: b });
})()
)JS");

QList<QPointer<BrowserWindow>> g_windows;

} // namespace

QList<BrowserWindow *> BrowserWindow::allWindows()
{
    QList<BrowserWindow *> alive;
    g_windows.removeIf(
        [](const QPointer<BrowserWindow> &pointer) { return pointer.isNull(); });
    for (const QPointer<BrowserWindow> &pointer : std::as_const(g_windows)) {
        alive.append(pointer.data());
    }
    return alive;
}

BrowserWindow::~BrowserWindow()
{
    g_windows.removeIf(
        [this](const QPointer<BrowserWindow> &pointer) { return pointer.data() == this; });
}

BrowserWindow::BrowserWindow(Browser *browser, QWidget *parent)
    : QMainWindow(parent)
    , m_browser(browser)
    , m_actions(new ActionRegistry(this))
{
    g_windows.append(QPointer<BrowserWindow>(this));
    setObjectName(QStringLiteral("browserWindow"));
    setWindowTitle(tr("Cosmic"));

    m_userId = QString::fromLatin1(kDefaultId);

    // --- tab strip + group strip ------------------------------------------
    m_tabs = new NebulaTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("tabWidget"));
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true); // drag & drop reordering (spec §6)
    m_tabs->setDocumentMode(true);
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->setUsesScrollButtons(true);

    m_groupStrip = new QWidget(this);
    m_groupStrip->setObjectName(QStringLiteral("groupStrip"));
    m_groupStripLayout = new QHBoxLayout(m_groupStrip);
    m_groupStripLayout->setContentsMargins(10, 5, 10, 3);
    m_groupStripLayout->setSpacing(6);
    m_groupStrip->setVisible(false);

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_groupStrip);
    centralLayout->addWidget(m_tabs);
    setCentralWidget(central);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &BrowserWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, &BrowserWindow::updateChrome);
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabs->tabBar(), &QTabBar::customContextMenuRequested, this,
            &BrowserWindow::showTabContextMenu);

    // --- status bar / load progress ---------------------------------------
    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("loadProgress"));
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(false);
    m_progress->setFixedWidth(180);
    m_progress->setVisible(false);
    statusBar()->addWidget(m_progress, 1);

    // --- actions (all shortcuts registered here, never in widgets) --------
    m_actions->define(QStringLiteral("nav.back"), tr("Back"), QKeySequence::Back, [this] {
        if (auto *tab = currentTab()) {
            tab->webView()->back();
        }
    });
    m_actions->define(QStringLiteral("nav.forward"), tr("Forward"), QKeySequence::Forward, [this] {
        if (auto *tab = currentTab()) {
            tab->webView()->forward();
        }
    });
    m_reloadAction = m_actions->define(QStringLiteral("nav.reload"), tr("Reload"),
                                       QKeySequence::Refresh, [this] {
        auto *tab = currentTab();
        if (!tab) {
            return;
        }
        if (tab->webView()->webPage()->isLoading()) {
            tab->webView()->stop();
        } else {
            tab->webView()->reload();
        }
    });
    m_actions->define(QStringLiteral("tab.new"), tr("New Tab"), QKeySequence(Qt::CTRL | Qt::Key_T),
                      [this] {
        auto *tab = addTab(QUrl(), false);
        if (tab) {
            tab->newTabPage()->focusSearchField();
        }
    });
    m_actions->define(QStringLiteral("tab.close"), tr("Close Tab"), QKeySequence(Qt::CTRL | Qt::Key_W),
                      [this] {
        if (m_tabs->count() > 0) {
            closeTab(m_tabs->currentIndex());
        }
    });
    m_actions->define(QStringLiteral("tab.reopen"), tr("Reopen Closed Tab"),
                      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T), [this] {
        reopenClosedTab();
    });
    m_actions->define(QStringLiteral("tab.next"), tr("Next Tab"), QKeySequence(Qt::CTRL | Qt::Key_Tab),
                      [this] {
        const int count = m_tabs->count();
        if (count > 0) {
            m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % count);
        }
    });
    m_actions->define(QStringLiteral("tab.prev"), tr("Previous Tab"),
                      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), [this] {
        const int count = m_tabs->count();
        if (count > 0) {
            m_tabs->setCurrentIndex((m_tabs->currentIndex() - 1 + count) % count);
        }
    });
    m_actions->define(QStringLiteral("tab.pin"), tr("Pin Tab"),
                      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P), [this] {
        const int index = m_tabs->currentIndex();
        if (TabContent *tab = currentTab()) {
            setTabPinned(index, !tab->isPinned());
        }
    });
    m_actions->define(QStringLiteral("tab.group.new"), tr("New Group from Tab…"), {},
                      [this] { newGroupFromCurrentTab(); });
    m_actions->define(QStringLiteral("tab.group.rename"), tr("Rename Group…"), {},
                      [this] {
        TabContent *tab = currentTab();
        if (!tab) {
            return;
        }
        const SessionGroup group = groupById(tab->groupId());
        if (group.id.isEmpty()) {
            return;
        }
        NameColorDialog dialog(tr("Rename tab group"), group.name, group.color, this);
        if (dialog.exec() == QDialog::Accepted) {
            renameGroup(group.id, dialog.nameValue());
            setGroupColor(group.id, dialog.colorValue());
        }
    });
    m_actions->define(QStringLiteral("tab.group.remove"), tr("Remove from Group"), {},
                      [this] { assignTabToGroup(m_tabs->currentIndex(), QString()); });
    m_actions->define(QStringLiteral("tab.group.delete"), tr("Delete Group"), {}, [this] {
        if (TabContent *tab = currentTab()) {
            deleteGroup(tab->groupId());
        }
    });
    for (int i = 0; i < NameColorDialog::presetColors().size(); ++i) {
        m_actions->define(QStringLiteral("tab.group.color.") + QString::number(i),
                          tr("Group color %1").arg(i + 1), {}, [this, i] {
                              if (TabContent *tab = currentTab()) {
                                  const SessionGroup group = groupById(tab->groupId());
                                  if (!group.id.isEmpty()) {
                                      setGroupColor(group.id,
                                                    NameColorDialog::presetColors().at(i));
                                  }
                              }
                          });
    }
    m_actions->define(QStringLiteral("urlbar.focus"), tr("Focus Address Bar"),
                      QKeySequence(Qt::CTRL | Qt::Key_L), [this] {
        m_urlBar->setFocus();
    });
    m_actions->define(QStringLiteral("window.new"), tr("New Tab"),
                      QKeySequence(Qt::CTRL | Qt::Key_N), [this] {
        // Single window: "new window" folds into a new tab here.
        if (TabContent *tab = addTab(QUrl(), false)) {
            tab->newTabPage()->focusSearchField();
        }
        present();
    });
    m_actions->define(QStringLiteral("settings.open"), tr("Settings"),
                      QKeySequence(Qt::CTRL | Qt::Key_Comma), [this] {
        openSettingsTab();
    });
    m_actions->define(QStringLiteral("settings.users"), tr("Users…"), {},
                      [this] { openSettingsTab(QStringLiteral("users")); });
    m_actions->define(QStringLiteral("bookmark.add"), tr("Bookmark This Page…"),
                      QKeySequence(Qt::CTRL | Qt::Key_D), [this] {
        if (!m_browser) {
            return;
        }
        TabContent *tab = currentTab();
        if (!tab) {
            return;
        }
        WebPage *page = tab->webView()->webPage();
        const QUrl url = page->url();
        if (!url.isValid() || url.isEmpty() || url.isRelative()
            || url.scheme() == QLatin1String("about")) {
            return;
        }
        if (m_browser->bookmarks()->contains(url)) {
            statusBar()->showMessage(tr("Already bookmarked."), 3000);
            return;
        }
        bool ok = false;
        const QString title = QInputDialog::getText(
            this, tr("Bookmark This Page"), tr("Name:"), QLineEdit::Normal,
            page->title(), &ok);
        if (ok) {
            m_browser->bookmarks()->add(title, url);
        }
    });
    m_actions->define(QStringLiteral("vault.savePassword"), tr("Save Password to Vault…"),
                      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), [this] {
        savePasswordForCurrentPage();
    });
    QAction *aiContextAction = m_actions->define(
        QStringLiteral("ai.pageContext"), tr("Let the AI read this page"), {}, [this] {
            setAiPageContext(!m_aiPageContext);
        });
    aiContextAction->setCheckable(true);
    struct AiService {
        const char *id;
        const char *name;
        const char *url;
    };
    const AiService aiServices[] = {
        {"ai.open.chatgpt", "ChatGPT", "https://chatgpt.com"},
        {"ai.open.claude", "Claude", "https://claude.ai"},
        {"ai.open.gemini", "Gemini", "https://gemini.google.com"},
        {"ai.open.perplexity", "Perplexity", "https://www.perplexity.ai"},
        {"ai.open.copilot", "Copilot", "https://copilot.microsoft.com"},
    };
    for (const AiService &service : aiServices) {
        const QUrl url = QUrl(QString::fromUtf8(service.url));
        m_actions->define(QString::fromUtf8(service.id), QString::fromUtf8(service.name),
                          {}, [this, url] { openAiService(url); });
    }
    QAction *ramSaverAction = m_actions->define(
        QStringLiteral("performance.ramSaver"), tr("RAM Saver (discard background tabs)"),
        {}, [this] {
            if (m_browser) {
                m_browser->settings()->setRamSaverEnabled(!m_browser->settings()->ramSaverEnabled());
            }
        });
    ramSaverAction->setCheckable(true);
    m_actions->define(QStringLiteral("performance.discardNow"),
                      tr("Free RAM now (discard background tabs)"), {}, [this] {
        discardBackgroundTabs();
    });
    m_actions->define(QStringLiteral("app.quit"), tr("Quit"), QKeySequence::Quit, [this] {
        Q_UNUSED(this);
        qApp->quit();
    });

    // Window-level association makes the shortcuts active (Qt::WindowShortcut).
    const auto actions = m_actions->allActions();
    for (QAction *action : actions) {
        addAction(action);
    }

    rebuildGroupActions();

    // --- toolbar ----------------------------------------------------------
    // Local SVG line icons (action → icon, not widget).
    m_actions->action(QStringLiteral("nav.back"))->setIcon(QIcon(QStringLiteral(":/icons/back.svg")));
    m_actions->action(QStringLiteral("nav.forward"))
        ->setIcon(QIcon(QStringLiteral(":/icons/forward.svg")));
    m_reloadAction->setIcon(QIcon(QStringLiteral(":/icons/reload.svg")));

    QToolBar *toolbar = addToolBar(tr("Navigation"));
    toolbar->setObjectName(QStringLiteral("navToolbar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setIconSize(QSize(20, 20));
    toolbar->setContentsMargins(10, 6, 10, 6);
    m_toolbar = toolbar;

    // --- user button (address bar companion, re-added on rebuild) ---------
    m_userButton = new QToolButton(toolbar);
    m_userButton->setObjectName(QStringLiteral("userButton"));
    m_userButton->setAutoRaise(true);
    m_userButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_userButton->setPopupMode(QToolButton::InstantPopup);
    m_userButton->setIcon(QIcon(QStringLiteral(":/icons/layers.svg")));
    m_userButton->setFocusPolicy(Qt::NoFocus);
    m_userMenu = new QMenu(m_userButton);
    m_userButton->setMenu(m_userMenu);
    connect(m_userMenu, &QMenu::aboutToShow, this, &BrowserWindow::buildUserMenu);

    m_urlBar = new UrlBar(this);
    m_urlBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_urlBar, &UrlBar::inputSubmitted, this, &BrowserWindow::handleInput);

    applyUILayout();

    if (m_browser) {
        ramSaverAction->setChecked(m_browser->settings()->ramSaverEnabled());
        connect(m_browser->settings(), &Settings::ramSaverChanged, this, [this](bool on) {
            m_actions->action(QStringLiteral("performance.ramSaver"))->setChecked(on);
            if (on) {
                discardBackgroundTabs();
            }
        });
        connect(m_browser->settings(), &Settings::tabPositionChanged, this,
                &BrowserWindow::applyTabPosition);
        connect(m_browser->settings(), &Settings::accentColorChanged, this,
                &BrowserWindow::applyUILayout);
    }

    // --- "+" painted after the last tab by the tab bar itself ------------
    if (auto *nebulaBar = qobject_cast<NebulaTabBar *>(m_tabs->tabBar())) {
        connect(nebulaBar, &NebulaTabBar::newTabRequested, this, [this] {
            m_actions->action(QStringLiteral("tab.new"))->trigger();
        });
    }
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        if (m_browser && m_browser->settings()->ramSaverEnabled()) {
            discardBackgroundTabs();
        }
    });

    // --- initial tab (New Tab page) ---------------------------------------
    addTab(QUrl(), false);

    applyTabPosition();
    resize(1280, 800);
    updateChrome();
}

// ---------------------------------------------------------------------------
// AppWindow
// ---------------------------------------------------------------------------

void BrowserWindow::present()
{
    show();
    raise();
    activateWindow();
}

void BrowserWindow::openUrl(const QUrl &url, bool background)
{
    if (url.toString() == QLatin1String(kSettingsUrl)) {
        openSettingsTab();
        return;
    }
    // A pristine New Tab page (first tab, never navigated) is reused
    // instead of spawning an extra tab — this is what opening a URL from
    // the command line should do.
    if (m_tabs->count() == 1) {
        TabContent *tab = tabAt(0);
        if (tab && tab->isShowingNewTab()) {
            navigateCurrent(url);
            return;
        }
    }
    addTab(url, background);
}

WebPage *BrowserWindow::activeWebPage() const
{
    const TabContent *tab = currentTab();
    return tab ? tab->webView()->webPage() : nullptr;
}

SessionWindow BrowserWindow::sessionState() const
{
    SessionWindow state;
    state.user = m_userId;
    state.groups = m_groups;
    for (int i = 0; i < m_tabs->count(); ++i) {
        const TabContent *tab = tabAt(i);
        if (!tab) {
            continue;
        }
        SessionTab tabState;
        const QUrl url = tab->webView()->webPage()->url();
        const bool navigable = url.isValid() && !url.isEmpty()
            && url.toString() != QLatin1String("about:blank");
        tabState.url = navigable ? url : QUrl();
        tabState.pinned = tab->isPinned();
        tabState.group = tab->groupId();
        tabState.user = tab->userId();
        state.tabs.append(tabState);
    }
    return state;
}

void BrowserWindow::restoreSessionState(const SessionWindow &state)
{
    QString windowUser = state.user;
    if (m_browser && m_browser->userCatalog()
        && !m_browser->userCatalog()->hasUser(windowUser)) {
        windowUser = QString::fromLatin1(kDefaultId);
    }
    m_userId = windowUser;

    m_groups.clear();
    for (const SessionGroup &group : state.groups) {
        if (group.id.isEmpty() || !groupById(group.id).id.isEmpty()) {
            continue;
        }
        m_groups.append(group);
    }

    TabContent *pristine = nullptr;
    if (m_tabs->count() == 1) {
        TabContent *first = tabAt(0);
        if (first && first->isShowingNewTab()) {
            pristine = first;
        }
    }

    bool pristineUsed = false;
    bool first = true;
    for (const SessionTab &tabState : state.tabs) {
        const QString effectiveUser =
            tabState.user.isEmpty() ? windowUser : tabState.user;
        const bool navigable = tabState.url.isValid() && !tabState.url.isEmpty();

        if (first) {
            first = false;
            if (pristine && pristine->userId() == effectiveUser) {
                pristineUsed = true;
                if (navigable) {
                    navigateCurrent(tabState.url);
                }
                if (tabState.pinned) {
                    setTabPinned(0, true);
                }
                if (!tabState.group.isEmpty()) {
                    assignTabToGroup(m_tabs->indexOf(pristine), tabState.group);
                }
                continue;
            }
        }
        addTab(navigable ? tabState.url : QUrl(), false, -1, effectiveUser,
               tabState.pinned, tabState.group);
    }

    if (pristine && !pristineUsed && !state.tabs.isEmpty()) {
        m_tabs->removeTab(0);
        pristine->deleteLater();
    }

    rebuildGroupActions();
    updateGroupStrip();
    applyGroupAppearance();
    updateChrome();
}

// ---------------------------------------------------------------------------
// WebPageDelegate
// ---------------------------------------------------------------------------

WebPage *BrowserWindow::createPageForWindowType(QWebEnginePage::WebWindowType type)
{
    switch (type) {
    case QWebEnginePage::WebBrowserTab: {
        auto *tab = addTab(QUrl(), false);
        return tab ? tab->webView()->webPage() : nullptr;
    }
    case QWebEnginePage::WebBrowserBackgroundTab: {
        auto *tab = addTab(QUrl(), true);
        return tab ? tab->webView()->webPage() : nullptr;
    }
    case QWebEnginePage::WebBrowserWindow:
    case QWebEnginePage::WebDialog: {
        // Qt WebEngine has no real popup windows (WebDialog), and Cosmic
        // is single-window: popups open as tabs instead. Documented in
        // docs/ARCHITECTURE.md.
        TabContent *tab = addTab(QUrl(), false);
        return tab ? tab->webView()->webPage() : nullptr;
    }
    default:
        return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------

TabContent *BrowserWindow::tabAt(int index) const
{
    return qobject_cast<TabContent *>(m_tabs->widget(index));
}

TabContent *BrowserWindow::currentTab() const
{
    return tabAt(m_tabs->currentIndex());
}

int BrowserWindow::tabCount() const
{
    return m_tabs->count();
}

void BrowserWindow::setUserId(const QString &userId)
{
    m_userId = userId.isEmpty() ? QString::fromLatin1(kDefaultId) : userId;
    updateChrome();
}

void BrowserWindow::switchToUser(const QString &userId)
{
    if (!m_browser || userId.isEmpty()) {
        return;
    }
    const QString target =
        (userId == QLatin1String(kDefaultId) || m_browser->userCatalog()->hasUser(userId))
        ? userId
        : QString::fromLatin1(kDefaultId);
    if (target == m_userId) {
        return;
    }
    m_browser->storeUserSession(m_userId, sessionState());

    for (int i = 0; i < m_tabs->count(); ++i) {
        if (TabContent *tab = tabAt(i)) {
            if (tab->isPinned()) {
                tab->deleteCloseButton();
            }
        }
    }
    while (m_tabs->count() > 0) {
        QWidget *page = m_tabs->widget(0);
        m_tabs->removeTab(0);
        if (page) {
            page->deleteLater();
        }
    }
    m_groups.clear();
    rebuildGroupActions();
    m_userId = target;
    m_browser->setCurrentUserId(target);

    const SessionWindow stored = m_browser->userSession(target);
    SessionWindow state = stored;
    state.user = target;
    if (state.tabs.isEmpty() && state.groups.isEmpty()) {
        addTab(QUrl(), false);
    } else {
        restoreSessionState(state);
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    updateGroupStrip();
    applyGroupAppearance();
    updateChrome();
}

TabContent *BrowserWindow::addTab(const QUrl &url, bool background, int insertAt,
                                  const QString &userId, bool pinned, const QString &groupId)
{
    const QString effectiveUser = userId.isEmpty() ? m_userId : userId;
    QWebEngineProfile *profile =
        m_browser ? m_browser->profileForUser(effectiveUser)
                  : QWebEngineProfile::defaultProfile();

    auto *content = new TabContent(this, profile, effectiveUser, m_tabs);
    TabContent *previousTab = background ? currentTab() : nullptr;

    int index = -1;
    if (insertAt >= 0 && insertAt <= m_tabs->count()) {
        index = m_tabs->insertTab(insertAt, content, tr("New Tab"));
    } else {
        index = m_tabs->addTab(content, tr("New Tab"));
    }
    if (index < 0) {
        delete content;
        return nullptr;
    }

    wireTab(content);

    if (!groupId.isEmpty() && !groupById(groupId).id.isEmpty()) {
        content->setGroupId(groupId);
    }
    if (pinned) {
        setTabPinned(index, true);
        index = m_tabs->indexOf(content);
    }

    if (previousTab) {
        m_tabs->setCurrentWidget(previousTab);
    } else {
        m_tabs->setCurrentIndex(index);
    }

    if (url.isValid() && !url.isEmpty()) {
        content->webView()->webPage()->setUrl(url);
    }

    applyGroupAppearance();
    updateChrome();
    return content;
}

void BrowserWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabs->count()) {
        return;
    }
    auto *content = tabAt(index);
    if (!content) {
        // Native tabs (settings page): no web state to record, just drop.
        // A lone settings tab becomes a fresh New Tab page (never an
        // empty window).
        if (QWidget *page = m_tabs->widget(index)) {
            m_tabs->removeTab(index);
            page->deleteLater();
        }
        if (m_tabs->count() == 0) {
            addTab(QUrl(), false);
        }
        updateChrome();
        return;
    }

    const QUrl url = content->webView()->webPage()->url();
    if (url.isValid() && !url.isEmpty() && url.toString() != QLatin1String("about:blank")) {
        m_closedTabs.push_back({url, m_tabs->tabText(index), index, content->userId(),
                                content->isPinned(), content->groupId()});
    }

    const QString groupId = content->groupId();
    if (m_tabs->count() == 1) {
        // Last tab: reset to a fresh New Tab page instead of closing the
        // window (Chrome-like), so navigation and the session stay alive.
        // The closed page was already recorded above for Reopen.
        if (content->isPinned()) {
            setTabPinned(0, false);
        }
        if (!groupId.isEmpty()) {
            assignTabToGroup(0, QString());
            pruneGroupIfEmpty(groupId);
        }
        content->showNewTab();
        content->webView()->webPage()->setUrl(QUrl(QStringLiteral("about:blank")));
        m_tabs->setTabText(0, tr("New Tab"));
        m_tabs->setTabIcon(0, QIcon());
        updateGroupStrip();
        applyGroupAppearance();
        updateChrome();
        return;
    }
    if (content->isPinned()) {
        content->deleteCloseButton(); // detached from the tab bar on pin
    }

    m_tabs->removeTab(index);
    content->deleteLater();

    pruneGroupIfEmpty(groupId);
    updateGroupStrip();
    applyGroupAppearance();
    updateChrome();
}

void BrowserWindow::pruneGroupIfEmpty(const QString &groupId)
{
    if (groupId.isEmpty() || groupTabCount(groupId) != 0) {
        return;
    }
    for (qsizetype i = 0; i < m_groups.size(); ++i) {
        if (m_groups.at(i).id == groupId) {
            m_groups.removeAt(i);
            break;
        }
    }
    rebuildGroupActions();
}

void BrowserWindow::reopenClosedTab()
{
    if (m_closedTabs.empty()) {
        return;
    }
    const ClosedTab closed = m_closedTabs.back();
    m_closedTabs.pop_back();

    auto *content = addTab(closed.url, false, closed.index, closed.userId, closed.pinned,
                           closed.groupId);
    if (content) {
        const int index = m_tabs->indexOf(content);
        if (index >= 0 && !closed.title.isEmpty() && !closed.pinned) {
            m_tabs->setTabText(index, closed.title);
        }
        m_tabs->setCurrentIndex(index);
    }
    updateChrome();
}

void BrowserWindow::navigateCurrent(const QUrl &url)
{
    if (!url.isValid()) {
        return;
    }
    auto *tab = currentTab();
    if (!tab) {
        addTab(url, false);
        return;
    }
    tab->webView()->webPage()->setUrl(url);
    tab->webView()->setFocus();
}

void BrowserWindow::handleInput(const QString &text)
{
    if (!m_browser) {
        return;
    }
    if (text.trimmed() == QLatin1String(kSettingsUrl)) {
        openSettingsTab();
        return;
    }
    // One resolution point for address bar and New Tab page: URL rules +
    // configured search engine (core decides, ui only routes).
    navigateCurrent(m_browser->resolveInput(text));
}

void BrowserWindow::openSettingsTab(const QString &section)
{
    if (!m_browser) {
        return;
    }
    // Chrome style: settings live in a tab, reused while open.
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *form = qobject_cast<SettingsForm *>(m_tabs->widget(i))) {
            m_tabs->setCurrentIndex(i);
            if (!section.isEmpty()) {
                form->showSection(section);
            }
            return;
        }
    }
    auto *form = new SettingsForm(m_browser->settings(), m_browser->requestInterceptor(),
                                 m_browser->userCatalog(),
                                 m_browser->passwordVault(), m_browser, m_tabs);
    connect(form, &SettingsForm::clearBrowsingDataRequested, this,
            &BrowserWindow::confirmClearBrowsingData);
    const int index =
        m_tabs->addTab(form, QIcon(QStringLiteral(":/icons/gear.svg")), tr("Settings"));
    m_tabs->setCurrentIndex(index);
    if (!section.isEmpty()) {
        form->showSection(section);
    }
    applyGroupAppearance();
    updateChrome();
}

void BrowserWindow::confirmClearBrowsingData()
{
    if (!m_browser) {
        return;
    }
    const auto answer = QMessageBox::question(
        this, tr("Clear browsing data"),
        tr("Delete cookies and cached files for all profiles?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    m_browser->clearBrowsingData();
    statusBar()->showMessage(tr("Cookies and cache cleared."), 4000);
}

// ---------------------------------------------------------------------------
// Pinned tabs
// ---------------------------------------------------------------------------

void BrowserWindow::setTabPinned(int index, bool pinned)
{
    if (index < 0 || index >= m_tabs->count()) {
        return;
    }
    TabContent *content = tabAt(index);
    if (!content || content->isPinned() == pinned) {
        return;
    }

    QTabBar *bar = m_tabs->tabBar();
    const auto closeSide = static_cast<QTabBar::ButtonPosition>(
        style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));
    const QWebEnginePage *page = content->webView()->webPage();

    if (pinned) {
        // Capture the close button (setTabButton(nullptr) hides but does
        // not delete it), move the tab into the leading pinned block.
        if (QWidget *button = bar->tabButton(index, closeSide)) {
            content->captureCloseButton(button);
            bar->setTabButton(index, closeSide, nullptr);
        }
        content->setPinnedState(true);

        int target = 0;
        while (target < index && tabAt(target)->isPinned()) {
            ++target;
        }
        if (index != target) {
            bar->moveTab(index, target);
            index = target;
        }

        if (bar->tabIcon(index).isNull()) {
            bar->setTabIcon(index, QIcon(QStringLiteral(":/icons/globe.svg")));
        }
        bar->setTabText(index, QString());
    } else {
        int pinnedCount = 0;
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (tabAt(i)->isPinned()) {
                ++pinnedCount;
            }
        }
        content->setPinnedState(false);
        if (pinnedCount > 0 && index != pinnedCount - 1) {
            bar->moveTab(index, pinnedCount - 1);
            index = pinnedCount - 1;
        }

        if (QWidget *button = content->takeCloseButton()) {
            bar->setTabButton(index, closeSide, button);
        }
        bar->setTabIcon(index, page->icon());
        const QString title = page->title();
        QString text = title;
        if (text.isEmpty()) {
            const QString host = page->url().host();
            text = host.isEmpty() ? tr("New Tab") : host;
        }
        bar->setTabText(index, text);
    }

    applyGroupAppearance();
    updateChrome();
}

// ---------------------------------------------------------------------------
// Tab groups
// ---------------------------------------------------------------------------

SessionGroup BrowserWindow::groupById(const QString &id) const
{
    if (!id.isEmpty()) {
        for (const SessionGroup &group : m_groups) {
            if (group.id == id) {
                return group;
            }
        }
    }
    return SessionGroup{};
}

QString BrowserWindow::createGroup(const QString &name, const QString &color)
{
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) {
        return QString();
    }
    SessionGroup group;
    group.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    while (!groupById(group.id).id.isEmpty()) {
        group.id += QLatin1Char('x');
    }
    group.name = cleanName;
    group.color = color.isEmpty() ? NameColorDialog::presetColors().first() : color;
    m_groups.append(group);
    rebuildGroupActions();
    updateGroupStrip();
    return group.id;
}

bool BrowserWindow::assignTabToGroup(int index, const QString &groupId)
{
    TabContent *content = tabAt(index);
    if (!content) {
        return false;
    }
    if (!groupId.isEmpty() && groupById(groupId).id != groupId) {
        return false;
    }
    const QString previousGroupId = content->groupId();
    if (previousGroupId == groupId) {
        return true;
    }
    content->setGroupId(groupId);

    if (!previousGroupId.isEmpty() && groupTabCount(previousGroupId) == 0) {
        for (qsizetype i = 0; i < m_groups.size(); ++i) {
            if (m_groups.at(i).id == previousGroupId) {
                m_groups.removeAt(i);
                break;
            }
        }
        rebuildGroupActions();
    }
    applyGroupAppearance();
    updateGroupStrip();
    return true;
}

void BrowserWindow::renameGroup(const QString &groupId, const QString &name)
{
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) {
        return;
    }
    for (SessionGroup &group : m_groups) {
        if (group.id == groupId) {
            group.name = cleanName;
            break;
        }
    }
    rebuildGroupActions();
    updateGroupStrip();
}

void BrowserWindow::setGroupColor(const QString &groupId, const QString &color)
{
    if (color.isEmpty()) {
        return;
    }
    for (SessionGroup &group : m_groups) {
        if (group.id == groupId) {
            group.color = color;
            break;
        }
    }
    applyGroupAppearance();
    updateGroupStrip();
}

void BrowserWindow::setGroupCollapsed(const QString &groupId, bool collapsed)
{
    for (SessionGroup &group : m_groups) {
        if (group.id == groupId) {
            if (group.collapsed == collapsed) {
                return;
            }
            group.collapsed = collapsed;
            break;
        }
    }
    applyGroupAppearance();
    updateGroupStrip();
}

void BrowserWindow::deleteGroup(const QString &groupId)
{
    if (groupById(groupId).id.isEmpty()) {
        return;
    }
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (TabContent *tab = tabAt(i); tab && tab->groupId() == groupId) {
            tab->setGroupId(QString());
        }
    }
    for (qsizetype i = 0; i < m_groups.size(); ++i) {
        if (m_groups.at(i).id == groupId) {
            m_groups.removeAt(i);
            break;
        }
    }
    rebuildGroupActions();
    applyGroupAppearance();
    updateGroupStrip();
}

int BrowserWindow::groupTabCount(const QString &groupId) const
{
    int count = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        const TabContent *tab = tabAt(i);
        if (tab && tab->groupId() == groupId) {
            ++count;
        }
    }
    return count;
}

bool BrowserWindow::newGroupFromCurrentTab()
{
    TabContent *tab = currentTab();
    if (!tab || !tab->groupId().isEmpty()) {
        return false;
    }
    NameColorDialog dialog(tr("New tab group"), QString(), QString(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const QString groupId = createGroup(dialog.nameValue(), dialog.colorValue());
    if (groupId.isEmpty()) {
        return false;
    }
    assignTabToGroup(m_tabs->currentIndex(), groupId);
    return true;
}

// ---------------------------------------------------------------------------
// Users
// ---------------------------------------------------------------------------

void BrowserWindow::removeDynamicActions(const QString &idPrefix)
{
    QStringList remaining;
    for (const QString &id : std::as_const(m_dynamicActionIds)) {
        if (id.startsWith(idPrefix)) {
            m_actions->remove(id);
        } else {
            remaining.append(id);
        }
    }
    m_dynamicActionIds = remaining;
}

void BrowserWindow::rebuildGroupActions()
{
    removeDynamicActions(QStringLiteral("tab.group.assign."));
    if (!m_browser) {
        return;
    }
    for (const SessionGroup &group : std::as_const(m_groups)) {
        const QString id = QStringLiteral("tab.group.assign.") + group.id;
        QAction *action = m_actions->define(
            id, tr("Group: %1").arg(group.name), QKeySequence(),
            [this, groupId = group.id] {
                TabContent *tab = currentTab();
                if (!tab) {
                    return;
                }
                assignTabToGroup(m_tabs->currentIndex(),
                                 tab->groupId() == groupId ? QString() : groupId);
            });
        action->setCheckable(true);
        m_dynamicActionIds.append(id);
    }
}

void BrowserWindow::buildUserMenu()
{
    if (!m_userMenu || !m_browser) {
        return;
    }
    m_userMenu->clear();

    QList<BrowseUser> users;
    users.append(UserCatalog::defaultUser());
    if (m_browser->userCatalog()) {
        users.append(m_browser->userCatalog()->users());
    }

    m_userMenu->addSection(tr("Switch user (tabs swap)"));
    for (const BrowseUser &user : std::as_const(users)) {
        const bool current = user.id == m_userId;
        QAction *action = m_userMenu->addAction(
            current ? tr("%1 — current").arg(user.name) : user.name, this,
            [this, userId = user.id] { switchToUser(userId); });
        action->setEnabled(!current);
        action->setToolTip(current ? tr("This window already shows %1").arg(user.name)
                                  : tr("Remember this user's tabs and show %1's")
                                        .arg(user.name));
    }

    m_userMenu->addSeparator();
    if (QAction *users = m_actions->action(QStringLiteral("settings.users"))) {
        m_userMenu->addAction(users);
    }
}

void BrowserWindow::showTabContextMenu(const QPoint &pos)
{
    const int index = m_tabs->tabBar()->tabAt(pos);
    if (index < 0) {
        return;
    }
    if (index != m_tabs->currentIndex()) {
        m_tabs->setCurrentIndex(index);
    }
    TabContent *tab = currentTab();
    if (!tab || !m_browser) {
        return;
    }

    QMenu menu(this);
    menu.addAction(m_actions->action(QStringLiteral("tab.close")));
    menu.addAction(m_actions->action(QStringLiteral("tab.pin")));
    menu.addSeparator();

    QMenu *groupMenu = menu.addMenu(tr("Tab group"));
    groupMenu->addAction(m_actions->action(QStringLiteral("tab.group.new")));
    const bool inGroup = !tab->groupId().isEmpty();
    m_actions->action(QStringLiteral("tab.group.rename"))->setEnabled(inGroup);
    m_actions->action(QStringLiteral("tab.group.remove"))->setEnabled(inGroup);
    m_actions->action(QStringLiteral("tab.group.delete"))->setEnabled(inGroup);
    if (inGroup) {
        const SessionGroup group = groupById(tab->groupId());
        groupMenu->addAction(m_actions->action(QStringLiteral("tab.group.rename")));
        QMenu *colorMenu = groupMenu->addMenu(tr("Change color"));
        const QList<QString> presets = NameColorDialog::presetColors();
        for (int i = 0; i < presets.size(); ++i) {
            QAction *action = m_actions->action(QStringLiteral("tab.group.color.")
                                                + QString::number(i));
            if (action) {
                action->setChecked(group.color == presets.at(i));
                colorMenu->addAction(action);
            }
        }
        groupMenu->addAction(m_actions->action(QStringLiteral("tab.group.remove")));
        groupMenu->addAction(m_actions->action(QStringLiteral("tab.group.delete")));
    }
    if (!m_groups.isEmpty()) {
        groupMenu->addSeparator();
        for (const SessionGroup &group : std::as_const(m_groups)) {
            if (QAction *action =
                    m_actions->action(QStringLiteral("tab.group.assign.") + group.id)) {
                action->setChecked(tab->groupId() == group.id);
                groupMenu->addAction(action);
            }
        }
    }

    menu.exec(m_tabs->tabBar()->mapToGlobal(pos));
}

// ---------------------------------------------------------------------------
// Presentation: group strip / colors / chrome
// ---------------------------------------------------------------------------

void BrowserWindow::updateGroupStrip()
{
    const bool show = !m_groups.isEmpty();
    m_groupStrip->setVisible(show);
    while (QLayoutItem *item = m_groupStripLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    if (!show) {
        return;
    }
    for (const SessionGroup &group : std::as_const(m_groups)) {
        auto *chip = new QToolButton(m_groupStrip);
        chip->setObjectName(QStringLiteral("groupChip"));
        chip->setAutoRaise(true);
        chip->setCheckable(true);
        chip->setChecked(!group.collapsed);
        chip->setIcon(QIcon(QStringLiteral(":/icons/folder.svg")));
        chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        chip->setText(QStringLiteral("%1 (%2)").arg(group.name).arg(groupTabCount(group.id)));
        chip->setToolTip(group.collapsed ? tr("Expand group %1").arg(group.name)
                                         : tr("Collapse group %1").arg(group.name));
        chip->setStyleSheet(QStringLiteral(
                                "QToolButton#groupChip { border: 1px solid #2e3341;"
                                " border-left: 3px solid %1; border-radius: 11px;"
                                " padding: 4px 12px; margin: 0; color: #e8e9ee; }"
                                "QToolButton#groupChip:checked { background: #24272f;"
                                " border-color: #3a3f4d; border-left: 3px solid %1; }"
                                "QToolButton#groupChip:hover { background: #22252e; }")
                                .arg(group.color));
        connect(chip, &QToolButton::clicked, this, [this, groupId = group.id] {
            const SessionGroup current = groupById(groupId);
            if (!current.id.isEmpty()) {
                setGroupCollapsed(groupId, !current.collapsed);
            }
        });
        m_groupStripLayout->addWidget(chip);
    }
    m_groupStripLayout->addStretch(1);
}

void BrowserWindow::applyGroupAppearance()
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        const TabContent *tab = tabAt(i);
        if (!tab) {
            continue;
        }
        const SessionGroup group = groupById(tab->groupId());
        m_tabs->tabBar()->setTabVisible(i, group.id.isEmpty() || !group.collapsed);
    }
    refreshTabColors();
}

void BrowserWindow::refreshTabColors()
{
    // Per-tab text colors cannot be expressed in QSS (a stylesheet color
    // rule would override them), so current/ungrouped/grouped colors are
    // computed here on every state change.
    QTabBar *bar = m_tabs->tabBar();
    const int current = m_tabs->currentIndex();
    for (int i = 0; i < m_tabs->count(); ++i) {
        const TabContent *tab = tabAt(i);
        if (!tab) {
            continue;
        }
        const SessionGroup group = groupById(tab->groupId());
        if (!group.id.isEmpty()) {
            bar->setTabTextColor(i, QColor(group.color));
        } else {
            bar->setTabTextColor(i, i == current ? QColor(QStringLiteral("#ffffff"))
                                                 : QColor(QStringLiteral("#9ba1ad")));
        }
    }
}

const PagePolicy *BrowserWindow::pagePolicy() const
{
    return m_browser ? m_browser->pagePolicy() : nullptr;
}

void BrowserWindow::handleFeaturePermissionAsk(WebPage *page, const QUrl &origin,
                                               QWebEnginePage::Feature feature)
{
    if (!page) {
        return;
    }
    const QString key = PagePolicy::featureKey(feature);
    QString what;
    if (key == QLatin1String("location")) {
        what = tr("use your location");
    } else if (key == QLatin1String("media")) {
        what = tr("use your camera and microphone");
    } else if (key == QLatin1String("notifications")) {
        what = tr("send you notifications");
    } else {
        page->setFeaturePermission(origin, feature, QWebEnginePage::PermissionDeniedByUser);
        return;
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Permission requested"),
        tr("The site \"%1\" wants to %2. Allow it?").arg(origin.host(), what),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    page->setFeaturePermission(
        origin, feature,
        answer == QMessageBox::Yes ? QWebEnginePage::PermissionGrantedByUser
                                   : QWebEnginePage::PermissionDeniedByUser);
}

void BrowserWindow::wireTab(TabContent *content)
{
    WebPage *page = content->webView()->webPage();

    connect(content, &TabContent::modeChanged, this, [this, content] {
        if (content == currentTab()) {
            updateChrome();
        }
    });
    connect(content, &TabContent::inputSubmitted, this, &BrowserWindow::handleInput);
    connect(content, &TabContent::featurePermissionAsk, this,
            &BrowserWindow::handleFeaturePermissionAsk);
    connect(content->webView(), &WebView::savePasswordRequested, this,
            &BrowserWindow::savePasswordForCurrentPage);

    connect(page, &WebPage::titleChanged, this, [this, content](const QString &title) {
        const int index = m_tabs->indexOf(content);
        if (index < 0) {
            return;
        }
        if (content->isPinned()) {
            m_tabs->setTabText(index, QString());
        } else {
            QString text = title;
            if (text.isEmpty()) {
                const QString host = content->webView()->webPage()->url().host();
                text = host.isEmpty() ? tr("New Tab") : host;
            }
            m_tabs->setTabText(index, text);
        }
        if (m_tabs->currentWidget() == content) {
            updateChrome();
        }
    });

    connect(page, &WebPage::iconChanged, this, [this, content](const QIcon &icon) {
        const int index = m_tabs->indexOf(content);
        if (index >= 0) {
            m_tabs->setTabIcon(index, icon);
        }
    });

    connect(page, &WebPage::urlChanged, this, [this, content](const QUrl &url) {
        const int index = m_tabs->indexOf(content);
        if (index >= 0) {
            m_tabs->setTabToolTip(index, url.toString());
        }
        if (m_tabs->currentWidget() == content) {
            updateChrome();
        }
    });

    connect(page, &WebPage::loadStarted, this, [this, content] {
        if (m_tabs->currentWidget() != content) {
            return;
        }
        m_progress->setValue(0);
        m_progress->setVisible(true);
        statusBar()->showMessage(
            tr("Loading %1…").arg(content->webView()->webPage()->url().host()));
        updateChrome();
    });

    connect(page, &WebPage::loadProgress, this, [this, content](int value) {
        if (m_tabs->currentWidget() == content) {
            m_progress->setValue(value);
        }
    });

    connect(page, &WebPage::loadFinished, this, [this, content](bool ok) {
        if (m_tabs->currentWidget() != content) {
            return;
        }
        m_progress->setVisible(false);
        statusBar()->clearMessage();
        if (!ok) {
            qCInfo(lcCosmic) << "load not successful:" << content->webView()->webPage()->url();
        }
        updateChrome();
        if (ok) {
            tryAutofill(content->webView()->webPage());
            QPointer<TabContent> guard(content);
            QTimer::singleShot(700, content->webView()->webPage(), [this, guard] {
                if (guard) {
                    tryAutofill(guard->webView()->webPage());
                }
            });
        }
    });
}

// ---------------------------------------------------------------------------
// Chrome state
// ---------------------------------------------------------------------------

void BrowserWindow::updateChrome()
{
    TabContent *tab = currentTab();
    if (!tab) {
        // Settings tab (Chrome style): no page behind the chrome.
        m_urlBar->setPage(nullptr);
        m_actions->action(QStringLiteral("nav.back"))->setEnabled(false);
        m_actions->action(QStringLiteral("nav.forward"))->setEnabled(false);
        m_actions->action(QStringLiteral("tab.reopen"))->setEnabled(!m_closedTabs.empty());
        if (QAction *pinAction = m_actions->action(QStringLiteral("tab.pin"))) {
            pinAction->setText(tr("Pin Tab"));
            pinAction->setEnabled(false);
        }
        m_reloadAction->setIcon(QIcon(QStringLiteral(":/icons/reload.svg")));
        m_reloadAction->setText(tr("Reload"));
        m_reloadAction->setToolTip(tr("Reload"));
        m_reloadAction->setEnabled(false);
        m_progress->setVisible(false);
        setWindowTitle(tr("Settings — Cosmic"));
        return;
    }
    WebPage *page = tab->webView()->webPage();

    // URL bar: cleared on the New Tab page, bound to the page otherwise.
    // UrlBar::setPage is a no-op when the binding is unchanged, so typed
    // text survives load events.
    if (tab->isShowingNewTab()) {
        m_urlBar->setPage(nullptr);
    } else {
        m_urlBar->setPage(page);
    }

    auto *back = m_actions->action(QStringLiteral("nav.back"));
    auto *forward = m_actions->action(QStringLiteral("nav.forward"));
    auto *reopen = m_actions->action(QStringLiteral("tab.reopen"));
    back->setEnabled(page->history()->canGoBack());
    forward->setEnabled(page->history()->canGoForward());
    reopen->setEnabled(!m_closedTabs.empty());

    // Pin action label mirrors the current tab state.
    const bool pinned = tab->isPinned();
    if (QAction *pinAction = m_actions->action(QStringLiteral("tab.pin"))) {
        pinAction->setText(pinned ? tr("Unpin Tab") : tr("Pin Tab"));
        pinAction->setEnabled(true);
    }

    // User button mirrors the window's owning user.
    if (m_userButton && m_browser && m_browser->userCatalog()) {
        BrowseUser user = m_browser->userCatalog()->userById(m_userId);
        if (user.id.isEmpty()) {
            user.id = m_userId;
            user.name = m_userId;
        }
        if (user.name.isEmpty()) {
            user = UserCatalog::defaultUser();
        }
        m_userButton->setText(user.name);
        m_userButton->setToolTip(
            tr("User: %1 — click to switch user (tabs swap)").arg(user.name));
    }

    // Reload action doubles as Stop while a page is loading.
    const bool loading = page->isLoading();
    m_reloadAction->setIcon(QIcon(loading ? QStringLiteral(":/icons/stop.svg")
                                          : QStringLiteral(":/icons/reload.svg")));
    m_reloadAction->setText(loading ? tr("Stop") : tr("Reload"));
    m_reloadAction->setToolTip(loading ? tr("Stop") : tr("Reload"));
    m_reloadAction->setEnabled(!tab->isShowingNewTab());

    m_progress->setVisible(loading);

    refreshTabColors();

    const QString title = m_tabs->tabText(m_tabs->currentIndex());
    setWindowTitle(title.isEmpty() ? tr("Cosmic") : title + QStringLiteral(" — Cosmic"));
}

// ---------------------------------------------------------------------------
// Bookmarks bar / AI / RAM Saver / vault
// ---------------------------------------------------------------------------

void BrowserWindow::handleBookmarkOpen(const QUrl &url, bool newTab)
{
    if (newTab) {
        addTab(url, false);
    } else {
        navigateCurrent(url);
    }
}

void BrowserWindow::applyUILayout()
{
    if (!m_browser || !m_toolbar) {
        return;
    }
    UIConfiguration cfg = m_browser->uiConfiguration();
    QString error;
    if (!UiLayout::validate(cfg, &error)) {
        qCWarning(lcCosmic).noquote()
            << "invalid UI configuration, falling back to default:" << error;
        cfg = UiLayout::defaultConfiguration();
    }

    for (QAction *action : m_toolbar->actions()) {
        m_toolbar->removeAction(action);
    }
    qDeleteAll(m_toolbarWidgets);
    m_toolbarWidgets.clear();
    m_toolbarActions.clear();
    m_bookmarksButton = nullptr;
    m_aiButton = nullptr;
    m_aiEyeButton = nullptr;
    m_ramButton = nullptr;
    m_keyButton = nullptr;

    const QList<QList<UiToolbarItem>> zones = {cfg.toolbar.left, cfg.toolbar.center,
                                              cfg.toolbar.right};
    const bool pushRight = cfg.toolbar.left.isEmpty() && cfg.toolbar.center.isEmpty()
        && !cfg.toolbar.right.isEmpty();
    for (int z = 0; z < zones.size(); ++z) {
        const QList<UiToolbarItem> &zone = zones.at(z);
        if (z == 2 && pushRight) {
            auto *spacer = new QWidget(m_toolbar);
            spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            spacer->setAttribute(Qt::WA_TransparentForMouseEvents);
            m_toolbar->addWidget(spacer);
            m_toolbarWidgets.append(spacer);
        }
        for (const UiToolbarItem &item : zone) {
            addToolbarComponent(item.component);
        }
    }
    const UiLayout::UiToolbarMetrics metrics = UiLayout::toolbarMetrics(cfg.toolbar);
    m_toolbar->setIconSize(QSize(metrics.iconSize, metrics.iconSize));
    if (QLayout *layout = m_toolbar->layout()) {
        layout->setSpacing(cfg.toolbar.spacing);
    }

    const int urlH = cfg.urlbar.height > 0 ? qBound(24, cfg.urlbar.height, 56)
                                           : metrics.urlHeight;
    m_urlBar->setMinimumHeight(urlH);
    m_urlBar->setMaximumHeight(qMax(urlH, metrics.urlHeight + 8));
    m_urlBar->setMaximumWidth(cfg.urlbar.maxWidth > 0 ? cfg.urlbar.maxWidth
                                                      : QWIDGETSIZE_MAX);
    m_urlBar->setStyleSheet(urlBarStyleSheet(cfg));

    if (cfg.toolbar.color.isEmpty()) {
        m_toolbar->setStyleSheet(QString());
    } else {
        m_toolbar->setStyleSheet(
            QStringLiteral("QToolBar#navToolbar { background: %1; border: none;"
                           " border-bottom: 1px solid #1e212a; }")
                .arg(cfg.toolbar.color));
    }

    if (auto *bar = qobject_cast<NebulaTabBar *>(m_tabs->tabBar())) {
        bar->setTabHeight(cfg.tabs.tabHeight);
        bar->setTabRadius(cfg.tabs.tabRadius);
        bar->setTabSpacing(cfg.tabs.tabSpacing);
        bar->setShowPlus(cfg.tabs.showPlus);
    }
    m_tabs->setStyleSheet(tabWidgetStyleSheet(cfg));
}

QString BrowserWindow::urlBarStyleSheet(const UIConfiguration &cfg) const
{
    const QString bg = cfg.urlbar.color.isEmpty() ? QStringLiteral("#1b1d24")
                                                  : cfg.urlbar.color;
    return Style::urlBarStyleSheet(bg, cfg.urlbar.radius, Style::currentAccent());
}

QString BrowserWindow::tabWidgetStyleSheet(const UIConfiguration &cfg) const
{
    QString sheet;
    if (cfg.tabs.position == QLatin1String("vertical")) {
        sheet += QStringLiteral("QTabWidget#tabWidget::pane { border: none;"
                                " border-left: 1px solid #1e212a;"
                                " background: #0f1013; }");
    }
    sheet += QStringLiteral("QTabBar::tab { margin: 4px %1px 4px 0;"
                            " border-radius: %2px; }")
                 .arg(cfg.tabs.tabSpacing / 2)
                 .arg(cfg.tabs.tabRadius);
    return sheet;
}

void BrowserWindow::addToolbarComponent(const QString &component)
{
    if (component == QLatin1String("container") || component == QLatin1String("user")) {
        m_toolbar->addWidget(m_userButton);
        return;
    }
    if (component == QLatin1String("urlbar")) {
        m_toolbar->addWidget(m_urlBar);
        return;
    }
    if (component == QLatin1String("nav.back") || component == QLatin1String("nav.forward")
        || component == QLatin1String("nav.reload")) {
        QAction *action = m_actions->action(component);
        if (!action) {
            return;
        }
        m_toolbar->addAction(action);
        m_toolbarActions.append(action);
        return;
    }

    auto *button = new QToolButton(m_toolbar);
    button->setAutoRaise(true);

    if (component == QLatin1String("bookmarks")) {
        m_bookmarksButton = button;
        button->setObjectName(QStringLiteral("bookmarksButton"));
        button->setIcon(QIcon(QStringLiteral(":/icons/star.svg")));
        button->setToolTip(tr("Bookmarks"));
        button->setPopupMode(QToolButton::InstantPopup);
        auto *menu = new QMenu(button);
        button->setMenu(menu);
        connect(menu, &QMenu::aboutToShow, this, &BrowserWindow::buildBookmarksMenu);
    } else if (component == QLatin1String("ai")) {
        m_aiButton = button;
        button->setObjectName(QStringLiteral("aiButton"));
        button->setIcon(QIcon(QStringLiteral(":/icons/ai.svg")));
        button->setToolTip(tr("AI assistants"));
        button->setPopupMode(QToolButton::InstantPopup);
        buildAiMenu();
    } else if (component == QLatin1String("ai.eye")) {
        m_aiEyeButton = button;
        button->setObjectName(QStringLiteral("aiEyeButton"));
        button->setCheckable(true);
        button->setIcon(QIcon(QStringLiteral(":/icons/eye.svg")));
        button->setToolTip(
            tr("Let the AI read this page (copies page context to the clipboard)"));
        connect(button, &QToolButton::toggled, this, [this](bool on) {
            setAiPageContext(on);
        });
        button->setChecked(m_aiPageContext);
    } else if (component == QLatin1String("ram")) {
        m_ramButton = button;
        button->setObjectName(QStringLiteral("ramButton"));
        button->setIcon(QIcon(QStringLiteral(":/icons/ram.svg")));
        button->setToolTip(tr("RAM Saver"));
        button->setPopupMode(QToolButton::InstantPopup);
        buildRamMenu();
    } else if (component == QLatin1String("vault")) {
        m_keyButton = button;
        button->setObjectName(QStringLiteral("keyButton"));
        button->setIcon(QIcon(QStringLiteral(":/icons/key.svg")));
        button->setToolTip(tr("Password manager"));
        connect(button, &QToolButton::clicked, this, [this] {
            openSettingsTab(QStringLiteral("passwords"));
        });
    } else if (component == QLatin1String("settings")) {
        m_actions->action(QStringLiteral("settings.open"))
            ->setIcon(QIcon(QStringLiteral(":/icons/gear.svg")));
        button->setObjectName(QStringLiteral("settingsButton"));
        button->setDefaultAction(m_actions->action(QStringLiteral("settings.open")));
    } else {
        qCWarning(lcCosmic).noquote() << "skipping unknown toolbar component:" << component;
        button->deleteLater();
        return;
    }
    m_toolbar->addWidget(button);
    m_toolbarWidgets.append(button);
}

void BrowserWindow::buildBookmarksMenu()
{
    QMenu *menu = m_bookmarksButton->menu();
    if (!menu) {
        return;
    }
    menu->clear();
    if (m_browser) {
        for (const Bookmark &bm : m_browser->bookmarks()->items()) {
            const QString id = bm.id;
            const QUrl url = bm.url;
            QMenu *siteMenu = menu->addMenu(
                QFontMetrics(menu->font()).elidedText(bm.title, Qt::ElideRight, 320));
            siteMenu->addAction(tr("Open"), this, [this, url] {
                handleBookmarkOpen(url, false);
            });
            siteMenu->addAction(tr("Open in new tab"), this, [this, url] {
                handleBookmarkOpen(url, true);
            });
            siteMenu->addAction(tr("Copy address"), this, [url] {
                QGuiApplication::clipboard()->setText(url.toString());
            });
            siteMenu->addSeparator();
            siteMenu->addAction(tr("Edit name…"), this, [this, id] {
                if (!m_browser) {
                    return;
                }
                const Bookmark found = m_browser->bookmarks()->findById(id);
                if (found.id.isEmpty()) {
                    return;
                }
                bool ok = false;
                const QString name = QInputDialog::getText(
                    this, tr("Edit bookmark"), tr("Name:"), QLineEdit::Normal,
                    found.title, &ok);
                if (ok && !name.trimmed().isEmpty()) {
                    m_browser->bookmarks()->rename(id, name.trimmed());
                }
            });
            siteMenu->addAction(tr("Edit address…"), this, [this, id] {
                if (!m_browser) {
                    return;
                }
                const Bookmark found = m_browser->bookmarks()->findById(id);
                if (found.id.isEmpty()) {
                    return;
                }
                bool ok = false;
                const QString urlText = QInputDialog::getText(
                    this, tr("Edit bookmark"), tr("Address:"), QLineEdit::Normal,
                    found.url.toString(), &ok);
                if (ok) {
                    const QUrl newUrl(urlText.trimmed());
                    if (newUrl.isValid() && !newUrl.isEmpty()) {
                        m_browser->bookmarks()->retarget(id, newUrl);
                    }
                }
            });
            siteMenu->addAction(tr("Remove"), this, [this, id] {
                if (m_browser) {
                    m_browser->bookmarks()->remove(id);
                }
            });
        }
        if (m_browser->bookmarks()->count() > 0) {
            menu->addSeparator();
        }
    }
    menu->addAction(m_actions->action(QStringLiteral("bookmark.add")));
}

void BrowserWindow::buildAiMenu()
{
    auto *menu = new QMenu(m_aiButton);
    menu->addAction(m_actions->action(QStringLiteral("ai.pageContext")));
    menu->addSeparator();
    menu->addAction(m_actions->action(QStringLiteral("ai.open.chatgpt")));
    menu->addAction(m_actions->action(QStringLiteral("ai.open.claude")));
    menu->addAction(m_actions->action(QStringLiteral("ai.open.gemini")));
    menu->addAction(m_actions->action(QStringLiteral("ai.open.perplexity")));
    menu->addAction(m_actions->action(QStringLiteral("ai.open.copilot")));
    m_aiButton->setMenu(menu);
}

void BrowserWindow::buildRamMenu()
{
    auto *menu = new QMenu(m_ramButton);
    menu->addAction(m_actions->action(QStringLiteral("performance.ramSaver")));
    menu->addAction(m_actions->action(QStringLiteral("performance.discardNow")));
    m_ramButton->setMenu(menu);
}

void BrowserWindow::setAiPageContext(bool enabled)
{
    m_aiPageContext = enabled;
    if (m_aiEyeButton) {
        m_aiEyeButton->setChecked(enabled);
    }
    if (QAction *action = m_actions->action(QStringLiteral("ai.pageContext"))) {
        action->setChecked(enabled);
    }
    statusBar()->showMessage(enabled ? tr("AI can read this page (context copied on use).")
                                     : tr("AI page reading off."),
                             3000);
}

void BrowserWindow::openAiService(const QUrl &url)
{
    TabContent *tab = currentTab();
    WebPage *page = tab ? tab->webView()->webPage() : nullptr;
    const bool readable = m_aiPageContext && page
        && (page->url().scheme() == QLatin1String("http")
            || page->url().scheme() == QLatin1String("https"));
    if (!readable) {
        addTab(url, false);
        return;
    }
    QPointer<WebPage> guard(page);
    QPointer<BrowserWindow> self(this);
    page->runJavaScript(kExtractPageContextScript, [self, guard, url](const QVariant &v) {
        if (!self) {
            return;
        }
        if (guard) {
            const QJsonObject obj =
                QJsonDocument::fromJson(v.toString().toUtf8()).object();
            const QString prompt =
                BrowserWindow::tr("Page: %1\nURL: %2\n\nExcerpt:\n%3\n\nMy question: ")
                    .arg(guard->title(), obj.value(QStringLiteral("url")).toString(),
                         obj.value(QStringLiteral("excerpt")).toString());
            QGuiApplication::clipboard()->setText(prompt);
            self->statusBar()->showMessage(
                BrowserWindow::tr("Page context copied — paste it into the AI chat."),
                6000);
        }
        self->addTab(url, false);
    });
}

void BrowserWindow::applyTabPosition()
{
    if (!m_browser) {
        return;
    }
    const bool vertical =
        m_browser->settings()->tabPosition() == QLatin1String("vertical");
    m_tabs->setTabPosition(vertical ? QTabWidget::West : QTabWidget::North);
    m_tabs->setStyleSheet(
        vertical ? QStringLiteral("QTabWidget#tabWidget::pane { border: none;"
                                  " border-left: 1px solid #1e212a;"
                                  " background: #0f1013; }")
                 : QString());
}

void BrowserWindow::discardBackgroundTabs()
{
    TabContent *current = currentTab();
    int discarded = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        TabContent *tab = tabAt(i);
        if (!tab || tab == current || tab->isShowingNewTab() || tab->isShowingCrash()) {
            continue;
        }
        WebPage *page = tab->webView()->webPage();
        if (page->isLoading()) {
            continue;
        }
        const QUrl url = page->url();
        if (url.scheme() != QLatin1String("http")
            && url.scheme() != QLatin1String("https")) {
            continue;
        }
        if (page->lifecycleState() != QWebEnginePage::LifecycleState::Discarded) {
            page->setLifecycleState(QWebEnginePage::LifecycleState::Discarded);
            ++discarded;
        }
    }
    qCInfo(lcCosmic).noquote() << "RAM saver discarded" << discarded
                               << "background tab(s)";
    statusBar()->showMessage(
        tr("RAM Saver: %1 background tab(s) suspended.").arg(discarded), 4000);
}

void BrowserWindow::tryAutofill(WebPage *page)
{
    if (!m_browser || !page) {
        return;
    }
    PasswordVault *vault = m_browser->passwordVault();
    if (!vault || !vault->isUnlocked()) {
        return;
    }
    if (page->url().scheme() != QLatin1String("http")
        && page->url().scheme() != QLatin1String("https")) {
        return;
    }
    const VaultEntry entry = vault->entryForUrl(page->url());
    if (entry.id.isEmpty() || entry.type != QLatin1String("password")
        || entry.password.isEmpty()) {
        return;
    }
    const QByteArray args =
        QJsonDocument(QJsonArray{entry.username, entry.password})
            .toJson(QJsonDocument::Compact);
    const QString script =
        kAutofillScript + QLatin1Char('(') + QString::fromUtf8(args)
        + QLatin1String(");");
    QPointer<WebPage> guard(page);
    page->runJavaScript(script, [guard](const QVariant &result) {
        if (guard && result.toString() == QLatin1String("filled")) {
            qCInfo(lcCosmic).noquote()
                << "autofill applied to" << guard->url().host();
        }
    });
}

void BrowserWindow::savePasswordForCurrentPage()
{
    if (!m_browser) {
        return;
    }
    TabContent *tab = currentTab();
    if (!tab) {
        return;
    }
    WebPage *page = tab->webView()->webPage();
    QPointer<WebPage> guard(page);
    QPointer<BrowserWindow> self(this);
    page->runJavaScript(kExtractPasswordScript, [guard, self](const QVariant &result) {
        if (!guard || !self) {
            return;
        }
        const QJsonObject obj =
            QJsonDocument::fromJson(result.toString().toUtf8()).object();
        if (!obj.value(QStringLiteral("hasField")).toBool()) {
            QMessageBox::information(
                self, BrowserWindow::tr("Password vault"),
                BrowserWindow::tr("This page has no password field."));
            return;
        }
        const QString password = obj.value(QStringLiteral("password")).toString();
        if (password.isEmpty()) {
            QMessageBox::information(
                self, BrowserWindow::tr("Password vault"),
                BrowserWindow::tr("Type the password into the page first, then save it."));
            return;
        }
        self->promptSavePassword(QUrl(obj.value(QStringLiteral("url")).toString()),
                                 obj.value(QStringLiteral("username")).toString(),
                                 password);
    });
}

void BrowserWindow::promptSavePassword(const QUrl &url, const QString &username,
                                       const QString &password)
{
    if (!m_browser) {
        return;
    }
    PasswordVault *vault = m_browser->passwordVault();
    if (!vault) {
        return;
    }
    if (!vault->isUnlocked()) {
        bool ok = false;
        const QString master = QInputDialog::getText(
            this, tr("Password vault"),
            vault->fileExists()
                ? tr("Vault locked — master password:")
                : tr("Choose a master password for the new vault (4+ characters):"),
            QLineEdit::Password, QString(), &ok);
        if (!ok || master.isEmpty()) {
            return;
        }
        const bool opened =
            vault->fileExists() ? vault->unlock(master) : vault->initialize(master);
        if (!opened) {
            QMessageBox::warning(this, tr("Password vault"), vault->lastError());
            return;
        }
    }

    QDialog prompt(this);
    prompt.setObjectName(QStringLiteral("vaultSaveDialog"));
    prompt.setWindowTitle(tr("Save password"));
    auto *layout = new QVBoxLayout(&prompt);
    auto *rows = new QFormLayout;
    auto *siteEdit = new QLineEdit(url.host(), &prompt);
    siteEdit->setObjectName(QStringLiteral("saveSiteEdit"));
    siteEdit->setReadOnly(true);
    auto *userEdit = new QLineEdit(username, &prompt);
    userEdit->setObjectName(QStringLiteral("saveUserEdit"));
    auto *passEdit = new QLineEdit(password, &prompt);
    passEdit->setObjectName(QStringLiteral("savePassEdit"));
    passEdit->setEchoMode(QLineEdit::Password);
    rows->addRow(tr("Site:"), siteEdit);
    rows->addRow(tr("Username:"), userEdit);
    rows->addRow(tr("Password:"), passEdit);
    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &prompt);
    connect(buttons, &QDialogButtonBox::accepted, &prompt, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &prompt, &QDialog::reject);
    layout->addLayout(rows);
    layout->addWidget(buttons);
    if (prompt.exec() != QDialog::Accepted) {
        return;
    }

    VaultEntry entryData;
    entryData.site = url.host();
    entryData.username = userEdit->text().trimmed();
    entryData.password = passEdit->text();
    entryData.type = QStringLiteral("password");
    if (entryData.site.isEmpty() || entryData.password.isEmpty()) {
        return;
    }
    const VaultEntry existing = vault->entryForUrl(url);
    if (!existing.id.isEmpty() && existing.username == entryData.username) {
        entryData.id = existing.id;
        entryData.note = existing.note;
        if (!vault->updateEntry(entryData)) {
            QMessageBox::warning(this, tr("Password vault"), vault->lastError());
            return;
        }
    } else if (vault->addEntry(entryData).isEmpty()) {
        QMessageBox::warning(this, tr("Password vault"), vault->lastError());
        return;
    }
    statusBar()->showMessage(
        tr("Password saved to the vault for %1.").arg(url.host()), 4000);
    qCInfo(lcCosmic).noquote() << "vault saved password for" << url.host();
}
