#include "pointcolorwidget.h"

#include <QPainter>
#include <QPainterPath>
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
constexpr float kFieldHueSpanDeg = 75.0f;   // field = centre hue +/- this, at least
constexpr float kFieldHueMarginDeg = 35.0f; //   and this beyond the samples' arc
constexpr float kDegToRad = PointColor::kPi / 180.0f;

QColor srgbColor(float r, float g, float b)
{
    auto c8 = [](float v) { return int(std::lround(qBound(0.0f, v, 1.0f) * 255.0f)); };
    return QColor(c8(r), c8(g), c8(b));
}

QColor sampleColor(const PointColor::Point &p, int i = 0)
{
    float r, g, b;
    p.sample(i, r, g, b);
    return srgbColor(r, g, b);
}

/* Sample i as the shifts take it (Variance aside: it barely moves a sample). */
QColor shiftedColor(const PointColor::Point &p, int i = 0)
{
    float sr, sg, sb, L, C, h;
    p.sample(i, sr, sg, sb);
    PointColor::rgbLch(sr, sg, sb, L, C, h);
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
    if (hasSel()) {
        env = PointColor::envelope(pts[sel]);
        Ls = env.Lc();
        Cs = env.Cc();
        hs = env.hc;
        hueSpan = qMin(PointColor::kPi,
                       qMax(kFieldHueSpanDeg * kDegToRad,
                            env.hh + kFieldHueMarginDeg * kDegToRad));
    }
    update();
}

void PointColorWidget::setDropperIcons(const QIcon &idle, const QIcon &armedIcon)
{
    dropIdle = idle;
    dropArmed = armedIcon;
    update();
}

void PointColorWidget::setPickArmed(bool on, bool addMode)
{
    addMode = on && addMode;
    if (armed == on && armedAdd == addMode) return;
    armed = on;
    armedAdd = addMode;
    update();
}

bool PointColorWidget::canAddSample() const
{
    return hasSel() && pts[sel].extraN < PointColor::kMaxExtra;
}

/* ---- Geometry ---------------------------------------------------------------------- */

QRect PointColorWidget::dropperRect() const
{
    return QRect(kPad, (kRowH - 22) / 2, 22, 22);
}

QRect PointColorWidget::swatchRect(int i) const
{
    /* Tighten the gap rather than run under the add-sample dropper on a narrow dock. */
    const int x0 = dropperRect().right() + 10;
    int step = kSwatch + kSwatchGap;
    if (n > 1) {
        const int avail = addSampleRect().left() - 8 - x0 - kSwatch;
        step = qBound(kSwatch + 2, avail / (n - 1), step);
    }
    return QRect(x0 + i * step, (kRowH - kSwatch) / 2, kSwatch, kSwatch);
}

QRect PointColorWidget::deleteRect() const
{
    return QRect(width() - kPad - 16, (kRowH - 16) / 2, 16, 16);
}

QRect PointColorWidget::addSampleRect() const
{
    return QRect(deleteRect().left() - 8 - 22, (kRowH - 22) / 2, 22, 22);
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

/* Chroma at the top of the field: room for Sat Shift +100 (x2) on the centre and for
   every sample, with a floor so a muted sample still shows a useful spread of colour. */
float PointColorWidget::fieldCMax() const
{
    return qMax(0.12f, qMax(Cs * (1.0f + PointColor::kSatUpMax), env.Cmax) * 1.1f);
}

QPointF PointColorWidget::fieldPos(float h, float C) const
{
    const QRectF f = fieldRect();
    const float dh = PointColor::wrapPi(h - hs) / hueSpan;
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
    if (imgFor == f.size() && imgL == Ls && imgC == Cs && imgH == hs && imgSpan == hueSpan
        && imgCMax == fieldCMax())
        return;
    imgFor = f.size();
    imgL = Ls; imgC = Cs; imgH = hs; imgSpan = hueSpan; imgCMax = fieldCMax();
    const int fw = qMax(1, f.width()), fh = qMax(1, f.height());
    fieldImg = QImage(fw, fh, QImage::Format_RGB32);
    const float cMax = fieldCMax();
    for (int y = 0; y < fh; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(fieldImg.scanLine(y));
        const float C = cMax * (1.0f - (y + 0.5f) / fh);
        for (int x = 0; x < fw; ++x) {
            const float t = (x + 0.5f) / fw * 2.0f - 1.0f;
            const float h = hs + t * hueSpan;
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

    /* Droppers: the new-point one, and (with a point selected) the add-sample one at
       the right. Each is outlined while it is the armed one, and greyed when it has no
       room (the tooltips say why). */
    const bool full = n >= PointColor::kMaxPts;
    auto dropper = [&](const QRect &dr, bool isArmed, bool dim, bool plus) {
        if (isArmed) {
            p.setPen(QPen(QColor(10, 132, 255), 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(dr).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        }
        const QIcon &ic = isArmed ? dropArmed : dropIdle;
        p.save();
        if (dim && !isArmed) p.setOpacity(p.opacity() * 0.3);
        if (!ic.isNull()) ic.paint(&p, dr.adjusted(3, 3, -3, -3));
        if (plus) {
            /* A small "+" badge at the bottom right: add to, rather than add. */
            const QPointF c(dr.right() - 4.0, dr.bottom() - 4.0);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(40, 40, 40));
            p.drawEllipse(c, 4.5, 4.5);
            p.setPen(QPen(isArmed ? QColor(10, 132, 255) : QColor(220, 220, 220), 1.4));
            p.drawLine(QPointF(c.x() - 2.5, c.y()), QPointF(c.x() + 2.5, c.y()));
            p.drawLine(QPointF(c.x(), c.y() - 2.5), QPointF(c.x(), c.y() + 2.5));
        }
        p.restore();
    };
    dropper(dropperRect(), armed && !armedAdd, full, false);
    if (hasSel()) dropper(addSampleRect(), armed && armedAdd, !canAddSample(), true);

    /* Swatches -- one vertical stripe per sample -- then the delete cross for the
       selected one. */
    for (int i = 0; i < n; ++i) {
        const QRectF r = swatchRect(i);
        const QRectF in = r.adjusted(0.5, 0.5, -0.5, -0.5);
        const int m = pts[i].sampleCount();
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(in, 3, 3);
        p.setClipPath(clip);
        p.setPen(Qt::NoPen);
        for (int k = 0; k < m; ++k) {
            p.setBrush(sampleColor(pts[i], k));
            p.drawRect(QRectF(in.left() + in.width() * k / m, in.top(),
                              in.width() / m + 0.5, in.height()));
        }
        p.restore();
        p.setPen(QPen(frame, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(in, 3, 3);
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

    /* Rings: small = the samples, large = where the shifts take the centre. With
       several samples a dashed box marks the region they span -- everything inside it
       is fully selected (see MULTIPLE SAMPLES in pointcolor.h). */
    const PointColor::Point &pt = pts[sel];
    const int m = pt.sampleCount();
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
    auto dashed = [&p](const QRectF &r) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 140), 2.6));
        p.drawRect(r);
        p.setPen(QPen(Qt::white, 1.0, Qt::DashLine));
        p.drawRect(r);
    };
    const qreal sx = s.center().x() + 0.5;
    if (m > 1) {
        const float h0 = env.hueGate ? hs - env.hh : hs - hueSpan;
        const float h1 = env.hueGate ? hs + env.hh : hs + hueSpan;
        const QPointF a = fieldPos(h0, env.Cmax), z = fieldPos(h1, env.Cmin);
        dashed(QRectF(a, z).normalized().adjusted(-3, -3, 3, 3));
        dashed(QRectF(QPointF(s.left() + 2.5, stripY(env.Lmax) - 3),
                      QPointF(s.right() - 1.5, stripY(env.Lmin) + 3)));
    }
    for (int k = 0; k < m; ++k) {
        float sr, sg, sb, L, C, h;
        pt.sample(k, sr, sg, sb);
        PointColor::rgbLch(sr, sg, sb, L, C, h);
        ring(fieldPos(h, C), 3.0, 1.2);
        ring(QPointF(sx, stripY(L)), 3.0, 1.2);
    }
    ring(fieldPos(ht, Ct), 6.5, 1.6);
    ring(QPointF(sx, stripY(Lt)), 6.0, 1.6);

    /* Before | after, one stripe per sample on each side. */
    QRectF left(b.left(), b.top(), b.width() / 2.0, b.height());
    QRectF right(left.right(), b.top(), b.width() - left.width(), b.height());
    p.setPen(Qt::NoPen);
    for (int k = 0; k < m; ++k) {
        p.setBrush(sampleColor(pt, k));
        p.drawRect(QRectF(left.left() + left.width() * k / m, left.top(),
                          left.width() / m + 0.5, left.height()));
        p.setBrush(shiftedColor(pt, k));
        p.drawRect(QRectF(right.left() + right.width() * k / m, right.top(),
                          right.width() / m + 0.5, right.height()));
    }
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
        const float ht = hs + (x * 2.0f - 1.0f) * hueSpan;
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
    if (hasSel() && addSampleRect().contains(pos)) {
        if (canAddSample() || (armed && armedAdd)) emit addSampleToggled();
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
                             (hasSel() && (deleteRect().contains(pos) ||
                                           addSampleRect().contains(pos)));
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
    menu.addSeparator();
    QAction *last = menu.addAction(tr("Remove Last Added Sample"));
    QAction *clr = menu.addAction(tr("Remove Added Samples"));
    last->setEnabled(pts[sw].extraN > 0);
    clr->setEnabled(pts[sw].extraN > 0);
    QAction *hit = menu.exec(e->globalPos());
    if (hit == del) emit pointDeleted(sw);
    else if (hit == all) emit allPointsDeleted();
    else if (hit == last) emit lastSampleRemoved(sw);
    else if (hit == clr) emit addedSamplesCleared(sw);
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
                      : tr("Sample a colour as a new point: click here, then click the "
                           "image. Shift-click the image to add the sample to the "
                           "selected point instead.");
        else if (hasSel() && addSampleRect().contains(pos))
            tip = canAddSample()
                      ? tr("Add a sample to the selected point: click here, then click "
                           "the image. Its colours widen the hue, saturation and "
                           "luminance the point adjusts. Shift-click the image to keep "
                           "adding samples.")
                      : tr("This point already has %1 samples -- remove one (right-click "
                           "its swatch) to add another.").arg(PointColor::kMaxSamples);
        else if (const int sw = swatchAt(pos); sw >= 0)
            tip = pts[sw].extraN > 0
                      ? tr("Select this point (%1 samples). Right-click to delete it or "
                           "remove samples.").arg(pts[sw].sampleCount())
                      : tr("Select this point. Right-click to delete.");
        else if (hasSel() && deleteRect().contains(pos))
            tip = tr("Delete the selected point.");
        else if (hasSel() && fieldRect().contains(pos))
            tip = tr("Drag to shift hue (across) and saturation (up / down). The small "
                     "rings are the sampled colours, the large one where they are "
                     "going.");
        else if (hasSel() && stripRect().contains(pos))
            tip = tr("Drag to shift luminance.");
        else if (hasSel() && barRect().contains(pos))
            tip = tr("The sampled colours, and the colours they become.");
        if (tip.isEmpty()) QToolTip::hideText();
        else QToolTip::showText(he->globalPos(), tip, this);
        return true;
    }
    return QWidget::event(e);
}
