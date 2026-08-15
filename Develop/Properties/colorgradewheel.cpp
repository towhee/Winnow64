#include "colorgradewheel.h"

#include <QPainter>
#include <QMouseEvent>
#include <cmath>

namespace {
/* Range dot letters (shadows / midtones / highlights / global). */
const char *kRangeLabel[4] = {"S", "M", "H", "G"};
}

ColorGradeWheel::ColorGradeWheel(QWidget *parent) : HueSatWheel(parent)
{
    discMargin = 12.0f;             // bare wheel: nothing is labelled outside the rim
    setMinimumHeight(120);
    setCursor(Qt::CrossCursor);
}

void ColorGradeWheel::setRange(int range, float hueDegVal, float satVal)
{
    if (range < 0 || range > 3) return;
    hueDeg[range]  = hueDegVal;
    satUnit[range] = satVal < 0.0f ? 0.0f : (satVal > 1.0f ? 1.0f : satVal);
    update();
}

void ColorGradeWheel::setActiveMask(int mask)
{
    activeMask = mask & 0xF;
    update();
}

void ColorGradeWheel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    paintDisc(p);

    /* Range dots. Active ones draw full size / opacity, inactive ones smaller and faded
       so the whole grade is legible while the checked range(s) stand out. */
    for (int r = 0; r < 4; ++r) {
        const bool active = (activeMask & (1 << r)) != 0;
        const QPointF d = posFor(hueDeg[r], satUnit[r]);
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
    float ang, s;
    hueSatAt(pos, ang, s);
    bool any = false;
    for (int r = 0; r < 4; ++r) {
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
    for (int r = 0; r < 4; ++r) {
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
