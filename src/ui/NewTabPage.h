#pragma once

#include <QVector>
#include <QUrl>
#include <QWidget>

struct Bookmark;
class QLineEdit;
class QGridLayout;
class QLabel;
class QPaintEvent;

/// Local start page with search, date/time and shortcuts to saved bookmarks.
class NewTabPage : public QWidget
{
    Q_OBJECT

public:
    explicit NewTabPage(QWidget *parent = nullptr);

    /// Gives keyboard focus to the central search field.
    void focusSearchField();
    void setBookmarks(const QVector<Bookmark> &bookmarks);
    void setPrivacyStatus(bool enabled, int blockedCount);

signals:
    /// Emitted when the user submits the search field; the window
    /// resolves the text (URL vs search, configured engine).
    void inputSubmitted(const QString &text);
    void bookmarkActivated(const QUrl &url);

private:
    void paintEvent(QPaintEvent *event) override;
    void updateDateTime();

    QLineEdit *m_search;
    QLabel *m_clock;
    QLabel *m_date;
    QLabel *m_privacyBadge;
    QLabel *m_emptyBookmarks;
    QGridLayout *m_bookmarkGrid;
};
