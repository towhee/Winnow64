#include "primarywheel.h"
#include "Develop/calibrate.h"

#include <QPainter>
#include <QMouseEvent>
#include <cmath>

namespace {
const char *kPrimaryLabel[3] = {"R", "G", "B"};

/* Where a primary sits on the disc when its sat is 0. Mid radius, so the dot has room to
   travel both inward (desaturate) and outward (saturate). */
constexpr float kMidRadius = 0.5f;

/* Slider full scale, shared with the maths so the wheel and the stored values agree. */
constexpr float kFull = Calibrate::kFullScale;
}

PrimaryWheel::PrimaryWheel(QWidget *parent) : HueSatWheel(parent)
{
    discMargin = 12.0f;
    setMinimumHeight(120);
    setCursor(Qt::CrossCursor);
}

void PrimaryWheel::setPrimary(int p, float hue, float sat)
{
    if (p < 0 || p > 2) return;
    hueVal[p] = qBound(-kFull, hue, kFull);
    satVal[p] = qBound(-kFull, sat, kFull);
    update();
}

void PrimaryWheel::setActiveMask(int mask)
{
    activeMask = mask & 0x7;
    update();
}

/* A primary's dot: home angle swung by its hue delta, at mid radius shifted by its sat
   delta. sat -100 lands on the centre, +100 on the rim. */
QPointF PrimaryWheel::dotPos(int p) const
{
    const float ang = homeAngle(p) + (hueVal[p] / kFull) * Calibrate::kMaxHueDeg;
    const float rr  = kMidRadius + (satVal[p] / kFull) * kMidRadius;
    return posFor(ang, rr);
}

void PrimaryWheel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    paintDisc(p);

    /* Home spokes: a faint line down each primary's rest angle, so the dot's offset from
       its home reads as the hue shift it is. */
    p.setPen(QPen(QColor(0, 0, 0, 70), 1.0, Qt::DotLine));
    for (int i = 0; i < 3; ++i)
        p.drawLine(centre, posFor(homeAngle(i), 1.0f));

    /* Primary dots. Active ones draw full size / opacity, inactive smaller and faded, so
       the whole calibration is legible while the checked primaries stand out. */
    for (int i = 0; i < 3; ++i) {
        const bool active = (activeMask & (1 << i)) != 0;
        const QPointF d = dotPos(i);
        const qreal rad = active ? 8.0 : 5.0;
        const int alpha = active ? 255 : 110;
        p.setPen(QPen(QColor(0, 0, 0, alpha), active ? 2.0 : 1.0));
        p.setBrush(QColor(255, 255, 255, alpha));
        p.drawEllipse(d, rad, rad);
        p.setPen(QColor(30, 30, 30, alpha));
        QFont f = p.font();
        f.setPixelSize(active ? 10 : 8);
        f.setBold(active);
        p.setFont(f);
        p.drawText(QRectF(d.x() - rad, d.y() - rad, rad * 2, rad * 2),
                   Qt::AlignCenter, QString::fromLatin1(kPrimaryLabel[i]));
    }
}

/* Cursor -> hue/sat for every checked primary. The angle is taken RELATIVE to that
   primary's home spoke and wrapped into (-180,180] before scaling, so dragging near
   red's home never reads as a +350 deg swing. */
void PrimaryWheel::applyPos(const QPointF &pos)
{
    float ang, s;
    hueSatAt(pos, ang, s);
    bool any = false;
    for (int i = 0; i < 3; ++i) {
        if (!(activeMask & (1 << i))) continue;
        float d = ang - homeAngle(i);
        while (d >  180.0f) d -= 360.0f;
        while (d < -180.0f) d += 360.0f;
        hueVal[i] = qBound(-kFull, (d / Calibrate::kMaxHueDeg) * kFull, kFull);
        satVal[i] = qBound(-kFull, ((s - kMidRadius) / kMidRadius) * kFull, kFull);
        any = true;
    }
    if (any) {
        update();
        emit primaryChanged();
    }
}

void PrimaryWheel::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || activeMask == 0) return;
    dragging = true;
    applyPos(e->position());
}

void PrimaryWheel::mouseMoveEvent(QMouseEvent *e)
{
    if (dragging) applyPos(e->position());
}

void PrimaryWheel::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !dragging) return;
    dragging = false;
    emit primaryCommitted();
}

/* Double-click returns every checked primary to its home (0 hue, 0 sat), so a calibration
   can be cleared without hunting for the exact spoke. */
void PrimaryWheel::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || activeMask == 0) return;
    dragging = false;
    bool any = false;
    for (int i = 0; i < 3; ++i) {
        if (!(activeMask & (1 << i))) continue;
        hueVal[i] = 0.0f;
        satVal[i] = 0.0f;
        any = true;
    }
    if (any) {
        update();
        emit primaryChanged();
        emit primaryCommitted();
    }
}
