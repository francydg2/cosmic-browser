#pragma once

#include <QString>

class QApplication;

/// Phase 1 dark look and feel: Fusion base style + bundled QSS
/// (resources/styles/cosmic-dark.qss) + a dark fallback palette.
///
/// The QSS carries @ACCENT@/@ACCENT_HI@/@ACCENT_LO@ tokens; applyAccent()
/// re-renders it with a user-chosen accent color (Settings → Appearance).
namespace Style {

void applyDark(QApplication *app);

void applyAccent(QApplication *app, const QString &hex);

QString currentAccent();

QString urlBarStyleSheet(const QString &background, int radius,
                         const QString &accent);

} // namespace Style
