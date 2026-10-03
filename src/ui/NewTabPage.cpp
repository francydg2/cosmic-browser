#include "ui/NewTabPage.h"
#include "ui/SaturnIllustration.h"

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

NewTabPage::NewTabPage(QWidget *parent)
    : QWidget(parent)
    , m_search(new QLineEdit(this))
{
    setObjectName(QStringLiteral("newTabPage"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(48, 64, 48, 64);
    layout->addStretch(1);

    auto *art = new SaturnIllustration(this);
    layout->addWidget(art, 0, Qt::AlignHCenter);

    auto *caption = new QLabel(QStringLiteral("COSMIC"), this);
    caption->setObjectName(QStringLiteral("ntpCaption"));
    caption->setAlignment(Qt::AlignHCenter);
    layout->addWidget(caption, 0, Qt::AlignHCenter);
    layout->addSpacing(28);

    m_search->setObjectName(QStringLiteral("ntpSearch"));
    m_search->setPlaceholderText(tr("Search the web or enter address"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(520);
    layout->addWidget(m_search, 0, Qt::AlignHCenter);

    layout->addStretch(2);

    connect(m_search, &QLineEdit::returnPressed, this,
            [this] { emit inputSubmitted(m_search->text()); });
}

void NewTabPage::focusSearchField()
{
    m_search->setFocus();
    m_search->selectAll();
}
