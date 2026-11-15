#include "ui/NewTabPage.h"
#include "core/Bookmarks.h"
#include "ui/SaturnIllustration.h"

#include <QDateTime>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

NewTabPage::NewTabPage(QWidget *parent)
    : QWidget(parent)
    , m_search(new QLineEdit(this))
    , m_clock(new QLabel(this))
    , m_date(new QLabel(this))
    , m_emptyBookmarks(new QLabel(this))
    , m_bookmarkGrid(new QGridLayout)
{
    setObjectName(QStringLiteral("newTabPage"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(36, 24, 36, 28);
    layout->setSpacing(10);
    layout->addStretch(1);

    m_clock->setObjectName(QStringLiteral("ntpClock"));
    m_clock->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_clock);

    m_date->setObjectName(QStringLiteral("ntpDate"));
    m_date->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_date);

    auto *art = new SaturnIllustration(this);
    art->setFixedSize(96, 96);
    layout->addWidget(art, 0, Qt::AlignHCenter);

    auto *caption = new QLabel(QStringLiteral("COSMIC"), this);
    caption->setObjectName(QStringLiteral("ntpCaption"));
    caption->setAlignment(Qt::AlignHCenter);
    layout->addWidget(caption, 0, Qt::AlignHCenter);
    layout->addSpacing(8);

    m_search->setObjectName(QStringLiteral("ntpSearch"));
    m_search->setPlaceholderText(tr("Search the web or enter an address"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(640);
    m_search->setMinimumHeight(52);
    layout->addWidget(m_search, 0, Qt::AlignHCenter);

    auto *hint = new QLabel(tr("Tip: press Ctrl+L to jump to the address bar"), this);
    hint->setObjectName(QStringLiteral("ntpHint"));
    hint->setAlignment(Qt::AlignHCenter);
    layout->addWidget(hint);

    auto *bookmarksTitle = new QLabel(tr("Your bookmarks"), this);
    bookmarksTitle->setObjectName(QStringLiteral("ntpBookmarksTitle"));

    auto *bookmarksContent = new QWidget(this);
    bookmarksContent->setObjectName(QStringLiteral("ntpBookmarksContent"));
    m_bookmarkGrid->setContentsMargins(0, 0, 0, 0);
    m_bookmarkGrid->setHorizontalSpacing(10);
    m_bookmarkGrid->setVerticalSpacing(10);
    bookmarksContent->setLayout(m_bookmarkGrid);

    m_emptyBookmarks->setObjectName(QStringLiteral("ntpEmptyBookmarks"));
    m_emptyBookmarks->setAlignment(Qt::AlignCenter);
    m_emptyBookmarks->setWordWrap(true);

    auto *bookmarksScroll = new QScrollArea(this);
    bookmarksScroll->setObjectName(QStringLiteral("ntpBookmarksScroll"));
    bookmarksScroll->setWidgetResizable(true);
    bookmarksScroll->setFrameShape(QFrame::NoFrame);
    bookmarksScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bookmarksScroll->setMaximumHeight(210);
    bookmarksScroll->setMaximumWidth(840);
    bookmarksScroll->setWidget(bookmarksContent);

    layout->addSpacing(18);
    layout->addWidget(bookmarksTitle, 0, Qt::AlignHCenter);
    layout->addWidget(bookmarksScroll, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    updateDateTime();
    auto *clockTimer = new QTimer(this);
    clockTimer->setInterval(60'000);
    connect(clockTimer, &QTimer::timeout, this, &NewTabPage::updateDateTime);
    clockTimer->start();

    connect(m_search, &QLineEdit::returnPressed, this,
            [this] { emit inputSubmitted(m_search->text()); });
}

void NewTabPage::focusSearchField()
{
    m_search->setFocus();
    m_search->selectAll();
}

void NewTabPage::setBookmarks(const QVector<Bookmark> &bookmarks)
{
    while (QLayoutItem *item = m_bookmarkGrid->takeAt(0)) {
        if (item->widget() == m_emptyBookmarks) {
            m_emptyBookmarks->hide();
        } else {
            delete item->widget();
        }
        delete item;
    }

    if (bookmarks.isEmpty()) {
        m_emptyBookmarks->setText(
            tr("Your saved pages will appear here. Bookmark a page with Ctrl+D."));
        m_emptyBookmarks->show();
        m_bookmarkGrid->addWidget(m_emptyBookmarks, 0, 0);
        return;
    }

    int index = 0;
    for (const Bookmark &bookmark : bookmarks) {
        auto *button = new QToolButton(this);
        button->setObjectName(QStringLiteral("ntpBookmark"));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIcon(QIcon(QStringLiteral(":/icons/globe.svg")));
        button->setIconSize(QSize(24, 24));
        button->setText(bookmark.title);
        button->setToolTip(bookmark.url.toDisplayString());
        button->setProperty("bookmarkUrl", bookmark.url);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumSize(142, 76);
        connect(button, &QToolButton::clicked, this,
                [this, url = bookmark.url] { emit bookmarkActivated(url); });

        m_bookmarkGrid->addWidget(button, index / 4, index % 4);
        ++index;
    }
}

void NewTabPage::updateDateTime()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QLocale locale;
    m_clock->setText(locale.toString(now.time(), QLocale::ShortFormat));
    m_date->setText(locale.toString(now.date(), QStringLiteral("dddd, d MMMM")));
}
