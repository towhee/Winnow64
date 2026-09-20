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

/* Remember where a fine drag starts: the cursor's hue/sat and every primary's current
   value. The fine branch below adds a fraction of the cursor's movement SINCE this
   point, so taking the anchor changes nothing by itself. */
void PrimaryWheel::takeFineAnchor(float ang, float s)
{
    fineAnchorHue = ang;
    fineAnchorSat = s;
    for (int i = 0; i < 3; ++i) {
        anchorHueVal[i] = hueVal[i];
        anchorSatVal[i] = satVal[i];
    }
}

/* How many primaries a drag will move. One is a placement; several is a nudge. */
int PrimaryWheel::activeCount() const
{
    return ((activeMask & 0x1) ? 1 : 0) + ((activeMask & 0x2) ? 1 : 0) +
           ((activeMask & 0x4) ? 1 : 0);
}

/* Cursor -> hue/sat for every checked primary, in one of two modes.

   PLACEMENT (absolute): one primary checked, no Shift. The angle is taken RELATIVE to
   that primary's home spoke and wrapped into (-180,180] before scaling, so dragging near
   red's home never reads as a +350 deg swing, and the dot sits under the pointer. This
   is what a wheel should do when there is one thing to point at.

   NUDGE (relative): Shift, or MORE THAN ONE primary checked. Each primary moves by the
   cursor's movement since the anchor -- all of it normally, kFineGain of it under Shift.
   Because the movement is a DIFFERENCE of cursor angles, the same delta reaches every
   checked primary whatever its home angle, so primaries set to different values KEEP
   their difference. Absolute placement cannot: it would put every checked primary on the
   one value the pointer names, so a single drag on an image that already carried a
   calibration would flatten it. That is why multi-select forces this mode, and it is the
   same reasoning that makes the Hue / Saturation sliders relative (see setCalAxis).

   The scale is identical in both modes -- full slider range is 30 deg of arc either way
   -- so switching modes does not change how far a given drag travels; only Shift does.
   The cost is that the dots no longer sit under the pointer in a multi drag, which is
   unavoidable: three dots cannot all be under one cursor. */
void PrimaryWheel::applyPos(const QPointF &pos, bool fine)
{
    float ang, s;
    hueSatAt(pos, ang, s);
    const bool relative = fine || dragRelative;
    /* The Shift state flipped (at press or mid-drag). Re-anchor here and move nothing, so
       the dots carry on from where they are instead of jumping: entering fine always
       needs it, and so does LEAVING fine while relative, where there is no absolute
       tracking to resume. A single-primary drag leaving fine resumes placement and needs
       no anchor. */
    if (fineStateChanged(fine) && relative) {
        takeFineAnchor(ang, s);
        return;
    }
    const float gain = fine ? kFineGain : 1.0f;
    const float dHue = wrapDeg(ang - fineAnchorHue) / Calibrate::kMaxHueDeg * kFull;
    const float dSat = (s - fineAnchorSat) / kMidRadius * kFull;
    bool any = false;
    for (int i = 0; i < 3; ++i) {
        if (!(activeMask & (1 << i))) continue;
        if (relative) {
            hueVal[i] = qBound(-kFull, anchorHueVal[i] + gain * dHue, kFull);
            satVal[i] = qBound(-kFull, anchorSatVal[i] + gain * dSat, kFull);
        }
        else {
            const float d = wrapDeg(ang - homeAngle(i));
            hueVal[i] = qBound(-kFull, (d / Calibrate::kMaxHueDeg) * kFull, kFull);
            satVal[i] = qBound(-kFull, ((s - kMidRadius) / kMidRadius) * kFull, kFull);
        }
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
    /* Placement or nudge is decided HERE and held for the whole drag, so a checkbox
       toggled between drags cannot leave a drag half in one mode with a stale anchor. */
    dragRelative = activeCount() > 1;
    const bool fine = e->modifiers() & Qt::ShiftModifier;
    if (dragRelative || fine) {
        /* A nudge starts from an anchor and moves nothing: the dots stay put rather than
           snapping to the pointer. Setting fineDrag first means a mid-drag Shift change
           still reads as a flip and re-anchors. */
        fineDrag = fine;
        float ang, s;
        hueSatAt(e->position(), ang, s);
        takeFineAnchor(ang, s);
        return;
    }
    fineDrag = false;
    applyPos(e->position(), false);
}

void PrimaryWheel::mouseMoveEvent(QMouseEvent *e)
{
    if (dragging) applyPos(e->position(), e->modifiers() & Qt::ShiftModifier);
}

void PrimaryWheel::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !dragging) return;
    dragging = false;
    fineDrag = false;               // the next drag re-decides its own mode at press
    dragRelative = false;
    emit primaryCommitted();
}

/* Double-click returns every checked primary to its home (0 hue, 0 sat), so a calibration
   can be cleared without hunting for the exact spoke. */
void PrimaryWheel::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || activeMask == 0) return;
    dragging = false;
    fineDrag = false;
    dragRelative = false;
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
