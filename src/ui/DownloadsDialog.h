#pragma once

#include <QDialog>
#include <QPointer>
#include <QVector>

class DownloadManager;
class QWebEngineDownloadRequest;
class QTableWidget;
class QPushButton;

class DownloadsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadsDialog(DownloadManager *manager, QWidget *parent = nullptr);

private:
    void refresh();
    void updateButtons();
    QWebEngineDownloadRequest *selectedRequest() const;

    DownloadManager *m_manager;
    QTableWidget *m_table;
    QPushButton *m_open;
    QPushButton *m_folder;
    QPushButton *m_cancel;
    QPushButton *m_resume;
    QVector<QPointer<QWebEngineDownloadRequest>> m_items;
};
