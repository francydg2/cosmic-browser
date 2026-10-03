#pragma once

#include <QStackedWidget>
#include <QUrl>
#include <QWebEnginePage>

class WebPageDelegate;
class WebPage;
class WebView;
class NewTabPage;
class QWidget;
class QWebEngineProfile;

/// Content of a single tab: stacked pages for
///   Web        — the WebView (spec §5)
///   NewTab     — the local Cosmic start page (spec §40)
///   Crash      — per-tab crash handler (spec §74)
///
/// A renderer crash shows the crash page for this tab only; the browser
/// and all other tabs keep running. Tab state (owning user, pin,
/// group) lives here; the window owns the presentation rules.
class TabContent : public QStackedWidget
{
    Q_OBJECT

public:
    enum Page {
        Web = 0,
        NewTab = 1,
        Crash = 2,
    };

    TabContent(WebPageDelegate *delegate, QWebEngineProfile *profile,
               const QString &userId, QWidget *parent = nullptr);

    WebView *webView() const { return m_web; }
    NewTabPage *newTabPage() const { return m_newTab; }

    bool isShowingNewTab() const;
    bool isShowingCrash() const;

    void showNewTab();
    void showWeb();
    void showCrash();

    /// Owning user this tab was opened in (immutable).
    QString userId() const { return m_userId; }

    bool isPinned() const { return m_pinned; }
    void setPinnedState(bool pinned) { m_pinned = pinned; }

    QString groupId() const { return m_groupId; }
    void setGroupId(const QString &id) { m_groupId = id; }

    /// Close-button widget of this tab in the tab bar (owned by the tab
    /// bar). Captured when the tab is pinned (the button is detached),
    /// given back when it is unpinned; deleteCloseButton() destroys a
    /// captured-but-detached button when the tab closes.
    void captureCloseButton(QWidget *button) { m_closeButton = button; }
    QWidget *takeCloseButton()
    {
        QWidget *button = m_closeButton;
        m_closeButton = nullptr;
        return button;
    }
    void deleteCloseButton();

signals:
    /// Emitted when the stack switches page (window chrome must update).
    void modeChanged();

    /// Emitted when the New Tab page submits raw input (resolved by the
    /// window against the configured search engine).
    void inputSubmitted(const QString &text);

    /// Relayed from the WebPage when a feature permission needs an
    /// "ask" decision from the user (Chromium-style dialog in the window).
    void featurePermissionAsk(WebPage *page, const QUrl &origin,
                              QWebEnginePage::Feature feature);

private:
    WebView *m_web;
    NewTabPage *m_newTab;
    QWidget *m_crashPage;
    QString m_userId;
    bool m_pinned = false;
    QString m_groupId;
    QWidget *m_closeButton = nullptr;
};
