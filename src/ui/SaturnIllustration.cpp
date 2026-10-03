#include "ui/SaturnIllustration.h"
#include "core/Logging.h"

#include <QPaintEvent>
#include <QPainter>
#include <QSvgRenderer>

SaturnIllustration::SaturnIllustration(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);

    QSvgRenderer renderer(QStringLiteral(":/icons/saturn.svg"));
    if (renderer.isValid()) {
        // Render at 2x for crisp display on HiDPI screens.
        QPixmap pixmap(renderer.defaultSize() * 2);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        m_svgPixmap = pixmap;
    } else {
        qCWarning(lcCosmic) << "saturn.svg missing or invalid";
    }
}

QSize SaturnIllustration::sizeHint() const
{
    return {160, 160};
}

void SaturnIllustration::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (!m_svgPixmap.isNull()) {
        painter.drawPixmap(rect(), m_svgPixmap, m_svgPixmap.rect());
    }
}
