#include "ui/DownloadsDialog.h"

#include "browser/DownloadManager.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QMetaEnum>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>

namespace {

enum Column { ColFile, ColStatus, ColProgress, ColSize, ColSpeed, ColRemaining, ColLocation,
              ColumnCount };

QString liveStateText(const QWebEngineDownloadRequest *request)
{
    using R = QWebEngineDownloadRequest;
    switch (request->state()) {
    case R::DownloadRequested:
        return DownloadsDialog::tr("Waiting");
    case R::DownloadInProgress:
        if (request->isPaused()) {
            return DownloadsDialog::tr("Paused");
        }
        return request->totalBytes() > 0
            ? DownloadsDialog::tr("%1%").arg(static_cast<int>(
                  100.0 * request->receivedBytes() / request->totalBytes()))
            : DownloadsDialog::tr("%1 received").arg(
                  QLocale().formattedDataSize(request->receivedBytes()));
    case R::DownloadCompleted:
        return DownloadsDialog::tr("Completed");
    case R::DownloadCancelled:
        return DownloadsDialog::tr("Cancelled");
    case R::DownloadInterrupted:
        if (QMetaEnum::fromType<R::DownloadInterruptReason>()
                .valueToKey(static_cast<int>(request->interruptReason()))) {
            return DownloadsDialog::tr("Interrupted: %1").arg(request->interruptReasonString());
        }
        return DownloadsDialog::tr("Transfer interrupted");
    }
    return {};
}

QString recordStateText(const DownloadRecord &record)
{
    if (record.state == QLatin1String("completed")) {
        return DownloadsDialog::tr("Completed");
    }
    if (record.state == QLatin1String("cancelled")) {
        return DownloadsDialog::tr("Cancelled");
    }
    return record.error.isEmpty() ? DownloadsDialog::tr("Failed")
                                  : DownloadsDialog::tr("Failed: %1").arg(record.error);
}

} // namespace

QString DownloadsDialog::speedText(double bytesPerSecond)
{
    return bytesPerSecond < 1 ? QString()
                              : tr("%1/s").arg(QLocale().formattedDataSize(
                                    static_cast<qint64>(bytesPerSecond)));
}

QString DownloadsDialog::durationText(qint64 seconds)
{
    if (seconds < 0) {
        return QStringLiteral("—");
    }
    if (seconds < 60) {
        return tr("%1 s").arg(seconds);
    }
    if (seconds < 3600) {
        return tr("%1 min %2 s").arg(seconds / 60).arg(seconds % 60);
    }
    return tr("%1 h %2 min").arg(seconds / 3600).arg((seconds % 3600) / 60);
}

DownloadsDialog::DownloadsDialog(DownloadManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_table(new QTableWidget(this))
    , m_search(new QLineEdit(this))
    , m_filter(new QComboBox(this))
    , m_empty(new QLabel(this))
    , m_open(new QPushButton(tr("Open file"), this))
    , m_folder(new QPushButton(tr("Open folder"), this))
    , m_pause(new QPushButton(tr("Pause"), this))
    , m_cancel(new QPushButton(tr("Cancel"), this))
    , m_resume(new QPushButton(tr("Resume"), this))
    , m_remove(new QPushButton(tr("Remove from list"), this))
    , m_refreshTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("downloadsDialog"));
    setWindowTitle(tr("Download Center"));
    resize(940, 480);
    auto *layout = new QVBoxLayout(this);
    auto *heading = new QLabel(tr("Download Center"), this);
    QFont font = heading->font();
    font.setPointSize(font.pointSize() + 4);
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);

    auto *filters = new QHBoxLayout;
    m_search->setObjectName(QStringLiteral("downloadsSearch"));
    m_search->setPlaceholderText(tr("Search by file name, address or folder"));
    m_search->setClearButtonEnabled(true);
    m_filter->setObjectName(QStringLiteral("downloadsFilter"));
    m_filter->addItem(tr("All"), QString());
    m_filter->addItem(tr("Active"), QStringLiteral("active"));
    m_filter->addItem(tr("Completed"), QStringLiteral("completed"));
    m_filter->addItem(tr("Failed"), QStringLiteral("failed"));
    m_filter->addItem(tr("Cancelled"), QStringLiteral("cancelled"));
    filters->addWidget(m_search, 1);
    filters->addWidget(m_filter);
    layout->addLayout(filters);

    m_table->setObjectName(QStringLiteral("downloadsTable"));
    m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({tr("File"), tr("Status"), tr("Progress"), tr("Size"),
                                        tr("Speed"), tr("Remaining"), tr("Location")});
    m_table->horizontalHeader()->setSectionResizeMode(ColFile, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColLocation, QHeaderView::Stretch);
    for (int column : {ColStatus, ColProgress, ColSize, ColSpeed, ColRemaining}) {
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table, 1);
    m_empty->setObjectName(QStringLiteral("downloadsEmpty"));
    m_empty->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_empty);

    auto *buttons = new QHBoxLayout;
    auto *clear = new QPushButton(tr("Clear history…"), this);
    m_open->setObjectName(QStringLiteral("openDownloadButton"));
    m_folder->setObjectName(QStringLiteral("openDownloadFolderButton"));
    m_pause->setObjectName(QStringLiteral("pauseDownloadButton"));
    m_cancel->setObjectName(QStringLiteral("cancelDownloadButton"));
    m_resume->setObjectName(QStringLiteral("resumeDownloadButton"));
    m_remove->setObjectName(QStringLiteral("removeDownloadButton"));
    clear->setObjectName(QStringLiteral("clearDownloadsButton"));
    for (QPushButton *button : {m_open, m_folder, m_pause, m_resume, m_cancel, m_remove}) {
        buttons->addWidget(button);
    }
    buttons->addStretch();
    buttons->addWidget(clear);
    layout->addLayout(buttons);

    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(150);
    connect(m_refreshTimer, &QTimer::timeout, this, &DownloadsDialog::refresh);
    connect(m_manager, &DownloadManager::changed, this, [this] {
        // Rows added or removed show at once; progress ticks are coalesced.
        if (m_manager->downloads().size() + m_manager->historyCount() != m_listSize) {
            m_refreshTimer->stop();
            refresh();
        } else if (!m_refreshTimer->isActive()) {
            m_refreshTimer->start();
        }
    });
    connect(m_search, &QLineEdit::textChanged, this, &DownloadsDialog::refresh);
    connect(m_filter, &QComboBox::currentIndexChanged, this, &DownloadsDialog::refresh);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &DownloadsDialog::updateButtons);
    connect(clear, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Download Center"),
                tr("Clear the download history? Downloaded files are not deleted. "
                   "Active transfers keep running."))
            == QMessageBox::Yes) {
            m_manager->clearFinished();
        }
    });
    connect(m_pause, &QPushButton::clicked, this, [this] {
        if (const Row *row = selectedRow()) {
            m_manager->pause(row->request);
        }
    });
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        if (const Row *row = selectedRow(); row && row->request) {
            row->request->cancel();
        }
    });
    connect(m_resume, &QPushButton::clicked, this, [this] {
        if (const Row *row = selectedRow()) {
            m_manager->resume(row->request);
        }
    });
    connect(m_remove, &QPushButton::clicked, this, [this] {
        if (const Row *row = selectedRow()) {
            m_manager->removeRecord(row->id);
        }
    });
    connect(m_open, &QPushButton::clicked, this, [this] {
        const Row *row = selectedRow();
        if (!row) {
            return;
        }
        const QString directory = row->request ? row->request->downloadDirectory()
                                               : row->record.directory;
        const QString name = row->request ? row->request->downloadFileName()
                                          : row->record.fileName;
        const QString path = QDir(directory).filePath(name);
        if (!QFileInfo::exists(path)
            || !QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
            QMessageBox::warning(this, tr("Download Center"),
                                 tr("Could not open the file. It may have been moved or deleted."));
        }
    });
    connect(m_folder, &QPushButton::clicked, this, [this] {
        const Row *row = selectedRow();
        if (!row) {
            return;
        }
        const QString directory = row->request ? row->request->downloadDirectory()
                                               : row->record.directory;
        if (!QFileInfo(directory).isDir()
            || !QDesktopServices::openUrl(QUrl::fromLocalFile(directory))) {
            QMessageBox::warning(this, tr("Download Center"),
                                 tr("Could not open the download folder."));
        }
    });
    refresh();
}

const DownloadsDialog::Row *DownloadsDialog::selectedRow() const
{
    const int row = m_table->currentRow();
    return row >= 0 && row < m_rows.size() ? &m_rows.at(row) : nullptr;
}

QString DownloadsDialog::stateKey(const Row &row) const
{
    using R = QWebEngineDownloadRequest;
    if (!row.request) {
        return row.record.state;
    }
    switch (row.request->state()) {
    case R::DownloadCompleted:
        return QStringLiteral("completed");
    case R::DownloadCancelled:
        return QStringLiteral("cancelled");
    case R::DownloadInterrupted:
        return row.request->isFinished() ? QStringLiteral("failed") : QStringLiteral("active");
    default:
        return QStringLiteral("active");
    }
}

void DownloadsDialog::updateButtons()
{
    const Row *row = selectedRow();
    const auto *request = row ? row->request.data() : nullptr;
    using R = QWebEngineDownloadRequest;
    m_pause->setEnabled(request && request->state() == R::DownloadInProgress
                        && !request->isPaused());
    m_cancel->setEnabled(request && !request->isFinished());
    m_resume->setEnabled(request && !request->isFinished()
                         && (request->isPaused() || request->state() == R::DownloadInterrupted));
    m_open->setEnabled(row && stateKey(*row) == QLatin1String("completed"));
    m_folder->setEnabled(row);
    m_remove->setEnabled(row && stateKey(*row) != QLatin1String("active"));
}

void DownloadsDialog::refresh()
{
    m_listSize = m_manager->downloads().size() + m_manager->historyCount();
    const QString selectedId = selectedRow() ? selectedRow()->id : QString();
    const QString query = m_search->text().trimmed();
    const QString stateFilter = m_filter->currentData().toString();

    QVector<Row> rows;
    QSet<QString> liveIds;
    for (const auto &pointer : m_manager->downloads()) {
        if (!pointer) {
            continue;
        }
        Row row;
        row.request = pointer;
        row.id = m_manager->recordId(pointer);
        liveIds.insert(row.id);
        rows.append(row);
    }
    for (const DownloadRecord &record : m_manager->history()) {
        if (!liveIds.contains(record.id)) {
            Row row;
            row.record = record;
            row.id = record.id;
            rows.append(row);
        }
    }
    QVector<Row> visible;
    for (const Row &row : std::as_const(rows)) {
        if (!stateFilter.isEmpty() && stateKey(row) != stateFilter) {
            continue;
        }
        if (!query.isEmpty()) {
            const QString name = row.request ? row.request->downloadFileName() : row.record.fileName;
            const QString url = row.request ? row.request->url().toString() : row.record.url.toString();
            const QString dir = row.request ? row.request->downloadDirectory() : row.record.directory;
            if (!name.contains(query, Qt::CaseInsensitive)
                && !url.contains(query, Qt::CaseInsensitive)
                && !dir.contains(query, Qt::CaseInsensitive)) {
                continue;
            }
        }
        visible.append(row);
    }

    bool structureChanged = visible.size() != m_rows.size();
    for (int i = 0; !structureChanged && i < visible.size(); ++i) {
        structureChanged = visible.at(i).id != m_rows.at(i).id
            || visible.at(i).request.isNull() != m_rows.at(i).request.isNull();
    }
    const QSignalBlocker blocker(m_table);
    m_rows = visible;
    if (structureChanged) {
        m_table->clearContents();
        m_table->setRowCount(m_rows.size());
    }
    int selected = -1;
    for (int r = 0; r < m_rows.size(); ++r) {
        const Row &row = m_rows.at(r);
        const bool live = !row.request.isNull();
        const QString name = live ? row.request->downloadFileName() : row.record.fileName;
        const QString dir = live ? row.request->downloadDirectory() : row.record.directory;
        const qint64 total = live ? row.request->totalBytes() : row.record.totalBytes;
        const qint64 received = live ? row.request->receivedBytes() : row.record.receivedBytes;
        const bool active = live && stateKey(row) == QLatin1String("active")
            && row.request->state() == QWebEngineDownloadRequest::DownloadInProgress
            && !row.request->isPaused();
        if (structureChanged) {
            for (int c = 0; c < ColumnCount; ++c) {
                if (c != ColProgress) {
                    m_table->setItem(r, c, new QTableWidgetItem());
                }
            }
            auto *progress = new QProgressBar(m_table);
            progress->setObjectName(QStringLiteral("downloadProgress"));
            progress->setMinimumWidth(110);
            progress->setMaximumHeight(9);
            progress->setTextVisible(false);
            progress->setStyleSheet(QStringLiteral(
                "QProgressBar { background: #303443; border: 0; border-radius: 4px; }"
                "QProgressBar::chunk { background: #7c6cf0; border-radius: 4px; }"));
            m_table->setCellWidget(r, ColProgress, progress);
        }
        m_table->item(r, ColFile)->setText(name);
        m_table->item(r, ColFile)->setToolTip(live ? row.request->url().toString()
                                                   : row.record.url.toString());
        m_table->item(r, ColStatus)->setText(live ? liveStateText(row.request)
                                                  : recordStateText(row.record));
        m_table->item(r, ColSize)->setText(total > 0 ? QLocale().formattedDataSize(total)
            : (received > 0 ? QLocale().formattedDataSize(received) : QStringLiteral("—")));
        m_table->item(r, ColSpeed)->setText(active ? speedText(m_manager->bytesPerSecond(row.request))
                                                   : QString());
        m_table->item(r, ColRemaining)->setText(
            active ? durationText(m_manager->secondsRemaining(row.request)) : QString());
        m_table->item(r, ColLocation)->setText(dir);
        auto *progress = qobject_cast<QProgressBar *>(m_table->cellWidget(r, ColProgress));
        const bool done = stateKey(row) == QLatin1String("completed");
        if (total > 0) {
            progress->setRange(0, 1000);
            progress->setValue(qBound(0, static_cast<int>(1000.0 * received / total), 1000));
        } else if (active) {
            progress->setRange(0, 0);
        } else {
            progress->setRange(0, 1000);
            progress->setValue(done ? 1000 : 0);
        }
        if (row.id == selectedId) {
            selected = r;
        }
    }
    if (selected >= 0) {
        m_table->selectRow(selected);
    }
    const bool empty = m_rows.isEmpty();
    m_table->setVisible(!empty);
    m_empty->setVisible(empty);
    m_empty->setText(rows.isEmpty() ? tr("No downloads yet. Files you download appear here.")
                                    : tr("No downloads match your search or filter."));
    updateButtons();
}
