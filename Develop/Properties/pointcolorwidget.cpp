#include "pointcolorwidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QHelpEvent>
#include <QMenu>
#include <QToolTip>
#include <cmath>

namespace {
constexpr int   kPad        = 6;      // left / right inset
constexpr int   kRowH       = 24;     // dropper + swatch row
constexpr int   kSwatch     = 18;
constexpr int   kSwatchGap  = 5;
constexpr int   kFieldTop   = kRowH + 8;
constexpr int   kFieldH     = 110;
constexpr int   kStripW     = 18;
constexpr int   kStripGap   = 8;
constexpr int   kBarGap     = 8;
constexpr int   kBarH       = 14;
constexpr float kFieldHueSpanDeg = 75.0f;   // field = sample hue +/- this
constexpr float kDegToRad = PointColor::kPi / 180.0f;

QColor srgbColor(float r, float g, float b)
{
    auto c8 = [](float v) { return int(std::lround(qBound(0.0f, v, 1.0f) * 255.0f)); };
    return QColor(c8(r), c8(g), c8(b));
}

QColor sampleColor(const PointColor::Point &p) { return srgbColor(p.r, p.g, p.b); }

/* The sample as the shifts take it (Variance aside: it has no effect on the sample). */
QColor shiftedColor(const PointColor::Point &p)
{
    float L, C, h;
    PointColor::sampleLch(p, L, C, h);
    float r, g, b;
    PointColor::lchToSrgb(PointColor::shiftedLightness(p, L),
                          PointColor::shiftedChroma(p, C),
                          PointColor::shiftedHue(p, h), r, g, b);
    return srgbColor(r, g, b);
}
}

PointColorWidget::PointColorWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setMinimumHeight(preferredHeight());
}

int PointColorWidget::preferredHeight()
{
    return kFieldTop + kFieldH + kBarGap + kBarH + 4;
}

QSize PointColorWidget::sizeHint() const
{
    return QSize(240, preferredHeight());
}

void PointColorWidget::setPoints(int count, const PointColor::Point *p, int selected)
{
    n = qBound(0, count, PointColor::kMaxPts);
    for (int i = 0; i < PointColor::kMaxPts; ++i)
        pts[i] = (i < n && p) ? p[i] : PointColor::Point();
    sel = (selected >= 0 && selected < n) ? selected : (n > 0 ? n - 1 : -1);
    if (hasSel()) PointColor::sampleLch(pts[sel], Ls, Cs, hs);
    update();
}

void PointColorWidget::setDropperIcons(const QIcon &idle, const QIcon &armedIcon)
{
    dropIdle = idle;
    dropArmed = armedIcon;
    update();
}

void PointColorWidget::setPickArmed(bool on)
{
    if (armed == on) return;
    armed = on;
    update();
}

/* ---- Geometry ---------------------------------------------------------------------- */

QRect PointColorWidget::dropperRect() const
{
    return QRect(kPad, (kRowH - 22) / 2, 22, 22);
}

QRect PointColorWidget::swatchRect(int i) const
{
    const int x0 = dropperRect().right() + 10;
    return QRect(x0 + i * (kSwatch + kSwatchGap), (kRowH - kSwatch) / 2, kSwatch, kSwatch);
}

QRect PointColorWidget::deleteRect() const
{
    return QRect(width() - kPad - 16, (kRowH - 16) / 2, 16, 16);
}

QRect PointColorWidget::fieldRect() const
{
    return QRect(kPad, kFieldTop, width() - 2 * kPad - kStripW - kStripGap, kFieldH);
}

QRect PointColorWidget::stripRect() const
{
    const QRect f = fieldRect();
    return QRect(f.right() + 1 + kStripGap, f.top(), kStripW, f.height());
}

QRect PointColorWidget::barRect() const
{
    return QRect(kPad, kFieldTop + kFieldH + kBarGap, width() - 2 * kPad, kBarH);
}

int PointColorWidget::swatchAt(const QPoint &pos) const
{
    for (int i = 0; i < n; ++i)
        if (swatchRect(i).adjusted(-2, -2, 2, 2).contains(pos)) return i;
    return -1;
}

/* Chroma at the top of the field: room for Sat Shift +100 (x2) on the sample, with a
   floor so a muted sample still shows a useful spread of colour. */
float PointColorWidget::fieldCMax() const
{
    return qMax(0.12f, Cs * (1.0f + PointColor::kSatUpMax) * 1.1f);
}

QPointF PointColorWidget::fieldPos(float h, float C) const
{
    const QRectF f = fieldRect();
    const float dh = PointColor::wrapPi(h - hs) / (kFieldHueSpanDeg * kDegToRad);
    const float x = qBound(-1.0f, dh, 1.0f) * 0.5f + 0.5f;
    const float y = 1.0f - qBound(0.0f, C / fieldCMax(), 1.0f);
    return QPointF(f.left() + x * f.width(), f.top() + y * f.height());
}

float PointColorWidget::stripY(float L) const
{
    const QRectF s = stripRect();
    return float(s.top() + (1.0 - qBound(0.0f, L, 1.0f)) * s.height());
}

void PointColorWidget::rebuildFieldImage()
{
    const QRect f = fieldRect();
    const QRect s = stripRect();
    if (imgFor == f.size() && imgL == Ls && imgC == Cs && imgH == hs) return;
    imgFor = f.size();
    imgL = Ls; imgC = Cs; imgH = hs;
    const int fw = qMax(1, f.width()), fh = qMax(1, f.height());
    fieldImg = QImage(fw, fh, QImage::Format_RGB32);
    const float cMax = fieldCMax();
    for (int y = 0; y < fh; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(fieldImg.scanLine(y));
        const float C = cMax * (1.0f - (y + 0.5f) / fh);
        for (int x = 0; x < fw; ++x) {
            const float t = (x + 0.5f) / fw * 2.0f - 1.0f;
            const float h = hs + t * kFieldHueSpanDeg * kDegToRad;
            float r, g, b;
            PointColor::lchToSrgb(Ls, C, h, r, g, b);
            line[x] = srgbColor(r, g, b).rgb();
        }
    }
    const int sh = qMax(1, s.height());
    stripImg = QImage(1, sh, QImage::Format_RGB32);
    for (int y = 0; y < sh; ++y) {
        const float L = 1.0f - (y + 0.5f) / sh;
        float r, g, b;
        PointColor::lchToSrgb(L, Cs, hs, r, g, b);
        stripImg.setPixel(0, y, srgbColor(r, g, b).rgb());
    }
}

/* ---- Paint ------------------------------------------------------------------------- */

void PointColorWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    /* A greyed panel (no image, or Develop disabled) dims the whole picture: its
       colours carry meaning, so they fade rather than vanish. */
    if (!isEnabled()) p.setOpacity(0.45);
    const QColor frame(120, 120, 120);
    const QColor text(170, 170, 170);

    /* Dropper: greyed when the point list is full (the tooltip says why). */
    const QRect dr = dropperRect();
    const bool full = n >= PointColor::kMaxPts;
    if (armed) {
        p.setPen(QPen(QColor(10, 132, 255), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(dr).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }
    const QIcon &ic = armed ? dropArmed : dropIdle;
    if (!ic.isNull()) {
        p.save();
        if (full && !armed) p.setOpacity(0.3);
        ic.paint(&p, dr.adjusted(3, 3, -3, -3));
        p.restore();
    }

    /* Swatches, then the delete cross for the selected one. */
    for (int i = 0; i < n; ++i) {
        const QRectF r = swatchRect(i);
        p.setPen(QPen(frame, 1));
        p.setBrush(sampleColor(pts[i]));
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
        if (i == sel) {
            p.setPen(QPen(Qt::white, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(-2, -2, 2, 2), 4, 4);
        }
    }
    if (hasSel()) {
        const QRectF x = deleteRect();
        p.setPen(QPen(text, 1.6));
        const qreal m = 4.0;
        p.drawLine(QPointF(x.left() + m, x.top() + m), QPointF(x.right() - m, x.bottom() - m));
        p.drawLine(QPointF(x.right() - m, x.top() + m), QPointF(x.left() + m, x.bottom() - m));
    }

    const QRect f = fieldRect();
    const QRect s = stripRect();
    const QRect b = barRect();
    if (!hasSel()) {
        /* No sample yet: the reason the sliders below are greyed, in their place. */
        p.setPen(QPen(frame, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(f.united(s)).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        p.setPen(text);
        p.drawText(f.united(s).adjusted(12, 0, -12, 0), Qt::AlignCenter | Qt::TextWordWrap,
                   tr("Click the eyedropper, then click a colour in the image to "
                      "adjust it."));
        return;
    }

    rebuildFieldImage();
    p.drawImage(f, fieldImg);
    p.drawImage(s, stripImg.scaled(s.width(), s.height()));
    p.setPen(QPen(frame, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(f).adjusted(0.5, 0.5, -0.5, -0.5));
    p.drawRect(QRectF(s).adjusted(0.5, 0.5, -0.5, -0.5));

    /* Rings: small = the sample, large = where the shifts take it. */
    const PointColor::Point &pt = pts[sel];
    const float ht = PointColor::shiftedHue(pt, hs);
    const float Ct = PointColor::shiftedChroma(pt, Cs);
    const float Lt = PointColor::shiftedLightness(pt, Ls);
    auto ring = [&p](const QPointF &c, qreal rad, qreal w) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 140), w + 2));
        p.drawEllipse(c, rad, rad);
        p.setPen(QPen(Qt::white, w));
        p.drawEllipse(c, rad, rad);
    };
    ring(fieldPos(hs, Cs), 3.0, 1.2);
    ring(fieldPos(ht, Ct), 6.5, 1.6);
    const qreal sx = s.center().x() + 0.5;
    ring(QPointF(sx, stripY(Ls)), 3.0, 1.2);
    ring(QPointF(sx, stripY(Lt)), 6.0, 1.6);

    /* Before | after. */
    QRectF left(b.left(), b.top(), b.width() / 2.0, b.height());
    QRectF right(left.right(), b.top(), b.width() - left.width(), b.height());
    p.setPen(Qt::NoPen);
    p.setBrush(sampleColor(pt));
    p.drawRect(left);
    p.setBrush(shiftedColor(pt));
    p.drawRect(right);
    p.setPen(QPen(frame, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(b).adjusted(0.5, 0.5, -0.5, -0.5));
}

/* ---- Mouse ------------------------------------------------------------------------- */

void PointColorWidget::dragTo(const QPoint &pos, bool commit)
{
    if (!hasSel()) return;
    PointColor::Point &pt = pts[sel];
    if (drag == Field) {
        const QRectF f = fieldRect();
        const float x = qBound(0.0f, float((pos.x() - f.left()) / f.width()), 1.0f);
        const float y = qBound(0.0f, float((pos.y() - f.top()) / f.height()), 1.0f);
        const float ht = hs + (x * 2.0f - 1.0f) * kFieldHueSpanDeg * kDegToRad;
        const float Ct = (1.0f - y) * fieldCMax();
        pt.hueShift = std::round(PointColor::hueShiftFor(hs, ht));
        pt.satShift = std::round(PointColor::satShiftFor(Cs, Ct));
    }
    else if (drag == Strip) {
        const QRectF s = stripRect();
        const float L = 1.0f - qBound(0.0f, float((pos.y() - s.top()) / s.height()), 1.0f);
        pt.lumShift = std::round(PointColor::lumShiftFor(Ls, L));
    }
    else return;
    update();
    emit shiftsDragged(pt.hueShift, pt.satShift, pt.lumShift, commit);
}

void PointColorWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    const QPoint pos = e->position().toPoint();
    if (dropperRect().contains(pos)) {
        if (n < PointColor::kMaxPts || armed) emit pickToggled();
        return;
    }
    const int sw = swatchAt(pos);
    if (sw >= 0) {
        if (sw != sel) emit pointSelected(sw);
        return;
    }
    if (hasSel() && deleteRect().contains(pos)) {
        emit pointDeleted(sel);
        return;
    }
    if (!hasSel()) return;
    if (fieldRect().contains(pos))      drag = Field;
    else if (stripRect().adjusted(-4, 0, 4, 0).contains(pos)) drag = Strip;
    else return;
    dragTo(pos, false);
}

void PointColorWidget::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint pos = e->position().toPoint();
    if (drag != None) { dragTo(pos, false); return; }
    const bool overControl = dropperRect().contains(pos) || swatchAt(pos) >= 0 ||
                             (hasSel() && deleteRect().contains(pos));
    const bool overPlot = hasSel() && (fieldRect().contains(pos) ||
                                       stripRect().contains(pos));
    setCursor(overPlot ? Qt::CrossCursor
                       : (overControl ? Qt::PointingHandCursor : Qt::ArrowCursor));
}

void PointColorWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (drag == None) return;
    dragTo(e->position().toPoint(), true);
    drag = None;
}

void PointColorWidget::contextMenuEvent(QContextMenuEvent *e)
{
    const int sw = swatchAt(e->pos());
    if (sw < 0) { e->ignore(); return; }
    QMenu menu(this);
    QAction *del = menu.addAction(tr("Delete Point"));
    QAction *all = menu.addAction(tr("Delete All Points"));
    QAction *hit = menu.exec(e->globalPos());
    if (hit == del) emit pointDeleted(sw);
    else if (hit == all) emit allPointsDeleted();
}

bool PointColorWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        auto *he = static_cast<QHelpEvent *>(e);
        const QPoint pos = he->pos();
        QString tip;
        if (dropperRect().contains(pos))
            tip = n >= PointColor::kMaxPts
                      ? tr("All %1 points are in use -- delete one to sample another.")
                            .arg(PointColor::kMaxPts)
                      : tr("Sample a colour: click here, then click the image.");
        else if (swatchAt(pos) >= 0)
            tip = tr("Select this point. Right-click to delete.");
        else if (hasSel() && deleteRect().contains(pos))
            tip = tr("Delete the selected point.");
        else if (hasSel() && fieldRect().contains(pos))
            tip = tr("Drag to shift hue (across) and saturation (up / down). The small "
                     "ring is the sampled colour, the large one where it is going.");
        else if (hasSel() && stripRect().contains(pos))
            tip = tr("Drag to shift luminance.");
        else if (hasSel() && barRect().contains(pos))
            tip = tr("The sampled colour, and the colour it becomes.");
        if (tip.isEmpty()) QToolTip::hideText();
        else QToolTip::showText(he->globalPos(), tip, this);
        return true;
    }
    return QWidget::event(e);
}
