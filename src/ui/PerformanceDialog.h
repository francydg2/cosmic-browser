#pragma once

#include <QDialog>

class BrowserWindow;
class QCheckBox;
class QLabel;
class QSpinBox;
class QTableWidget;
class QTimer;

/// Performance Center: measured RAM/CPU of Cosmic's processes, the
/// state of every tab, and actions to free memory.
class PerformanceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PerformanceDialog(BrowserWindow *window);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void refresh();
    int selectedTab() const;

    BrowserWindow *m_window;
    QLabel *m_summary;
    QLabel *m_unsupported;
    QTableWidget *m_processes;
    QTableWidget *m_tabs;
    QTimer *m_timer;
    QList<int> m_rowToTab;
};
