#include "ui/DownloadsDialog.h"

#include "browser/DownloadManager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMetaEnum>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>

DownloadsDialog::DownloadsDialog(DownloadManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_table(new QTableWidget(this))
    , m_open(new QPushButton(tr("Open file"), this))
    , m_folder(new QPushButton(tr("Open folder"), this))
    , m_cancel(new QPushButton(tr("Cancel download"), this))
    , m_resume(new QPushButton(tr("Resume"), this))
{
    setObjectName(QStringLiteral("downloadsDialog"));
    setWindowTitle(tr("Downloads"));
    resize(760, 420);
    auto *layout = new QVBoxLayout(this);
    auto *heading = new QLabel(tr("Downloads"), this);
    QFont font = heading->font();
    font.setPointSize(font.pointSize() + 4);
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
    auto *hint = new QLabel(tr("Downloads from this session appear here."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_table->setObjectName(QStringLiteral("downloadsTable"));
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("File"), tr("Status"), tr("Progress"), tr("Location")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table, 1);

    auto *buttons = new QHBoxLayout;
    auto *clear = new QPushButton(tr("Clear finished"), this);
    m_open->setObjectName(QStringLiteral("openDownloadButton"));
    m_folder->setObjectName(QStringLiteral("openDownloadFolderButton"));
    m_cancel->setObjectName(QStringLiteral("cancelDownloadButton"));
    m_resume->setObjectName(QStringLiteral("resumeDownloadButton"));
    buttons->addWidget(m_open);
    buttons->addWidget(m_folder);
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_resume);
    buttons->addStretch();
    buttons->addWidget(clear);
    layout->addLayout(buttons);

    connect(m_manager, &DownloadManager::changed, this, &DownloadsDialog::refresh);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &DownloadsDialog::updateButtons);
    connect(clear, &QPushButton::clicked, m_manager, &DownloadManager::clearFinished);
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        if (auto *request = selectedRequest()) {
            request->cancel();
        }
    });
    connect(m_resume, &QPushButton::clicked, this, [this] {
        m_manager->resume(selectedRequest());
    });
    connect(m_open, &QPushButton::clicked, this, [this] {
        auto *request = selectedRequest();
        if (!request || request->state() != QWebEngineDownloadRequest::DownloadCompleted) {
            return;
        }
        const QString path = QDir(request->downloadDirectory()).filePath(request->downloadFileName());
        if (!QFileInfo::exists(path)
            || !QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
            QMessageBox::warning(this, tr("Downloads"), tr("Could not open the downloaded file."));
        }
    });
    connect(m_folder, &QPushButton::clicked, this, [this] {
        auto *request = selectedRequest();
        if (request && !QDesktopServices::openUrl(QUrl::fromLocalFile(request->downloadDirectory()))) {
            QMessageBox::warning(this, tr("Downloads"), tr("Could not open the download folder."));
        }
    });
    refresh();
}

QWebEngineDownloadRequest *DownloadsDialog::selectedRequest() const
{
    const int row = m_table->currentRow();
    return row >= 0 && row < m_items.size() ? m_items.at(row).data() : nullptr;
}

void DownloadsDialog::updateButtons()
{
    const auto *request = selectedRequest();
    const bool active = request && request->state() == QWebEngineDownloadRequest::DownloadInProgress;
    m_cancel->setEnabled(active);
    m_resume->setEnabled(request && !request->isFinished()
                         && (request->isPaused()
                             || request->state() == QWebEngineDownloadRequest::DownloadInterrupted));
    m_open->setEnabled(request && request->state() == QWebEngineDownloadRequest::DownloadCompleted);
    m_folder->setEnabled(request);
}

void DownloadsDialog::refresh()
{
    QPointer<QWebEngineDownloadRequest> selected = selectedRequest();
    const QSignalBlocker blocker(m_table);
    const auto items = m_manager->downloads();
    const bool rowsChanged = m_items != items;
    if (rowsChanged) {
        m_table->clearContents();
        m_items = items;
        m_table->setRowCount(m_items.size());
    }
    int selectedRow = -1;
    for (int row = 0; row < m_items.size(); ++row) {
        auto *request = m_items.at(row).data();
        if (!request) {
            continue;
        }
        QString state;
        switch (request->state()) {
        case QWebEngineDownloadRequest::DownloadRequested:
            state = tr("Waiting");
            break;
        case QWebEngineDownloadRequest::DownloadInProgress:
            state = request->totalBytes() > 0
                ? tr("%1%").arg(static_cast<int>(100.0 * request->receivedBytes() / request->totalBytes()))
                : tr("%1 received").arg(QLocale().formattedDataSize(request->receivedBytes()));
            break;
        case QWebEngineDownloadRequest::DownloadCompleted:
            state = tr("Completed");
            break;
        case QWebEngineDownloadRequest::DownloadCancelled:
            state = tr("Cancelled");
            break;
        case QWebEngineDownloadRequest::DownloadInterrupted:
            if (QMetaEnum::fromType<QWebEngineDownloadRequest::DownloadInterruptReason>()
                    .valueToKey(static_cast<int>(request->interruptReason()))) {
                state = tr("Interrupted: %1").arg(request->interruptReasonString());
            } else {
                state = tr("Transfer interrupted");
            }
            break;
        }
        if (rowsChanged) {
            m_table->setItem(row, 0, new QTableWidgetItem(request->downloadFileName()));
            m_table->setItem(row, 1, new QTableWidgetItem());
            auto *progress = new QProgressBar(m_table);
            progress->setObjectName(QStringLiteral("downloadProgress"));
            progress->setMinimumWidth(120);
            progress->setMaximumHeight(9);
            progress->setTextVisible(false);
            progress->setStyleSheet(QStringLiteral(
                "QProgressBar { background: #303443; border: 0; border-radius: 4px; }"
                "QProgressBar::chunk { background: #7c6cf0; border-radius: 4px; }"));
            m_table->setCellWidget(row, 2, progress);
            m_table->setItem(row, 3, new QTableWidgetItem(request->downloadDirectory()));
        }
        m_table->item(row, 1)->setText(state);
        auto *progress = qobject_cast<QProgressBar *>(m_table->cellWidget(row, 2));
        if (request->totalBytes() > 0) {
            progress->setRange(0, 1000);
            progress->setValue(qBound(0, static_cast<int>(1000.0 * request->receivedBytes()
                                                         / request->totalBytes()), 1000));
        } else if (request->state() == QWebEngineDownloadRequest::DownloadInProgress) {
            progress->setRange(0, 0);
        } else {
            progress->setRange(0, 1000);
            progress->setValue(request->state() == QWebEngineDownloadRequest::DownloadCompleted ? 1000 : 0);
        }
        if (selected == request) {
            selectedRow = row;
        }
    }
    if (selectedRow >= 0) {
        m_table->selectRow(selectedRow);
    }
    updateButtons();
}
