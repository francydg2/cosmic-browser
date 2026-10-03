#include "ui/Style.h"
#include "core/Logging.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>

namespace Style {

namespace {

QString s_accent = QStringLiteral("#7c6cf0");
QString s_template;

QColor effectiveColor(const QString &hex, const QColor &fallback)
{
    const QColor color(hex);
    if (!color.isValid() || color.alpha() != 255) {
        return fallback;
    }
    return color;
}

void resolveAccent(const QColor &base, QColor *hi, QColor *lo)
{
    if (base.name() == QLatin1String("#7c6cf0")) {
        *hi = QColor(QStringLiteral("#8f81f5"));
        *lo = QColor(QStringLiteral("#6a5ae0"));
        return;
    }
    *hi = base.lighter(118);
    *lo = base.darker(118);
}

QPalette darkPalette(const QColor &highlight)
{
    QPalette p;
    const QColor window(0x0f, 0x10, 0x13);
    const QColor base(0x1b, 0x1d, 0x24);
    const QColor alt(0x21, 0x24, 0x2d);
    const QColor text(0xe8, 0xe9, 0xee);
    const QColor disabled(0x6f, 0x75, 0x81);

    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, alt);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, window);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, disabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    p.setColor(QPalette::ToolTipBase, QColor(0x23, 0x26, 0x2f));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Link, highlight);
    return p;
}

void loadTemplate()
{
    if (!s_template.isEmpty()) {
        return;
    }
    QFile file(QStringLiteral(":/styles/cosmic-dark.qss"));
    if (file.open(QIODevice::ReadOnly)) {
        s_template = QString::fromUtf8(file.readAll());
    }
}

void apply(QApplication *app)
{
    const QColor base = effectiveColor(s_accent, QColor(QStringLiteral("#7c6cf0")));
    s_accent = base.name();
    QColor hi;
    QColor lo;
    resolveAccent(base, &hi, &lo);

    app->setPalette(darkPalette(base));

    if (s_template.isEmpty()) {
        qCCritical(lcCosmic) << "stylesheet resource missing; using palette fallback only";
        return;
    }
    QString sheet = s_template;
    sheet.replace(QStringLiteral("@ACCENT@"), base.name());
    sheet.replace(QStringLiteral("@ACCENT_HI@"), hi.name());
    sheet.replace(QStringLiteral("@ACCENT_LO@"), lo.name());
    app->setStyleSheet(sheet);
}

} // namespace

void applyDark(QApplication *app)
{
    app->setStyle(QStyleFactory::create(QStringLiteral("fusion")));
    loadTemplate();
    apply(app);
    qCInfo(lcCosmic) << "dark stylesheet applied; accent" << s_accent;
}

void applyAccent(QApplication *app, const QString &hex)
{
    const QColor base = effectiveColor(hex, QColor(QStringLiteral("#7c6cf0")));
    if (s_accent == base.name() && !s_template.isEmpty()) {
        return;
    }
    s_accent = base.name();
    loadTemplate();
    apply(app);
    qCInfo(lcCosmic) << "accent color applied;" << s_accent;
}

QString currentAccent()
{
    return s_accent;
}

QString urlBarStyleSheet(const QString &background, int radius,
                         const QString &accent)
{
    return QStringLiteral(
               "QLineEdit#urlBar { background: %1; border: 1px solid #2a2e39;"
               " border-radius: %2px; padding: 4px 24px 4px 14px; font-size: 13px;"
               " selection-background-color: %3; selection-color: #ffffff; }"
               " QLineEdit#urlBar:hover { border-color: #363b49; }"
               " QLineEdit#urlBar:focus { border-color: %3; }")
        .arg(background, QString::number(radius), accent);
}

} // namespace Style
