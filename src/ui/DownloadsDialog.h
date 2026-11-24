#pragma once

#include "core/DownloadHistory.h"

#include <QDialog>
#include <QPointer>
#include <QVector>

class DownloadManager;
class QWebEngineDownloadRequest;
class QTableWidget;
class QPushButton;
class QLineEdit;
class QComboBox;
class QLabel;
class QTimer;

/// Download Center: live transfers plus the persistent history, with
/// search, state filters, speed and remaining time.
class DownloadsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadsDialog(DownloadManager *manager, QWidget *parent = nullptr);

    /// Compact "1.2 MB/s" style text; empty when not transferring.
    static QString speedText(double bytesPerSecond);
    /// "2 min 5 s" style text; "—" when unknown.
    static QString durationText(qint64 seconds);

private:
    struct Row
    {
        QPointer<QWebEngineDownloadRequest> request; // null for saved history
        DownloadRecord record;                        // used when request is null
        QString id;
    };

    void refresh();
    void updateButtons();
    const Row *selectedRow() const;
    QString stateKey(const Row &row) const;

    DownloadManager *m_manager;
    QTableWidget *m_table;
    QLineEdit *m_search;
    QComboBox *m_filter;
    QLabel *m_empty;
    QPushButton *m_open;
    QPushButton *m_folder;
    QPushButton *m_pause;
    QPushButton *m_cancel;
    QPushButton *m_resume;
    QPushButton *m_remove;
    QTimer *m_refreshTimer;
    QVector<Row> m_rows;
    qsizetype m_listSize = -1; // live + saved entries at the last refresh
};
