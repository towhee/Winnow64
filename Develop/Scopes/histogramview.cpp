#include "Develop/Scopes/histogramview.h"
#include "Main/global.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

HistogramView::HistogramView(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("HistogramView::HistogramView");
    std::memset(hist, 0, sizeof(hist));
    setAttribute(Qt::WA_OpaquePaintEvent);      // we fill the whole rect ourselves
}

void HistogramView::setData(const ScopeData &d)
{
    std::memcpy(hist, d.hist, sizeof(hist));
    hasData = true;
    update();
}

void HistogramView::clear()
{
    std::memset(hist, 0, sizeof(hist));
    hasData = false;
    update();
}

void HistogramView::setMarker(int r, int g, int b)
{
    if (hasMarker && marker[0] == r && marker[1] == g && marker[2] == b) return;
    marker[0] = r; marker[1] = g; marker[2] = b;
    hasMarker = true;
    update();
}

void HistogramView::clearMarker()
{
    if (!hasMarker) return;
    hasMarker = false;
    update();
}

QSize HistogramView::sizeHint() const
{
    return QSize(180, 140);
}

void HistogramView::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();   // consume so the dock does not treat it as un/redock
}

void HistogramView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QRect r = rect();
    p.fillRect(r, QColor(20, 20, 20));
    if (!hasData) return;

    /* Normalise to the tallest single-channel bin, ignoring the two end bins (pure
       black / pure white spikes routinely dwarf the rest and would flatten the curve).
       A small floor avoids divide-by-zero on a blank image. */
    quint32 peak = 1;
    for (int c = 0; c < 3; ++c)
        for (int v = 1; v < 255; ++v)
            if (hist[c][v] > peak) peak = hist[c][v];

    const int w = r.width();
    const int h = r.height();
    const double xScale = (w - 1) / 255.0;
    const double yScale = (h - 2) / static_cast<double>(peak);

    /* Map a bin count to a y. A zero count lands EXACTLY on the baseline (y == h) so empty
       tonal ranges add no fill -- otherwise every channel paints a 1px baseline strip that,
       blended, shows as a white line across the bottom (visible whenever the data collapses
       to one end, e.g. exposure crushed to black). */
    auto yOf = [&](quint32 count) {
        return h - qMin<double>(h - 2, count * yScale);
    };

    /* Split the additive histogram into two parts so the white can be dimmed independently:
         - the NEUTRAL CORE = the per-bin min of R,G,B (0..m), where all three channels overlap
           and would blend to solid white; drawn once at whiteOpacity.
         - the COLOUR EXCESS = each channel as a BAND from m up to its own height, blended
           additively; where a channel IS the minimum its band is zero-height and adds nothing.
       whiteOpacity 1.0 reproduces the classic full-white core, 0.0 hides it. */
    auto neutralAt = [&](int v) {
        return qMin(hist[0][v], qMin(hist[1][v], hist[2][v]));
    };
    auto bandPath = [&](int c) {
        QPainterPath path;
        path.moveTo(0, yOf(hist[c][0]));
        for (int v = 1; v < 256; ++v)
            path.lineTo(v * xScale, yOf(hist[c][v]));
        for (int v = 255; v >= 0; --v)
            path.lineTo(v * xScale, yOf(neutralAt(v)));
        path.closeSubpath();
        return path;
    };

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    /* Neutral core, drawn at whiteOpacity (SourceOver so the opacity reads literally). */
    if (whiteOpacity > 0.0) {
        QPainterPath core;
        core.moveTo(0, h);
        for (int v = 0; v < 256; ++v)
            core.lineTo(v * xScale, yOf(neutralAt(v)));
        core.lineTo((255) * xScale, h);
        core.closeSubpath();
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setBrush(QColor(255, 255, 255, qBound(0, qRound(whiteOpacity * 255.0), 255)));
        p.drawPath(core);
    }

    /* Colour excess above the neutral core, additive ("lighten") blend on the dark ground. */
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    const QColor fill[3] = { QColor(200, 50, 50), QColor(50, 190, 50), QColor(60, 90, 220) };
    for (int c = 0; c < 3; ++c) {
        p.setBrush(fill[c]);
        p.drawPath(bandPath(c));
    }

    /* Luma as a faint outline over the top (normal blend). */
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    QPainterPath luma;
    luma.moveTo(0, yOf(hist[3][0]));
    for (int v = 1; v < 256; ++v)
        luma.lineTo(v * xScale, yOf(hist[3][v]));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(180, 180, 180, 120), 1));
    p.drawPath(luma);

    /* Cursor readout: a vertical tick per channel at the hovered pixel's R/G/B value, on the same
       x-axis as the bins. */
    if (hasMarker) {
        p.setRenderHint(QPainter::Antialiasing, false);
        const QColor tick[3] = { QColor(255, 80, 80), QColor(90, 255, 90), QColor(110, 150, 255) };
        for (int c = 0; c < 3; ++c) {
            const int x = qRound(qBound(0, marker[c], 255) * xScale);
            p.setPen(QPen(tick[c], 1));
            p.drawLine(x, 0, x, h - 1);
        }
    }
}
