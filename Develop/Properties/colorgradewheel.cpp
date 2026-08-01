#include "colorgradewheel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <cmath>

namespace {
constexpr float kPi     = 3.14159265358979323846f;
constexpr float kMargin = 12.0f;    // px between the disc rim and the widget edge

/* Range dot letters (shadows / midtones / highlights). */
const char *kRangeLabel[3] = {"S", "M", "H"};
}

ColorGradeWheel::ColorGradeWheel(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(120);
    setCursor(Qt::CrossCursor);
    setAttribute(Qt::WA_TranslucentBackground);
}

void ColorGradeWheel::setRange(int range, float hueDegVal, float satVal)
{
    if (range < 0 || range > 2) return;
    hueDeg[range]  = hueDegVal;
    satUnit[range] = satVal < 0.0f ? 0.0f : (satVal > 1.0f ? 1.0f : satVal);
    update();
}

void ColorGradeWheel::setActiveMask(int mask)
{
    activeMask = mask & 0x7;
    update();
}

void ColorGradeWheel::resizeEvent(QResizeEvent *)
{
    rebuildWheel();
}

/* Render the HSV disc once per size into an ARGB cache: hue = angle (0 deg at +x, growing
   anticlockwise), saturation = radius / disc radius, value fixed at 1. A one-pixel alpha
   ramp at the rim keeps the circle edge smooth. */
void ColorGradeWheel::rebuildWheel()
{
    if (width() <= 0 || height() <= 0) return;
    centre = QPointF(width() / 2.0, height() / 2.0);
    radius = static_cast<float>(qMax(4.0, qMin(width(), height()) / 2.0 - kMargin));

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

QPointF ColorGradeWheel::dotPos(int range) const
{
    const float rad = hueDeg[range] * kPi / 180.0f;
    const float rr  = satUnit[range] * radius;
    return QPointF(centre.x() + std::cos(rad) * rr,
                   centre.y() - std::sin(rad) * rr);
}

void ColorGradeWheel::paintEvent(QPaintEvent *)
{
    if (wheelCache.size() != size()) rebuildWheel();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawImage(0, 0, wheelCache);

    /* Neutral-centre marker + rim ring. */
    p.setPen(QPen(QColor(0, 0, 0, 110), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(centre, radius, radius);

    /* Range dots. Active ones draw full size / opacity, inactive ones smaller and faded
       so the whole grade is legible while the checked range(s) stand out. */
    for (int r = 0; r < 3; ++r) {
        const bool active = (activeMask & (1 << r)) != 0;
        const QPointF d = dotPos(r);
        const qreal rad = active ? 8.0 : 5.0;
        const int alpha = active ? 255 : 110;
        p.setPen(QPen(QColor(0, 0, 0, alpha), active ? 2.0 : 1.0));
        p.setBrush(QColor(255, 255, 255, alpha));
        p.drawEllipse(d, rad, rad);
        QColor label(30, 30, 30, alpha);
        p.setPen(label);
        QFont f = p.font();
        f.setPixelSize(active ? 10 : 8);
        f.setBold(active);
        p.setFont(f);
        p.drawText(QRectF(d.x() - rad, d.y() - rad, rad * 2, rad * 2),
                   Qt::AlignCenter, QString::fromLatin1(kRangeLabel[r]));
    }
}

void ColorGradeWheel::applyPos(const QPointF &pos)
{
    const float dx = static_cast<float>(pos.x() - centre.x());
    const float dy = static_cast<float>(centre.y() - pos.y());
    const float dist = std::sqrt(dx * dx + dy * dy);
    float ang = std::atan2(dy, dx) * 180.0f / kPi;
    if (ang < 0.0f) ang += 360.0f;
    const float s = qMin(1.0f, dist / radius);
    bool any = false;
    for (int r = 0; r < 3; ++r) {
        if (activeMask & (1 << r)) {
            hueDeg[r]  = ang;
            satUnit[r] = s;
            any = true;
        }
    }
    if (any) {
        update();
        emit gradeChanged();
    }
}

void ColorGradeWheel::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || activeMask == 0) return;
    dragging = true;
    applyPos(e->position());
}

void ColorGradeWheel::mouseMoveEvent(QMouseEvent *e)
{
    if (dragging) applyPos(e->position());
}

void ColorGradeWheel::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !dragging) return;
    dragging = false;
    emit gradeCommitted();
}

/* Double-click resets every checked range back to neutral (centre: hue 0, sat 0), so a
   grade can be cleared without dragging to the middle. gradeChanged updates the params +
   preview live; gradeCommitted persists it. */
void ColorGradeWheel::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || activeMask == 0) return;
    dragging = false;
    bool any = false;
    for (int r = 0; r < 3; ++r) {
        if (activeMask & (1 << r)) {
            hueDeg[r]  = 0.0f;
            satUnit[r] = 0.0f;
            any = true;
        }
    }
    if (any) {
        update();
        emit gradeChanged();
        emit gradeCommitted();
    }
}
