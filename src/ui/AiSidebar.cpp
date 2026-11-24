#include "ui/AiSidebar.h"

#include "browser/AiClient.h"
#include "browser/AiService.h"
#include "ui/AiSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QPushButton>
#include <QSharedPointer>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

AiSidebar::AiSidebar(AiService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
    , m_client(new AiClient(this))
    , m_conversations(new QComboBox(this))
    , m_transcript(new QTextBrowser(this))
    , m_input(new QPlainTextEdit(this))
    , m_send(new QPushButton(tr("Send"), this))
    , m_stop(new QPushButton(tr("Stop"), this))
    , m_status(new QLabel(this))
    , m_renderTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("aiSidebar"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *top = new QHBoxLayout;
    m_conversations->setObjectName(QStringLiteral("aiConversations"));
    m_conversations->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_conversations->setMinimumContentsLength(14);
    m_conversations->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *newButton = new QPushButton(tr("New"), this);
    auto *deleteButton = new QPushButton(tr("Delete"), this);
    auto *settingsButton = new QPushButton(tr("Settings"), this);
    newButton->setObjectName(QStringLiteral("aiNew"));
    deleteButton->setObjectName(QStringLiteral("aiDelete"));
    settingsButton->setObjectName(QStringLiteral("aiSettings"));
    top->addWidget(m_conversations, 1);
    top->addWidget(newButton);
    top->addWidget(deleteButton);
    top->addWidget(settingsButton);
    layout->addLayout(top);

    auto *quick = new QHBoxLayout;
    auto *summarize = new QPushButton(tr("Summarize page"), this);
    auto *selection = new QPushButton(tr("Explain selection"), this);
    auto *tabs = new QPushButton(tr("Other tabs…"), this);
    summarize->setObjectName(QStringLiteral("aiSummarize"));
    selection->setObjectName(QStringLiteral("aiSelection"));
    tabs->setObjectName(QStringLiteral("aiTabs"));
    quick->addWidget(summarize);
    quick->addWidget(selection);
    quick->addWidget(tabs);
    layout->addLayout(quick);

    m_transcript->setObjectName(QStringLiteral("aiTranscript"));
    m_transcript->setOpenLinks(false); // links in model output never navigate
    m_transcript->setOpenExternalLinks(false);
    layout->addWidget(m_transcript, 1);

    m_status->setObjectName(QStringLiteral("aiStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_pageCheck = new QCheckBox(tr("Include the current page with my message"), this);
    m_pageCheck->setObjectName(QStringLiteral("aiIncludePage"));
    m_pageCheck->setChecked(m_includePage);
    layout->addWidget(m_pageCheck);

    m_input->setObjectName(QStringLiteral("aiInput"));
    m_input->setPlaceholderText(tr("Ask something… (Ctrl+Enter to send)"));
    m_input->setMaximumHeight(90);
    layout->addWidget(m_input);
    auto *buttons = new QHBoxLayout;
    m_send->setObjectName(QStringLiteral("aiSend"));
    m_stop->setObjectName(QStringLiteral("aiStop"));
    buttons->addStretch();
    buttons->addWidget(m_stop);
    buttons->addWidget(m_send);
    layout->addLayout(buttons);

    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(80);
    connect(m_renderTimer, &QTimer::timeout, this, &AiSidebar::renderTranscript);
    connect(m_pageCheck, &QCheckBox::toggled, this, [this](bool on) { m_includePage = on; });
    connect(m_conversations, &QComboBox::activated, this, [this](int index) {
        m_conversationId = m_conversations->itemData(index).toString();
        m_streaming.clear();
        renderTranscript();
    });
    connect(newButton, &QPushButton::clicked, this, &AiSidebar::startNewConversation);
    connect(deleteButton, &QPushButton::clicked, this, &AiSidebar::deleteConversation);
    connect(settingsButton, &QPushButton::clicked, this, &AiSidebar::showSettings);
    connect(m_send, &QPushButton::clicked, this, &AiSidebar::sendFromInput);
    connect(m_stop, &QPushButton::clicked, this, [this] {
        if (m_busy) {
            m_client->cancel();
            finishStream(QString());
        }
    });
    connect(summarize, &QPushButton::clicked, this, [this] {
        ask(tr("Summarize this page in a few clear bullet points."), Context::CurrentPage);
    });
    connect(selection, &QPushButton::clicked, this, [this] {
        ask(tr("Explain the selected text."), Context::Selection);
    });
    connect(tabs, &QPushButton::clicked, this, &AiSidebar::chooseTabs);
    connect(m_client, &AiClient::delta, this, [this](const QString &text) {
        m_streaming += text;
        if (!m_renderTimer->isActive()) {
            m_renderTimer->start();
        }
    });
    connect(m_client, &AiClient::finished, this, [this] { finishStream(QString()); });
    connect(m_client, &AiClient::failed, this, [this](const QString &message) {
        finishStream(message);
    });
    connect(m_service, &AiService::configChanged, this, &AiSidebar::updateControls);

    // Ctrl+Enter sends; plain Enter inserts a newline.
    m_input->installEventFilter(this);
    reloadConversations();
    updateControls();
}

AiSidebar::~AiSidebar() = default;

bool AiSidebar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
            && (key->modifiers() & Qt::ControlModifier)) {
            sendFromInput();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool AiSidebar::isBusy() const
{
    return m_busy || m_gathering;
}

void AiSidebar::setStatus(const QString &text, bool error)
{
    m_status->setStyleSheet(error ? QStringLiteral("color: #e5584f;")
                                  : QStringLiteral("color: #98a2b2;"));
    m_status->setText(text);
}

void AiSidebar::updateControls()
{
    const Ai::ProviderConfig config = m_service->config();
    const bool busy = isBusy();
    m_send->setEnabled(!busy);
    m_stop->setEnabled(m_busy);
    m_stop->setVisible(busy);
    if (!busy) {
        const QString problem = Ai::validate(config);
        setStatus(problem.isEmpty()
                      ? tr("%1 · %2%3").arg(Ai::kindLabel(config.kind), config.model,
                                            Ai::isLocalEndpoint(QUrl(config.baseUrl))
                                                ? tr(" · runs on this computer") : QString())
                      : tr("Not configured: %1").arg(problem),
                  !problem.isEmpty());
    }
}

void AiSidebar::reloadConversations(const QString &select)
{
    const QSignalBlocker blocker(m_conversations);
    m_conversations->clear();
    const auto items = m_service->conversations()->conversations();
    for (const AiConversation &c : items) {
        m_conversations->addItem(c.title.isEmpty() ? tr("New conversation") : c.title, c.id);
    }
    int index = m_conversations->findData(select.isEmpty() ? m_conversationId : select);
    if (index < 0 && m_conversations->count() > 0) {
        index = 0;
    }
    m_conversations->setCurrentIndex(index);
    m_conversationId = index >= 0 ? m_conversations->itemData(index).toString() : QString();
    renderTranscript();
}

void AiSidebar::renderTranscript()
{
    const AiConversation conversation = m_service->conversations()->find(m_conversationId);
    QString markdown;
    if (conversation.messages.isEmpty() && m_streaming.isEmpty()) {
        markdown = tr("*Ask about the page you are reading, or press “Summarize page”. "
                      "Nothing is sent to the provider until you send a message.*");
    }
    for (int i = 0; i < conversation.messages.size(); ++i) {
        const Ai::Message &message = conversation.messages.at(i);
        const bool user = message.role == QLatin1String("user");
        const bool streamingLast = m_busy && i == conversation.messages.size() - 1 && !user;
        markdown += QStringLiteral("**%1**\n\n%2\n\n").arg(
            user ? tr("You") : tr("Assistant"),
            streamingLast ? m_streaming : message.content);
    }
    m_transcript->setMarkdown(markdown);
    m_transcript->verticalScrollBar()->setValue(m_transcript->verticalScrollBar()->maximum());
}

void AiSidebar::startNewConversation()
{
    if (isBusy()) {
        return;
    }
    m_conversationId = m_service->conversations()->create();
    m_service->conversations()->save();
    reloadConversations(m_conversationId);
    m_input->setFocus();
}

void AiSidebar::deleteConversation()
{
    if (isBusy() || m_conversationId.isEmpty()) {
        return;
    }
    if (QMessageBox::question(this, tr("Delete conversation"),
            tr("Delete this conversation from this computer?")) != QMessageBox::Yes) {
        return;
    }
    m_service->conversations()->remove(m_conversationId);
    m_service->conversations()->save();
    m_conversationId.clear();
    reloadConversations();
}

void AiSidebar::showSettings()
{
    AiSettingsDialog dialog(m_service, this);
    dialog.exec();
    updateControls();
}

void AiSidebar::sendFromInput()
{
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() || isBusy()) {
        return;
    }
    ask(text, m_includePage ? Context::CurrentPage : Context::None);
}

void AiSidebar::chooseTabs()
{
    if (isBusy() || !m_sources.tabs) {
        return;
    }
    const QList<TabEntry> tabs = m_sources.tabs();
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Ask about several tabs"));
    dialog.resize(480, 420);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        tr("Choose the tabs whose text is sent with your question. Only HTTP(S) pages can be "
           "included."), &dialog));
    auto *list = new QListWidget(&dialog);
    for (const TabEntry &entry : tabs) {
        auto *item = new QListWidgetItem(entry.title.isEmpty() ? entry.url.toString() : entry.title, list);
        item->setData(Qt::UserRole, entry.index);
        item->setToolTip(Ai::safeAddress(entry.url));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        if (Ai::safeAddress(entry.url).isEmpty()) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
    }
    layout->addWidget(list);
    auto *question = new QPlainTextEdit(&dialog);
    question->setPlaceholderText(tr("Your question about the selected tabs"));
    question->setMaximumHeight(70);
    question->setPlainText(tr("Compare these pages and summarize the main differences."));
    layout->addWidget(question);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(box);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    QList<int> chosen;
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->checkState() == Qt::Checked) {
            chosen.append(list->item(i)->data(Qt::UserRole).toInt());
        }
    }
    if (chosen.isEmpty() || question->toPlainText().trimmed().isEmpty()) {
        return;
    }
    ask(question->toPlainText().trimmed(), Context::Tabs, chosen);
}

void AiSidebar::ask(const QString &question, Context context, const QList<int> &tabIndexes)
{
    if (isBusy() || question.trimmed().isEmpty()) {
        return;
    }
    const Ai::ProviderConfig config = m_service->config();
    const QString problem = Ai::validate(config);
    if (!problem.isEmpty()) {
        setStatus(tr("Not configured: %1").arg(problem), true);
        showSettings();
        if (!Ai::validate(m_service->config()).isEmpty()) {
            return;
        }
    }

    if (context == Context::None) {
        beginRequest(question, {}, {});
        return;
    }
    if (context == Context::Selection) {
        const QString selected = m_sources.selectedText ? m_sources.selectedText().trimmed() : QString();
        if (selected.isEmpty()) {
            setStatus(tr("Select some text on the page first."), true);
            return;
        }
        Ai::PageContext page;
        page.title = tr("Selected text");
        page.text = selected;
        beginRequest(question, {page}, {tr("selected text")});
        return;
    }

    m_gathering = true;
    updateControls();
    setStatus(tr("Reading the page…"));
    auto pages = QSharedPointer<QList<Ai::PageContext>>::create();
    QPointer<AiSidebar> self(this);
    const auto done = [self, question, pages] {
        if (!self) {
            return;
        }
        self->m_gathering = false;
        QStringList titles;
        QList<Ai::PageContext> usable;
        for (const Ai::PageContext &page : *pages) {
            if (!page.text.trimmed().isEmpty()) {
                usable.append(page);
                titles << (page.title.isEmpty() ? Ai::safeAddress(page.url) : page.title);
            }
        }
        if (usable.isEmpty()) {
            self->updateControls();
            self->setStatus(tr("No readable text was found. Open a web page and try again; "
                               "nothing was sent."), true);
            return;
        }
        self->beginRequest(question, usable, titles);
    };
    if (context == Context::CurrentPage) {
        if (!m_sources.currentPage) {
            done();
            return;
        }
        m_sources.currentPage([pages, done](Ai::PageContext page) {
            pages->append(page);
            done();
        });
        return;
    }
    // Several tabs: fetch one after the other.
    auto remaining = QSharedPointer<QList<int>>::create(tabIndexes);
    auto step = QSharedPointer<std::function<void()>>::create();
    const auto sources = m_sources;
    *step = [pages, remaining, step, sources, done] {
        if (remaining->isEmpty() || !sources.tabPage) {
            done();
            return;
        }
        const int index = remaining->takeFirst();
        sources.tabPage(index, [pages, step](Ai::PageContext page) {
            pages->append(page);
            (*step)();
        });
    };
    (*step)();
}

void AiSidebar::beginRequest(const QString &question, const QList<Ai::PageContext> &pages,
                             const QStringList &sourceTitles)
{
    const Ai::ProviderConfig config = m_service->config();
    if (!pages.isEmpty() && m_service->needsConsent(config)) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(tr("Send page content?"));
        box.setText(tr("The text of the following page(s) will be sent to %1 (%2):\n\n%3\n\n"
                       "Passwords, cookies and form fields are not included, but the page text "
                       "may contain personal information.")
                        .arg(Ai::kindLabel(config.kind), QUrl(config.baseUrl).host(),
                             sourceTitles.join(QLatin1Char('\n'))));
        auto *remember = new QCheckBox(tr("Don't ask again for this provider until Cosmic closes"), &box);
        box.setCheckBox(remember);
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.setDefaultButton(QMessageBox::No);
        if (box.exec() != QMessageBox::Yes) {
            updateControls();
            setStatus(tr("Nothing was sent."));
            return;
        }
        if (remember->isChecked()) {
            m_service->grantSessionConsent(config);
        }
    }

    AiConversations *store = m_service->conversations();
    if (m_conversationId.isEmpty() || store->find(m_conversationId).id.isEmpty()) {
        m_conversationId = store->create();
    }
    const QList<Ai::Message> history = store->find(m_conversationId).messages;
    QString shown = question;
    if (!sourceTitles.isEmpty()) {
        shown += QStringLiteral("\n\n*— %1*").arg(tr("with: %1").arg(sourceTitles.join(QStringLiteral(", "))));
    }
    store->append(m_conversationId, {QStringLiteral("user"), shown});
    store->append(m_conversationId, {QStringLiteral("assistant"), QString()});
    m_input->clear();
    m_streaming.clear();
    m_busy = true;
    reloadConversations(m_conversationId);
    updateControls();
    setStatus(tr("Waiting for %1…").arg(Ai::kindLabel(config.kind)));
    m_client->send(config, Ai::buildMessages(history, question, pages, m_service->language()));
}

void AiSidebar::finishStream(const QString &error)
{
    if (!m_busy) {
        return;
    }
    m_busy = false;
    m_renderTimer->stop();
    AiConversations *store = m_service->conversations();
    if (m_streaming.isEmpty() && !error.isEmpty()) {
        store->updateLastAssistant(m_conversationId, tr("*No answer: %1*").arg(error));
    } else {
        QString text = m_streaming;
        if (!error.isEmpty()) {
            text += tr("\n\n*[interrupted: %1]*").arg(error);
        }
        store->updateLastAssistant(m_conversationId, text);
    }
    m_streaming.clear();
    store->save();
    reloadConversations(m_conversationId);
    updateControls();
    if (!error.isEmpty()) {
        setStatus(error, true);
    }
}
