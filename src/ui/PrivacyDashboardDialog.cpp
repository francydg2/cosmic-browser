#include "ui/PrivacyDashboardDialog.h"

#include "browser/Browser.h"
#include "core/Settings.h"
#include "ui/TrackerRadarDialog.h"
#include "web/BlocklistInterceptor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

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

void fillCounts(QTableWidget *table, const QHash<QString, int> &counts, int limit,
                const QString &emptyLabel)
{
    QList<QPair<int, QString>> rows;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        rows.append({it.value(), it.key().isEmpty() ? emptyLabel : it.key()});
    }
    std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    const int count = qMin<int>(limit, rows.size());
    table->setRowCount(count);
    for (int i = 0; i < count; ++i) {
        table->setItem(i, 0, new QTableWidgetItem(rows.at(i).second));
        auto *number = new QTableWidgetItem(QString::number(rows.at(i).first));
        number->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(i, 1, number);
    }
}

QString featureLabel(const QString &feature)
{
    if (feature == QLatin1String("location")) {
        return PrivacyDashboardDialog::tr("Location");
    }
    if (feature == QLatin1String("media")) {
        return PrivacyDashboardDialog::tr("Camera and microphone");
    }
    if (feature == QLatin1String("notifications")) {
        return PrivacyDashboardDialog::tr("Notifications");
    }
    return feature;
}

} // namespace

PrivacyDashboardDialog::PrivacyDashboardDialog(Browser *browser, QWidget *parent)
    : QDialog(parent)
    , m_browser(browser)
    , m_refreshTimer(new QTimer(this))
    , m_cookieTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("privacyDashboard"));
    setWindowTitle(tr("Privacy Dashboard"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(780, 560);
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("privacyTabs"));
    tabs->addTab(buildOverviewTab(), tr("Overview"));
    tabs->addTab(buildProtectionTab(), tr("Protection"));
    tabs->addTab(buildCookiesTab(), tr("Cookies and data"));
    tabs->addTab(buildPermissionsTab(), tr("Site permissions"));
    layout->addWidget(tabs);

    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(300);
    connect(m_refreshTimer, &QTimer::timeout, this, &PrivacyDashboardDialog::refreshOverview);
    connect(m_browser->requestInterceptor(), &BlocklistInterceptor::statisticsChanged, this, [this] {
        if (!m_refreshTimer->isActive()) {
            m_refreshTimer->start();
        }
    });
    m_cookieTimer->setSingleShot(true);
    m_cookieTimer->setInterval(400);
    connect(m_cookieTimer, &QTimer::timeout, this, &PrivacyDashboardDialog::renderCookies);
    connect(m_browser->settings(), &Settings::sectionChanged, this, [this](const QString &name) {
        if (name == QLatin1String("sitePermissions")) {
            refreshPermissions();
        }
    });
    refreshOverview();
    refreshPermissions();
    scanCookies();
}

QWidget *PrivacyDashboardDialog::buildOverviewTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    m_shield = new QLabel(page);
    m_shield->setObjectName(QStringLiteral("privacyShield"));
    QFont font = m_shield->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    m_shield->setFont(font);
    m_totals = new QLabel(page);
    m_totals->setObjectName(QStringLiteral("privacyTotals"));
    m_totals->setWordWrap(true);
    layout->addWidget(m_shield);
    layout->addWidget(m_totals);

    auto *columns = new QHBoxLayout;
    auto addColumn = [&](const QString &title, QTableWidget **slot, const QString &first) {
        auto *box = new QVBoxLayout;
        box->addWidget(new QLabel(title, page));
        *slot = makeTable({first, tr("Blocked")}, page);
        box->addWidget(*slot);
        columns->addLayout(box);
    };
    addColumn(tr("Sites with the most blocked requests"), &m_sites, tr("Site"));
    addColumn(tr("Most blocked tracker domains"), &m_hosts, tr("Domain"));
    addColumn(tr("By list category"), &m_categories, tr("Category"));
    m_sites->setObjectName(QStringLiteral("privacySites"));
    m_hosts->setObjectName(QStringLiteral("privacyHosts"));
    m_categories->setObjectName(QStringLiteral("privacyCategories"));
    layout->addLayout(columns, 1);

    auto *buttons = new QHBoxLayout;
    auto *log = new QPushButton(tr("Event log…"), page);
    auto *reset = new QPushButton(tr("Reset statistics"), page);
    log->setObjectName(QStringLiteral("privacyEventLog"));
    reset->setObjectName(QStringLiteral("privacyReset"));
    buttons->addWidget(log);
    buttons->addStretch();
    buttons->addWidget(reset);
    layout->addLayout(buttons);
    connect(log, &QPushButton::clicked, this, [this] {
        auto *dialog = new TrackerRadarDialog(m_browser->requestInterceptor(), this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    connect(reset, &QPushButton::clicked, m_browser->requestInterceptor(),
            &BlocklistInterceptor::clearStatistics);
    return page;
}

void PrivacyDashboardDialog::refreshOverview()
{
    BlocklistInterceptor *interceptor = m_browser->requestInterceptor();
    const auto stats = interceptor->statistics();
    const bool enabled = m_browser->settings()->requestBlockingEnabled();
    m_shield->setText(enabled ? tr("Cosmic Shield is on") : tr("Cosmic Shield is off"));
    m_totals->setText(enabled
        ? tr("%1 requests blocked since Cosmic started (or the last reset), matching %2 of the "
             "%3 domains in the list. Cosmic blocks network requests to listed domains; it does "
             "not remove page elements, and trackers outside the list are not detected.")
              .arg(stats.blocked).arg(stats.matchedRules).arg(interceptor->ruleCount())
        : tr("Blocking is off, so no trackers are detected or counted. Turn it on in the "
             "Protection tab."));
    fillCounts(m_sites, stats.bySite, 15, tr("(unknown site)"));
    fillCounts(m_hosts, stats.byHost, 15, tr("(unknown)"));
    QHash<QString, int> categories;
    for (auto it = stats.byCategory.cbegin(); it != stats.byCategory.cend(); ++it) {
        QString name;
        switch (static_cast<BlocklistInterceptor::Category>(it.key())) {
        case BlocklistInterceptor::Category::Advertising: name = tr("Advertising"); break;
        case BlocklistInterceptor::Category::AnalyticsOrTracking: name = tr("Analytics / tracking"); break;
        case BlocklistInterceptor::Category::Social: name = tr("Social tracking"); break;
        case BlocklistInterceptor::Category::Other: name = tr("Other classified"); break;
        case BlocklistInterceptor::Category::Unclassified: name = tr("Unclassified"); break;
        }
        categories[name] += it.value();
    }
    fillCounts(m_categories, categories, 10, QString());
}

QWidget *PrivacyDashboardDialog::buildProtectionTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    Settings *settings = m_browser->settings();
    const auto addSwitch = [&](const QString &name, const QString &text, const QString &hint, bool value,
                               std::function<void(bool)> apply) {
        auto *check = new QCheckBox(text, page);
        check->setObjectName(name);
        check->setChecked(value);
        auto *note = new QLabel(hint, page);
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral("color: #98a2b2; margin-left: 24px;"));
        layout->addWidget(check);
        layout->addWidget(note);
        connect(check, &QCheckBox::toggled, this, [settings, apply](bool on) {
            apply(on);
            settings->save();
        });
    };
    addSwitch(QStringLiteral("protectBlocking"), tr("Block requests to known trackers and ads"),
              tr("Cancels requests to domains in the built-in list and your own rules."),
              settings->requestBlockingEnabled(),
              [settings](bool on) { settings->setRequestBlockingEnabled(on); });
    addSwitch(QStringLiteral("protectDnt"), tr("Send “Do Not Track”"),
              tr("Adds the DNT header. Sites are free to ignore it."), settings->doNotTrack(),
              [settings](bool on) { settings->setDoNotTrack(on); });
    addSwitch(QStringLiteral("protectSessionCookies"), tr("Keep cookies only until Cosmic closes"),
              tr("Takes effect after a restart. Sites will forget you every time."),
              settings->sessionCookiesOnly(),
              [settings](bool on) { settings->setSessionCookiesOnly(on); });
    addSwitch(QStringLiteral("protectHttps"), tr("HTTPS-only: upgrade http:// pages"),
              tr("Pages that do not support HTTPS may fail to load."), settings->httpsOnly(),
              [settings](bool on) { settings->setHttpsOnly(on); });
    layout->addStretch();
    return page;
}

QWidget *PrivacyDashboardDialog::buildCookiesTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    m_cookieSummary = new QLabel(page);
    m_cookieSummary->setObjectName(QStringLiteral("cookieSummary"));
    layout->addWidget(m_cookieSummary);
    m_cookieTable = makeTable({tr("Domain"), tr("Cookies")}, page);
    m_cookieTable->setObjectName(QStringLiteral("cookieTable"));
    layout->addWidget(m_cookieTable, 1);
    auto *buttons = new QHBoxLayout;
    auto *refresh = new QPushButton(tr("Refresh"), page);
    auto *removeDomain = new QPushButton(tr("Delete cookies of selected site"), page);
    auto *clearAll = new QPushButton(tr("Clear all cookies and cache…"), page);
    removeDomain->setObjectName(QStringLiteral("cookieDeleteDomain"));
    clearAll->setObjectName(QStringLiteral("cookieClearAll"));
    buttons->addWidget(refresh);
    buttons->addWidget(removeDomain);
    buttons->addStretch();
    buttons->addWidget(clearAll);
    layout->addLayout(buttons);
    auto *note = new QLabel(
        tr("Cosmic does not keep a browsing-history database of its own, so there is no history "
           "to clear here."), page);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #98a2b2;"));
    layout->addWidget(note);

    connect(refresh, &QPushButton::clicked, this, &PrivacyDashboardDialog::scanCookies);
    connect(removeDomain, &QPushButton::clicked, this, [this] {
        const int row = m_cookieTable->currentRow();
        if (row < 0) {
            return;
        }
        const QString domain = m_cookieTable->item(row, 0)->text();
        const auto records = m_cookies;
        for (auto it = records.cbegin(); it != records.cend(); ++it) {
            QString d = it->cookie.domain();
            if (d.startsWith(QLatin1Char('.'))) {
                d.remove(0, 1);
            }
            if (d == domain && it->profile && it->profile->cookieStore()) {
                it->profile->cookieStore()->deleteCookie(it->cookie);
                m_cookies.remove(it.key());
            }
        }
        renderCookies();
    });
    connect(clearAll, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Clear browsing data"),
                tr("Delete all cookies and cached files for every user? You will be signed out "
                   "of websites.")) == QMessageBox::Yes) {
            m_browser->clearBrowsingData();
            m_cookies.clear();
            renderCookies();
        }
    });
    return page;
}

void PrivacyDashboardDialog::scanCookies()
{
    m_cookies.clear();
    for (QWebEngineProfile *profile : m_browser->allProfiles()) {
        QWebEngineCookieStore *store = profile->cookieStore();
        if (!store) {
            continue;
        }
        const QPointer<QWebEngineProfile> guard(profile);
        connect(store, &QWebEngineCookieStore::cookieAdded, this,
                [this, guard](const QNetworkCookie &cookie) {
            if (!guard) {
                return;
            }
            const QString key = QString::number(reinterpret_cast<quintptr>(guard.data()))
                + QLatin1Char('|') + cookie.domain() + QLatin1Char('|') + cookie.path()
                + QLatin1Char('|') + QString::fromUtf8(cookie.name());
            m_cookies.insert(key, {cookie, guard});
            m_cookieTimer->start();
        }, Qt::UniqueConnection);
        connect(store, &QWebEngineCookieStore::cookieRemoved, this,
                [this, guard](const QNetworkCookie &cookie) {
            const QString key = QString::number(reinterpret_cast<quintptr>(guard.data()))
                + QLatin1Char('|') + cookie.domain() + QLatin1Char('|') + cookie.path()
                + QLatin1Char('|') + QString::fromUtf8(cookie.name());
            if (m_cookies.remove(key) > 0) {
                m_cookieTimer->start();
            }
        }, Qt::UniqueConnection);
        store->loadAllCookies();
    }
    m_cookieSummary->setText(tr("Reading cookies…"));
    m_cookieTimer->start();
}

void PrivacyDashboardDialog::renderCookies()
{
    QHash<QString, int> perDomain;
    for (const CookieRecord &record : std::as_const(m_cookies)) {
        QString domain = record.cookie.domain();
        if (domain.startsWith(QLatin1Char('.'))) {
            domain.remove(0, 1);
        }
        perDomain[domain]++;
    }
    fillCounts(m_cookieTable, perDomain, 5000, tr("(no domain)"));
    m_cookieSummary->setText(tr("%1 cookies from %2 sites, across all users.")
                                 .arg(m_cookies.size()).arg(perDomain.size()));
}

QWidget *PrivacyDashboardDialog::buildPermissionsTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("Default for sites that have no remembered choice:"), page));
    auto *form = new QFormLayout;
    Settings *settings = m_browser->settings();
    for (const QString &feature : {QStringLiteral("location"), QStringLiteral("media"),
                                   QStringLiteral("notifications")}) {
        auto *combo = new QComboBox(page);
        combo->setObjectName(QStringLiteral("defaultPermission_") + feature);
        combo->addItem(tr("Ask"), QStringLiteral("ask"));
        combo->addItem(tr("Allow"), QStringLiteral("allow"));
        combo->addItem(tr("Block"), QStringLiteral("block"));
        combo->setCurrentIndex(qMax(0, combo->findData(settings->permissionPolicy(feature))));
        connect(combo, &QComboBox::activated, this, [settings, combo, feature] {
            settings->setPermissionPolicy(feature, combo->currentData().toString());
            settings->save();
        });
        form->addRow(featureLabel(feature), combo);
    }
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("Remembered choices:"), page));
    m_permissionTable = makeTable({tr("Site"), tr("Permission"), tr("Decision")}, page);
    m_permissionTable->setObjectName(QStringLiteral("permissionTable"));
    layout->addWidget(m_permissionTable, 1);
    auto *buttons = new QHBoxLayout;
    auto *forget = new QPushButton(tr("Forget selected"), page);
    auto *forgetAll = new QPushButton(tr("Forget all"), page);
    forget->setObjectName(QStringLiteral("permissionForget"));
    forgetAll->setObjectName(QStringLiteral("permissionForgetAll"));
    buttons->addWidget(forget);
    buttons->addWidget(forgetAll);
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(forget, &QPushButton::clicked, this, [this] {
        const int row = m_permissionTable->currentRow();
        if (row < 0) {
            return;
        }
        m_browser->setSitePermission(m_permissionTable->item(row, 0)->text(),
                                     m_permissionTable->item(row, 0)->data(Qt::UserRole).toString(),
                                     QString());
    });
    connect(forgetAll, &QPushButton::clicked, this, [this] {
        const auto sites = m_browser->sitePermissions();
        for (auto it = sites.cbegin(); it != sites.cend(); ++it) {
            const int bar = it.key().indexOf(QLatin1Char('|'));
            m_browser->setSitePermission(it.key().left(bar), it.key().mid(bar + 1), QString());
        }
    });
    return page;
}

void PrivacyDashboardDialog::refreshPermissions()
{
    const auto sites = m_browser->sitePermissions();
    QStringList keys = sites.keys();
    keys.sort();
    m_permissionTable->setRowCount(keys.size());
    for (int row = 0; row < keys.size(); ++row) {
        const QString &key = keys.at(row);
        const int bar = key.indexOf(QLatin1Char('|'));
        const QString feature = key.mid(bar + 1);
        auto *site = new QTableWidgetItem(key.left(bar));
        site->setData(Qt::UserRole, feature);
        m_permissionTable->setItem(row, 0, site);
        m_permissionTable->setItem(row, 1, new QTableWidgetItem(featureLabel(feature)));
        m_permissionTable->setItem(row, 2, new QTableWidgetItem(
            sites.value(key) == QLatin1String("allow") ? tr("Allowed") : tr("Blocked")));
    }
}
