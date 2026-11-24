#include "browser/FocusController.h"

#include "core/Settings.h"
#include "web/PagePolicy.h"

#include <QTimer>

FocusController::FocusController(Settings *settings, PagePolicy *policy, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_policy(policy)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &FocusController::onTick);
}

qint64 FocusController::remainingMs() const
{
    return m_active ? qMax<qint64>(0, QDateTime::currentDateTime().msecsTo(m_deadline)) : 0;
}

void FocusController::start(const FocusProfile &profile, int minutes)
{
    m_profile = profile;
    m_profile.blockedHosts = Focus::parseHosts(profile.blockedHosts.join(QLatin1Char('\n')));
    m_deadline = QDateTime::currentDateTime().addSecs(qint64(qBound(1, minutes, 480)) * 60);
    m_policy->focusBlocked = m_profile.blockedHosts;
    m_policy->focusActive = true;
    m_active = true;
    m_timer->start();
    emit started();
    emit tick(remainingMs());
}

void FocusController::stop()
{
    end(false);
}

void FocusController::end(bool completed)
{
    if (!m_active) {
        return;
    }
    m_active = false;
    m_timer->stop();
    m_policy->focusActive = false;
    m_policy->focusBlocked.clear();
    emit stopped(completed);
}

void FocusController::onTick()
{
    if (!m_active) {
        return;
    }
    if (remainingMs() <= 0) {
        end(true);
        return;
    }
    emit tick(remainingMs());
}

void FocusController::rememberProfile(const QString &id)
{
    m_settings->setSection(QStringLiteral("focus"),
                           Focus::withLastProfileId(m_settings->section(QStringLiteral("focus")), id));
    m_settings->save();
}

QJsonObject FocusController::section() const
{
    return m_settings->section(QStringLiteral("focus"));
}

void FocusController::saveCustomProfile(const FocusProfile &profile)
{
    m_settings->setSection(QStringLiteral("focus"),
                           Focus::withCustomProfile(m_settings->section(QStringLiteral("focus")), profile));
    m_settings->save();
}
