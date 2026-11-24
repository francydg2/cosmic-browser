#pragma once

#include <QDialog>
#include <QHash>
#include <QNetworkCookie>
#include <QSet>

class Browser;
class QLabel;
class QTableWidget;
class QTimer;
class QWebEngineProfile;

/// Privacy Dashboard: real tracker-blocking statistics, protection
/// switches, cookies, remembered site permissions and data clearing.
/// Every number comes from a measurement (request interceptor, cookie
/// stores, saved decisions); nothing is estimated.
class PrivacyDashboardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PrivacyDashboardDialog(Browser *browser, QWidget *parent = nullptr);

private:
    QWidget *buildOverviewTab();
    QWidget *buildProtectionTab();
    QWidget *buildCookiesTab();
    QWidget *buildPermissionsTab();

    void refreshOverview();
    void scanCookies();
    void renderCookies();
    void refreshPermissions();

    struct CookieRecord
    {
        QNetworkCookie cookie;
        QWebEngineProfile *profile = nullptr;
    };

    Browser *m_browser;
    QLabel *m_shield = nullptr;
    QLabel *m_totals = nullptr;
    QTableWidget *m_sites = nullptr;
    QTableWidget *m_hosts = nullptr;
    QTableWidget *m_categories = nullptr;
    QTableWidget *m_cookieTable = nullptr;
    QLabel *m_cookieSummary = nullptr;
    QTableWidget *m_permissionTable = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QTimer *m_cookieTimer = nullptr;
    QHash<QString, CookieRecord> m_cookies;
    QSet<QObject *> m_cookieStores;
};
