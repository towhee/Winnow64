#include "huesatwheel.h"

#include <QPainter>
#include <QResizeEvent>
#include <cmath>

HueSatWheel::HueSatWheel(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void HueSatWheel::resizeEvent(QResizeEvent *)
{
    rebuildWheel();
}

/* Render the HSV disc once per size into an ARGB cache: hue = angle (0 deg at +x, growing
   anticlockwise), saturation = radius / disc radius, value fixed at 1. A one-pixel alpha
   ramp at the rim keeps the circle edge smooth. */
void HueSatWheel::rebuildWheel()
{
    if (width() <= 0 || height() <= 0) return;
    centre = QPointF(width() / 2.0, height() / 2.0);
    radius = static_cast<float>(qMax(4.0, qMin(width(), height()) / 2.0 - discMargin));

    wheelCache = QImage(size(), QImage::Format_ARGB32);   // straight alpha while we fill
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
            const float s = qMin(1.0f, dist / radius);
            QColor c = QColor::fromHsvF(qMin(0.9999f, ang / 360.0f), s, 1.0);
            float a = 1.0f;
            if (dist > radius) a = radius + 1.0f - dist;   // rim antialias
            const int ai = static_cast<int>(a * 255.0f + 0.5f);
            c.setAlpha(ai < 0 ? 0 : (ai > 255 ? 255 : ai));
            row[x] = c.rgba();
        }
    }
    /* Premultiply for fast drawing. */
    wheelCache = wheelCache.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

/* Draw the cached disc plus the rim ring. Subclasses call this first in paintEvent, then
   paint their own dots / sectors / handles on top. */
void HueSatWheel::paintDisc(QPainter &p)
{
    if (wheelCache.size() != size()) rebuildWheel();
    p.drawImage(0, 0, wheelCache);
    p.setPen(QPen(QColor(0, 0, 0, 110), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(centre, radius, radius);
}

QPointF HueSatWheel::posFor(float hueDeg, float satUnit) const
{
    const float rad = hueDeg * kPi / 180.0f;
    const float rr  = qBound(0.0f, satUnit, 1.0f) * radius;
    return QPointF(centre.x() + std::cos(rad) * rr,
                   centre.y() - std::sin(rad) * rr);
}

/* Widget pixels -> hue (0..360) + saturation (0..1, clamped at the rim). */
void HueSatWheel::hueSatAt(const QPointF &pos, float &hueDeg, float &sat) const
{
    const float dx = static_cast<float>(pos.x() - centre.x());
    const float dy = static_cast<float>(centre.y() - pos.y());
    const float dist = std::sqrt(dx * dx + dy * dy);
    float ang = std::atan2(dy, dx) * 180.0f / kPi;
    if (ang < 0.0f) ang += 360.0f;
    hueDeg = ang;
    sat = qMin(1.0f, dist / radius);
}
