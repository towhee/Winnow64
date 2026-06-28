#include "Develop/Scopes/vectorscopeview.h"
#include "Main/global.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <cmath>

VectorscopeView::VectorscopeView(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("VectorscopeView::VectorscopeView");
    std::memset(vec, 0, sizeof(vec));
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void VectorscopeView::setData(const ScopeData &d)
{
    std::memcpy(vec, d.vec, sizeof(vec));
    hasData = true;
    update();
}

void VectorscopeView::clear()
{
    std::memset(vec, 0, sizeof(vec));
    hasData = false;
    update();
}

void VectorscopeView::setMarker(int r, int g, int b)
{
    if (hasMarker && marker[0] == r && marker[1] == g && marker[2] == b) return;
    marker[0] = r; marker[1] = g; marker[2] = b;
    hasMarker = true;
    update();
}

void VectorscopeView::clearMarker()
{
    if (!hasMarker) return;
    hasMarker = false;
    update();
}

void VectorscopeView::setZoom(double z)
{
    if (qFuzzyCompare(zoom, z)) return;
    zoom = z;
    update();
}

void VectorscopeView::setSkinLine(bool on)
{
    if (showSkinLine == on) return;
    showSkinLine = on;
    update();
}

void VectorscopeView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    const double zooms[3] = { 1.0, 1.5, 2.0 };
    const char *labels[3] = { "100%", "150%", "200%" };
    for (int i = 0; i < 3; ++i) {
        QAction *a = menu.addAction(QString::fromLatin1(labels[i]));
        a->setCheckable(true);
        a->setChecked(qFuzzyCompare(zoom, zooms[i]));
        connect(a, &QAction::triggered, this, [this, z = zooms[i]] {
            setZoom(z);
            emit zoomChanged(z);    // MW persists the choice
        });
    }
    menu.addSeparator();
    QAction *skin = menu.addAction("Skin tone line");
    skin->setCheckable(true);
    skin->setChecked(showSkinLine);
    connect(skin, &QAction::toggled, this, [this](bool on) {
        setSkinLine(on);
        emit skinLineChanged(on);   // MW persists the choice
    });
    menu.exec(event->globalPos());
}

QSize VectorscopeView::sizeHint() const
{
    return QSize(140, 140);
}

void VectorscopeView::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();   // consume so the dock does not treat it as un/redock
}

void VectorscopeView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QRect r = rect();
    p.fillRect(r, QColor(20, 20, 20));

    /* Square graticule centred in the widget; the trace is plotted within it so a
       neutral image collapses to the centre. */
    const int side = qMin(r.width(), r.height()) - 4;
    if (side <= 0) return;
    const double cx = r.width() / 2.0;
    const double cy = r.height() / 2.0;
    const double radius = side / 2.0;

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(70, 70, 70), 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), radius, radius);
    p.drawLine(QPointF(cx - radius, cy), QPointF(cx + radius, cy));
    p.drawLine(QPointF(cx, cy - radius), QPointF(cx, cy + radius));

    /* Skin-tone (flesh) line = the YIQ I-axis. From our BT.601 Cb/Cr coefficients the I-axis is
       (Cr-128) = -1.946*(Cb-128); skin lies on the warm ray (Cb<128, Cr>128) -> upper-left here.
       Drawn from the centre to the graticule edge; it is an angle guide, so it ignores zoom. */
    if (showSkinLine) {
        constexpr double sux = -0.4571, suy = 0.8896;   // unit (Cb,Cr) dir of the skin ray
        p.setPen(QPen(QColor(210, 180, 150), 1, Qt::DashLine));
        p.drawLine(QPointF(cx, cy), QPointF(cx + sux * radius, cy - suy * radius));   // +Cr up
    }

    /* Zoom magnifies the plotted trace/marker outward from the centre; the graticule circle above
       stays as the 100% reference, so higher zoom reveals low-saturation detail (and clips the
       outer fully-saturated points). */
    const double pr = radius * zoom;
    if (zoom > 1.0) {
        p.setPen(QColor(150, 150, 150));
        p.drawText(r.adjusted(0, 1, -3, 0), Qt::AlignTop | Qt::AlignRight,
                   QString::number(qRound(zoom * 100)) + "%");
    }

    if (!hasData) return;

    /* Log-scaled intensity: chroma counts span a huge range (the neutral cluster
       dominates), so log keeps faint coloured excursions visible. */
    quint32 peak = 1;
    for (int j = 0; j < ScopeData::VN; ++j)
        for (int i = 0; i < ScopeData::VN; ++i)
            if (vec[j][i] > peak) peak = vec[j][i];
    const double logPeak = std::log(static_cast<double>(peak) + 1.0);

    const double binToUnit = 2.0 / ScopeData::VN;   // bin 0..VN -> -1..+1 (Cb/Cr around 128)
    p.setPen(Qt::NoPen);
    for (int j = 0; j < ScopeData::VN; ++j) {
        for (int i = 0; i < ScopeData::VN; ++i) {
            const quint32 n = vec[j][i];
            if (!n) continue;
            /* i = Cb bin (x, blue-yellow), j = Cr bin (y, red-cyan). Screen y is
               inverted so +Cr (red) is up. */
            const double ux = (i + 0.5) * binToUnit - 1.0;
            const double uy = (j + 0.5) * binToUnit - 1.0;
            const double px = cx + ux * pr;
            const double py = cy - uy * pr;
            const int a = qBound(0, static_cast<int>(255.0 * std::log(n + 1.0) / logPeak), 255);
            /* Colour each point by its own hue (angle), so the trace paints the
               familiar coloured vectorscope rather than a monochrome blob. */
            constexpr double kTwoPi = 6.283185307179586;   // M_PI is not portable (MSVC)
            QColor col = QColor::fromHsvF(
                std::fmod(std::atan2(uy, ux) / kTwoPi + 1.0, 1.0),
                qBound(0.0, std::hypot(ux, uy), 1.0),
                1.0);
            col.setAlpha(a);
            p.fillRect(QRectF(px - 0.6, py - 0.6, 1.2, 1.2), col);
        }
    }

    /* Cursor readout: a ring at the hovered pixel's Cb/Cr (same BT.601 coeffs as the sample pass).
       Neutral pixels land at the centre; saturation is radius, hue is angle. */
    if (hasMarker) {
        const int r = marker[0], g = marker[1], b = marker[2];
        const int cb = qBound(0, 128 + ((-43 * r - 85 * g + 128 * b) >> 8), 255);
        const int cr = qBound(0, 128 + ((128 * r - 107 * g - 21 * b) >> 8), 255);
        const double px = cx + ((cb - 128) / 128.0) * pr;
        const double py = cy - ((cr - 128) / 128.0) * pr;       // +Cr up
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 180), 3));                // dark halo for contrast
        p.drawEllipse(QPointF(px, py), 4.5, 4.5);
        p.setPen(QPen(QColor(255, 255, 255), 1.5));
        p.drawEllipse(QPointF(px, py), 4.5, 4.5);
    }
}
