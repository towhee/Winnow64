#include "colorrangewheel.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QResizeEvent>
#include <cmath>

namespace {
constexpr float kPi     = 3.14159265358979323846f;
constexpr float kMargin = 26.0f;    // rim-to-edge gap; leaves room for hue labels OUTSIDE
constexpr float kHueMax = 90.0f;    // matches the dock Hue Lo/Hi slider range
constexpr float kHitPx  = 10.0f;    // grab-handle hit radius
}

ColorRangeWheel::ColorRangeWheel(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(180);   // taller so the disc stays large with the outside labels
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);
}

void ColorRangeWheel::setSamples(const QVector<QPointF> &s)
{
    m_samples = s;
    update();
}

void ColorRangeWheel::setBounds(float hueLoDeg, float hueHiDeg, float satLo, float satHi)
{
    m_hueLo = qBound(0.0f, hueLoDeg, kHueMax);
    m_hueHi = qBound(0.0f, hueHiDeg, kHueMax);
    m_satLo = qBound(0.0f, satLo, 1.0f);
    m_satHi = qBound(0.0f, satHi, 1.0f);
    update();
}

void ColorRangeWheel::resizeEvent(QResizeEvent *)
{
    rebuildWheel();
}

/* Render the HSV disc once per size (hue = angle at +x growing anticlockwise, sat =
   radius / disc radius, value 1), with a one-pixel rim alpha ramp. */
void ColorRangeWheel::rebuildWheel()
{
    if (width() <= 0 || height() <= 0) return;
    centre = QPointF(width() / 2.0, height() / 2.0);
    radius = static_cast<float>(qMax(4.0, qMin(width(), height()) / 2.0 - kMargin));

    wheelCache = QImage(size(), QImage::Format_ARGB32);
    wheelCache.fill(Qt::transparent);
    const float cx = static_cast<float>(centre.x());
    const float cy = static_cast<float>(centre.y());
    for (int y = 0; y < height(); ++y) {
        QRgb *row = reinterpret_cast<QRgb *>(wheelCache.scanLine(y));
        for (int x = 0; x < width(); ++x) {
            const float dx = x + 0.5f - cx;
            const float dy = cy - (y + 0.5f);          // screen y grows downward
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > radius + 1.0f) continue;
            float ang = std::atan2(dy, dx) * 180.0f / kPi;
            if (ang < 0.0f) ang += 360.0f;
            const float ssat = qMin(1.0f, dist / radius);
            QColor c = QColor::fromHsvF(qMin(0.9999f, ang / 360.0f), ssat, 1.0);
            float a = 1.0f;
            if (dist > radius) a = radius + 1.0f - dist;
            const int ai = static_cast<int>(a * 255.0f + 0.5f);
            c.setAlpha(ai < 0 ? 0 : (ai > 255 ? 255 : ai));
            row[x] = c.rgba();
        }
    }
    wheelCache = wheelCache.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QPointF ColorRangeWheel::posFor(float hueDeg, float satUnit) const
{
    const float rad = hueDeg * kPi / 180.0f;
    const float rr  = qBound(0.0f, satUnit, 1.0f) * radius;
    return QPointF(centre.x() + std::cos(rad) * rr,
                   centre.y() - std::sin(rad) * rr);
}

bool ColorRangeWheel::anchor(float &hueDeg, float &sat) const
{
    if (m_samples.isEmpty()) return false;
    hueDeg = static_cast<float>(m_samples.first().x());
    sat    = static_cast<float>(m_samples.first().y());
    return true;
}

/* The four grab handles on the anchor sample: the two hue edges sit on the disc PERIMETER
   (sat = 1) so they read as the angular bounds of the whole wedge; the inner / outer
   saturation arcs sit on the sample's own hue spoke. */
void ColorRangeWheel::handlePositions(QPointF out[4]) const
{
    float h, s;
    if (!anchor(h, s)) { for (int i = 0; i < 4; ++i) out[i] = centre; return; }
    out[HueLoH]  = posFor(h - m_hueLo, 1.0f);
    out[HueHiH]  = posFor(h + m_hueHi, 1.0f);
    out[SatInH]  = posFor(h, qMax(0.0f, s - m_satLo));
    out[SatOutH] = posFor(h, qMin(1.0f, s + m_satHi));
}

int ColorRangeWheel::hitHandle(const QPointF &pos) const
{
    if (m_samples.isEmpty()) return None;
    QPointF hp[4]; handlePositions(hp);
    for (int i = 0; i < 4; ++i)
        if (QLineF(pos, hp[i]).length() <= kHitPx) return i;
    return None;
}

/* Signed smallest angular difference a-b in degrees, wrapped to (-180, 180]. */
static double angDiffDeg(double a, double b)
{
    double d = a - b;
    while (d >  180.0) d -= 360.0;
    while (d < -180.0) d += 360.0;
    return d;
}

void ColorRangeWheel::applyDrag(const QPointF &pos)
{
    float h, s;
    if (!anchor(h, s)) return;
    const double dx = pos.x() - centre.x();
    const double dy = centre.y() - pos.y();
    double ang = std::atan2(dy, dx) * 180.0 / kPi;
    if (ang < 0.0) ang += 360.0;
    const double dist = qBound(0.0, std::sqrt(dx * dx + dy * dy) / radius, 1.0);
    const double dAng = angDiffDeg(ang, h);       // +ve = hi (anticlockwise) side

    switch (m_drag) {
    case HueLoH: m_hueLo = qBound(0.0f, float(-dAng), kHueMax); break;
    case HueHiH: m_hueHi = qBound(0.0f, float( dAng), kHueMax); break;
    case SatInH: m_satLo = qBound(0.0f, float(s - dist), 1.0f); break;
    case SatOutH:m_satHi = qBound(0.0f, float(dist - s), 1.0f); break;
    default: return;
    }
    update();
    emit boundsChanged();
}

void ColorRangeWheel::paintEvent(QPaintEvent *)
{
    if (wheelCache.size() != size()) rebuildWheel();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawImage(0, 0, wheelCache);

    /* Rim ring + neutral centre. */
    p.setPen(QPen(QColor(0, 0, 0, 110), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(centre, radius, radius);

    if (m_samples.isEmpty()) {
        p.setPen(QColor(210, 210, 210, 200));
        p.drawText(rect(), Qt::AlignCenter, "Sample a colour on the image");
        return;
    }

    /* Band sector for every sample (all share the bounds). The sector is a polygon
       sampled along the outer + inner arcs (posFor already handles the y-flip), so no
       QPainter arc-angle sign juggling is needed. */
    for (const QPointF &sm : m_samples) {
        const float h = static_cast<float>(sm.x());
        const float s = static_cast<float>(sm.y());
        const float a0 = h - m_hueLo, a1 = h + m_hueHi;
        const float r0 = qMax(0.0f, s - m_satLo), r1 = qMin(1.0f, s + m_satHi);
        if (r1 <= r0 + 0.001f && (m_hueLo + m_hueHi) < 0.5f) continue;
        QPainterPath path;
        const int steps = qMax(2, int((m_hueLo + m_hueHi) / 2.0f) + 1);
        for (int i = 0; i <= steps; ++i) {                       // outer arc a0 -> a1
            const float t = a0 + (a1 - a0) * i / steps;
            const QPointF q = posFor(t, r1);
            if (i == 0) path.moveTo(q); else path.lineTo(q);
        }
        for (int i = steps; i >= 0; --i) {                       // inner arc a1 -> a0
            const float t = a0 + (a1 - a0) * i / steps;
            path.lineTo(posFor(t, r0));
        }
        path.closeSubpath();
        p.setPen(QPen(QColor(255, 255, 255, 210), 1.4));
        p.setBrush(QColor(255, 255, 255, 45));
        p.drawPath(path);
    }

    /* Sample pick dots (drawn in each sample's own hue/sat at full value). */
    for (const QPointF &sm : m_samples) {
        const QPointF d = posFor(static_cast<float>(sm.x()), static_cast<float>(sm.y()));
        p.setPen(QPen(QColor(0, 0, 0, 200), 1.5));
        p.setBrush(QColor::fromHsvF(qMin(0.9999, sm.x() / 360.0),
                                    qBound(0.0, sm.y(), 1.0), 1.0));
        p.drawEllipse(d, 5.0, 5.0);
        p.setPen(QPen(QColor(255, 255, 255, 220), 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(d, 5.0, 5.0);
    }

    /* Grab handles on the anchor sample. */
    QPointF hp[4]; handlePositions(hp);
    for (int i = 0; i < 4; ++i) {
        const bool hot = (m_drag == i);
        p.setPen(QPen(QColor(0, 0, 0, 210), hot ? 2.2 : 1.6));
        p.setBrush(QColor(255, 255, 255, hot ? 255 : 220));
        p.drawEllipse(hp[i], hot ? 6.0 : 5.0, hot ? 6.0 : 5.0);
    }

    /* Absolute hue (degrees) at each rim hue handle -- the low/high edges of the selected
       hue range. Drawn in a small dark pill just OUTSIDE the rim (kMargin reserves the
       space) so it never sits over the disc colours. */
    float ah, as;
    if (anchor(ah, as)) {
        auto wrap360 = [](float d){ while (d < 0.0f) d += 360.0f;
                                    while (d >= 360.0f) d -= 360.0f; return d; };
        const int hueDegAt[2] = { int(wrap360(ah - m_hueLo) + 0.5f) % 360,
                                  int(wrap360(ah + m_hueHi) + 0.5f) % 360 };
        QFont hf = p.font();
        hf.setPixelSize(10);
        p.setFont(hf);
        const QFontMetrics fm(hf);
        for (int i = 0; i < 2; ++i) {
            const QString t = QString::number(hueDegAt[i]);
            QPointF dir = hp[i] - centre;
            const double len = std::hypot(dir.x(), dir.y());
            dir = (len > 1e-3) ? dir / len : QPointF(0, -1);
            const QPointF c = hp[i] + dir * 12.0;            // just outside the rim
            const double w = fm.horizontalAdvance(t) + 8.0, h = fm.height() + 1.0;
            const QRectF box(c.x() - w / 2.0, c.y() - h / 2.0, w, h);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(18, 18, 18, 205));
            p.drawRoundedRect(box, 3.0, 3.0);
            p.setPen(QColor(240, 240, 240));
            p.drawText(box, Qt::AlignCenter, t);
        }
    }
}

void ColorRangeWheel::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    m_drag = hitHandle(e->position());
    if (m_drag != None) { applyDrag(e->position()); }
}

void ColorRangeWheel::mouseMoveEvent(QMouseEvent *e)
{
    if (m_drag != None) { applyDrag(e->position()); return; }
    /* Hover feedback: a pointing hand over a grab handle. */
    setCursor(hitHandle(e->position()) != None ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void ColorRangeWheel::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || m_drag == None) return;
    m_drag = None;
    update();
    emit boundsCommitted();
}
