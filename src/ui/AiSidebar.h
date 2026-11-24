#pragma once

#include "core/AiPrompt.h"
#include "core/AiProvider.h"

#include <QWidget>
#include <functional>

class AiClient;
class AiService;
class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QTimer;

/// Cosmic AI Assistant: streaming chat panel with saved conversations.
/// Page content is attached only when the user asks for it, and only
/// after consent when the provider is not on this machine.
class AiSidebar : public QWidget
{
    Q_OBJECT

public:
    struct TabEntry
    {
        QString title;
        QUrl url;
        int index = -1;
    };
    /// What the browser window can provide. All callbacks run on the GUI
    /// thread; page text is delivered asynchronously.
    struct Sources
    {
        std::function<void(std::function<void(Ai::PageContext)>)> currentPage;
        std::function<QString()> selectedText;
        std::function<QList<TabEntry>()> tabs;
        std::function<void(int, std::function<void(Ai::PageContext)>)> tabPage;
    };

    enum class Context { None, CurrentPage, Selection, Tabs };

    explicit AiSidebar(AiService *service, QWidget *parent = nullptr);
    ~AiSidebar() override;

    void setSources(const Sources &sources) { m_sources = sources; }

    /// Sends \p question with the chosen context. \p tabIndexes is used
    /// for Context::Tabs.
    void ask(const QString &question, Context context, const QList<int> &tabIndexes = {});
    bool isBusy() const;

    /// Opens the provider configuration dialog.
    void showSettings();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void reloadConversations(const QString &select = QString());
    void renderTranscript();
    void startNewConversation();
    void deleteConversation();
    void sendFromInput();
    void chooseTabs();
    void beginRequest(const QString &question, const QList<Ai::PageContext> &pages,
                      const QStringList &sourceTitles);
    void finishStream(const QString &error);
    void setStatus(const QString &text, bool error = false);
    void updateControls();

    AiService *m_service;
    AiClient *m_client;
    Sources m_sources;
    QComboBox *m_conversations;
    QTextBrowser *m_transcript;
    QPlainTextEdit *m_input;
    QPushButton *m_send;
    QPushButton *m_stop;
    QLabel *m_status;
    QTimer *m_renderTimer;
    QString m_conversationId;
    bool m_includePage = true;
    QCheckBox *m_pageCheck = nullptr;
    QString m_streaming; // assistant text being received
    bool m_busy = false;
    bool m_gathering = false;
};
