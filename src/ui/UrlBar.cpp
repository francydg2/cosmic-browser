#include "ui/UrlBar.h"
#include "web/WebPage.h"

#include <QAction>
#include <QFocusEvent>
#include <QIcon>

UrlBar::UrlBar(QWidget *parent)
    : QLineEdit(parent)
{
    setObjectName(QStringLiteral("urlBar"));
    setPlaceholderText(tr("Search or enter address"));
    setClearButtonEnabled(true);
    setFixedHeight(34);

    m_siteInfoAction = addAction(QIcon(QStringLiteral(":/icons/globe.svg")),
                                 QLineEdit::LeadingPosition);
    m_siteInfoAction->setObjectName(QStringLiteral("urlSiteInfoAction"));
    m_siteInfoAction->setToolTip(tr("Site information"));
    m_siteInfoAction->setEnabled(false);
    connect(m_siteInfoAction, &QAction::triggered, this, &UrlBar::siteInfoRequested);

    m_bookmarkAction = addAction(QIcon(QStringLiteral(":/icons/star.svg")),
                                 QLineEdit::TrailingPosition);
    m_bookmarkAction->setObjectName(QStringLiteral("urlBookmarkAction"));
    m_bookmarkAction->setCheckable(true);
    m_bookmarkAction->setToolTip(tr("Add bookmark"));
    m_bookmarkAction->setEnabled(false);
    connect(m_bookmarkAction, &QAction::triggered, this, &UrlBar::bookmarkRequested);

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
        updateUrlActions({});
        return;
    }

    m_connections.append(connect(page, &WebPage::urlChanged, this, [this](const QUrl &url) {
        applyUrl(url);
        updateUrlActions(url);
    }));
    applyUrl(page->url());
    updateUrlActions(page->url());
}

void UrlBar::setBookmarked(bool bookmarked)
{
    if (!m_bookmarkAction) {
        return;
    }
    m_bookmarkAction->setChecked(bookmarked);
    m_bookmarkAction->setIcon(QIcon(bookmarked ? QStringLiteral(":/icons/star-active.svg")
                                                : QStringLiteral(":/icons/star.svg")));
    m_bookmarkAction->setToolTip(bookmarked ? tr("Remove bookmark") : tr("Add bookmark"));
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

void UrlBar::updateUrlActions(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    const bool webAddress = scheme == QLatin1String("http")
        || scheme == QLatin1String("https");
    const bool secure = scheme == QLatin1String("https");
    m_siteInfoAction->setEnabled(webAddress);
    m_siteInfoAction->setIcon(QIcon(secure ? QStringLiteral(":/icons/lock.svg")
                                            : QStringLiteral(":/icons/globe.svg")));
    m_siteInfoAction->setToolTip(
        secure ? tr("Secure connection (HTTPS)") : tr("Connection not secure (HTTP)"));
    m_bookmarkAction->setEnabled(webAddress);
    if (!webAddress) {
        setBookmarked(false);
    }
}
