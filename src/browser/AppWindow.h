#pragma once

#include "core/Session.h"

class WebPage;

/// Abstract browser window, implemented by ui/BrowserWindow.
///
/// browser/ is not allowed to include ui/ headers: main() (the composition
/// root) injects a factory that creates concrete windows, and everything
/// else talks to this interface.
class AppWindow
{
public:
    virtual ~AppWindow() = default;

    /// Shows and raises the window.
    virtual void present() = 0;

    /// Opens \p url; an empty url shows the New Tab page.
    virtual void openUrl(const QUrl &url, bool background = false) = 0;

    /// Page of the currently active tab (used when Chromium asks for a
    /// new top-level window).
    virtual WebPage *activeWebPage() const = 0;

    /// Full window state for session persistence (tabs, pin, groups,
    /// owning user).
    virtual SessionWindow sessionState() const = 0;

    /// Rebuilds the window's tabs/groups from \p state (session restore).
    virtual void restoreSessionState(const SessionWindow &state) = 0;

    /// Owning user id of this window's tabs (single-user windows).
    virtual QString userId() const = 0;
};
