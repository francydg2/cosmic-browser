#pragma once

#include <QLineEdit>
#include <QUrl>
#include <QList>

class WebPage;

/// Address bar: navigates the current page and mirrors its URL.
/// URL-vs-search detection lives in core/UrlUtils (not here).
class UrlBar : public QLineEdit
{
    Q_OBJECT

public:
    explicit UrlBar(QWidget *parent = nullptr);

    /// Binds the bar to \p page (nullptr unbinds). Re-binding is cheap.
    void setPage(WebPage *page);

signals:
    /// Emitted on Enter with the raw text; the window resolves it against
    /// the configured search engine (core/Browser::resolveInput).
    void inputSubmitted(const QString &text);

protected:
    void focusInEvent(QFocusEvent *event) override;

private:
    void applyUrl(const QUrl &url);

    WebPage *m_page = nullptr;
    QList<QMetaObject::Connection> m_connections;
};
