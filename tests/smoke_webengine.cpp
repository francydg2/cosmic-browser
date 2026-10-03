// Smoke test: proves Qt WebEngine can complete a real HTTPS navigation
// on this machine, offscreen (no display required).
//
// Exit codes: 0 = OK, 1 = load failed, 2 = not HTTPS, 3 = timeout.

#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QWebEnginePage>
#include <QWebEngineProfile>

#include <cstdio>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("cosmic-smoke"));

    // Off-the-record profile: no data written to the user's XDG dirs.
    QWebEngineProfile profile; // default ctor = off-the-record

    QWebEnginePage page(&profile);
    QEventLoop loop;
    bool finished = false;
    bool success = false;

    QObject::connect(&page, &QWebEnginePage::loadFinished, &loop,
                     [&](bool ok) {
                         finished = true;
                         success = ok;
                         loop.quit();
                     });
    QTimer::singleShot(45000, &loop, [&] { loop.quit(); });

    const QUrl target(QStringLiteral("https://example.com"));
    page.load(target);
    loop.exec();

    if (!finished) {
        std::fprintf(stderr, "SMOKE FAIL: timeout loading %s\n", qPrintable(target.toString()));
        return 3;
    }
    if (!success) {
        std::fprintf(stderr, "SMOKE FAIL: loadFinished(false) for %s\n",
                     qPrintable(target.toString()));
        return 1;
    }

    const QUrl finalUrl = page.url();
    const QString title = page.title();
    std::printf("SMOKE OK: url=%s title=%s\n", qPrintable(finalUrl.toString()),
                qPrintable(title));

    if (finalUrl.scheme() != QLatin1String("https")) {
        std::fprintf(stderr, "SMOKE FAIL: final scheme is not https (%s)\n",
                     qPrintable(finalUrl.scheme()));
        return 2;
    }
    if (title.isEmpty()) {
        std::fprintf(stderr, "SMOKE FAIL: empty page title\n");
        return 1;
    }
    return 0;
}
