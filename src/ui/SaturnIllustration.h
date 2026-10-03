#pragma once

#include <QPixmap>
#include <QWidget>

/// Local, resolution-independent Saturn illustration for the New Tab page.
/// Vector artwork bundled in resources/icons/saturn.svg — no CDN, no emoji.
/// Replaced wholesale by the Theme/UI Engine in Phase 3.
class SaturnIllustration : public QWidget
{
    Q_OBJECT

public:
    explicit SaturnIllustration(QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_svgPixmap; // rendered once from the bundled SVG
};
