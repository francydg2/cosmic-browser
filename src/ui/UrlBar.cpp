#include "ui/UrlBar.h"
#include "web/WebPage.h"

#include <QFocusEvent>

UrlBar::UrlBar(QWidget *parent)
    : QLineEdit(parent)
{
    setObjectName(QStringLiteral("urlBar"));
    setPlaceholderText(tr("Search or enter address"));
    setClearButtonEnabled(true);
    setFixedHeight(34);

    connect(this, &QLineEdit::returnPressed, this,
            [this] { emit inputSubmitted(text()); });
}

void UrlBar::setPage(WebPage *page)
{
    // Same binding (e.g. updateChrome on every load/title event): keep the
    // current text so what the user typed mid-load is not clobbered.
    if (page == m_page) {
        return;
    }

    for (auto &connection : m_connections) {
        disconnect(connection);
    }
    m_connections.clear();

    m_page = page;
    if (!page) {
        clear();
        return;
    }

    m_connections.append(connect(page, &WebPage::urlChanged, this, &UrlBar::applyUrl));
    applyUrl(page->url());
}

void UrlBar::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
    // Keyboard focus (e.g. Ctrl+L) selects everything; a mouse click that
    // arrives right after repositions the cursor itself.
    selectAll();
}

void UrlBar::applyUrl(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty() || url.toString() == QLatin1String("about:blank")) {
        clear();
        return;
    }
    setText(url.toString());
    setCursorPosition(0);
}
