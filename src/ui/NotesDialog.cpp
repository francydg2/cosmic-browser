#include "ui/NotesDialog.h"

#include <QCloseEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

NotesDialog::NotesDialog(Notes *notes, PageNote pageNote, QWidget *parent)
    : QDialog(parent)
    , m_notes(notes)
    , m_pageNote(std::move(pageNote))
    , m_list(new QListWidget(this))
    , m_search(new QLineEdit(this))
    , m_title(new QLineEdit(this))
    , m_category(new QLineEdit(this))
    , m_content(new QPlainTextEdit(this))
    , m_page(new QLabel(this))
    , m_dates(new QLabel(this))
    , m_status(new QLabel(this))
    , m_delete(new QPushButton(tr("Delete"), this))
    , m_saveTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("notesDialog"));
    setWindowTitle(tr("Quick Notes"));
    resize(840, 560);
    auto *layout = new QVBoxLayout(this);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(tr("New note"), this);
    auto *fromPage = new QPushButton(tr("Note for current page"), this);
    auto *fromSelection = new QPushButton(tr("Save selected text"), this);
    add->setObjectName(QStringLiteral("newNoteButton"));
    fromPage->setObjectName(QStringLiteral("pageNoteButton"));
    fromSelection->setObjectName(QStringLiteral("selectionNoteButton"));
    m_delete->setObjectName(QStringLiteral("deleteNoteButton"));
    buttons->addWidget(add);
    buttons->addWidget(fromPage);
    buttons->addWidget(fromSelection);
    buttons->addStretch();
    buttons->addWidget(m_delete);
    layout->addLayout(buttons);

    auto *splitter = new QSplitter(this);
    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    m_search->setObjectName(QStringLiteral("notesSearch"));
    m_search->setPlaceholderText(tr("Search notes"));
    m_list->setObjectName(QStringLiteral("notesList"));
    leftLayout->addWidget(m_search);
    leftLayout->addWidget(m_list);
    auto *right = new QWidget(splitter);
    auto *editor = new QVBoxLayout(right);
    editor->setContentsMargins(0, 0, 0, 0);
    auto *form = new QFormLayout;
    m_title->setObjectName(QStringLiteral("noteTitle"));
    m_category->setObjectName(QStringLiteral("noteCategory"));
    m_content->setObjectName(QStringLiteral("noteContent"));
    m_page->setTextFormat(Qt::PlainText);
    form->addRow(tr("Title"), m_title);
    form->addRow(tr("Category"), m_category);
    form->addRow(tr("Page"), m_page);
    editor->addLayout(form);
    editor->addWidget(m_content, 1);
    editor->addWidget(m_dates);
    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({250, 560});
    layout->addWidget(splitter, 1);
    auto *privacy = new QLabel(tr("Notes are stored locally without encryption. "
                                  "Page addresses and selected text are saved only when requested."), this);
    privacy->setWordWrap(true);
    layout->addWidget(privacy);
    m_status->setObjectName(QStringLiteral("notesStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(500);

    connect(m_notes, &Notes::changed, this, &NotesDialog::refreshList);
    connect(m_search, &QLineEdit::textChanged, this, &NotesDialog::refreshList);
    connect(m_title, &QLineEdit::textEdited, this, &NotesDialog::updateDraft);
    connect(m_category, &QLineEdit::textEdited, this, &NotesDialog::updateDraft);
    connect(m_content, &QPlainTextEdit::textChanged, this, &NotesDialog::updateDraft);
    connect(m_saveTimer, &QTimer::timeout, this, [this] { saveDraft(); });
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        const QString next = item ? item->data(Qt::UserRole).toString() : QString();
        if (next == m_currentId) {
            return;
        }
        if (!saveDraft()) {
            refreshList();
            return;
        }
        showNote(next);
        refreshList();
    });
    connect(add, &QPushButton::clicked, this, [this] { createNote(false, false); });
    connect(fromPage, &QPushButton::clicked, this, [this] { createNote(true, false); });
    connect(fromSelection, &QPushButton::clicked, this, [this] { createNote(true, true); });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        if (m_currentId.isEmpty() || QMessageBox::question(
                this, tr("Delete note"), tr("Delete this note permanently?"))
                != QMessageBox::Yes) {
            return;
        }
        QString error;
        if (!m_notes->remove(m_currentId, &error)) {
            m_status->setText(error);
            return;
        }
        m_dirty = false;
        m_saveTimer->stop();
        showNote(QString());
        refreshList();
    });
    showNote(QString());
    if (!m_notes->isAvailable()) {
        m_status->setText(tr("Notes could not be loaded. The file was not overwritten."));
        add->setEnabled(false);
        fromPage->setEnabled(false);
        fromSelection->setEnabled(false);
    }
    refreshList();
}

NotesDialog::~NotesDialog()
{
    saveDraft();
}

void NotesDialog::reject()
{
    if (saveDraft()) {
        QDialog::reject();
    }
}

void NotesDialog::updateDraft()
{
    if (m_currentId.isEmpty()) {
        return;
    }
    m_dirty = true;
    m_status->setText(tr("Saving…"));
    m_saveTimer->start();
}

bool NotesDialog::saveDraft()
{
    if (!m_dirty) {
        return true;
    }
    m_saveTimer->stop();
    Note note = m_notes->find(m_currentId);
    note.title = m_title->text();
    note.category = m_category->text();
    note.content = m_content->toPlainText();
    QString error;
    if (!m_notes->update(note, &error)) {
        m_status->setText(tr("Not saved: %1").arg(error));
        return false;
    }
    m_dirty = false;
    m_status->setText(tr("Saved"));
    const Note saved = m_notes->find(m_currentId);
    m_dates->setText(tr("Created: %1 · Modified: %2")
                         .arg(QLocale().toString(saved.created.toLocalTime(), QLocale::ShortFormat),
                              QLocale().toString(saved.modified.toLocalTime(), QLocale::ShortFormat)));
    return true;
}

void NotesDialog::refreshList()
{
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    const QString query = m_search->text().trimmed();
    for (const Note &note : m_notes->items()) {
        if (!query.isEmpty() && !note.title.contains(query, Qt::CaseInsensitive)
            && !note.content.contains(query, Qt::CaseInsensitive)
            && !note.category.contains(query, Qt::CaseInsensitive)
            && !note.pageUrl.toString().contains(query, Qt::CaseInsensitive)) {
            continue;
        }
        auto *item = new QListWidgetItem(
            note.title.trimmed().isEmpty() ? tr("Untitled") : note.title, m_list);
        item->setData(Qt::UserRole, note.id);
        if (note.id == m_currentId) {
            m_list->setCurrentItem(item);
        }
    }
}

void NotesDialog::showNote(const QString &id)
{
    m_currentId = id;
    const Note note = m_notes->find(id);
    const bool enabled = !note.id.isEmpty();
    const QSignalBlocker blockContent(m_content);
    m_title->setText(note.title);
    m_category->setText(note.category);
    m_content->setPlainText(note.content);
    m_page->setText(note.pageUrl.isEmpty() ? tr("None") : note.pageUrl.toDisplayString());
    m_page->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_dates->setText(enabled ? tr("Created: %1 · Modified: %2")
                                  .arg(QLocale().toString(note.created.toLocalTime(), QLocale::ShortFormat),
                                       QLocale().toString(note.modified.toLocalTime(), QLocale::ShortFormat))
                             : QString());
    m_title->setEnabled(enabled);
    m_category->setEnabled(enabled);
    m_content->setEnabled(enabled);
    m_delete->setEnabled(enabled);
}

void NotesDialog::createNote(bool page, bool selection)
{
    if (!saveDraft()) {
        return;
    }
    Note note;
    if (page) {
        note = m_pageNote(selection);
        if (note.pageUrl.isEmpty() || (selection && note.content.isEmpty())) {
            m_status->setText(selection ? tr("Select text on a web page first.")
                                        : tr("Open a web page first."));
            return;
        }
    }
    QString id;
    QString error;
    if (!m_notes->create(note, &id, &error)) {
        m_status->setText(tr("Not saved: %1").arg(error));
        return;
    }
    m_search->clear();
    showNote(id);
    refreshList();
    m_status->setText(tr("Saved"));
    m_title->setFocus();
}

void NotesDialog::closeEvent(QCloseEvent *event)
{
    if (!saveDraft()) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}
