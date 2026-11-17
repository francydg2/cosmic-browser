#pragma once

#include "core/Notes.h"

#include <QDialog>
#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class QCloseEvent;

class NotesDialog : public QDialog
{
    Q_OBJECT
public:
    using PageNote = std::function<Note(bool selection)>;
    explicit NotesDialog(Notes *notes, PageNote pageNote, QWidget *parent = nullptr);
    ~NotesDialog() override;
    void reject() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    bool saveDraft();
    void refreshList();
    void showNote(const QString &id);
    void createNote(bool page, bool selection);
    void updateDraft();

    Notes *m_notes;
    PageNote m_pageNote;
    QListWidget *m_list;
    QLineEdit *m_search;
    QLineEdit *m_title;
    QLineEdit *m_category;
    QPlainTextEdit *m_content;
    QLabel *m_page;
    QLabel *m_dates;
    QLabel *m_status;
    QPushButton *m_delete;
    QTimer *m_saveTimer;
    QString m_currentId;
    bool m_dirty = false;
};
