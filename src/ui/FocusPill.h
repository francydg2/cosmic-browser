#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

/// Small always-visible indicator of an active Focus session (time left
/// and an End button), shown at the bottom left of the window.
class FocusPill : public QFrame
{
    Q_OBJECT

public:
    explicit FocusPill(QWidget *parent);
    void setRemaining(const QString &text);

signals:
    void endRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void reposition();
    QLabel *m_label;
};
