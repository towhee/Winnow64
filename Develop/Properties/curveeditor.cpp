#include "Develop/Properties/curveeditor.h"
#include "Develop/develop.h"
#include "Main/global.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QtMath>
#include <algorithm>

namespace {
    constexpr int kMargin   = CurveEditor::kPlotMargin;   // border around the plot
    constexpr int kHitPx    = 7;      // grab radius for a control point
    constexpr int kPtR      = 4;      // control point radius
    constexpr int kSamples  = 128;    // polyline resolution across the plot
    /* Full-height vertical drag = this much slider travel. A band spans the plot, so a
       drag from the bottom to the top of the plot sweeps the whole -100..100 range. */
    constexpr double kBandRange = 200.0;

    /* Per-channel curve colour: white for the RGB composite, the channel's own hue
       otherwise, matching the histogram's fills. */
    QColor curveColor(int channel)
    {
        switch (channel) {
        case 1:  return QColor(225, 90, 90);
        case 2:  return QColor(90, 210, 90);
        case 3:  return QColor(110, 140, 240);
        default: return QColor(230, 230, 230);
        }
    }
}

CurveEditor::CurveEditor(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("CurveEditor::CurveEditor");
    hist.clear();
    setMouseTracking(true);                     // hover highlight for points / bands
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_OpaquePaintEvent);      // we fill the whole rect ourselves
}

QSize CurveEditor::sizeHint() const
{
    return QSize(200, 200);
}

void CurveEditor::setMode(Mode m)
{
    if (curMode == m) return;
    curMode = m;
    dragPt = dragBand = hoverPt = hoverBand = -1;
    /* Say what the drag does. Neither gesture is guessable from a plot alone, and the
       shaded band / point cursor only tells you WHERE, not what it will change. */
    setToolTip(m == Point
                   ? "Drag a point to move it, click the curve to add one, double-click "
                     "a point to remove it. Double-click the background to reset."
                   : "Drag up or down inside a band to move its Basic tone slider "
                     "(blacks, shadows, highlights, whites). Double-click a band to "
                     "reset it.");
    setCursor(Qt::ArrowCursor);
    update();
}

void CurveEditor::setChannel(int c)
{
    c = qBound(0, c, ToneCurve::kChannels - 1);
    if (curChannel == c) return;
    curChannel = c;
    dragPt = hoverPt = -1;
    update();
}

void CurveEditor::setParams(const EditParams &p)
{
    prm = p;
    update();               // no signal: this is a programmatic sync, not a user edit
}

void CurveEditor::setScopeData(const ScopeData &d)
{
    std::memcpy(hist.hist, d.hist, sizeof(hist.hist));
    hasHist = true;
    update();
}

void CurveEditor::clearScopeData()
{
    hist.clear();
    hasHist = false;
    update();
}

/* ---- geometry ---------------------------------------------------------------------- */

QRect CurveEditor::plotRect() const
{
    return rect().adjusted(kMargin, kMargin, -kMargin, -kMargin);
}

QPointF CurveEditor::toPlot(double x, double y) const
{
    const QRect r = plotRect();
    return QPointF(r.left() + qBound(0.0, x, 1.0) * (r.width() - 1),
                   r.bottom() - qBound(0.0, y, 1.0) * (r.height() - 1));
}

QPointF CurveEditor::fromPlot(const QPointF &pt) const
{
    const QRect r = plotRect();
    const double w = r.width() - 1, h = r.height() - 1;
    const double x = w > 0 ? (pt.x() - r.left()) / w : 0.0;
    const double y = h > 0 ? (r.bottom() - pt.y()) / h : 0.0;
    return QPointF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0));
}

int CurveEditor::pointAt(const QPoint &pos) const
{
    int best = -1;
    double bestD = kHitPx + 1;
    for (int i = 0; i < pointCount(); ++i) {
        const QPointF p = toPlot(px()[i], py()[i]);
        const double d = QLineF(p, QPointF(pos)).length();
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void CurveEditor::bandSplits(double s[3]) const
{
    s[0] = qBound(0.0, static_cast<double>(prm.toneShadowCenter), 1.0);
    s[1] = qBound(s[0], static_cast<double>(prm.toneCrossover), 1.0);
    s[2] = qBound(s[1], static_cast<double>(prm.toneHighlightCenter), 1.0);
}

int CurveEditor::bandAt(const QPoint &pos) const
{
    if (!plotRect().contains(pos)) return -1;
    double s[3];
    bandSplits(s);
    const double x = fromPlot(QPointF(pos)).x();
    if (x < s[0]) return 0;             // blacks
    if (x < s[1]) return 1;             // shadows
    if (x < s[2]) return 2;             // highlights
    return 3;                           // whites
}

float *CurveEditor::bandParam(int band)
{
    switch (band) {
    case 0:  return &prm.blacks;
    case 1:  return &prm.shadows;
    case 2:  return &prm.highlights;
    case 3:  return &prm.whites;
    default: return nullptr;
    }
}

QString CurveEditor::bandName(int band)
{
    switch (band) {
    case 0:  return "Blacks";
    case 1:  return "Shadows";
    case 2:  return "Highlights";
    default: return "Whites";
    }
}

/* ---- curve sampling ------------------------------------------------------- */

QPolygonF CurveEditor::curvePolyline() const
{
    QPolygonF poly;
    poly.reserve(kSamples);
    if (curMode == Parametric) {
        float ys[kSamples];
        Develop::ParametricCurve(prm, ys, kSamples);
        for (int i = 0; i < kSamples; ++i)
            poly << toPlot(static_cast<double>(i) / (kSamples - 1), ys[i]);
        return poly;
    }
    ToneCurve::Spline sp;
    sp.build(px(), py(), pointCount());
    for (int i = 0; i < kSamples; ++i) {
        const double x = static_cast<double>(i) / (kSamples - 1);
        poly << toPlot(x, qBound(0.0f, sp.eval(static_cast<float>(x)), 1.0f));
    }
    return poly;
}

void CurveEditor::insertPointAt(double x)
{
    int &n = prm.curveN[curChannel];
    if (n >= ToneCurve::kMaxPts) return;
    float *xs = px(), *ys = py();
    const float nx = static_cast<float>(qBound(0.0, x, 1.0));
    /* Never stack two points on one x: the segment slope would be undefined. */
    for (int i = 0; i < n; ++i)
        if (std::fabs(xs[i] - nx) < ToneCurve::kMinGap) return;

    /* The new point lands ON the current curve, so adding one changes nothing until it is
       dragged -- the user is placing an anchor, not an edit. */
    ToneCurve::Spline sp;
    sp.build(xs, ys, n);
    const float ny = qBound(0.0f, sp.eval(nx), 1.0f);

    int at = 0;
    while (at < n && xs[at] < nx) ++at;
    for (int i = n; i > at; --i) { xs[i] = xs[i - 1]; ys[i] = ys[i - 1]; }
    xs[at] = nx; ys[at] = ny;
    ++n;
    dragPt = at;
}

void CurveEditor::resetChannel()
{
    ToneCurve::setIdentity(prm.curveN[curChannel], px(), py());
    dragPt = hoverPt = -1;
}

/* ---- mouse ---------------------------------------------------------------- */

void CurveEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) { QWidget::mousePressEvent(event); return; }

    if (curMode == Parametric) {
        dragBand = bandAt(event->pos());
        if (dragBand < 0) return;
        const float *f = bandParam(dragBand);
        pressY = event->pos().y();
        pressValue = f ? *f : 0.0;
        update();
        return;
    }

    dragPt = pointAt(event->pos());
    if (dragPt < 0) {
        if (!plotRect().contains(event->pos())) return;
        insertPointAt(fromPlot(QPointF(event->pos())).x());
        if (dragPt < 0) return;
        update();
        emit curveChanged();
        return;
    }
    update();
}

void CurveEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (curMode == Parametric) {
        if (dragBand < 0) {
            const int b = bandAt(event->pos());
            if (b != hoverBand) {
                hoverBand = b;
                setCursor(b >= 0 ? Qt::SizeVerCursor : Qt::ArrowCursor);
                update();
            }
            return;
        }
        /* Up brightens. Offset from the PRESS, so a coalesced move cannot drift. */
        const QRect r = plotRect();
        const double perPx = r.height() > 1 ? kBandRange / (r.height() - 1) : 0.0;
        const double v = qBound(-100.0, pressValue + (pressY - event->pos().y()) * perPx,
                                100.0);
        float *f = bandParam(dragBand);
        if (!f || *f == static_cast<float>(v)) return;
        *f = static_cast<float>(v);
        update();
        emit parametricChanged(dragBand, v);
        return;
    }

    if (dragPt < 0) {
        const int p = pointAt(event->pos());
        if (p != hoverPt) {
            hoverPt = p;
            setCursor(p >= 0 ? Qt::SizeAllCursor : Qt::CrossCursor);
            update();
        }
        return;
    }

    const QPointF v = fromPlot(QPointF(event->pos()));
    float *xs = px(), *ys = py();
    const int n = pointCount();
    /* Endpoints are pinned in x (0 and 1) but free in y -- the black-/white-point move.
       Interior points stay strictly between their neighbours. */
    if (dragPt > 0 && dragPt < n - 1) {
        const double lo = xs[dragPt - 1] + ToneCurve::kMinGap;
        const double hi = xs[dragPt + 1] - ToneCurve::kMinGap;
        xs[dragPt] = static_cast<float>(qBound(lo, v.x(), hi));
    }
    ys[dragPt] = static_cast<float>(v.y());
    update();
    emit curveChanged();
}

void CurveEditor::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    if (dragBand >= 0) { dragBand = -1; update(); emit parametricCommitted(); return; }
    if (dragPt >= 0)   { dragPt = -1;   update(); emit curveCommitted(); }
}

void CurveEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    /* Always consume: an unhandled double click floats / redocks the Develop dock. */
    if (event->button() != Qt::LeftButton) { event->accept(); return; }

    if (curMode == Parametric) {
        /* Reset just this band's Basic slider, the counterpart of double-clicking the
           slider itself. */
        const int b = bandAt(event->pos());
        if (b < 0) { event->accept(); return; }
        float *f = bandParam(b);
        if (f) *f = 0.0f;
        dragBand = -1;
        update();
        emit parametricChanged(b, 0.0);
        emit parametricCommitted();
        event->accept();
        return;
    }

    const int p = pointAt(event->pos());
    const int n = pointCount();
    if (p > 0 && p < n - 1) {                   // delete an interior point
        float *xs = px(), *ys = py();
        for (int i = p; i < n - 1; ++i) { xs[i] = xs[i + 1]; ys[i] = ys[i + 1]; }
        --prm.curveN[curChannel];
        dragPt = hoverPt = -1;
    }
    else {                                  // background / endpoint: reset the channel
        resetChannel();
    }
    update();
    emit curveChanged();
    emit curveCommitted();
    event->accept();
}

void CurveEditor::leaveEvent(QEvent *event)
{
    if (hoverPt >= 0 || hoverBand >= 0) { hoverPt = hoverBand = -1; update(); }
    QWidget::leaveEvent(event);
}

/* ---- paint ---------------------------------------------------------------- */

void CurveEditor::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 20));
    const QRect r = plotRect();
    if (r.width() < 8 || r.height() < 8) return;

    /* Histogram behind the plot: the neutral (per-bin min of R/G/B) silhouette only. The
       plot is the foreground here, so this stays a dim backdrop rather than the scope's
       full additive rendering. */
    if (hasHist) {
        quint32 peak = 1;
        for (int c = 0; c < 3; ++c)
            for (int v = 1; v < 255; ++v)
                if (hist.hist[c][v] > peak) peak = hist.hist[c][v];
        QPainterPath path;
        path.moveTo(r.left(), r.bottom());
        for (int v = 0; v < 256; ++v) {
            const quint32 n = qMin(hist.hist[0][v],
                                   qMin(hist.hist[1][v], hist.hist[2][v]));
            const double x = r.left() + (v / 255.0) * (r.width() - 1);
            const double y = r.bottom() - qMin(1.0, n / double(peak)) * (r.height() - 1);
            path.lineTo(x, y);
        }
        path.lineTo(r.right(), r.bottom());
        path.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 28));
        p.setRenderHint(QPainter::Antialiasing, true);
        p.drawPath(path);
    }

    /* Parametric band shading + splits, so the four draggable regions are visible rather
       than something the user has to know about. */
    if (curMode == Parametric) {
        double s[3];
        bandSplits(s);
        const double edge[5] = {0.0, s[0], s[1], s[2], 1.0};
        for (int b = 0; b < 4; ++b) {
            const int x0 = qRound(toPlot(edge[b], 0).x());
            const int x1 = qRound(toPlot(edge[b + 1], 0).x());
            if (x1 <= x0) continue;
            const int a = (b == hoverBand || b == dragBand) ? 26 : ((b % 2) ? 12 : 0);
            if (a) p.fillRect(QRect(x0, r.top(), x1 - x0, r.height()),
                              QColor(140, 200, 200, a));
        }
        p.setPen(QPen(QColor(90, 90, 90), 1, Qt::DashLine));
        for (int i = 0; i < 3; ++i) {
            const int x = qRound(toPlot(s[i], 0).x());
            p.drawLine(x, r.top(), x, r.bottom());
        }
    }

    /* Grid + the identity diagonal. */
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QColor(52, 52, 52));
    for (int i = 1; i < 4; ++i) {
        const int x = qRound(toPlot(i / 4.0, 0).x());
        const int y = qRound(toPlot(0, i / 4.0).y());
        p.drawLine(x, r.top(), x, r.bottom());
        p.drawLine(r.left(), y, r.right(), y);
    }
    p.setPen(QColor(70, 70, 70));
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));
    p.drawLine(toPlot(0, 0), toPlot(1, 1));

    /* The curve. */
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(curveColor(curMode == Parametric ? 0 : curChannel), 1.6));
    p.drawPolyline(curvePolyline());

    if (curMode != Point) return;

    /* Control points. The dragged one is filled, the hovered one ringed. */
    for (int i = 0; i < pointCount(); ++i) {
        const QPointF c = toPlot(px()[i], py()[i]);
        const bool active = (i == dragPt);
        const bool hot    = active || (i == hoverPt);
        p.setPen(QPen(QColor(20, 20, 20), 1));
        p.setBrush(active ? QColor(120, 200, 195)
                          : (hot ? QColor(230, 230, 230) : QColor(170, 170, 170)));
        p.drawEllipse(c, kPtR, kPtR);
    }
}
