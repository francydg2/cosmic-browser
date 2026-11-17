#pragma once

#include <QDialog>

class BlocklistInterceptor;
class QLabel;
class QTableWidget;
class QTimer;

class TrackerRadarDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TrackerRadarDialog(BlocklistInterceptor *interceptor, QWidget *parent = nullptr);

private:
    void refresh();
    BlocklistInterceptor *m_interceptor;
    QLabel *m_summary;
    QTableWidget *m_table;
    QTimer *m_refreshTimer;
};
