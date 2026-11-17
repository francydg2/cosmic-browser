#include "ui/NewTabPage.h"
#include "core/Bookmarks.h"
#include "ui/SaturnIllustration.h"

#include <QDateTime>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QLocale>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QIcon bookmarkIcon(const Bookmark &bookmark)
{
    const QString key = bookmark.url.host().isEmpty()
        ? bookmark.title.trimmed()
        : bookmark.url.host();
    quint32 colorSeed = 0;
    for (QChar character : key.toLower()) {
        colorSeed = colorSeed * 33U + character.unicode();
    }

    QPixmap pixmap(36, 36);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor::fromHsv(static_cast<int>(colorSeed % 360U), 125, 210));
    painter.drawRoundedRect(pixmap.rect().adjusted(1, 1, -1, -1), 12, 12);
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(16);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter,
                     bookmark.title.trimmed().left(1).toUpper());
    return QIcon(pixmap);
}

} // namespace

NewTabPage::NewTabPage(QWidget *parent)
    : QWidget(parent)
    , m_search(new QLineEdit(this))
    , m_clock(new QLabel(this))
    , m_date(new QLabel(this))
    , m_privacyBadge(new QLabel(this))
    , m_emptyBookmarks(new QLabel(this))
    , m_bookmarkGrid(new QGridLayout)
{
    setObjectName(QStringLiteral("newTabPage"));
    setAttribute(Qt::WA_OpaquePaintEvent);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 26, 40, 30);
    layout->setSpacing(7);
    layout->addStretch(1);

    m_clock->setObjectName(QStringLiteral("ntpClock"));
    m_clock->setAlignment(Qt::AlignHCenter);

    m_date->setObjectName(QStringLiteral("ntpDate"));
    m_date->setAlignment(Qt::AlignHCenter);

    auto *art = new SaturnIllustration(this);
    art->setFixedSize(82, 82);
    layout->addWidget(art, 0, Qt::AlignHCenter);

    auto *caption = new QLabel(QStringLiteral("Cosmic"), this);
    caption->setObjectName(QStringLiteral("ntpCaption"));
    caption->setAlignment(Qt::AlignHCenter);
    layout->addWidget(caption, 0, Qt::AlignHCenter);

    auto *subtitle = new QLabel(tr("A calmer, more personal web"), this);
    subtitle->setObjectName(QStringLiteral("ntpSubtitle"));
    subtitle->setAlignment(Qt::AlignHCenter);
    layout->addWidget(subtitle);
    layout->addWidget(m_clock);
    layout->addWidget(m_date);
    layout->addSpacing(8);

    m_search->setObjectName(QStringLiteral("ntpSearch"));
    m_search->setPlaceholderText(tr("Search the web or enter an address"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(820);
    m_search->setMinimumHeight(62);
    m_search->addAction(QIcon(QStringLiteral(":/icons/search.svg")),
                        QLineEdit::LeadingPosition);
    layout->addWidget(m_search, 0, Qt::AlignHCenter);

    m_privacyBadge->setObjectName(QStringLiteral("ntpPrivacyBadge"));
    m_privacyBadge->setAlignment(Qt::AlignCenter);
    setPrivacyStatus(true, 0);
    layout->addWidget(m_privacyBadge, 0, Qt::AlignHCenter);

    auto *hint = new QLabel(tr("Tip: press Ctrl+L to jump to the address bar"), this);
    hint->setObjectName(QStringLiteral("ntpHint"));
    hint->setAlignment(Qt::AlignHCenter);
    layout->addWidget(hint);

    auto *bookmarksTitle = new QLabel(tr("Your bookmarks"), this);
    bookmarksTitle->setObjectName(QStringLiteral("ntpBookmarksTitle"));
    bookmarksTitle->setAlignment(Qt::AlignLeft);

    auto *bookmarksContent = new QWidget(this);
    bookmarksContent->setObjectName(QStringLiteral("ntpBookmarksContent"));
    m_bookmarkGrid->setContentsMargins(0, 0, 0, 0);
    m_bookmarkGrid->setHorizontalSpacing(16);
    m_bookmarkGrid->setVerticalSpacing(16);
    for (int column = 0; column < 5; ++column) {
        m_bookmarkGrid->setColumnStretch(column, 1);
    }
    bookmarksContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bookmarksContent->setLayout(m_bookmarkGrid);

    m_emptyBookmarks->setObjectName(QStringLiteral("ntpEmptyBookmarks"));
    m_emptyBookmarks->setAlignment(Qt::AlignCenter);
    m_emptyBookmarks->setWordWrap(true);

    auto *bookmarksScroll = new QScrollArea(this);
    bookmarksScroll->setObjectName(QStringLiteral("ntpBookmarksScroll"));
    bookmarksScroll->setWidgetResizable(true);
    bookmarksScroll->setFrameShape(QFrame::NoFrame);
    bookmarksScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bookmarksScroll->setMinimumHeight(190);
    bookmarksScroll->setMaximumHeight(330);
    bookmarksScroll->setWidget(bookmarksContent);

    auto *bookmarksSection = new QWidget(this);
    bookmarksSection->setObjectName(QStringLiteral("ntpBookmarksSection"));
    bookmarksSection->setMaximumWidth(1160);
    auto *bookmarksLayout = new QVBoxLayout(bookmarksSection);
    bookmarksLayout->setContentsMargins(18, 16, 18, 16);
    bookmarksLayout->setSpacing(12);
    bookmarksLayout->addWidget(bookmarksTitle);
    bookmarksLayout->addWidget(bookmarksScroll);
    layout->addSpacing(14);
    layout->addWidget(bookmarksSection, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    updateDateTime();
    auto *clockTimer = new QTimer(this);
    clockTimer->setInterval(60'000);
    connect(clockTimer, &QTimer::timeout, this, &NewTabPage::updateDateTime);
    clockTimer->start();

    connect(m_search, &QLineEdit::returnPressed, this,
            [this] { emit inputSubmitted(m_search->text()); });
}

void NewTabPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    if (width() <= 0 || height() <= 0) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient base(rect().topLeft(), rect().bottomRight());
    base.setColorAt(0.0, QColor(QStringLiteral("#171b2a")));
    base.setColorAt(0.52, QColor(QStringLiteral("#10141d")));
    base.setColorAt(1.0, QColor(QStringLiteral("#111923")));
    painter.fillRect(rect(), base);

    QRadialGradient violet(QPointF(width() * 0.16, height() * 0.12),
                           qMax(width(), height()) * 0.78);
    violet.setColorAt(0.0, QColor(117, 96, 220, 74));
    violet.setColorAt(0.42, QColor(82, 70, 160, 28));
    violet.setColorAt(1.0, QColor(82, 70, 160, 0));
    painter.fillRect(rect(), violet);

    QRadialGradient blue(QPointF(width() * 0.88, height() * 0.82),
                         qMax(width(), height()) * 0.62);
    blue.setColorAt(0.0, QColor(45, 122, 151, 42));
    blue.setColorAt(0.48, QColor(37, 85, 117, 17));
    blue.setColorAt(1.0, QColor(37, 85, 117, 0));
    painter.fillRect(rect(), blue);

    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 72; ++i) {
        const int x = (i * 193 + 61) % width();
        const int y = (i * 97 + 23) % height();
        if (x > width() * 0.28 && x < width() * 0.72
            && y > height() * 0.2 && y < height() * 0.82) {
            continue;
        }
        painter.setBrush(QColor(220, 225, 255, 18 + (i * 13) % 28));
        const qreal radius = i % 7 == 0 ? 1.15 : 0.65;
        painter.drawEllipse(QPointF(x, y), radius, radius);
    }
}

void NewTabPage::focusSearchField()
{
    m_search->setFocus();
    m_search->selectAll();
}

void NewTabPage::setPrivacyStatus(bool enabled, int blockedCount)
{
    m_privacyBadge->setProperty("protectionActive", enabled);
    m_privacyBadge->setText(
        enabled ? tr("Cosmic Shield is on · %1 requests blocked").arg(qMax(0, blockedCount))
                : tr("Cosmic Shield is paused"));
    m_privacyBadge->setToolTip(
        enabled ? tr("Known ad and tracker requests blocked this session.")
                : tr("Request blocking is disabled in Privacy settings."));
    m_privacyBadge->style()->unpolish(m_privacyBadge);
    m_privacyBadge->style()->polish(m_privacyBadge);
    m_privacyBadge->update();
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
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIcon(bookmarkIcon(bookmark));
        button->setIconSize(QSize(34, 34));
        button->setText(bookmark.title);
        button->setToolTip(bookmark.url.toDisplayString());
        button->setProperty("bookmarkUrl", bookmark.url);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumSize(180, 72);
        button->setMaximumSize(220, 84);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        connect(button, &QToolButton::clicked, this,
                [this, url = bookmark.url] { emit bookmarkActivated(url); });

        m_bookmarkGrid->addWidget(button, index / 5, index % 5);
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
