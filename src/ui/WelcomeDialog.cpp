#include "ui/WelcomeDialog.h"

#include "browser/Browser.h"
#include "core/Bookmarks.h"
#include "core/SearchEngines.h"
#include "core/Settings.h"
#include "core/UserCatalog.h"
#include "ui/BrowserWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QProcess>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

QLabel *makeLabel(const QString &text, const QString &objectName, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

} // namespace

WelcomeDialog::WelcomeDialog(Browser *browser, BrowserWindow *window, QWidget *parent)
    : QDialog(parent)
    , m_browser(browser)
    , m_window(window)
{
    setObjectName(QStringLiteral("welcomeDialog"));
    setWindowTitle(tr("Welcome to Cosmic"));
    setModal(true);
    setMinimumSize(760, 610);
    resize(860, 680);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(30, 26, 30, 22);
    root->setSpacing(18);

    auto *header = new QHBoxLayout;
    auto *brand = makeLabel(tr("COSMIC  /  FIRST RUN"), QStringLiteral("welcomeBrand"), this);
    header->addWidget(brand);
    header->addStretch(1);
    m_stepLabel = makeLabel(QString(), QStringLiteral("welcomeStep"), this);
    m_stepLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header->addWidget(m_stepLabel);
    root->addLayout(header);

    auto *progress = new QProgressBar(this);
    progress->setObjectName(QStringLiteral("welcomeProgress"));
    progress->setRange(0, 4);
    progress->setValue(0);
    progress->setTextVisible(false);
    root->addWidget(progress);

    m_pages = new QStackedWidget(this);
    m_pages->setObjectName(QStringLiteral("welcomePages"));
    root->addWidget(m_pages, 1);

    auto *welcomePage = makePage(
        tr("A calmer corner of the web."),
        tr("Cosmic is ready to make browsing feel like yours. Take a minute to set up "
           "your privacy, speed, search and personal space."));
    auto *featureRow = new QHBoxLayout;
    featureRow->setSpacing(12);
    featureRow->addWidget(makeFeatureCard(
        tr("Private by default"),
        tr("Known tracker requests are blocked. Site permissions stay in your hands.")));
    featureRow->addWidget(makeFeatureCard(
        tr("Fast when you need it"),
        tr("Optional RAM Saver unloads background tabs until you return to them.")));
    featureRow->addWidget(makeFeatureCard(
        tr("Yours, not a cloud"),
        tr("Profiles and bookmarks stay on this device. No account or sync required.")));
    qobject_cast<QVBoxLayout *>(welcomePage->layout())->addLayout(featureRow);
    qobject_cast<QVBoxLayout *>(welcomePage->layout())->addStretch(1);
    m_pages->addWidget(welcomePage);

    auto *importPage = makePage(
        tr("Bring your favorite places."),
        tr("Choose an HTML bookmarks export from another browser. Cosmic imports web links "
           "only; passwords and browsing history are not read."));
    auto *importButton = new QPushButton(tr("Choose bookmarks HTML…"), importPage);
    importButton->setObjectName(QStringLiteral("welcomeImportButton"));
    connect(importButton, &QPushButton::clicked, this, &WelcomeDialog::chooseBookmarksFile);
    m_bookmarkFileLabel = makeLabel(
        tr("Nothing selected — you can skip this and import later."),
        QStringLiteral("welcomeImportStatus"), importPage);
    auto *importCard = new QWidget(importPage);
    importCard->setObjectName(QStringLiteral("welcomeCard"));
    auto *importLayout = new QVBoxLayout(importCard);
    importLayout->setContentsMargins(20, 20, 20, 20);
    importLayout->setSpacing(12);
    importLayout->addWidget(importButton, 0, Qt::AlignLeft);
    importLayout->addWidget(m_bookmarkFileLabel);
    qobject_cast<QVBoxLayout *>(importPage->layout())->addWidget(importCard);
    qobject_cast<QVBoxLayout *>(importPage->layout())->addStretch(1);
    m_pages->addWidget(importPage);

    auto *privacyPage = makePage(
        tr("Choose your comfort level."),
        tr("These are local browser preferences. You can change any of them later in Settings."));
    auto *privacyCard = new QWidget(privacyPage);
    privacyCard->setObjectName(QStringLiteral("welcomeCard"));
    auto *privacyLayout = new QVBoxLayout(privacyCard);
    privacyLayout->setContentsMargins(20, 18, 20, 18);
    privacyLayout->setSpacing(14);

    m_blockingCheck = new QCheckBox(tr("Block known ad and tracker domains"), privacyCard);
    m_blockingCheck->setChecked(m_browser->settings()->requestBlockingEnabled());
    m_dntCheck = new QCheckBox(tr("Send a Do Not Track signal"), privacyCard);
    m_dntCheck->setChecked(m_browser->settings()->doNotTrack());
    m_sessionCookiesCheck =
        new QCheckBox(tr("Delete cookies when Cosmic closes"), privacyCard);
    m_sessionCookiesCheck->setChecked(m_browser->settings()->sessionCookiesOnly());
    m_ramSaverCheck =
        new QCheckBox(tr("Unload background tabs to save memory (RAM Saver)"), privacyCard);
    m_ramSaverCheck->setChecked(m_browser->settings()->ramSaverEnabled());

    privacyLayout->addWidget(m_blockingCheck);
    privacyLayout->addWidget(makeLabel(
        tr("Blocks requests to known domains; a few sites may need it turned off."),
        QStringLiteral("welcomeHint"), privacyCard));
    privacyLayout->addWidget(m_dntCheck);
    privacyLayout->addWidget(m_sessionCookiesCheck);
    privacyLayout->addWidget(m_ramSaverCheck);
    privacyLayout->addWidget(makeLabel(
        tr("RAM Saver can make a discarded tab reload when you open it again."),
        QStringLiteral("welcomeHint"), privacyCard));
    qobject_cast<QVBoxLayout *>(privacyPage->layout())->addWidget(privacyCard);
    qobject_cast<QVBoxLayout *>(privacyPage->layout())->addStretch(1);
    m_pages->addWidget(privacyPage);

    auto *personalizePage = makePage(
        tr("Set your starting point."),
        tr("Pick a search engine and, if you like, create a separate local profile for another "
           "person or purpose."));
    auto *personalizeCard = new QWidget(personalizePage);
    personalizeCard->setObjectName(QStringLiteral("welcomeCard"));
    auto *personalizeLayout = new QVBoxLayout(personalizeCard);
    personalizeLayout->setContentsMargins(20, 18, 20, 18);
    personalizeLayout->setSpacing(10);

    auto *searchTitle = makeLabel(tr("Search engine"), QStringLiteral("welcomeSectionTitle"),
                                  personalizeCard);
    personalizeLayout->addWidget(searchTitle);
    m_engineCombo = new QComboBox(personalizeCard);
    m_engineCombo->setObjectName(QStringLiteral("welcomeSearchEngine"));
    m_engineCombo->setMinimumHeight(42);
    for (const SearchEngine &engine : SearchEngines::builtins()) {
        m_engineCombo->addItem(engine.name, engine.id);
    }
    int engineIndex = m_engineCombo->findData(m_browser->settings()->defaultSearchEngine());
    m_engineCombo->setCurrentIndex(engineIndex >= 0 ? engineIndex : 0);
    personalizeLayout->addWidget(m_engineCombo);

    auto *profileTitle = makeLabel(tr("Add a local profile (optional)"),
                                   QStringLiteral("welcomeSectionTitle"), personalizeCard);
    personalizeLayout->addSpacing(8);
    personalizeLayout->addWidget(profileTitle);
    m_profileNameEdit = new QLineEdit(personalizeCard);
    m_profileNameEdit->setObjectName(QStringLiteral("welcomeProfileName"));
    m_profileNameEdit->setPlaceholderText(tr("e.g. Work or Alex"));
    m_profileNameEdit->setClearButtonEnabled(true);
    m_profileNameEdit->setMinimumHeight(42);
    personalizeLayout->addWidget(m_profileNameEdit);
    personalizeLayout->addWidget(makeLabel(
        tr("Profiles separate cookies, logins and tabs on this device. They do not create an "
           "online account or sync data."),
        QStringLiteral("welcomeHint"), personalizeCard));

    m_defaultBrowserCheck =
        new QCheckBox(tr("Make Cosmic my default web browser"), personalizeCard);
    m_defaultBrowserCheck->setObjectName(QStringLiteral("welcomeDefaultBrowser"));
#ifdef Q_OS_LINUX
    personalizeLayout->addSpacing(6);
    personalizeLayout->addWidget(m_defaultBrowserCheck);
    personalizeLayout->addWidget(makeLabel(
        tr("Cosmic will ask your desktop environment to use it for web links. "
           "This applies only if Cosmic is installed in your applications menu."),
        QStringLiteral("welcomeHint"), personalizeCard));
#endif
    qobject_cast<QVBoxLayout *>(personalizePage->layout())->addWidget(personalizeCard);
    qobject_cast<QVBoxLayout *>(personalizePage->layout())->addStretch(1);
    m_pages->addWidget(personalizePage);

    auto *finishPage = makePage(
        tr("Your browser. Your rules."),
        tr("You can revisit every choice in Settings. Cosmic keeps this setup on your device."));
    auto *finishCard = new QWidget(finishPage);
    finishCard->setObjectName(QStringLiteral("welcomeFinishCard"));
    auto *finishLayout = new QVBoxLayout(finishCard);
    finishLayout->setContentsMargins(24, 24, 24, 24);
    finishLayout->setSpacing(12);
    finishLayout->addWidget(makeLabel(
        tr("A small tip: pin your most-used tabs, drag the vertical tab edge to resize it, "
           "and use Ctrl+L whenever you want to jump straight to search."),
        QStringLiteral("welcomeFinishTitle"), finishCard));
    finishLayout->addWidget(makeLabel(
        tr("Nothing is sent to an AI or cloud service by this setup. Imported bookmarks and "
           "your preferences remain in your Cosmic profile."),
        QStringLiteral("welcomeHint"), finishCard));
    qobject_cast<QVBoxLayout *>(finishPage->layout())->addWidget(finishCard);
    qobject_cast<QVBoxLayout *>(finishPage->layout())->addStretch(1);
    m_pages->addWidget(finishPage);

    m_errorLabel = makeLabel(QString(), QStringLiteral("welcomeError"), this);
    m_errorLabel->setVisible(false);
    root->addWidget(m_errorLabel);

    auto *footer = new QHBoxLayout;
    auto *skipButton = new QPushButton(tr("Skip setup"), this);
    skipButton->setObjectName(QStringLiteral("welcomeSkipButton"));
    connect(skipButton, &QPushButton::clicked, this, &QDialog::reject);
    m_backButton = new QPushButton(tr("Back"), this);
    m_backButton->setObjectName(QStringLiteral("welcomeBackButton"));
    connect(m_backButton, &QPushButton::clicked, this, [this] {
        setPage(m_pages->currentIndex() - 1);
    });
    m_nextButton = new QPushButton(tr("Continue"), this);
    m_nextButton->setObjectName(QStringLiteral("welcomeNextButton"));
    m_nextButton->setDefault(true);
    connect(m_nextButton, &QPushButton::clicked, this, [this] {
        if (m_pages->currentIndex() == m_pages->count() - 1) {
            if (finishSetup()) {
                accept();
            }
            return;
        }
        setPage(m_pages->currentIndex() + 1);
    });
    footer->addWidget(skipButton);
    footer->addStretch(1);
    footer->addWidget(m_backButton);
    footer->addWidget(m_nextButton);
    root->addLayout(footer);

    setPage(0);
}

QWidget *WelcomeDialog::makePage(const QString &title, const QString &description)
{
    auto *page = new QWidget(m_pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(title, QStringLiteral("welcomeTitle"), page));
    layout->addWidget(makeLabel(description, QStringLiteral("welcomeDescription"), page));
    return page;
}

QWidget *WelcomeDialog::makeFeatureCard(const QString &title, const QString &description)
{
    auto *card = new QWidget(m_pages);
    card->setObjectName(QStringLiteral("welcomeFeatureCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);
    layout->addWidget(makeLabel(title, QStringLiteral("welcomeSectionTitle"), card));
    layout->addWidget(makeLabel(description, QStringLiteral("welcomeHint"), card));
    return card;
}

void WelcomeDialog::setPage(int index)
{
    const int page = qBound(0, index, m_pages->count() - 1);
    m_pages->setCurrentIndex(page);
    m_stepLabel->setText(tr("%1 / %2").arg(page + 1).arg(m_pages->count()));
    if (auto *progress = findChild<QProgressBar *>(QStringLiteral("welcomeProgress"))) {
        progress->setValue(page);
    }
    m_backButton->setEnabled(page > 0);
    m_nextButton->setText(page == m_pages->count() - 1 ? tr("Finish setup")
                                                       : tr("Continue"));
    m_errorLabel->clear();
    m_errorLabel->setVisible(false);
}

void WelcomeDialog::chooseBookmarksFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import bookmarks"), QString(),
        tr("Bookmarks HTML (*.html *.htm);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    m_bookmarkFile = path;
    m_bookmarkFileLabel->setText(tr("Ready to import: %1").arg(QFileInfo(path).fileName()));
}

bool WelcomeDialog::finishSetup()
{
    m_errorLabel->clear();
    const QString profileName = m_profileNameEdit->text().trimmed();
    QString createdProfile;
    if (!profileName.isEmpty()) {
        createdProfile = m_browser->userCatalog()->addUser(
            profileName, m_browser->settings()->accentColor());
        if (createdProfile.isEmpty() || !m_browser->userCatalog()->save()) {
            if (!createdProfile.isEmpty()) {
                m_browser->userCatalog()->removeUser(createdProfile);
            }
            m_errorLabel->setText(tr("Could not save the new local profile."));
            m_errorLabel->setVisible(true);
            return false;
        }
    }

    Settings *settings = m_browser->settings();
    settings->setRequestBlockingEnabled(m_blockingCheck->isChecked());
    settings->setDoNotTrack(m_dntCheck->isChecked());
    settings->setSessionCookiesOnly(m_sessionCookiesCheck->isChecked());
    settings->setRamSaverEnabled(m_ramSaverCheck->isChecked());
    settings->setDefaultSearchEngine(m_engineCombo->currentData().toString());
    if (!settings->save()) {
        m_errorLabel->setText(tr("Cosmic could not save your preferences. Check the profile "
                                 "folder permissions and try again."));
        m_errorLabel->setVisible(true);
        return false;
    }

    if (!m_bookmarkFile.isEmpty()) {
        int imported = 0;
        QString error;
        if (!m_browser->bookmarks()->importHtml(m_bookmarkFile, &imported, &error)) {
            m_errorLabel->setText(tr("Bookmark import failed: %1").arg(error));
            m_errorLabel->setVisible(true);
            return false;
        }
        if (imported > 0) {
            m_bookmarkFileLabel->setText(tr("Imported %1 bookmark(s).").arg(imported));
        } else {
            m_bookmarkFileLabel->setText(tr("All valid bookmarks were already here."));
        }
    }

#ifdef Q_OS_LINUX
    if (m_defaultBrowserCheck->isChecked()) {
        const int result = QProcess::execute(
            QStringLiteral("xdg-settings"),
            {QStringLiteral("set"), QStringLiteral("default-web-browser"),
             QStringLiteral("cosmic.desktop")});
        if (result != 0) {
            QMessageBox::information(
                this, tr("One last system step"),
                tr("Your desktop could not set Cosmic as the default browser. "
                   "Cosmic may need to be installed in the applications menu first."));
        }
    }
#endif

    if (!createdProfile.isEmpty() && m_window) {
        m_window->switchToUser(createdProfile);
    }
    return true;
}
