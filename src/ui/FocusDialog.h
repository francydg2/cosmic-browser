#pragma once

#include "core/Focus.h"

#include <QDialog>

class BrowserWindow;
class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QSpinBox;

/// Chooses a Focus profile, duration and restricted sites, then starts
/// the session; while a session runs it shows the time left and ends it.
class FocusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FocusDialog(BrowserWindow *window);

private:
    void loadProfile(const QString &id);
    FocusProfile currentValues() const;

    BrowserWindow *m_window;
    QComboBox *m_profile;
    QSpinBox *m_minutes;
    QPlainTextEdit *m_hosts;
    QCheckBox *m_hideChrome;
    QComboBox *m_workspace;
    QLabel *m_status;
    QPushButton *m_start;
    QPushButton *m_end;
};
