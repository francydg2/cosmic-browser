#pragma once

#include "web/WebPageDelegate.h"

#include <QWebEnginePage>

class QWebEngineProfile;

/// QWebEnginePage that routes new-window/new-tab requests through the
/// WebPageDelegate instead of silently creating unmanaged pages. Lives
/// on the given profile (per-user storage isolation).
class WebPage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit WebPage(WebPageDelegate *delegate, QWebEngineProfile *profile,
                     QObject *parent = nullptr);

    /// True between loadStarted and loadFinished (drives Stop/Reload).
    bool isLoading() const { return m_loading; }

signals:
    /// A page requested a feature permission and the policy is "ask";
    /// the UI answers with setFeaturePermission (no receiver → deny).
    void featurePermissionAsk(WebPage *page, const QUrl &origin,
                              QWebEnginePage::Feature feature);

protected:
    QWebEnginePage *createWindow(WebWindowType type) final;

    /// Enforces the HTTPS-only policy for top-level navigations
    /// (main frame, http:// with a host → re-navigates to https://).
    bool acceptNavigationRequest(const QUrl &url, NavigationType type,
                                 bool isMainFrame) final;

private:
    WebPageDelegate *m_delegate; // not owned
    bool m_loading = false;
};
