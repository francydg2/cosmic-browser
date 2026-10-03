#pragma once

#include <QWebEnginePage>

class WebPage;
struct PagePolicy;

/// Implemented by whoever owns tabs (BrowserWindow). QWebEnginePage calls it
/// when a page wants a new window/tab (window.open, target=_blank, ...).
///
/// Defined in web/ so the engine layer knows nothing about the UI module:
/// web/ must never include headers from ui/.
class WebPageDelegate
{
public:
    virtual ~WebPageDelegate() = default;

    /// Returns the page Chromium should navigate for \p type, or nullptr to
    /// deny the request.
    virtual WebPage *createPageForWindowType(QWebEnginePage::WebWindowType type) = 0;

    /// Live page policy (HTTPS-only, permission rules); nullptr is safe
    /// (defaults: no upgrade, permissions ask → deny when unanswered).
    virtual const PagePolicy *pagePolicy() const { return nullptr; }
};
