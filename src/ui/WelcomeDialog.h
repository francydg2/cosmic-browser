#pragma once

#include <QDialog>

class Browser;
class BrowserWindow;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

class WelcomeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WelcomeDialog(Browser *browser, BrowserWindow *window,
                           QWidget *parent = nullptr);

private:
    QWidget *makePage(const QString &title, const QString &description);
    QWidget *makeFeatureCard(const QString &title, const QString &description);
    void setPage(int index);
    void chooseBookmarksFile();
    bool finishSetup();

    Browser *m_browser;
    BrowserWindow *m_window;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_stepLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
    QLabel *m_bookmarkFileLabel = nullptr;
    QCheckBox *m_blockingCheck = nullptr;
    QCheckBox *m_dntCheck = nullptr;
    QCheckBox *m_sessionCookiesCheck = nullptr;
    QCheckBox *m_ramSaverCheck = nullptr;
    QCheckBox *m_defaultBrowserCheck = nullptr;
    QComboBox *m_engineCombo = nullptr;
    QLineEdit *m_profileNameEdit = nullptr;
    QPushButton *m_backButton = nullptr;
    QPushButton *m_nextButton = nullptr;
    QString m_bookmarkFile;
};
