#include "web/WebPage.h"
#include "web/WebView.h"
#include "web/PagePolicy.h"
#include "core/Logging.h"
#include "core/UrlUtils.h"

#include <QTimer>
#include <QWebEngineProfile>

WebPage::WebPage(WebPageDelegate *delegate, QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile ? profile : QWebEngineProfile::defaultProfile(), parent)
    , m_delegate(delegate)
{
    connect(this, &QWebEnginePage::loadStarted, this, [this] { m_loading = true; });
    connect(this, &QWebEnginePage::loadFinished, this, [this](bool) { m_loading = false; });

    connect(this, &QWebEnginePage::renderProcessTerminated, this,
            [this](RenderProcessTerminationStatus status, int exitCode) {
                qCWarning(lcCosmic).noquote()
                    << "renderer terminated for" << url().toString()
                    << "status" << status << "exitCode" << exitCode
                    << "- the browser keeps running";
            });

    connect(this, &QWebEnginePage::featurePermissionRequested, this,
            [this](const QUrl &origin, Feature feature) {
                const QString key = PagePolicy::featureKey(feature);
                if (key.isEmpty()) {
                    setFeaturePermission(origin, feature, PermissionDeniedByUser);
                    return;
                }
                const PagePolicy *policy = m_delegate ? m_delegate->pagePolicy() : nullptr;
                const QString current =
                    policy ? policy->permission(key) : QStringLiteral("ask");
                if (current == QLatin1String("allow")) {
                    setFeaturePermission(origin, feature, PermissionGrantedByUser);
                } else if (current == QLatin1String("block")) {
                    setFeaturePermission(origin, feature, PermissionDeniedByUser);
                } else {
                    emit featurePermissionAsk(this, origin, feature);
                }
            });
}

QWebEnginePage *WebPage::createWindow(WebWindowType type)
{
    if (!m_delegate) {
        qCWarning(lcCosmic) << "no delegate; denying new window request, type" << type;
        return nullptr;
    }
    WebPage *page = m_delegate->createPageForWindowType(type);
    qCInfo(lcCosmic) << "createWindow type" << type << "->" << (page ? "page created" : "denied");
    return page;
}

bool WebPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    const PagePolicy *policy = m_delegate ? m_delegate->pagePolicy() : nullptr;
    if (isMainFrame && policy && policy->httpsOnly && url.isValid()
        && url.scheme() == QLatin1String("http") && !url.host().isEmpty()) {
        const QUrl upgraded = UrlUtils::upgradedToHttps(url);
        if (upgraded != url) {
            qCInfo(lcCosmic).noquote() << "https-only upgrade:" << url.toString() << "->"
                                       << upgraded.toString();
            QTimer::singleShot(0, this, [this, upgraded] { setUrl(upgraded); });
            return false;
        }
    }
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}
