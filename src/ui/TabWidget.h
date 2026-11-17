#pragma once

#include <QTabBar>
#include <QTabWidget>

class NebulaTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit NebulaTabBar(QWidget *parent = nullptr);

    QSize tabSizeHint(int index) const override;
    void updateCloseButtons();
    QRect newTabButtonRect() const;

    void setTabHeight(int height);
    void setVerticalTabWidth(int width);
    int verticalTabWidth() const { return m_tabWidth; }
    void setTabRadius(int radius);
    void setTabSpacing(int spacing);
    void setShowPlus(bool show);

signals:
    void newTabRequested();
    void verticalTabWidthResized(int width);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void tabInserted(int index) override;

private:
    bool isVertical() const;
    bool isEast() const;
    bool isResizeHandle(const QPoint &position) const;
    void paintNewTabButton(QPainter *painter);
    int m_hoverIndex = -1;
    bool m_plusHover = false;
    int m_tabHeight = 36;
    int m_tabWidth = 250;
    int m_tabRadius = 9;
    int m_tabSpacing = 6;
    bool m_showPlus = true;
    bool m_resizing = false;
    bool m_resizeHover = false;
    int m_resizeStartX = 0;
    int m_resizeStartWidth = 250;
};

class NebulaTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit NebulaTabWidget(QWidget *parent = nullptr);

    using QTabWidget::setTabBar;

signals:
    void tabsLayoutChanged();

protected:
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
};
