#pragma once

#include "core/TabAnalysis.h"

#include <QDialog>

class BrowserWindow;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimer;

/// Smart Tab Manager of one window: search, duplicates, group
/// suggestions, inactive tabs, group rename/move, workspaces.
class TabManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TabManagerDialog(BrowserWindow *window);

private:
    void refresh();
    int selectedTabIndex() const;
    void closeDuplicates();
    void showSuggestions();
    void suspendInactive();
    void moveSelectedToGroup(const QString &groupId);
    void renameSelectedGroup();
    void updateButtons();

    BrowserWindow *m_window;
    QLineEdit *m_search;
    QTableWidget *m_table;
    QLabel *m_summary;
    QPushButton *m_duplicates;
    QPushButton *m_suggestions;
    QPushButton *m_suspend;
    QPushButton *m_groupMenu;
    QSpinBox *m_minutes;
    QTimer *m_timer;
    QList<TabInfo> m_infos;
    QList<int> m_rowToTab;
};
