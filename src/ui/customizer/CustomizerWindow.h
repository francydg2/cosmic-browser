#pragma once

#include <QDialog>
#include <QString>

class Browser;
class LayoutEditor;
class ComponentLibrary;
class ComponentProperties;
class CustomizerPreview;
class QLabel;
class QPushButton;
class QSplitter;
class QToolButton;

class CustomizerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit CustomizerWindow(Browser *browser, QWidget *parent = nullptr);

    LayoutEditor *editor() const { return m_editor; }
    CustomizerPreview *preview() const { return m_preview; }
    ComponentLibrary *library() const { return m_library; }

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void onSelectionChanged();
    void onLibraryAdd(const QString &component);
    void save();
    void discard();
    void updateDirty();

    Browser *m_browser;
    LayoutEditor *m_editor;
    ComponentLibrary *m_library = nullptr;
    CustomizerPreview *m_preview = nullptr;
    ComponentProperties *m_properties = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_dirtyLabel = nullptr;
    QToolButton *m_editModeButton = nullptr;
    QPushButton *m_saveButton = nullptr;
};
