#include "ui/FocusDialog.h"

#include "browser/Browser.h"
#include "browser/FocusController.h"
#include "ui/BrowserWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

FocusDialog::FocusDialog(BrowserWindow *window)
    : QDialog(window)
    , m_window(window)
    , m_profile(new QComboBox(this))
    , m_minutes(new QSpinBox(this))
    , m_hosts(new QPlainTextEdit(this))
    , m_hideChrome(new QCheckBox(tr("Hide the tab bar, groups, status bar and side panels"), this))
    , m_workspace(new QComboBox(this))
    , m_status(new QLabel(this))
    , m_start(new QPushButton(tr("Start Focus"), this))
    , m_end(new QPushButton(tr("End Focus"), this))
{
    setObjectName(QStringLiteral("focusDialog"));
    setWindowTitle(tr("Cosmic Focus"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(480, 0);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("Focus limits the sites below and hides extra interface. Nothing is closed or deleted, "
           "and you can end it at any time."), this));
    layout->itemAt(0)->widget()->setProperty("wordWrap", true);
    static_cast<QLabel *>(layout->itemAt(0)->widget())->setWordWrap(true);

    auto *form = new QFormLayout;
    m_profile->setObjectName(QStringLiteral("focusProfile"));
    m_minutes->setObjectName(QStringLiteral("focusMinutes"));
    m_hosts->setObjectName(QStringLiteral("focusHosts"));
    m_hideChrome->setObjectName(QStringLiteral("focusHideChrome"));
    m_workspace->setObjectName(QStringLiteral("focusWorkspace"));
    m_status->setObjectName(QStringLiteral("focusStatus"));
    m_start->setObjectName(QStringLiteral("focusStart"));
    m_end->setObjectName(QStringLiteral("focusEnd"));
    for (const FocusProfile &profile : Focus::builtInProfiles()) {
        m_profile->addItem(tr(profile.name.toUtf8().constData()), profile.id);
    }
    m_profile->addItem(tr("Custom"), QStringLiteral("custom"));
    m_minutes->setRange(1, 480);
    m_minutes->setSuffix(tr(" min"));
    m_hosts->setPlaceholderText(tr("One site per line, e.g. youtube.com"));
    m_hosts->setMaximumHeight(130);
    m_workspace->addItem(tr("None"), QString());
    for (const QString &name : m_window->workspaceNames()) {
        m_workspace->addItem(name, name);
    }
    form->addRow(tr("Profile"), m_profile);
    form->addRow(tr("Duration"), m_minutes);
    form->addRow(tr("Limited sites"), m_hosts);
    form->addRow(QString(), m_hideChrome);
    form->addRow(tr("Open workspace"), m_workspace);
    layout->addLayout(form);
    layout->addWidget(m_status);

    auto *buttons = new QHBoxLayout;
    auto *save = new QPushButton(tr("Save as my custom profile"), this);
    save->setObjectName(QStringLiteral("focusSaveCustom"));
    buttons->addWidget(save);
    buttons->addStretch();
    buttons->addWidget(m_end);
    buttons->addWidget(m_start);
    layout->addLayout(buttons);

    FocusController *focus = m_window->m_browser->focus();
    const QString last = Focus::lastProfileId(focus->section());
    m_profile->setCurrentIndex(qMax(0, m_profile->findData(last)));
    loadProfile(m_profile->currentData().toString());

    const auto updateState = [this, focus] {
        const bool active = focus->isActive();
        m_start->setText(active ? tr("Restart Focus") : tr("Start Focus"));
        m_end->setEnabled(active);
        m_status->setText(active ? tr("Focus is on: %1 left.").arg(Focus::formatRemaining(focus->remainingMs()))
                                 : QString());
    };
    updateState();
    connect(focus, &FocusController::tick, this, updateState);
    connect(focus, &FocusController::started, this, updateState);
    connect(focus, &FocusController::stopped, this, updateState);
    connect(m_profile, &QComboBox::activated, this,
            [this] { loadProfile(m_profile->currentData().toString()); });
    connect(m_start, &QPushButton::clicked, this, [this, focus] {
        const FocusProfile values = currentValues();
        if (values.blockedHosts.isEmpty()
            && m_hosts->toPlainText().trimmed().isEmpty()) {
            m_status->setText(tr("No sites are limited; Focus will only hide interface elements."));
        }
        focus->rememberProfile(values.id);
        focus->start(values, values.minutes);
        m_window->applyFocusWorkspace(values.workspace);
        accept();
    });
    connect(m_end, &QPushButton::clicked, this, [focus] { focus->stop(); });
    connect(save, &QPushButton::clicked, this, [this, focus] {
        FocusProfile values = currentValues();
        values.id = QStringLiteral("custom");
        focus->saveCustomProfile(values);
        m_profile->setCurrentIndex(m_profile->findData(QStringLiteral("custom")));
        m_status->setText(tr("Saved as your custom profile."));
    });
}

void FocusDialog::loadProfile(const QString &id)
{
    FocusProfile profile;
    if (id == QLatin1String("custom")) {
        profile = Focus::customProfile(m_window->m_browser->focus()->section());
    } else {
        for (const FocusProfile &candidate : Focus::builtInProfiles()) {
            if (candidate.id == id) {
                profile = candidate;
            }
        }
    }
    m_minutes->setValue(profile.minutes > 0 ? profile.minutes : 25);
    m_hosts->setPlainText(profile.blockedHosts.join(QLatin1Char('\n')));
    m_hideChrome->setChecked(profile.hideChrome);
    m_workspace->setCurrentIndex(qMax(0, m_workspace->findData(profile.workspace)));
}

FocusProfile FocusDialog::currentValues() const
{
    FocusProfile profile;
    profile.id = m_profile->currentData().toString();
    profile.name = m_profile->currentText();
    profile.minutes = m_minutes->value();
    profile.blockedHosts = Focus::parseHosts(m_hosts->toPlainText());
    profile.hideChrome = m_hideChrome->isChecked();
    profile.workspace = m_workspace->currentData().toString();
    return profile;
}
