#include "web/WebView.h"
#include "web/WebPage.h"

#include <QContextMenuEvent>
#include <QMenu>

WebView::WebView(WebPageDelegate *delegate, QWebEngineProfile *profile, QWidget *parent)
    : QWebEngineView(parent)
    , m_page(new WebPage(delegate, profile, this))
{
    setPage(m_page);
}

WebPage *WebView::webPage() const
{
    return m_page;
}

void WebView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    const QUrl url = this->url();
    if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")) {
        menu->addSeparator();
        menu->addAction(tr("Save password to vault…"), this,
                        &WebView::savePasswordRequested);
    }
    menu->popup(event->globalPos());
}
