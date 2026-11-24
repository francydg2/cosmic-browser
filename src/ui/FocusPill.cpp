#include "ui/FocusPill.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

FocusPill::FocusPill(QWidget *parent)
    : QFrame(parent)
    , m_label(new QLabel(this))
{
    setObjectName(QStringLiteral("focusPill"));
    setStyleSheet(QStringLiteral(
        "#focusPill { background: #1b1e29; border: 1px solid #7c6cf0; border-radius: 14px; }"
        "#focusPill QLabel { color: #e8e9ee; font-weight: 600; }"
        "#focusPill QPushButton { background: #7c6cf0; color: white; border: 0; border-radius: 10px;"
        " padding: 3px 12px; }"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 6, 8, 6);
    layout->addWidget(m_label);
    auto *end = new QPushButton(tr("End"), this);
    end->setObjectName(QStringLiteral("focusPillEnd"));
    end->setToolTip(tr("End Cosmic Focus"));
    layout->addWidget(end);
    connect(end, &QPushButton::clicked, this, &FocusPill::endRequested);
    hide();
    if (parent) {
        parent->installEventFilter(this);
    }
}

void FocusPill::setRemaining(const QString &text)
{
    m_label->setText(tr("Focus · %1").arg(text));
    adjustSize();
    reposition();
}

bool FocusPill::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        reposition();
    }
    return QFrame::eventFilter(watched, event);
}

void FocusPill::reposition()
{
    if (parentWidget()) {
        move(16, parentWidget()->height() - height() - 16);
    }
}
