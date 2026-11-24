#include "ui/PerformanceDialog.h"

#include "browser/Browser.h"
#include "browser/PerformanceMonitor.h"
#include "core/Settings.h"
#include "ui/BrowserWindow.h"
#include "ui/TabContent.h"
#include "web/WebPage.h"
#include "web/WebView.h"

#include <algorithm>
#include <QCheckBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QTableWidget *makeTable(const QStringList &headers, QWidget *parent)
{
    auto *table = new QTableWidget(parent);
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < headers.size(); ++c) {
        table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    return table;
}

void setCell(QTableWidget *table, int row, int column, const QString &text,
             Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *item = table->item(row, column);
    if (!item) {
        item = new QTableWidgetItem;
        table->setItem(row, column, item);
    }
    item->setText(text);
    item->setTextAlignment(alignment);
}

QString kindLabel(const QString &kind)
{
    if (kind == QLatin1String("browser")) {
        return PerformanceDialog::tr("Browser");
    }
    if (kind == QLatin1String("renderer")) {
        return PerformanceDialog::tr("Page renderer");
    }
    if (kind == QLatin1String("gpu")) {
        return PerformanceDialog::tr("Graphics (GPU)");
    }
    if (kind == QLatin1String("utility")) {
        return PerformanceDialog::tr("Utility");
    }
    return PerformanceDialog::tr("Other");
}

} // namespace

PerformanceDialog::PerformanceDialog(BrowserWindow *window)
    : QDialog(window)
    , m_window(window)
    , m_summary(new QLabel(this))
    , m_unsupported(new QLabel(this))
    , m_processes(makeTable({tr("Process"), tr("PID"), tr("Memory"), tr("CPU")}, this))
    , m_tabs(makeTable({tr("Tab"), tr("State"), tr("Memory"), tr("Idle")}, this))
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("performanceDialog"));
    setWindowTitle(tr("Performance Center"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(780, 600);
    auto *layout = new QVBoxLayout(this);
    m_summary->setObjectName(QStringLiteral("performanceSummary"));
    QFont font = m_summary->font();
    font.setBold(true);
    m_summary->setFont(font);
    m_unsupported->setObjectName(QStringLiteral("performanceUnsupported"));
    m_unsupported->setWordWrap(true);
    m_unsupported->setStyleSheet(QStringLiteral("color: #e5a93d;"));
    layout->addWidget(m_summary);
    layout->addWidget(m_unsupported);

    m_processes->setObjectName(QStringLiteral("performanceProcesses"));
    m_tabs->setObjectName(QStringLiteral("performanceTabs"));
    layout->addWidget(new QLabel(tr("Processes"), this));
    layout->addWidget(m_processes, 1);
    layout->addWidget(new QLabel(tr("Tabs"), this));
    layout->addWidget(m_tabs, 1);

    auto *buttons = new QHBoxLayout;
    auto *suspend = new QPushButton(tr("Suspend selected tab"), this);
    auto *suspendAll = new QPushButton(tr("Suspend all background tabs"), this);
    suspend->setObjectName(QStringLiteral("performanceSuspend"));
    suspendAll->setObjectName(QStringLiteral("performanceSuspendAll"));
    buttons->addWidget(suspend);
    buttons->addWidget(suspendAll);
    buttons->addStretch();
    layout->addLayout(buttons);

    Browser *browser = nullptr;
    // The window owns its Browser; reach it through the friend access.
    browser = m_window->m_browser;
    auto *settingsRow = new QHBoxLayout;
    auto *ramSaver = new QCheckBox(tr("RAM Saver: suspend background tabs automatically"), this);
    ramSaver->setObjectName(QStringLiteral("performanceRamSaver"));
    ramSaver->setChecked(browser->settings()->ramSaverEnabled());
    connect(ramSaver, &QCheckBox::toggled, this, [browser](bool on) {
        browser->settings()->setRamSaverEnabled(on);
        browser->settings()->save();
    });
    settingsRow->addWidget(ramSaver);
    layout->addLayout(settingsRow);

    auto *warnRow = new QHBoxLayout;
    auto *warn = new QCheckBox(tr("Warn when Cosmic uses more than"), this);
    warn->setObjectName(QStringLiteral("performanceWarn"));
    auto *limit = new QSpinBox(this);
    limit->setObjectName(QStringLiteral("performanceLimit"));
    limit->setRange(256, 262144);
    limit->setSingleStep(256);
    limit->setSuffix(tr(" MB"));
    PerformanceMonitor *monitor = browser->performance();
    warn->setChecked(monitor->warningEnabled());
    limit->setValue(monitor->warningMegabytes());
    warnRow->addWidget(warn);
    warnRow->addWidget(limit);
    warnRow->addStretch();
    layout->addLayout(warnRow);
    const auto applyWarning = [monitor, warn, limit] {
        monitor->setWarning(warn->isChecked(), limit->value());
    };
    connect(warn, &QCheckBox::toggled, this, applyWarning);
    connect(limit, &QSpinBox::editingFinished, this, applyWarning);

    connect(suspend, &QPushButton::clicked, this, [this] {
        const int index = selectedTab();
        if (index < 0) {
            return;
        }
        if (m_window->suspendTab(index)) {
            m_window->notify(tr("Tab suspended. It reloads when you open it."), QStringLiteral("success"));
        } else {
            m_window->notify(tr("This tab cannot be suspended: it is the current tab, is playing "
                                "audio, is loading, or is not a web page."), QStringLiteral("warning"));
        }
        refresh();
    });
    connect(suspendAll, &QPushButton::clicked, this, [this] {
        int count = 0;
        for (int i = 0; i < m_window->tabWidget()->count(); ++i) {
            count += m_window->suspendTab(i) ? 1 : 0;
        }
        m_window->notify(tr("%1 background tab(s) suspended.").arg(count), QStringLiteral("success"));
        refresh();
    });
    m_timer->setInterval(2000);
    connect(m_timer, &QTimer::timeout, this, &PerformanceDialog::refresh);
    refresh();
}

void PerformanceDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_timer->start();
    refresh();
}

void PerformanceDialog::hideEvent(QHideEvent *event)
{
    m_timer->stop();
    QDialog::hideEvent(event);
}

int PerformanceDialog::selectedTab() const
{
    const int row = m_tabs->currentRow();
    return row >= 0 && row < m_rowToTab.size() ? m_rowToTab.at(row) : -1;
}

void PerformanceDialog::refresh()
{
    const auto snapshot = m_window->m_browser->performance()->sampleNow();
    QLocale locale;
    const QList<TabInfo> infos = m_window->tabInfos();
    const int suspended = std::count_if(infos.cbegin(), infos.cend(),
                                        [](const TabInfo &t) { return t.suspended; });
    QString summary = tr("%1 tabs · %2 suspended").arg(infos.size()).arg(suspended);
    if (snapshot.supported) {
        summary = tr("%1 processes · %2 memory · CPU %3 · ").arg(snapshot.processes.size())
                      .arg(locale.formattedDataSize(snapshot.totalRssBytes))
                      .arg(snapshot.totalCpuPercent < 0 ? tr("measuring…")
                               : QStringLiteral("%1 %").arg(snapshot.totalCpuPercent, 0, 'f', 0))
                  + summary;
    }
    m_summary->setText(summary);
    m_unsupported->setVisible(!snapshot.supported);
    m_unsupported->setText(snapshot.reason + QLatin1Char(' ')
                           + tr("Tab counts and states below are still accurate."));
    m_processes->setVisible(snapshot.supported);

    if (snapshot.supported) {
        m_processes->setRowCount(snapshot.processes.size());
        for (int row = 0; row < snapshot.processes.size(); ++row) {
            const auto &process = snapshot.processes.at(row);
            setCell(m_processes, row, 0, kindLabel(process.kind));
            setCell(m_processes, row, 1, QString::number(process.pid), Qt::AlignRight | Qt::AlignVCenter);
            setCell(m_processes, row, 2, locale.formattedDataSize(process.rssBytes),
                    Qt::AlignRight | Qt::AlignVCenter);
            setCell(m_processes, row, 3, process.cpuPercent < 0 ? QStringLiteral("—")
                        : QStringLiteral("%1 %").arg(process.cpuPercent, 0, 'f', 1),
                    Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    QHash<qint64, qint64> rssByPid;
    for (const auto &process : snapshot.processes) {
        rssByPid.insert(process.pid, process.rssBytes);
    }
    const int previous = selectedTab();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_rowToTab.clear();
    m_tabs->setRowCount(infos.size());
    int selectRow = -1;
    for (int row = 0; row < infos.size(); ++row) {
        const TabInfo &info = infos.at(row);
        m_rowToTab.append(info.index);
        const TabContent *tab = m_window->tabAt(info.index);
        QString state = info.suspended ? tr("Suspended") : info.current ? tr("Active")
            : TabAnalysis::isWebTab(info) ? tr("Background") : tr("Not a web page");
        if (info.audible) {
            state += tr(" · audio");
        }
        QString memory = QStringLiteral("—");
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (tab && TabAnalysis::isWebTab(info) && !info.suspended) {
            const qint64 pid = tab->webView()->webPage()->renderProcessPid();
            if (rssByPid.contains(pid)) {
                memory = locale.formattedDataSize(rssByPid.value(pid));
            }
        }
#else
        Q_UNUSED(tab)
#endif
        setCell(m_tabs, row, 0, info.title.isEmpty() ? info.url.toString() : info.title);
        setCell(m_tabs, row, 1, state);
        setCell(m_tabs, row, 2, memory, Qt::AlignRight | Qt::AlignVCenter);
        setCell(m_tabs, row, 3, info.current ? QString() : tr("%1 min").arg((now - info.lastActiveMs) / 60'000),
                Qt::AlignRight | Qt::AlignVCenter);
        if (info.index == previous) {
            selectRow = row;
        }
    }
    if (selectRow >= 0 && m_tabs->currentRow() != selectRow) {
        m_tabs->selectRow(selectRow);
    }
}
