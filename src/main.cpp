#include "browser/Browser.h"
#include "browser/SingleInstance.h"
#include "core/Logging.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include "ui/BrowserWindow.h"
#include "ui/Style.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // XDG paths (spec §55): with no organization name, Qt resolves
    // ~/.config/cosmic, ~/.local/share/cosmic, ~/.cache/cosmic.
    QCoreApplication::setOrganizationName(QString());
    QCoreApplication::setApplicationName(QStringLiteral("cosmic"));
    QGuiApplication::setDesktopFileName(QStringLiteral("cosmic"));

    QIcon windowIcon = QIcon::fromTheme(QStringLiteral("cosmic"));
    if (windowIcon.isNull()) {
        windowIcon = QIcon(QStringLiteral(":/icons/saturn.svg"));
    }
    app.setWindowIcon(windowIcon);
#ifdef COSMIC_VERSION
    QCoreApplication::setApplicationVersion(QStringLiteral(COSMIC_VERSION));
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Cosmic — a privacy-focused, extremely customizable browser"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption debugOption(
        QStringList{QStringLiteral("d"), QStringLiteral("debug")},
        QStringLiteral("Enable debug logging (cosmic.debug)"));
    parser.addOption(debugOption);
    const QCommandLineOption quitAfterOption(
        QStringLiteral("quit-after"),
        QStringLiteral("Self-check: quit automatically after <ms> (automation aid)"),
        QStringLiteral("ms"));
    parser.addOption(quitAfterOption);
    const QCommandLineOption restoreOption(
        QStringLiteral("restore"),
        QStringLiteral("Restore the previous session even if disabled in Settings"));
    parser.addOption(restoreOption);
    const QCommandLineOption noRestoreOption(
        QStringLiteral("no-restore"),
        QStringLiteral("Do not restore the previous session"));
    parser.addOption(noRestoreOption);
    parser.addPositionalArgument(QStringLiteral("url"),
                                 QStringLiteral("URL to open (treated like the address bar)"));
    parser.process(app);

    Logging::init(parser.isSet(debugOption));
    qCInfo(lcCosmic).noquote() << "Cosmic" << QCoreApplication::applicationVersion() << "starting";

    if (!Paths::ensureDirs()) {
        qCWarning(lcCosmic) << "some XDG directories could not be created";
    }
    Paths::migrateLegacyProfile();

    Style::applyDark(&app);

    // Composition root: browser/ never sees ui/ — the factory wires them.
    // Windows are tracked here so they can be destroyed (see below) while
    // QApplication is still alive.
    Browser browser;
    Style::applyAccent(&app, browser.settings()->accentColor());
    QObject::connect(browser.settings(), &Settings::accentColorChanged, &app,
                     [&app](const QString &hex) { Style::applyAccent(&app, hex); });
    QList<BrowserWindow *> windows;
    browser.setWindowFactory([&browser, &windows] {
        auto *window = new BrowserWindow(&browser);
        windows.append(window);
        return static_cast<AppWindow *>(window);
    });

    AppWindow *window = browser.newWindow();
    if (!window) {
        qCCritical(lcCosmic) << "failed to create the first window";
        return 1;
    }

    const QStringList arguments = parser.positionalArguments();
    const bool hasCliUrl = !arguments.isEmpty();
    const QString cliPayload = hasCliUrl ? arguments.first() : QString();

    // --- single window ----------------------------------------------------
    // Exactly one window exists at a time: a second launch forwards its
    // URL (if any) to the running instance and exits. Set the
    // COSMIC_ALLOW_MULTIPLE environment variable (any value) to disable
    // the gate for automation and debugging.
    SingleInstance single;
    bool primary = true;
    if (qEnvironmentVariableIsEmpty("COSMIC_ALLOW_MULTIPLE")) {
        primary = single.acquire();
        if (!primary && !SingleInstance::forward(cliPayload)) {
            primary = single.acquire();
        }
    }
    if (!primary) {
        return 0;
    }

    // --- session restore --------------------------------------------------
    // Precedence: explicit CLI URL beats session; --no-restore beats the
    // setting; --restore beats the setting. Single window: every stored
    // window of the current user merges into it (tabs concatenated,
    // group ids are unique); other users keep their stored states.
    const bool wantRestore = !hasCliUrl && !parser.isSet(noRestoreOption)
        && (parser.isSet(restoreOption) || browser.settings()->restoreSessionOnStartup());
    if (wantRestore) {
        QMap<QString, QList<SessionWindow>> byUser;
        if (browser.loadUserSessions()) {
            byUser = browser.userSessions();
        }
        const QString me = browser.currentUserId();
        SessionWindow merged;
        merged.user = me;
        int tabCount = 0;
        for (const SessionWindow &state : byUser.value(me)) {
            merged.tabs += state.tabs;
            merged.groups += state.groups;
            tabCount += state.tabs.size();
        }
        qCInfo(lcCosmic).noquote() << "session restore requested:" << tabCount
                                    << "tab(s) for user" << me;
        if (auto *firstWindow = dynamic_cast<BrowserWindow *>(window)) {
            firstWindow->setUserId(me);
        }
        if (!merged.tabs.isEmpty() || !merged.groups.isEmpty()) {
            window->restoreSessionState(merged);
        }
    }

    QObject::connect(&single, &SingleInstance::messageReceived,
                     [&browser, window](const QString &message) {
                         if (!message.isEmpty()) {
                             window->openUrl(browser.resolveInput(message));
                         }
                         window->present();
                     });

    if (hasCliUrl) {
        window->openUrl(browser.resolveInput(arguments.first()));
    }
    window->present();

    // Self-check aid (used by automated verification of the real desktop
    // session): quit cleanly after N ms so the exit path runs for real.
    if (parser.isSet(quitAfterOption)) {
        const int ms = parser.value(quitAfterOption).toInt();
        QTimer::singleShot(qMax(0, ms), &app, &QCoreApplication::quit);
    }

    const int result = app.exec();

    // --- session save: always write the latest state per user so that
    // enabling "restore" later still has sessions to restore. -----------
    {
        QList<AppWindow *> appWindows;
        appWindows.reserve(windows.size());
        for (BrowserWindow *w : windows) {
            appWindows.append(w);
        }
        browser.saveUserSessions(appWindows);
    }

    // Destroy windows before ~QApplication: QtWebEngine's internal QQuick
    // render items are not safe to tear down from the application
    // destructor (seen as SIGSEGV in libQt6Quick on this machine).
    for (BrowserWindow *w : windows) {
        delete w;
    }
    return result;
}
