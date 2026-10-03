#pragma once

#include <QUrl>
#include <QWidget>

class QLineEdit;

/// Phase 1 predisposition of the future Cosmic start page (spec §40-41):
/// dark gray background, local Saturn illustration, central search bar.
/// Self-contained: the Theme/UI Engine (Phase 3) can replace this class
/// without touching tabs or the window shell.
class NewTabPage : public QWidget
{
    Q_OBJECT

public:
    explicit NewTabPage(QWidget *parent = nullptr);

    /// Gives keyboard focus to the central search field.
    void focusSearchField();

signals:
    /// Emitted when the user submits the search field; the window
    /// resolves the text (URL vs search, configured engine).
    void inputSubmitted(const QString &text);

private:
    QLineEdit *m_search;
};
