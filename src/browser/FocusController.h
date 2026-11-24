#pragma once

#include "core/Focus.h"

#include <QDateTime>
#include <QObject>

class QTimer;
class Settings;
struct PagePolicy;

/// The single Cosmic Focus session of the application. While active it
/// asks every page, through the shared PagePolicy, to refuse main-frame
/// navigation to the profile's listed sites. Nothing is closed or
/// deleted; ending the session (button, timer or stop()) lifts the rule
/// at once. A session does not survive restarting Cosmic.
class FocusController : public QObject
{
    Q_OBJECT

public:
    FocusController(Settings *settings, PagePolicy *policy, QObject *parent = nullptr);

    bool isActive() const { return m_active; }
    FocusProfile profile() const { return m_profile; }
    qint64 remainingMs() const;

    /// Starts (or replaces) a session of \p minutes (1–480).
    void start(const FocusProfile &profile, int minutes);
    void stop();

    /// Remembers the profile choice for the next time.
    void rememberProfile(const QString &id);
    QJsonObject section() const;
    void saveCustomProfile(const FocusProfile &profile);

signals:
    void started();
    void stopped(bool completed);
    void tick(qint64 remainingMs);

private:
    void onTick();
    void end(bool completed);

    Settings *m_settings;
    PagePolicy *m_policy;
    QTimer *m_timer;
    FocusProfile m_profile;
    QDateTime m_deadline;
    bool m_active = false;
};
