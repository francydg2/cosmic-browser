#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <functional>

class AiClient;
class AiService;
class WebPage;

/// In-page translation through the configured AI provider. The page's
/// text nodes are replaced one by one (markup, layout and scripts are
/// not touched) and their original text is kept inside the page so
/// restore() brings back exactly what was there.
class PageTranslator : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Working, Translated };

    PageTranslator(AiService *service, QObject *parent = nullptr);
    ~PageTranslator() override;

    State state(WebPage *page) const;
    QString target(WebPage *page) const;
    bool isBusy() const { return m_busy; }

    /// Translates the visible text of \p page. The caller has already
    /// obtained the user's consent for sending the text to the provider.
    void translate(WebPage *page, const QString &targetCode);
    void restore(WebPage *page);

    /// Translates short strings (e.g. a selection). Calls \p done once.
    void translateStrings(const QStringList &texts, const QString &targetCode,
                          std::function<void(bool ok, const QStringList &result,
                                             const QString &error)> done);

signals:
    void stateChanged(WebPage *page);
    /// kind: info, success, warning, error
    void message(const QString &text, const QString &kind);

private:
    struct Entry
    {
        State state = State::Idle;
        QString target;
    };
    void setState(WebPage *page, State state, const QString &target = QString());
    void runBatches(const QPointer<WebPage> &page, const QStringList &texts,
                    const QString &targetCode, quint64 generation);
    void requestBatch(const QStringList &texts, const QString &targetCode, bool strict,
                      std::function<void(bool, const QStringList &, const QString &)> done);

    AiService *m_service;
    AiClient *m_client;
    QHash<WebPage *, Entry> m_entries;
    bool m_busy = false;
    quint64 m_generation = 0;
};
