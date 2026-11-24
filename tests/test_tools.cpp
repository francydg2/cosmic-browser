// Integration tests for the Cosmic 0.2.0 tools, run offscreen against the
// real BrowserWindow: AI assistant, translator, tab manager, privacy
// dashboard, performance centre, web inspector, Focus and the download
// centre. Network use is limited to loopback fake servers.

#include <QtTest>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabBar>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QWebEngineScript>

#include "browser/AiService.h"
#include "browser/Browser.h"
#include "browser/DownloadManager.h"
#include "browser/FocusController.h"
#include "core/AiConversations.h"
#include "core/DownloadHistory.h"
#include "core/Settings.h"
#include "ui/AiSidebar.h"
#include "ui/BrowserWindow.h"
#include "ui/DownloadsDialog.h"
#include "ui/TabContent.h"
#include "ui/Toast.h"
#include "web/BlocklistInterceptor.h"
#include "web/PagePolicy.h"
#include "web/WebPage.h"
#include "web/WebView.h"

namespace {

void deleteAllBrowserWindows()
{
    QList<BrowserWindow *> windows;
    for (QWidget *w : QApplication::topLevelWidgets()) {
        if (auto *bw = qobject_cast<BrowserWindow *>(w)) {
            windows.append(bw);
        }
    }
    qDeleteAll(windows);
}

QAction *findAction(BrowserWindow *window, const QString &id)
{
    for (QAction *a : window->findChildren<QAction *>()) {
        if (a->objectName() == id) {
            return a;
        }
    }
    return nullptr;
}

BrowserWindow *makeWindow(Browser &browser)
{
    browser.setWindowFactory([&browser] {
        return static_cast<AppWindow *>(new BrowserWindow(&browser));
    });
    return dynamic_cast<BrowserWindow *>(browser.newWindow());
}

// Loads HTML into a tab's page under a fake address and waits for it.
void loadHtml(WebPage *page, const QString &html, const QString &address)
{
    QSignalSpy loaded(page, &QWebEnginePage::loadFinished);
    page->setHtml(html, QUrl(address));
    QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 10000);
}

QVariant evalJs(WebPage *page, const QString &script)
{
    QVariant result;
    bool done = false;
    page->runJavaScript(script, [&](const QVariant &value) {
        result = value;
        done = true;
    });
    QTest::qWaitFor([&] { return done; }, 10000);
    return result;
}

// Loopback HTTP server whose reply is computed from the request body.
class FakeServer : public QObject
{
public:
    using Handler = std::function<QByteArray(const QByteArray &body)>;
    explicit FakeServer(Handler handler) : m_handler(std::move(handler))
    {
        m_server.listen(QHostAddress::LocalHost, 0);
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *socket = m_server.nextPendingConnection();
            auto *buffer = new QByteArray;
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                *buffer += socket->readAll();
                const int headerEnd = buffer->indexOf("\r\n\r\n");
                if (headerEnd < 0) {
                    return;
                }
                const QRegularExpressionMatch match = QRegularExpression(
                    QStringLiteral("content-length: (\\d+)"),
                    QRegularExpression::CaseInsensitiveOption)
                    .match(QString::fromLatin1(buffer->left(headerEnd)));
                const int length = match.captured(1).toInt();
                if (buffer->size() < headerEnd + 4 + length) {
                    return;
                }
                const QByteArray body = buffer->mid(headerEnd + 4, length);
                lastBody = body;
                buffer->clear();
                socket->write(m_handler(body));
                socket->flush();
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, this, [socket, buffer] {
                delete buffer;
                socket->deleteLater();
            });
        });
    }
    QString baseUrl() const { return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()); }
    QByteArray lastBody;

    static QByteArray sse(const QString &text)
    {
        const QJsonObject chunk{{QStringLiteral("choices"), QJsonArray{QJsonObject{
            {QStringLiteral("delta"), QJsonObject{{QStringLiteral("content"), text}}}}}}};
        return "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n"
               "data: " + QJsonDocument(chunk).toJson(QJsonDocument::Compact) + "\n\ndata: [DONE]\n\n";
    }

private:
    QTcpServer m_server;
    Handler m_handler;
};

void useProvider(Browser &browser, const FakeServer &server)
{
    Ai::ProviderConfig config;
    config.kind = Ai::Kind::OpenAiCompatible;
    config.baseUrl = server.baseUrl();
    config.model = QStringLiteral("fake");
    config.timeoutSeconds = 20;
    browser.ai()->setConfig(config, QStringLiteral("English"));
}

// Clicks a button of the next modal dialog that has the given object name.
void clickInModal(const QString &dialogName, const QString &buttonText, QSharedPointer<bool> found)
{
    auto *timer = new QTimer;
    QObject::connect(timer, &QTimer::timeout, [=] {
        QWidget *modal = QApplication::activeModalWidget();
        if (!modal || (!dialogName.isEmpty() && modal->objectName() != dialogName)) {
            return;
        }
        for (auto *button : modal->findChildren<QPushButton *>()) {
            if (button->text().remove(QLatin1Char('&')) == buttonText) {
                *found = true;
                button->click();
                timer->stop();
                timer->deleteLater();
                return;
            }
        }
    });
    timer->start(20);
}

} // namespace

class TestTools : public QObject
{
    Q_OBJECT
private slots:
    void cleanup() { deleteAllBrowserWindows(); }

    void toastShowsAndReplaces();
    void aiSidebarStreamsAnswerWithoutPageContent();
    void aiSidebarSendsPageOnlyWhenAsked();
    void translatesInPlaceAndRestores();
    void tabManagerClosesDuplicatesAndGroups();
    void privacyDashboardReportsRealBlocking();
    void sitePermissionsAreRemembered();
    void performanceCenterMeasuresAndSuspends();
    void webInspectorFollowsTabs();
    void focusBlocksSitesAndKeepsTabs();
    void downloadCenterFiltersAndPersistsHistory();
};

void TestTools::toastShowsAndReplaces()
{
    Browser browser;
    BrowserWindow *window = makeWindow(browser);
    QVERIFY(window);
    window->present();
    window->notify(QStringLiteral("first"), QStringLiteral("info"));
    QVERIFY(window->toast()->isVisible());
    QCOMPARE(window->toast()->message(), QStringLiteral("first"));
    window->notify(QStringLiteral("second"), QStringLiteral("error"));
    QCOMPARE(window->toast()->message(), QStringLiteral("second"));
}

void TestTools::aiSidebarStreamsAnswerWithoutPageContent()
{
    FakeServer server([](const QByteArray &) { return FakeServer::sse(QStringLiteral("Hello from the fake model")); });
    Browser browser;
    useProvider(browser, server);
    BrowserWindow *window = makeWindow(browser);
    QVERIFY(window);
    window->present();
    loadHtml(window->activeWebPage(), QStringLiteral("<p>SECRET-PAGE-TEXT</p>"),
             QStringLiteral("https://ai.test/page"));

    findAction(window, QStringLiteral("ai.assistant"))->trigger();
    auto *sidebar = window->findChild<AiSidebar *>();
    QVERIFY(sidebar);
    sidebar->findChild<QCheckBox *>(QStringLiteral("aiIncludePage"))->setChecked(false);
    sidebar->findChild<QPlainTextEdit *>(QStringLiteral("aiInput"))->setPlainText(QStringLiteral("Say hi"));
    sidebar->findChild<QPushButton *>(QStringLiteral("aiSend"))->click();

    auto *transcript = sidebar->findChild<QTextBrowser *>(QStringLiteral("aiTranscript"));
    QTRY_VERIFY_WITH_TIMEOUT(transcript->toPlainText().contains(QStringLiteral("Hello from the fake model")), 10000);
    QVERIFY(server.lastBody.contains("Say hi"));
    QVERIFY2(!server.lastBody.contains("SECRET-PAGE-TEXT"), "page text must not be sent unless requested");
    QTRY_VERIFY(!sidebar->isBusy());
    const auto saved = browser.ai()->conversations()->conversations();
    QCOMPARE(saved.size(), 1);
    QCOMPARE(saved.first().messages.size(), 2);
    QCOMPARE(saved.first().messages.last().content, QStringLiteral("Hello from the fake model"));
}

void TestTools::aiSidebarSendsPageOnlyWhenAsked()
{
    FakeServer server([](const QByteArray &) { return FakeServer::sse(QStringLiteral("A summary")); });
    Browser browser;
    useProvider(browser, server);
    BrowserWindow *window = makeWindow(browser);
    window->present();
    loadHtml(window->activeWebPage(), QStringLiteral("<article><p>The page talks about saturn rings.</p></article>"),
             QStringLiteral("https://ai.test/saturn?token=abc"));
    findAction(window, QStringLiteral("ai.assistant"))->trigger();
    auto *sidebar = window->findChild<AiSidebar *>();
    QVERIFY(sidebar);
    sidebar->findChild<QPushButton *>(QStringLiteral("aiSummarize"))->click();
    QTRY_VERIFY_WITH_TIMEOUT(!server.lastBody.isEmpty(), 10000);
    QVERIFY(server.lastBody.contains("saturn rings"));
    QVERIFY2(!server.lastBody.contains("token=abc"), "query strings are not sent");
    QVERIFY(server.lastBody.contains("untrusted"));
    QTRY_VERIFY(!sidebar->isBusy());
}

void TestTools::translatesInPlaceAndRestores()
{
    FakeServer server([](const QByteArray &body) {
        const QJsonObject request = QJsonDocument::fromJson(body).object();
        const QString content = request.value("messages").toArray().last().toObject().value("content").toString();
        QJsonArray out;
        for (const QJsonValue &value : QJsonDocument::fromJson(content.toUtf8()).array()) {
            out.append(QStringLiteral("IT:") + value.toString());
        }
        return FakeServer::sse(QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact)));
    });
    Browser browser;
    useProvider(browser, server);
    BrowserWindow *window = makeWindow(browser);
    window->present();
    WebPage *page = window->activeWebPage();
    loadHtml(page, QStringLiteral("<h1 id='a'>Hello world, this is a test page.</h1>"
                                  "<p id='b'>Second paragraph with <b id='c'>bold words</b> inside.</p>"
                                  "<script id='s'>var untouched = 'script text';</script>"),
             QStringLiteral("https://translate.test/"));

    auto clicked = QSharedPointer<bool>::create(false);
    clickInModal(QStringLiteral("translateDialog"), QStringLiteral("Translate"), clicked);
    findAction(window, QStringLiteral("page.translate"))->trigger();
    QVERIFY(*clicked);

    QTRY_VERIFY_WITH_TIMEOUT(evalJs(page, QStringLiteral("document.getElementById('a').textContent"))
                                 .toString().startsWith(QStringLiteral("IT:")), 15000);
    // Markup survives: the bold element still exists and is itself translated.
    QCOMPARE(evalJs(page, QStringLiteral("document.getElementById('c').tagName")).toString(), QStringLiteral("B"));
    QVERIFY(evalJs(page, QStringLiteral("document.getElementById('c').textContent")).toString()
                .startsWith(QStringLiteral("IT:")));
    QCOMPARE(evalJs(page, QStringLiteral("document.getElementById('s').textContent")).toString(),
             QStringLiteral("var untouched = 'script text';"));

    auto *indicator = window->findChild<QToolButton *>(QStringLiteral("translateIndicator"));
    QVERIFY(indicator);
    QTRY_VERIFY_WITH_TIMEOUT(indicator->isVisible() && indicator->isEnabled(), 5000);
    QVERIFY(indicator->text().contains(QStringLiteral("Show original")));
    indicator->click();
    QTRY_COMPARE_WITH_TIMEOUT(evalJs(page, QStringLiteral("document.getElementById('a').textContent")).toString(),
                              QStringLiteral("Hello world, this is a test page."), 5000);
    QVERIFY(!indicator->isVisible());
}

void TestTools::tabManagerClosesDuplicatesAndGroups()
{
    Browser browser;
    BrowserWindow *window = makeWindow(browser);
    window->present();
    loadHtml(window->activeWebPage(), QStringLiteral("<p>1</p>"), QStringLiteral("https://dup.test/page"));
    window->openUrl(QUrl(QStringLiteral("about:blank")), true);
    window->openUrl(QUrl(QStringLiteral("about:blank")), true);
    QCOMPARE(window->tabCount(), 3);
    loadHtml(window->tabAt(1)->webView()->webPage(), QStringLiteral("<p>2</p>"),
             QStringLiteral("https://www.dup.test/page/?utm_source=x"));
    loadHtml(window->tabAt(2)->webView()->webPage(), QStringLiteral("<p>3</p>"),
             QStringLiteral("https://other.test/"));

    findAction(window, QStringLiteral("tabs.manager"))->trigger();
    auto *dialog = window->findChild<QDialog *>(QStringLiteral("tabManagerDialog"));
    QVERIFY(dialog && dialog->isVisible());
    auto *duplicates = dialog->findChild<QPushButton *>(QStringLiteral("tabManagerDuplicates"));
    auto *suggestions = dialog->findChild<QPushButton *>(QStringLiteral("tabManagerSuggestions"));
    auto *table = dialog->findChild<QTableWidget *>(QStringLiteral("tabManagerTable"));
    QTRY_VERIFY(duplicates->text().contains(QStringLiteral("(1)")));
    QVERIFY(duplicates->isEnabled());
    QVERIFY(suggestions->text().contains(QStringLiteral("(1)")));
    QCOMPARE(table->rowCount(), 3);

    // Search narrows the list.
    dialog->findChild<QLineEdit *>(QStringLiteral("tabManagerSearch"))->setText(QStringLiteral("other.test"));
    QTRY_COMPARE(table->rowCount(), 1);
    dialog->findChild<QLineEdit *>(QStringLiteral("tabManagerSearch"))->clear();
    QTRY_COMPARE(table->rowCount(), 3);

    auto confirmed = QSharedPointer<bool>::create(false);
    clickInModal(QString(), QStringLiteral("Yes"), confirmed);
    duplicates->click();
    QVERIFY(*confirmed);
    QTRY_COMPARE(window->tabCount(), 2);
    QTRY_VERIFY(!duplicates->isEnabled());
}

void TestTools::privacyDashboardReportsRealBlocking()
{
    Browser browser;
    BrowserWindow *window = makeWindow(browser);
    window->present();
    QVERIFY(browser.requestInterceptor()->tryBlock(QUrl(QStringLiteral("https://doubleclick.net/ad")),
                                                   QUrl(QStringLiteral("https://news.test/"))));
    findAction(window, QStringLiteral("privacy.dashboard"))->trigger();
    auto *dialog = window->findChild<QDialog *>(QStringLiteral("privacyDashboard"));
    QVERIFY(dialog && dialog->isVisible());
    auto *sites = dialog->findChild<QTableWidget *>(QStringLiteral("privacySites"));
    QTRY_COMPARE(sites->rowCount(), 1);
    QCOMPARE(sites->item(0, 0)->text(), QStringLiteral("news.test"));
    QCOMPARE(sites->item(0, 1)->text(), QStringLiteral("1"));
    auto *hosts = dialog->findChild<QTableWidget *>(QStringLiteral("privacyHosts"));
    QCOMPARE(hosts->item(0, 0)->text(), QStringLiteral("doubleclick.net"));
    QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("privacyTotals"))->text().contains(QStringLiteral("1 requests blocked")));

    // Reset and switches act on the real settings.
    dialog->findChild<QPushButton *>(QStringLiteral("privacyReset"))->click();
    QTRY_COMPARE(sites->rowCount(), 0);
    auto *blocking = dialog->findChild<QCheckBox *>(QStringLiteral("protectBlocking"));
    QVERIFY(blocking->isChecked());
    blocking->setChecked(false);
    QVERIFY(!browser.settings()->requestBlockingEnabled());
    QTRY_VERIFY(dialog->findChild<QLabel *>(QStringLiteral("privacyShield"))->text().contains(QStringLiteral("off")));
    QVERIFY(dialog->findChild<QTableWidget *>(QStringLiteral("cookieTable")));
}

void TestTools::sitePermissionsAreRemembered()
{
    QTemporaryDir dir;
    Browser browser;
    browser.setSitePermission(QStringLiteral("Cam.Test"), QStringLiteral("media"), QStringLiteral("allow"));
    browser.setSitePermission(QStringLiteral("cam.test"), QStringLiteral("location"), QStringLiteral("block"));
    const PagePolicy *policy = browser.pagePolicy();
    QCOMPARE(policy->permission(QStringLiteral("media"), QStringLiteral("cam.test")), QStringLiteral("allow"));
    QCOMPARE(policy->permission(QStringLiteral("location"), QStringLiteral("cam.test")), QStringLiteral("block"));
    QCOMPARE(policy->permission(QStringLiteral("media"), QStringLiteral("other.test")), QStringLiteral("ask"));

    Settings reloaded(browser.settings()->filePath());
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.section(QStringLiteral("sitePermissions")).size(), 2);

    BrowserWindow *window = makeWindow(browser);
    window->present();
    findAction(window, QStringLiteral("privacy.dashboard"))->trigger();
    auto *dialog = window->findChild<QDialog *>(QStringLiteral("privacyDashboard"));
    auto *table = dialog->findChild<QTableWidget *>(QStringLiteral("permissionTable"));
    QTRY_COMPARE(table->rowCount(), 2);
    table->selectRow(0);
    dialog->findChild<QPushButton *>(QStringLiteral("permissionForget"))->click();
    QTRY_COMPARE(table->rowCount(), 1);
    QCOMPARE(browser.sitePermissions().size(), 1);
    dialog->findChild<QPushButton *>(QStringLiteral("permissionForgetAll"))->click();
    QTRY_COMPARE(table->rowCount(), 0);
    QCOMPARE(policy->permission(QStringLiteral("media"), QStringLiteral("cam.test")), QStringLiteral("ask"));
}

void TestTools::performanceCenterMeasuresAndSuspends()
{
    Browser browser;
    BrowserWindow *window = makeWindow(browser);
    window->present();
    loadHtml(window->activeWebPage(), QStringLiteral("<p>first</p>"), QStringLiteral("https://perf-a.test/"));
    window->openUrl(QUrl(QStringLiteral("about:blank")), true);
    loadHtml(window->tabAt(1)->webView()->webPage(), QStringLiteral("<p>second</p>"),
             QStringLiteral("https://perf-b.test/"));

    findAction(window, QStringLiteral("performance.center"))->trigger();
    auto *dialog = window->findChild<QDialog *>(QStringLiteral("performanceDialog"));
    QVERIFY(dialog && dialog->isVisible());
    auto *tabs = dialog->findChild<QTableWidget *>(QStringLiteral("performanceTabs"));
    auto *processes = dialog->findChild<QTableWidget *>(QStringLiteral("performanceProcesses"));
    QCOMPARE(tabs->rowCount(), 2);
#if defined(Q_OS_LINUX)
    QTRY_VERIFY_WITH_TIMEOUT(processes->rowCount() >= 1, 5000);
    bool browserRow = false;
    for (int row = 0; row < processes->rowCount(); ++row) {
        browserRow = browserRow || processes->item(row, 0)->text() == QStringLiteral("Browser");
    }
    QVERIFY(browserRow);
    QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("performanceSummary"))->text().contains(QStringLiteral("processes")));
#endif
    // The current tab cannot be suspended; the background one can.
    tabs->selectRow(0);
    dialog->findChild<QPushButton *>(QStringLiteral("performanceSuspend"))->click();
    QVERIFY(window->tabAt(0)->webView()->webPage()->lifecycleState() == QWebEnginePage::LifecycleState::Active);
    tabs->selectRow(1);
    dialog->findChild<QPushButton *>(QStringLiteral("performanceSuspend"))->click();
    QTRY_VERIFY_WITH_TIMEOUT(window->tabAt(1)->webView()->webPage()->lifecycleState()
                                 == QWebEnginePage::LifecycleState::Discarded, 5000);
    QTRY_VERIFY2(tabs->item(1, 1)->text().contains(QStringLiteral("Suspended")),
                 qPrintable(tabs->item(1, 1)->text()));

    // The warning threshold is saved in settings.
    auto *limit = dialog->findChild<QSpinBox *>(QStringLiteral("performanceLimit"));
    limit->setValue(2048);
    limit->editingFinished();
    QCOMPARE(browser.settings()->section(QStringLiteral("performance")).value(QStringLiteral("warnMb")).toInt(), 2048);
}

void TestTools::webInspectorFollowsTabs()
{
    Browser browser;
    BrowserWindow *window = makeWindow(browser);
    window->present();
    WebPage *first = window->activeWebPage();
    loadHtml(first, QStringLiteral("<p>one</p>"), QStringLiteral("https://inspect-a.test/"));
    findAction(window, QStringLiteral("inspector.toggle"))->trigger();
    auto *dock = window->findChild<QDockWidget *>(QStringLiteral("inspectorDock"));
    QVERIFY(dock && dock->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(first->devToolsPage() != nullptr, 5000);

    window->openUrl(QUrl(QStringLiteral("about:blank")), false);
    WebPage *second = window->activeWebPage();
    QVERIFY(second != first);
    loadHtml(second, QStringLiteral("<p>two</p>"), QStringLiteral("https://inspect-b.test/"));
    QTRY_VERIFY_WITH_TIMEOUT(second->devToolsPage() != nullptr, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(first->devToolsPage() == nullptr, 5000);

    findAction(window, QStringLiteral("inspector.toggle"))->trigger();
    QVERIFY(!dock->isVisible());
    QTRY_VERIFY(second->devToolsPage() == nullptr);
}

void TestTools::focusBlocksSitesAndKeepsTabs()
{
    Browser browser;
    BrowserWindow *window = makeWindow(browser);
    window->present();
    window->openUrl(QUrl(QStringLiteral("about:blank")), true);
    const int tabsBefore = window->tabCount();
    QTabBar *bar = window->tabWidget()->tabBar();
    QVERIFY(bar->isVisibleTo(window));

    FocusProfile profile;
    profile.id = QStringLiteral("custom");
    profile.blockedHosts = {QStringLiteral("blocked.test")};
    FocusController *focus = browser.focus();
    focus->start(profile, 5);
    QVERIFY(focus->isActive());
    QVERIFY(!bar->isVisibleTo(window));
    auto *pill = window->findChild<QFrame *>(QStringLiteral("focusPill"));
    QVERIFY(pill && pill->isVisible());
    QVERIFY(browser.pagePolicy()->isFocusBlocked(QStringLiteral("www.blocked.test")));

    WebPage *page = window->activeWebPage();
    QSignalSpy loaded(page, &QWebEnginePage::loadFinished);
    page->setUrl(QUrl(QStringLiteral("https://blocked.test/video")));
    QTRY_VERIFY_WITH_TIMEOUT(evalJs(page, QStringLiteral("document.body ? document.body.innerText : ''"))
                                 .toString().contains(QStringLiteral("Cosmic Focus is on")), 10000);
    QCOMPARE(window->tabCount(), tabsBefore); // nothing was closed

    pill->findChild<QPushButton *>(QStringLiteral("focusPillEnd"))->click();
    QVERIFY(!focus->isActive());
    QVERIFY(bar->isVisibleTo(window));
    QVERIFY(!pill->isVisible());
    QVERIFY(!browser.pagePolicy()->isFocusBlocked(QStringLiteral("blocked.test")));
}

void TestTools::downloadCenterFiltersAndPersistsHistory()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("downloads.json"));
    {
        DownloadHistory history(path);
        const auto add = [&history](const char *id, const char *name, const char *state) {
            DownloadRecord record;
            record.id = QString::fromLatin1(id);
            record.fileName = QString::fromLatin1(name);
            record.directory = QStringLiteral("/tmp");
            record.state = QString::fromLatin1(state);
            record.url = QUrl(QStringLiteral("https://files.test/") + record.fileName);
            record.totalBytes = record.receivedBytes = 1000;
            history.upsert(record);
        };
        add("1", "report.pdf", "completed");
        add("2", "movie.mkv", "failed");
        add("3", "song.mp3", "cancelled");
        QVERIFY(history.save());
    }
    DownloadManager manager(path);
    QCOMPARE(manager.history().size(), 3); // survives a restart
    DownloadsDialog dialog(&manager);
    dialog.show();
    auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("downloadsTable"));
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 3);
    dialog.findChild<QLineEdit *>(QStringLiteral("downloadsSearch"))->setText(QStringLiteral("movie"));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("movie.mkv"));
    dialog.findChild<QLineEdit *>(QStringLiteral("downloadsSearch"))->clear();
    auto *filter = dialog.findChild<QComboBox *>(QStringLiteral("downloadsFilter"));
    filter->setCurrentIndex(filter->findData(QStringLiteral("completed")));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("report.pdf"));
    filter->setCurrentIndex(filter->findData(QStringLiteral("active")));
    QCOMPARE(table->rowCount(), 0);
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("downloadsEmpty"))->isVisible());

    filter->setCurrentIndex(0);
    table->selectRow(0);
    dialog.findChild<QPushButton *>(QStringLiteral("removeDownloadButton"))->click();
    QCOMPARE(manager.history().size(), 2);
    DownloadManager reloaded(path);
    QCOMPARE(reloaded.history().size(), 2);
    manager.clearFinished();
    DownloadManager cleared(path);
    QVERIFY(cleared.history().isEmpty());
}

int main(int argc, char *argv[])
{
    const QString tmp = QDir::temp().filePath(QStringLiteral("cosmic-test-tools-xdg"));
    QDir(tmp).removeRecursively();
    qputenv("XDG_CONFIG_HOME", (tmp + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_DATA_HOME", (tmp + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_CACHE_HOME", (tmp + QStringLiteral("/cache")).toUtf8());
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QString());
    QCoreApplication::setApplicationName(QStringLiteral("cosmic-test-tools"));
    TestTools test;
    const int result = QTest::qExec(&test, argc, argv);
    deleteAllBrowserWindows();
    return result;
}

#include "test_tools.moc"
