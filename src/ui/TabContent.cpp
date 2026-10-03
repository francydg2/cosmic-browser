#include "ui/TabContent.h"
#include "ui/NewTabPage.h"
#include "web/WebView.h"
#include "web/WebPage.h"
#include "core/Logging.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

TabContent::TabContent(WebPageDelegate *delegate, QWebEngineProfile *profile,
                       const QString &userId, QWidget *parent)
    : QStackedWidget(parent)
    , m_web(new WebView(delegate, profile, this))
    , m_newTab(new NewTabPage(this))
    , m_crashPage(new QWidget(this))
    , m_userId(userId)
{
    setObjectName(QStringLiteral("tabContent"));

    connect(m_web->webPage(), &WebPage::featurePermissionAsk, this,
            &TabContent::featurePermissionAsk);

    // --- crash page -------------------------------------------------------
    m_crashPage->setObjectName(QStringLiteral("crashPage"));
    auto *crashLayout = new QVBoxLayout(m_crashPage);
    crashLayout->addStretch(1);

    auto *crashTitle = new QLabel(tr("This tab crashed."), m_crashPage);
    crashTitle->setObjectName(QStringLiteral("crashTitle"));
    crashTitle->setAlignment(Qt::AlignHCenter);
    crashLayout->addWidget(crashTitle);

    auto *crashHint = new QLabel(tr("The rest of the browser is still running."), m_crashPage);
    crashHint->setObjectName(QStringLiteral("crashHint"));
    crashHint->setAlignment(Qt::AlignHCenter);
    crashLayout->addWidget(crashHint);
    crashLayout->addSpacing(20);

    auto *reloadButton = new QPushButton(tr("Reload page"), m_crashPage);
    reloadButton->setObjectName(QStringLiteral("crashReload"));
    reloadButton->setCursor(Qt::PointingHandCursor);
    connect(reloadButton, &QPushButton::clicked, this, [this] {
        m_web->webPage()->triggerAction(QWebEnginePage::Reload);
    });
    crashLayout->addWidget(reloadButton, 0, Qt::AlignHCenter);
    crashLayout->addStretch(2);

    // --- stack ------------------------------------------------------------
    addWidget(m_web);      // Web
    addWidget(m_newTab);   // NewTab
    addWidget(m_crashPage); // Crash
    setCurrentIndex(NewTab);

    connect(this, &QStackedWidget::currentChanged, this, &TabContent::modeChanged);

    // Any navigation (user input, redirects, window.open targets) reveals
    // the web view; loading a page also recovers from a crash page.
    connect(m_web, &WebView::urlChanged, this, [this](const QUrl &url) {
        if (currentWidget() != m_crashPage && !url.isEmpty()
            && url.toString() != QLatin1String("about:blank")) {
            showWeb();
        }
    });
    connect(m_web, &WebView::loadStarted, this, [this] {
        if (currentWidget() == m_crashPage) {
            showWeb();
        }
    });

    connect(m_newTab, &NewTabPage::inputSubmitted, this, &TabContent::inputSubmitted);

    connect(m_web->webPage(), &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus status, int exitCode) {
                qCWarning(lcCosmic).noquote() << "tab crashed, status" << status
                                              << "exit" << exitCode << "- showing crash page";
                showCrash();
            });
}

bool TabContent::isShowingNewTab() const
{
    return currentWidget() == m_newTab;
}

bool TabContent::isShowingCrash() const
{
    return currentWidget() == m_crashPage;
}

void TabContent::showNewTab()
{
    setCurrentIndex(NewTab);
}

void TabContent::showWeb()
{
    setCurrentIndex(Web);
}

void TabContent::showCrash()
{
    setCurrentIndex(Crash);
}

void TabContent::deleteCloseButton()
{
    if (m_closeButton) {
        m_closeButton->deleteLater();
        m_closeButton = nullptr;
    }
}
