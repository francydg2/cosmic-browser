#pragma once

#include <QString>
#include <QWidget>

class LayoutEditor;
class QLabel;
class QFormLayout;
class QVBoxLayout;

class ComponentProperties : public QWidget
{
    Q_OBJECT

public:
    explicit ComponentProperties(QWidget *parent = nullptr);

    void setEditor(LayoutEditor *editor);

    void showNothing();
    void showToolbar();
    void showComponent(const QString &id, const QString &zone, int index);
    void showTabs();
    void refresh();

private:
    void clearEditors();

private:
    QWidget *colorRow(const QString &current, const char *buttonName,
                      void (ComponentProperties::*apply)(const QString &));
    void applyToolbarColor(const QString &color);
    void applyUrlColor(const QString &color);

    LayoutEditor *m_editor = nullptr;
    QString m_kind;
    QString m_component;
    QString m_zone;
    int m_index = -1;

    QLabel *m_iconLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_descriptionLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_locationLabel = nullptr;
    QWidget *m_editArea = nullptr;
    QFormLayout *m_editForm = nullptr;
};
