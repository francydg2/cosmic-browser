#pragma once

#include <QWebEngineView>

class WebPage;
class WebPageDelegate;
class QWebEngineProfile;
class QContextMenuEvent;

/// QWebEngineView pre-wired with a WebPage that delegates window creation.
/// This is a QWidget only because QWebEngineView is one (engine API);
/// it contains no Cosmic UI code (ui/ module).
class WebView : public QWebEngineView
{
    Q_OBJECT

public:
    explicit WebView(WebPageDelegate *delegate, QWebEngineProfile *profile,
                     QWidget *parent = nullptr);

    WebPage *webPage() const;

signals:
    void savePasswordRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    WebPage *m_page; // owned by this view (child QObject)
};
