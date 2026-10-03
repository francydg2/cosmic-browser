#include "ui/NameColorDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

const QList<QString> &NameColorDialog::presetColors()
{
    static const QList<QString> colors = {
        QStringLiteral("#7c6cf0"), QStringLiteral("#2ec4b6"), QStringLiteral("#ff9f1c"),
        QStringLiteral("#ef476f"), QStringLiteral("#06d6a0"), QStringLiteral("#ffd166"),
    };
    return colors;
}

NameColorDialog::NameColorDialog(const QString &title, const QString &initialName,
                                 const QString &initialColor, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("nameColorDialog"));
    setWindowTitle(title);
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto *nameLabel = new QLabel(tr("Name:"), this);
    layout->addWidget(nameLabel);

    m_nameEdit = new QLineEdit(initialName, this);
    m_nameEdit->setObjectName(QStringLiteral("nameEdit"));
    m_nameEdit->setClearButtonEnabled(true);
    m_nameEdit->setPlaceholderText(tr("e.g. Work"));
    layout->addWidget(m_nameEdit);

    auto *colorLabel = new QLabel(tr("Color:"), this);
    layout->addWidget(colorLabel);

    auto *swatchRow = new QHBoxLayout;
    swatchRow->setSpacing(8);
    const QList<QString> colors = presetColors();
    for (const QString &color : colors) {
        auto *swatch = new QToolButton(this);
        swatch->setObjectName(QStringLiteral("colorSwatch"));
        swatch->setProperty("color", color);
        swatch->setCheckable(true);
        swatch->setFixedSize(36, 28);
        swatch->setToolTip(color);
        swatch->setStyleSheet(QStringLiteral(
            "QToolButton#colorSwatch { background-color: %1; border: 2px solid transparent;"
            " border-radius: 6px; }"
            "QToolButton#colorSwatch:checked { border-color: #e8e9ee; }"
            "QToolButton#colorSwatch:hover { border-color: #9aa0ae; }")
                                  .arg(color));
        connect(swatch, &QToolButton::clicked, this,
                [this, swatch] { selectSwatch(swatch); });
        swatchRow->addWidget(swatch);
        m_swatchButtons.append(swatch);
    }
    swatchRow->addStretch(1);
    layout->addLayout(swatchRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &NameColorDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &NameColorDialog::reject);
    layout->addWidget(buttons);

    // Initial selection.
    QString color = initialColor;
    if (!colors.contains(color)) {
        color = colors.first();
    }
    m_color = color;
    for (QToolButton *swatch : m_swatchButtons) {
        const bool selected = swatch->property("color").toString() == color;
        swatch->setChecked(selected);
    }

    m_nameEdit->selectAll();
    m_nameEdit->setFocus();
}

QString NameColorDialog::nameValue() const
{
    return m_nameEdit->text().trimmed();
}

QString NameColorDialog::colorValue() const
{
    return m_color;
}

void NameColorDialog::accept()
{
    if (nameValue().isEmpty()) {
        m_nameEdit->setFocus();
        return;
    }
    QDialog::accept();
}

void NameColorDialog::selectSwatch(QToolButton *button)
{
    m_color = button->property("color").toString();
    for (QToolButton *swatch : m_swatchButtons) {
        swatch->setChecked(swatch == button);
    }
}
