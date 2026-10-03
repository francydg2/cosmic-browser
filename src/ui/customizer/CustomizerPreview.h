#pragma once

#include <QPoint>
#include <QWidget>

class LayoutEditor;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class NebulaTabBar;

class CustomizerPreview : public QWidget
{
    Q_OBJECT

public:
    explicit CustomizerPreview(LayoutEditor *editor, QWidget *parent = nullptr);

    void refresh();
    void setEditMode(bool on);
    bool editMode() const { return m_editMode; }

    bool hasToolbarSelection() const;
    int selectedZone() const { return m_selZone; }
    int selectedIndex() const { return m_selIndex; }
    bool tabsSelected() const { return m_selTabs; }
    bool toolbarSelected() const { return m_selToolbar; }
    QString selectedComponent() const;
    void clearSelection();
    void selectToolbar();
    QMenu *itemMenu(int zone, int index);

signals:
    void selectionChanged();
    void statusMessage(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuildChrome();
    QWidget *makeItemWidget(const QString &component, int zone, int index);
    void selectToolbarItem(int zone, int index);
    void selectTabs();
    void showItemMenu(const QPoint &globalPos, int zone, int index);
    int dropIndex(QWidget *zoneFrame, int zone, const QPoint &pos) const;
    void updateStyleSheet();
    void setDropActive(int zone);

    LayoutEditor *m_editor;
    bool m_editMode = true;
    QVBoxLayout *m_rootLayout = nullptr;
    QWidget *m_toolbarRow = nullptr;
    QWidget *m_zoneFrames[3] = {};
    QHBoxLayout *m_zoneLayouts[3] = {};
    NebulaTabBar *m_tabBar = nullptr;
    QWidget *m_contentArea = nullptr;

    int m_selZone = -1;
    int m_selIndex = -1;
    bool m_selTabs = false;
    bool m_selToolbar = false;

    QPoint m_pressPos;
    QWidget *m_pressWidget = nullptr;
    QPoint m_zonePressPos;
    int m_zonePressZone = -1;
    int m_dropZone = -1;
};
