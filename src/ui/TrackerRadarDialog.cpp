#include "ui/TrackerRadarDialog.h"
#include "web/BlocklistInterceptor.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

TrackerRadarDialog::TrackerRadarDialog(BlocklistInterceptor *interceptor, QWidget *parent)
    : QDialog(parent)
    , m_interceptor(interceptor)
    , m_summary(new QLabel(this))
    , m_table(new QTableWidget(this))
    , m_refreshTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("trackerRadarDialog"));
    setWindowTitle(tr("Tracker Radar"));
    resize(850, 480);
    auto *layout = new QVBoxLayout(this);
    m_summary->setObjectName(QStringLiteral("radarSummary"));
    layout->addWidget(m_summary);
    auto *description = new QLabel(
        tr("Blocked requests from all profiles in this session. Categories follow the bundled "
           "host list; user rules are unclassified. Only the latest 500 events are shown."), this);
    description->setWordWrap(true);
    layout->addWidget(description);
    m_table->setObjectName(QStringLiteral("radarTable"));
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Time"), tr("Blocked domain"), tr("Origin site"),
                                        tr("List category")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table, 1);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *clear = new QPushButton(tr("Clear statistics"), this);
    clear->setObjectName(QStringLiteral("radarClearButton"));
    buttons->addWidget(clear);
    layout->addLayout(buttons);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(150);
    connect(m_interceptor, &BlocklistInterceptor::statisticsChanged,
            this, [this] { if (!m_refreshTimer->isActive()) m_refreshTimer->start(); });
    connect(m_refreshTimer, &QTimer::timeout, this, &TrackerRadarDialog::refresh);
    connect(clear, &QPushButton::clicked, m_interceptor, &BlocklistInterceptor::clearStatistics);
    refresh();
}

void TrackerRadarDialog::refresh()
{
    const auto stats = m_interceptor->statistics();
    m_summary->setText(tr("%1 blocked requests · %2 matched list domains")
                           .arg(stats.blocked).arg(stats.matchedRules));
    m_table->setRowCount(stats.recent.size());
    for (int row = 0; row < stats.recent.size(); ++row) {
        const auto &event = stats.recent.at(stats.recent.size() - row - 1);
        QString category;
        switch (event.category) {
        case BlocklistInterceptor::Category::Advertising: category = tr("Advertising"); break;
        case BlocklistInterceptor::Category::AnalyticsOrTracking: category = tr("Analytics / tracking"); break;
        case BlocklistInterceptor::Category::Social: category = tr("Social tracking"); break;
        case BlocklistInterceptor::Category::Other: category = tr("Other classified"); break;
        case BlocklistInterceptor::Category::Unclassified: category = tr("Unclassified"); break;
        }
        m_table->setItem(row, 0, new QTableWidgetItem(
            QLocale().toString(event.time.toLocalTime(), QLocale::ShortFormat)));
        m_table->setItem(row, 1, new QTableWidgetItem(event.host));
        m_table->setItem(row, 2, new QTableWidgetItem(
            event.site.isEmpty() ? tr("Unknown") : event.site));
        m_table->setItem(row, 3, new QTableWidgetItem(category));
    }
}
