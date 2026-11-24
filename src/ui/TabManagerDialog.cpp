#include "ui/TabManagerDialog.h"

#include "ui/BrowserWindow.h"
#include "ui/NameColorDialog.h"
#include "ui/TabContent.h"
#include "web/WebPage.h"
#include "web/WebView.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

TabManagerDialog::TabManagerDialog(BrowserWindow *window)
    : QDialog(window)
    , m_window(window)
    , m_search(new QLineEdit(this))
    , m_table(new QTableWidget(this))
    , m_summary(new QLabel(this))
    , m_duplicates(new QPushButton(this))
    , m_suggestions(new QPushButton(this))
    , m_suspend(new QPushButton(tr("Suspend inactive tabs"), this))
    , m_groupMenu(new QPushButton(tr("Group…"), this))
    , m_minutes(new QSpinBox(this))
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("tabManagerDialog"));
    setWindowTitle(tr("Tab Manager"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(820, 520);
    auto *layout = new QVBoxLayout(this);

    m_search->setObjectName(QStringLiteral("tabManagerSearch"));
    m_search->setPlaceholderText(tr("Search tabs by title or address"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    m_table->setObjectName(QStringLiteral("tabManagerTable"));
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Tab"), tr("Site"), tr("Group"), tr("Status")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table, 1);

    m_summary->setObjectName(QStringLiteral("tabManagerSummary"));
    layout->addWidget(m_summary);

    auto *row = new QHBoxLayout;
    m_duplicates->setObjectName(QStringLiteral("tabManagerDuplicates"));
    m_suggestions->setObjectName(QStringLiteral("tabManagerSuggestions"));
    m_suspend->setObjectName(QStringLiteral("tabManagerSuspend"));
    m_minutes->setObjectName(QStringLiteral("tabManagerMinutes"));
    m_minutes->setRange(1, 1440);
    m_minutes->setValue(30);
    m_minutes->setSuffix(tr(" min"));
    m_minutes->setToolTip(tr("Tabs not used for this long are considered inactive."));
    row->addWidget(m_duplicates);
    row->addWidget(m_suggestions);
    row->addStretch();
    row->addWidget(new QLabel(tr("Inactive after"), this));
    row->addWidget(m_minutes);
    row->addWidget(m_suspend);
    layout->addLayout(row);

    auto *row2 = new QHBoxLayout;
    auto *go = new QPushButton(tr("Go to tab"), this);
    auto *close = new QPushButton(tr("Close tab"), this);
    auto *workspaces = new QPushButton(tr("Workspaces…"), this);
    go->setObjectName(QStringLiteral("tabManagerGo"));
    close->setObjectName(QStringLiteral("tabManagerClose"));
    m_groupMenu->setObjectName(QStringLiteral("tabManagerGroup"));
    row2->addWidget(go);
    row2->addWidget(close);
    row2->addWidget(m_groupMenu);
    row2->addStretch();
    row2->addWidget(workspaces);
    layout->addLayout(row2);

    auto *menu = new QMenu(m_groupMenu);
    m_groupMenu->setMenu(menu);
    connect(menu, &QMenu::aboutToShow, this, [this, menu] {
        menu->clear();
        const int index = selectedTabIndex();
        const TabContent *tab = index >= 0 ? m_window->tabAt(index) : nullptr;
        const bool grouped = tab && !tab->groupId().isEmpty();
        menu->addAction(tr("New group…"), this, [this] { moveSelectedToGroup(QStringLiteral("*new")); });
        for (const SessionGroup &group : m_window->groups()) {
            menu->addAction(tr("Move to “%1”").arg(group.name), this,
                            [this, id = group.id] { moveSelectedToGroup(id); });
        }
        menu->addSeparator();
        auto *rename = menu->addAction(tr("Rename or recolor group…"), this,
                                       [this] { renameSelectedGroup(); });
        auto *remove = menu->addAction(tr("Remove from group"), this,
                                       [this] { moveSelectedToGroup(QString()); });
        rename->setEnabled(grouped);
        remove->setEnabled(grouped);
    });

    connect(m_search, &QLineEdit::textChanged, this, &TabManagerDialog::refresh);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &TabManagerDialog::updateButtons);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this] {
        const int index = selectedTabIndex();
        if (index >= 0) {
            m_window->tabWidget()->setCurrentIndex(index);
            m_window->activateWindow();
        }
    });
    connect(go, &QPushButton::clicked, this, [this] {
        const int index = selectedTabIndex();
        if (index >= 0) {
            m_window->tabWidget()->setCurrentIndex(index);
            m_window->activateWindow();
        }
    });
    connect(close, &QPushButton::clicked, this, [this] {
        const int index = selectedTabIndex();
        if (index >= 0) {
            m_window->closeTab(index);
            refresh();
        }
    });
    connect(m_duplicates, &QPushButton::clicked, this, &TabManagerDialog::closeDuplicates);
    connect(m_suggestions, &QPushButton::clicked, this, &TabManagerDialog::showSuggestions);
    connect(m_suspend, &QPushButton::clicked, this, &TabManagerDialog::suspendInactive);
    connect(workspaces, &QPushButton::clicked, m_window, &BrowserWindow::showWorkspaces);
    m_timer->setInterval(2000);
    connect(m_timer, &QTimer::timeout, this, &TabManagerDialog::refresh);
    m_timer->start();
    refresh();
}

int TabManagerDialog::selectedTabIndex() const
{
    const int row = m_table->currentRow();
    return row >= 0 && row < m_rowToTab.size() ? m_rowToTab.at(row) : -1;
}

void TabManagerDialog::refresh()
{
    const int previous = selectedTabIndex();
    m_infos = m_window->tabInfos();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 threshold = qint64(m_minutes->value()) * 60'000;
    QSet<int> duplicateIndexes;
    for (const QList<int> &set : TabAnalysis::duplicates(m_infos)) {
        for (int index : set) {
            duplicateIndexes.insert(index);
        }
    }
    const QList<int> inactiveList = TabAnalysis::inactiveTabs(m_infos, now, threshold);
    const QSet<int> inactive(inactiveList.cbegin(), inactiveList.cend());
    const QList<int> shown = TabAnalysis::search(m_infos, m_search->text());

    const QSignalBlocker blocker(m_table);
    m_table->setRowCount(shown.size());
    m_rowToTab = shown;
    int selectRow = -1;
    for (int row = 0; row < shown.size(); ++row) {
        const TabInfo &info = m_infos.at(shown.at(row));
        QStringList status;
        if (info.current) {
            status << tr("Current");
        }
        if (info.pinned) {
            status << tr("Pinned");
        }
        if (info.audible) {
            status << tr("Playing audio");
        }
        if (duplicateIndexes.contains(info.index)) {
            status << tr("Duplicate");
        }
        if (info.suspended) {
            status << tr("Suspended");
        } else if (inactive.contains(info.index)) {
            status << tr("Inactive %1 min").arg((now - info.lastActiveMs) / 60'000);
        }
        const SessionGroup group = m_window->groupById(info.groupId);
        const QString title = info.title.isEmpty() ? (TabAnalysis::isWebTab(info)
            ? info.url.toString() : tr("New tab")) : info.title;
        const QStringList cells{title,
            TabAnalysis::isWebTab(info) ? TabAnalysis::siteKey(info.url.host()) : QString(),
            group.name, status.join(QStringLiteral(" · "))};
        for (int c = 0; c < cells.size(); ++c) {
            auto *item = m_table->item(row, c);
            if (!item) {
                item = new QTableWidgetItem;
                m_table->setItem(row, c, item);
            }
            item->setText(cells.at(c));
            item->setToolTip(info.url.toString());
        }
        if (info.index == previous) {
            selectRow = row;
        }
    }
    if (selectRow >= 0) {
        m_table->selectRow(selectRow);
    }
    const int redundant = TabAnalysis::redundantDuplicates(m_infos).size();
    const int suggestions = TabAnalysis::suggestGroups(m_infos).size();
    m_duplicates->setText(tr("Close duplicates (%1)").arg(redundant));
    m_duplicates->setEnabled(redundant > 0);
    m_suggestions->setText(tr("Group by site (%1)…").arg(suggestions));
    m_suggestions->setEnabled(suggestions > 0);
    m_suspend->setEnabled(!inactive.isEmpty());
    m_suspend->setText(tr("Suspend inactive tabs (%1)").arg(inactive.size()));
    m_summary->setText(tr("%1 tabs · %2 groups · %3 suspended")
                           .arg(m_infos.size()).arg(m_window->groups().size())
                           .arg(std::count_if(m_infos.cbegin(), m_infos.cend(),
                                              [](const TabInfo &t) { return t.suspended; })));
    updateButtons();
}

void TabManagerDialog::updateButtons()
{
    const bool selected = selectedTabIndex() >= 0;
    m_groupMenu->setEnabled(selected);
    for (const QString &name : {QStringLiteral("tabManagerGo"), QStringLiteral("tabManagerClose")}) {
        if (auto *button = findChild<QPushButton *>(name)) {
            button->setEnabled(selected);
        }
    }
}

void TabManagerDialog::closeDuplicates()
{
    const QList<int> redundant = TabAnalysis::redundantDuplicates(m_infos);
    if (redundant.isEmpty()) {
        return;
    }
    if (QMessageBox::question(this, tr("Close duplicates"),
            tr("Close %1 duplicate tab(s)? One tab per page is kept; the current, pinned and "
               "audible tabs are preferred.").arg(redundant.size())) != QMessageBox::Yes) {
        return;
    }
    // Highest index first so the remaining indexes stay valid.
    QList<int> sorted = redundant;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int index : sorted) {
        m_window->closeTab(index);
    }
    refresh();
}

void TabManagerDialog::showSuggestions()
{
    const auto suggestions = TabAnalysis::suggestGroups(m_infos);
    if (suggestions.isEmpty()) {
        return;
    }
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("tabSuggestionsDialog"));
    dialog.setWindowTitle(tr("Group tabs by site"));
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Choose which groups to create. Pinned and already grouped "
                                    "tabs are left alone."), &dialog));
    QList<QCheckBox *> checks;
    QList<QLineEdit *> names;
    for (const auto &suggestion : suggestions) {
        auto *row = new QHBoxLayout;
        auto *check = new QCheckBox(tr("%1 tabs from %2").arg(suggestion.indexes.size()).arg(suggestion.site), &dialog);
        check->setChecked(true);
        auto *name = new QLineEdit(suggestion.name, &dialog);
        row->addWidget(check, 1);
        row->addWidget(name);
        layout->addLayout(row);
        checks << check;
        names << name;
    }
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(box);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QList<QString> &colors = NameColorDialog::presetColors();
    int colorIndex = m_window->groups().size();
    for (int i = 0; i < suggestions.size(); ++i) {
        if (!checks.at(i)->isChecked() || names.at(i)->text().trimmed().isEmpty()) {
            continue;
        }
        const QString id = m_window->createGroup(names.at(i)->text(), colors.at(colorIndex++ % colors.size()));
        for (int tabIndex : suggestions.at(i).indexes) {
            m_window->assignTabToGroup(tabIndex, id);
        }
    }
    refresh();
}

void TabManagerDialog::suspendInactive()
{
    const auto inactive = TabAnalysis::inactiveTabs(
        m_infos, QDateTime::currentMSecsSinceEpoch(), qint64(m_minutes->value()) * 60'000);
    int suspended = 0;
    for (int index : inactive) {
        suspended += m_window->suspendTab(index) ? 1 : 0;
    }
    m_window->notify(tr("%1 inactive tab(s) suspended. They reload when you open them.")
                         .arg(suspended), QStringLiteral("success"));
    refresh();
}

void TabManagerDialog::moveSelectedToGroup(const QString &groupId)
{
    const int index = selectedTabIndex();
    if (index < 0) {
        return;
    }
    QString target = groupId;
    if (groupId == QLatin1String("*new")) {
        NameColorDialog dialog(tr("New tab group"), QString(), QString(), this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        target = m_window->createGroup(dialog.nameValue(), dialog.colorValue());
        if (target.isEmpty()) {
            return;
        }
    }
    m_window->assignTabToGroup(index, target);
    refresh();
}

void TabManagerDialog::renameSelectedGroup()
{
    const int index = selectedTabIndex();
    const TabContent *tab = index >= 0 ? m_window->tabAt(index) : nullptr;
    if (!tab) {
        return;
    }
    const SessionGroup group = m_window->groupById(tab->groupId());
    if (group.id.isEmpty()) {
        return;
    }
    NameColorDialog dialog(tr("Rename tab group"), group.name, group.color, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_window->renameGroup(group.id, dialog.nameValue());
        m_window->setGroupColor(group.id, dialog.colorValue());
        refresh();
    }
}
