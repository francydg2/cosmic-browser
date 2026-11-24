#include "ui/Toast.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>

Toast::Toast(QWidget *parent)
    : QFrame(parent)
    , m_label(new QLabel(this))
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("toast"));
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    m_label->setWordWrap(true);
    m_label->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(m_label);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &QWidget::hide);
    hide();
    if (parent) {
        parent->installEventFilter(this);
    }
}

QString Toast::message() const
{
    return m_label->text();
}

void Toast::showMessage(const QString &message, const QString &kind, int durationMs)
{
    QString accent = QStringLiteral("#7c6cf0");
    if (kind == QLatin1String("success")) {
        accent = QStringLiteral("#4cc38a");
    } else if (kind == QLatin1String("warning")) {
        accent = QStringLiteral("#e5a93d");
    } else if (kind == QLatin1String("error")) {
        accent = QStringLiteral("#e5584f");
    }
    setStyleSheet(QStringLiteral(
        "#toast { background: #1b1e29; border: 1px solid %1; border-radius: 10px; }"
        "#toast QLabel { color: #e8e9ee; }").arg(accent));
    m_label->setText(message);
    setFixedWidth(qMin(380, qMax(220, parentWidget() ? parentWidget()->width() - 32 : 380)));
    adjustSize();
    reposition();
    show();
    raise();
    m_timer->start(durationMs);
}

bool Toast::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        reposition();
    }
    return QFrame::eventFilter(watched, event);
}

void Toast::reposition()
{
    if (parentWidget()) {
        move(parentWidget()->width() - width() - 16,
             parentWidget()->height() - height() - 44);
    }
}
