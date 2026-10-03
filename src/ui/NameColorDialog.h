#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QLineEdit;
class QToolButton;

/// Small modal dialog: a name plus one of the preset group/user
/// colors. Used when creating a tab group and when adding a
/// user. Returns the chosen values via accessors; ok() via
/// QDialog::exec()/done() (name must be non-empty to accept).
class NameColorDialog : public QDialog
{
    Q_OBJECT

public:
    /// The 6 preset colors (also styled in cosmic-dark.qss).
    static const QList<QString> &presetColors();

    explicit NameColorDialog(const QString &title, const QString &initialName = QString(),
                             const QString &initialColor = QString(),
                             QWidget *parent = nullptr);

    QString nameValue() const;
    QString colorValue() const;

    void accept() override;

private:
    void selectSwatch(QToolButton *button);

    QLineEdit *m_nameEdit = nullptr;
    QList<QToolButton *> m_swatchButtons;
    QString m_color;
};
