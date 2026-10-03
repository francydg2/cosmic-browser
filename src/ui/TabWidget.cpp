#include "ui/TabWidget.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionTab>

NebulaTabBar::NebulaTabBar(QWidget *parent)
    : QTabBar(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    connect(this, &QTabBar::currentChanged, this, &NebulaTabBar::updateCloseButtons);
}

bool NebulaTabBar::isVertical() const
{
    return shape() == QTabBar::RoundedWest || shape() == QTabBar::RoundedEast
        || shape() == QTabBar::TriangularWest || shape() == QTabBar::TriangularEast;
}

QSize NebulaTabBar::tabSizeHint(int index) const
{
    const QSize hint = QTabBar::tabSizeHint(index);
    if (!isVertical()) {
        return QSize(hint.width(), m_tabHeight);
    }
    return QSize(200, m_tabHeight);
}

void NebulaTabBar::setTabHeight(int height)
{
    const int clamped = qBound(28, height, 56);
    if (m_tabHeight == clamped) {
        return;
    }
    m_tabHeight = clamped;
    updateGeometry();
    update();
}

void NebulaTabBar::setTabRadius(int radius)
{
    const int clamped = qBound(0, radius, 16);
    if (m_tabRadius == clamped) {
        return;
    }
    m_tabRadius = clamped;
    update();
}

void NebulaTabBar::setTabSpacing(int spacing)
{
    const int clamped = qBound(0, spacing, 16);
    if (m_tabSpacing == clamped) {
        return;
    }
    m_tabSpacing = clamped;
    update();
}

void NebulaTabBar::setShowPlus(bool show)
{
    if (m_showPlus == show) {
        return;
    }
    m_showPlus = show;
    update();
}

void NebulaTabBar::paintEvent(QPaintEvent *event)
{
    if (!isVertical()) {
        QTabBar::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        paintNewTabButton(&painter);
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto closeSide = static_cast<QTabBar::ButtonPosition>(
        style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, this));

    for (int i = 0; i < count(); ++i) {
        if (!isTabVisible(i)) {
            continue;
        }
        const QRect rect = tabRect(i);
        if (rect.isEmpty()) {
            continue;
        }

        const bool selected = i == currentIndex();
        const bool hovered = i == m_hoverIndex && !selected;
        const int vm = m_tabSpacing / 2;
        const QRect pill = rect.adjusted(4, vm, -4, -vm);

        if (selected || hovered) {
            QPainterPath path;
            path.addRoundedRect(pill, m_tabRadius, m_tabRadius);
            painter.fillPath(path, QColor(selected ? QStringLiteral("#24272f")
                                                   : QStringLiteral("#1a1d24")));
            if (selected) {
                painter.setPen(QPen(QColor(QStringLiteral("#313643")), 1));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(pill, m_tabRadius, m_tabRadius);
            }
        }

        int textLeft = pill.left() + 13;
        int textRight = pill.right() - 13;
        if (QWidget *closeButton = tabButton(i, closeSide)) {
            if (closeButton->isVisible()) {
                textRight = qMin(textRight, closeButton->geometry().left() - 6);
            }
        }

        const QIcon icon = tabIcon(i);
        if (!icon.isNull()) {
            const int size = iconSize().width();
            const QRect iconRect(textLeft, pill.center().y() - size / 2, size, size);
            icon.paint(&painter, iconRect);
            textLeft = iconRect.right() + 7;
        }

        QColor textColor = tabTextColor(i);
        if (!textColor.isValid()) {
            textColor = QColor(selected ? QStringLiteral("#ffffff")
                                        : QStringLiteral("#9ba1ad"));
        }
        painter.setPen(textColor);
        QRect textRect(textLeft, pill.top(), qMax(0, textRight - textLeft), pill.height());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                         fontMetrics().elidedText(tabText(i), elideMode(),
                                                  textRect.width()));
    }
    paintNewTabButton(&painter);
}

void NebulaTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QTabBar::mouseMoveEvent(event);
    const int index = tabAt(event->position().toPoint());
    if (index != m_hoverIndex) {
        m_hoverIndex = index;
        updateCloseButtons();
        update();
    }
    const bool plus = newTabButtonRect().contains(event->position().toPoint());
    if (plus != m_plusHover) {
        m_plusHover = plus;
        update();
    }
}

void NebulaTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton
        && newTabButtonRect().contains(event->position().toPoint())) {
        emit newTabRequested();
        event->accept();
        return;
    }
    QTabBar::mousePressEvent(event);
}

void NebulaTabBar::leaveEvent(QEvent *event)
{
    QTabBar::leaveEvent(event);
    if (m_hoverIndex != -1) {
        m_hoverIndex = -1;
        updateCloseButtons();
        update();
    }
    if (m_plusHover) {
        m_plusHover = false;
        update();
    }
}

void NebulaTabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    updateCloseButtons();
}

QRect NebulaTabBar::newTabButtonRect() const
{
    const int last = count() - 1;
    if (last < 0 || !m_showPlus) {
        return {};
    }
    const QRect rect = tabRect(last);
    if (rect.isEmpty()) {
        return {};
    }
    constexpr int size = 28;
    int x = 0;
    int y = 0;
    if (isVertical()) {
        x = (width() - size) / 2;
        y = height() - size - 4;
        if (y < 4) {
            return {};
        }
    } else {
        x = rect.right() + 5;
        y = rect.center().y() - size / 2;
        x = qMin(x, width() - size - 4);
        x = qMax(x, 4);
    }
    return {x, y, size, size};
}

void NebulaTabBar::paintNewTabButton(QPainter *painter)
{
    const QRect rect = newTabButtonRect();
    if (rect.isEmpty()) {
        return;
    }
    QPainterPath path;
    path.addRoundedRect(rect, 9, 9);
    painter->fillPath(path, QColor(m_plusHover ? QStringLiteral("#24272f")
                                               : QStringLiteral("#1b1d24")));
    painter->setPen(QPen(QColor(m_plusHover ? QStringLiteral("#ffffff")
                                            : QStringLiteral("#9ba1ad")),
                          1.8));
    const QPoint center = rect.center();
    painter->drawLine(center.x() - 6, center.y(), center.x() + 6, center.y());
    painter->drawLine(center.x(), center.y() - 6, center.x(), center.y() + 6);
}
void NebulaTabBar::updateCloseButtons()
{
    const auto closeSide = static_cast<QTabBar::ButtonPosition>(
        style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, this));
    for (int i = 0; i < count(); ++i) {
        QWidget *button = tabButton(i, closeSide);
        if (!button) {
            continue;
        }
        button->setVisible(true);
    }
}

NebulaTabWidget::NebulaTabWidget(QWidget *parent)
    : QTabWidget(parent)
{
    setTabBar(new NebulaTabBar(this));
}

void NebulaTabWidget::tabInserted(int index)
{
    QTabWidget::tabInserted(index);
    emit tabsLayoutChanged();
}

void NebulaTabWidget::tabRemoved(int index)
{
    QTabWidget::tabRemoved(index);
    emit tabsLayoutChanged();
}
