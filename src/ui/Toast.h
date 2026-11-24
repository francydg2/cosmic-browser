#pragma once

#include <QFrame>

class QLabel;
class QTimer;

/// Small non-modal notification shown at the top-right of its parent
/// window and hidden automatically. A new message replaces the current
/// one. It never takes keyboard focus.
class Toast : public QFrame
{
    Q_OBJECT

public:
    explicit Toast(QWidget *parent);

    /// Shows \p message for \p durationMs. \p kind is "info", "success",
    /// "warning" or "error" (accent colour only).
    void showMessage(const QString &message, const QString &kind = QStringLiteral("info"),
                     int durationMs = 4500);
    QString message() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void reposition();

    QLabel *m_label;
    QTimer *m_timer;
};
