// Cosmic 0.2.0 tools of BrowserWindow: AI assistant, tab manager,
// privacy dashboard, performance centre, web inspector and Focus.
// Kept in its own translation unit so BrowserWindow.cpp does not grow.

#include "ui/BrowserWindow.h"

#include "browser/Browser.h"
#include "core/ActionRegistry.h"
#include "browser/AiService.h"
#include "core/Settings.h"
#include "core/Translator.h"
#include "ui/AiSidebar.h"
#include "ui/PageTranslator.h"
#include "ui/Toast.h"
#include "ui/TabContent.h"
#include "web/WebPage.h"
#include "web/WebView.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDockWidget>
#include <QMenu>
#include <QPointer>
#include <QSharedPointer>
#include <QTabWidget>
#include <QTimer>
#include <QWebEngineScript>

void BrowserWindow::fetchPageContext(TabContent *tab, std::function<void(Ai::PageContext)> done)
{
    Ai::PageContext context;
    if (!tab || tab->isShowingNewTab() || tab->isShowingCrash()) {
        done(context);
        return;
    }
    WebPage *page = tab->webView()->webPage();
    context.title = page->title();
    context.url = page->url();
    if (Ai::safeAddress(context.url).isEmpty()) {
        done(context);
        return;
    }
    // runJavaScript never calls back for a discarded or crashed page, so
    // a timer guarantees the caller continues.
    auto finished = QSharedPointer<bool>::create(false);
    auto callback = QSharedPointer<std::function<void(Ai::PageContext)>>::create(std::move(done));
    const auto finish = [finished, callback](Ai::PageContext result) {
        if (*finished) {
            return;
        }
        *finished = true;
        (*callback)(result);
    };
    QTimer::singleShot(6000, this, [finish, context] { finish(context); });
    QPointer<WebPage> guard(page);
    page->runJavaScript(
        QStringLiteral("(document.body ? document.body.innerText : '').slice(0, 80000)"),
        QWebEngineScript::ApplicationWorld,
        [finish, context, guard](const QVariant &result) mutable {
            if (!guard) {
                finish(context);
                return;
            }
            context.text = result.toString();
            finish(context);
        });
}

void BrowserWindow::toggleAiSidebar()
{
    if (!m_browser) {
        return;
    }
    if (!m_aiDock) {
        m_aiSidebar = new AiSidebar(m_browser->ai(), this);
        AiSidebar::Sources sources;
        sources.currentPage = [this](std::function<void(Ai::PageContext)> done) {
            fetchPageContext(focusedTab(), std::move(done));
        };
        sources.selectedText = [this]() -> QString {
            TabContent *tab = focusedTab();
            return tab && !tab->isShowingNewTab() && !tab->isShowingCrash()
                ? tab->webView()->webPage()->selectedText() : QString();
        };
        sources.tabs = [this] {
            QList<AiSidebar::TabEntry> entries;
            for (int i = 0; i < m_tabs->count(); ++i) {
                if (TabContent *tab = tabAt(i); tab && !tab->isShowingNewTab()
                                                && !tab->isShowingCrash()) {
                    entries.append({m_tabs->tabText(i), tab->webView()->webPage()->url(), i});
                }
            }
            return entries;
        };
        sources.tabPage = [this](int index, std::function<void(Ai::PageContext)> done) {
            fetchPageContext(tabAt(index), std::move(done));
        };
        m_aiSidebar->setSources(sources);
        m_aiDock = new QDockWidget(tr("Cosmic AI"), this);
        m_aiDock->setObjectName(QStringLiteral("aiDock"));
        m_aiDock->setWidget(m_aiSidebar);
        m_aiDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        m_aiDock->setMinimumWidth(320);
        addDockWidget(Qt::RightDockWidgetArea, m_aiDock);
        m_aiDock->resize(380, height());
        return;
    }
    m_aiDock->setVisible(!m_aiDock->isVisible());
}

void BrowserWindow::buildToolsMenuEntries(QMenu *menu)
{
    menu->addAction(m_actions->action(QStringLiteral("ai.assistant")));
}

// ---------------------------------------------------------------------------
// Universal Translator
// ---------------------------------------------------------------------------

PageTranslator *BrowserWindow::translator()
{
    if (!m_translator && m_browser) {
        m_translator = new PageTranslator(m_browser->ai(), this);
        m_translateIndicator = new QToolButton(this);
        m_translateIndicator->setObjectName(QStringLiteral("translateIndicator"));
        m_translateIndicator->setAutoRaise(true);
        m_translateIndicator->hide();
        statusBar()->addPermanentWidget(m_translateIndicator);
        connect(m_translateIndicator, &QToolButton::clicked, this, [this] {
            TabContent *tab = currentTab();
            if (tab && m_translator->state(tab->webView()->webPage()) == PageTranslator::State::Translated) {
                m_translator->restore(tab->webView()->webPage());
            }
        });
        connect(m_translator, &PageTranslator::stateChanged, this,
                [this](WebPage *) { updateTranslationIndicator(); });
        connect(m_translator, &PageTranslator::message, this,
                [this](const QString &text, const QString &kind) {
            statusBar()->showMessage(text, 5000);
            if (kind != QLatin1String("info")) {
                notify(text, kind);
            }
        });
        connect(m_tabs, &QTabWidget::currentChanged, this, [this] { updateTranslationIndicator(); });
    }
    return m_translator;
}

void BrowserWindow::updateTranslationIndicator()
{
    if (!m_translator || !m_translateIndicator) {
        return;
    }
    TabContent *tab = currentTab();
    WebPage *page = tab && !tab->isShowingNewTab() && !tab->isShowingCrash()
        ? tab->webView()->webPage() : nullptr;
    const auto state = m_translator->state(page);
    if (!page || state == PageTranslator::State::Idle) {
        m_translateIndicator->hide();
        return;
    }
    const QString language = Translator::languageName(m_translator->target(page));
    if (state == PageTranslator::State::Working) {
        m_translateIndicator->setText(tr("Translating to %1…").arg(language));
        m_translateIndicator->setEnabled(false);
        m_translateIndicator->setToolTip(QString());
    } else {
        m_translateIndicator->setText(tr("Translated to %1 · Show original").arg(language));
        m_translateIndicator->setEnabled(true);
        m_translateIndicator->setToolTip(tr("Restore the original page"));
    }
    m_translateIndicator->show();
}

void BrowserWindow::translateCurrentPage()
{
    TabContent *tab = focusedTab();
    if (!m_browser || !tab || tab->isShowingNewTab() || tab->isShowingCrash()) {
        statusBar()->showMessage(tr("Open a web page to translate it."), 4000);
        return;
    }
    WebPage *page = tab->webView()->webPage();
    const QUrl url = page->url();
    if ((url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
        || url.host().isEmpty()) {
        statusBar()->showMessage(tr("Only HTTP and HTTPS pages can be translated."), 5000);
        return;
    }
    AiService *ai = m_browser->ai();
    const Ai::ProviderConfig config = ai->config();
    if (!Ai::validate(config).isEmpty()) {
        // No usable AI provider: keep the Google Translate hand-off.
        translateWithGoogle();
        return;
    }

    PageTranslator *engine = translator();
    const QJsonObject prefs = m_browser->settings()->section(QStringLiteral("translation"));
    const QString host = url.host();

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("translateDialog"));
    dialog.setWindowTitle(tr("Translate page"));
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Translate this page in place with %1 (%2).")
                                     .arg(Ai::kindLabel(config.kind), config.model), &dialog));
    auto *combo = new QComboBox(&dialog);
    combo->setObjectName(QStringLiteral("translateTarget"));
    const QString current = Translator::targetLanguage(prefs, QStringLiteral("it"));
    for (const Translator::Language &language : Translator::languages()) {
        combo->addItem(language.name, language.code);
    }
    combo->setCurrentIndex(qMax(0, combo->findData(current)));
    layout->addWidget(combo);
    auto *always = new QCheckBox(tr("Always translate %1 into this language").arg(host), &dialog);
    always->setObjectName(QStringLiteral("translateAlways"));
    always->setChecked(Translator::siteRule(prefs, host) == QLatin1String("always"));
    always->setToolTip(tr("The text of every page on this site is then sent to the provider "
                          "automatically when it finishes loading."));
    layout->addWidget(always);
    auto *notice = new QLabel(
        tr("The visible text of the page is sent to the provider. Nothing else (no cookies, "
           "passwords or form values)."), &dialog);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *box = new QDialogButtonBox(&dialog);
    auto *translate = box->addButton(tr("Translate"), QDialogButtonBox::AcceptRole);
    auto *original = box->addButton(tr("Show original"), QDialogButtonBox::ActionRole);
    auto *google = box->addButton(tr("Use Google Translate"), QDialogButtonBox::ActionRole);
    box->addButton(QDialogButtonBox::Cancel);
    original->setEnabled(engine->state(page) == PageTranslator::State::Translated);
    layout->addWidget(box);
    connect(translate, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(original, &QPushButton::clicked, &dialog, [&] {
        engine->restore(page);
        dialog.reject();
    });
    bool useGoogle = false;
    connect(google, &QPushButton::clicked, &dialog, [&] {
        useGoogle = true;
        dialog.reject();
    });
    if (dialog.exec() != QDialog::Accepted) {
        if (useGoogle) {
            translateWithGoogle();
        }
        return;
    }

    const QString target = combo->currentData().toString();
    if (ai->needsConsent(config)) {
        QMessageBox question(this);
        question.setIcon(QMessageBox::Question);
        question.setWindowTitle(tr("Send page text?"));
        question.setText(tr("The text of \"%1\" will be sent to %2 (%3) to be translated.")
                             .arg(page->title().left(80), Ai::kindLabel(config.kind),
                                  QUrl(config.baseUrl).host()));
        auto *remember = new QCheckBox(tr("Don't ask again for this provider until Cosmic closes"),
                                       &question);
        question.setCheckBox(remember);
        question.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        question.setDefaultButton(QMessageBox::No);
        if (question.exec() != QMessageBox::Yes) {
            return;
        }
        if (remember->isChecked()) {
            ai->grantSessionConsent(config);
        }
    }
    QJsonObject updated = Translator::withTargetLanguage(prefs, target);
    updated = Translator::withSiteRule(updated, host,
        always->isChecked() ? QStringLiteral("always:") + target : QString());
    m_browser->settings()->setSection(QStringLiteral("translation"), updated);
    m_browser->settings()->save();
    engine->translate(page, target);
}

void BrowserWindow::maybeAutoTranslate(WebPage *page)
{
    if (!m_browser || !page || page != (currentTab() ? currentTab()->webView()->webPage() : nullptr)) {
        return;
    }
    const QUrl url = page->url();
    if (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http")) {
        return;
    }
    const QString target = Translator::siteTarget(
        m_browser->settings()->section(QStringLiteral("translation")), url.host());
    if (target.isEmpty() || !Ai::validate(m_browser->ai()->config()).isEmpty()) {
        return;
    }
    PageTranslator *engine = translator();
    if (engine->isBusy() || engine->state(page) != PageTranslator::State::Idle) {
        return;
    }
    engine->translate(page, target);
}

void BrowserWindow::translateSelection()
{
    TabContent *tab = focusedTab();
    if (!m_browser || !tab || tab->isShowingNewTab() || tab->isShowingCrash()) {
        statusBar()->showMessage(tr("Open a page and select some text first."), 4000);
        return;
    }
    const QString text = tab->webView()->webPage()->selectedText().trimmed();
    if (text.isEmpty()) {
        statusBar()->showMessage(tr("Select some text on the page first."), 4000);
        return;
    }
    AiService *ai = m_browser->ai();
    const Ai::ProviderConfig config = ai->config();
    if (const QString problem = Ai::validate(config); !problem.isEmpty()) {
        QMessageBox::information(this, tr("Translate selection"),
            tr("Translating text needs an AI provider. %1\n\nOpen Cosmic AI Assistant → "
               "Settings to set one up.").arg(problem));
        return;
    }
    if (text.size() > 4000) {
        QMessageBox::information(this, tr("Translate selection"),
                                 tr("Select a shorter passage (up to 4000 characters)."));
        return;
    }
    if (ai->needsConsent(config)
        && QMessageBox::question(this, tr("Send selected text?"),
            tr("The selected text will be sent to %1 (%2) to be translated.")
                .arg(Ai::kindLabel(config.kind), QUrl(config.baseUrl).host())) != QMessageBox::Yes) {
        return;
    }
    const QString target = Translator::targetLanguage(
        m_browser->settings()->section(QStringLiteral("translation")), QStringLiteral("it"));
    statusBar()->showMessage(tr("Translating selection…"), 5000);
    QPointer<BrowserWindow> self(this);
    translator()->translateStrings({text}, target,
        [self, text, target](bool ok, const QStringList &result, const QString &error) {
        if (!self) {
            return;
        }
        if (!ok) {
            self->notify(error.isEmpty() ? tr("Translation failed.") : error, QStringLiteral("error"));
            return;
        }
        auto *dialog = new QDialog(self);
        dialog->setObjectName(QStringLiteral("selectionTranslation"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(tr("Translation — %1").arg(Translator::languageName(target)));
        auto *layout = new QVBoxLayout(dialog);
        auto *view = new QPlainTextEdit(result.value(0), dialog);
        view->setReadOnly(true);
        layout->addWidget(view);
        auto *copy = new QPushButton(tr("Copy"), dialog);
        QObject::connect(copy, &QPushButton::clicked, dialog, [view] { view->selectAll(); view->copy(); });
        layout->addWidget(copy);
        dialog->resize(520, 300);
        dialog->show();
    });
}
