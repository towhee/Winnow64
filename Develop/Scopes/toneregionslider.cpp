#include "Develop/Scopes/toneregionslider.h"
#include "Main/global.h"
#include <QPainter>
#include <QMouseEvent>

namespace {
    constexpr int kTrackH  = 9;     // gradient track height
    constexpr int kHandleH = 8;     // handle triangle height below the track
    constexpr int kHandleW = 9;     // handle triangle width
    constexpr int kHitPx   = 8;     // grab radius
}

ToneRegionSlider::ToneRegionSlider(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("ToneRegionSlider::ToneRegionSlider");
    setMouseTracking(false);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(sizeHint().height());
}

QSize ToneRegionSlider::sizeHint() const
{
    return QSize(180, kTrackH + kHandleH + 2);
}

void ToneRegionSlider::setPositions(double shadow, double crossover, double highlight)
{
    pos[0] = shadow; pos[1] = crossover; pos[2] = highlight;
    clampOrder();
    update();           // no valueChanged: this is a programmatic sync, not a user edit
}

int ToneRegionSlider::xOf(double p) const
{
    /* Map 0..1 across the full width so the handles line up with the histogram's x-axis. */
    return qRound(qBound(0.0, p, 1.0) * (width() - 1));
}

double ToneRegionSlider::pOf(int x) const
{
    const int w = width() - 1;
    return w > 0 ? qBound(0.0, static_cast<double>(x) / w, 1.0) : 0.0;
}

int ToneRegionSlider::handleAt(int x) const
{
    int best = -1, bestDist = kHitPx + 1;
    for (int i = 0; i < 3; ++i) {
        const int d = qAbs(x - xOf(pos[i]));
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

void ToneRegionSlider::clampOrder()
{
    pos[0] = qBound(0.0, pos[0], 1.0);
    pos[1] = qBound(0.0, pos[1], 1.0);
    pos[2] = qBound(0.0, pos[2], 1.0);
    if (pos[1] < pos[0] + kMinGap) pos[1] = pos[0] + kMinGap;
    if (pos[2] < pos[1] + kMinGap) pos[2] = pos[1] + kMinGap;
    /* If pushed past the right edge, walk back left so all three keep their gaps. */
    if (pos[2] > 1.0) { pos[2] = 1.0; if (pos[1] > pos[2] - kMinGap) pos[1] = pos[2] - kMinGap;
                                      if (pos[0] > pos[1] - kMinGap) pos[0] = pos[1] - kMinGap; }
}

void ToneRegionSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) { QWidget::mousePressEvent(event); return; }
    dragHandle = handleAt(event->pos().x());
    if (dragHandle >= 0) {
        pos[dragHandle] = pOf(event->pos().x());
        clampOrder();
        update();
        emit valueChanged(pos[0], pos[1], pos[2]);
    }
}

void ToneRegionSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (dragHandle < 0) return;
    pos[dragHandle] = pOf(event->pos().x());
    clampOrder();
    update();
    emit valueChanged(pos[0], pos[1], pos[2]);
}

void ToneRegionSlider::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    dragHandle = -1;
}

void ToneRegionSlider::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) { QWidget::mouseDoubleClickEvent(event); return; }
    /* Reset the split handles to their defaults; emit so the params update and re-render. */
    dragHandle = -1;
    pos[0] = 0.25; pos[1] = 0.50; pos[2] = 0.75;
    update();
    emit valueChanged(pos[0], pos[1], pos[2]);
}

void ToneRegionSlider::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 20));

    /* Black -> white gradient track at the top. */
    const QRect track(0, 0, width(), kTrackH);
    QLinearGradient g(track.topLeft(), track.topRight());
    g.setColorAt(0.0, QColor(0, 0, 0));
    g.setColorAt(1.0, QColor(235, 235, 235));
    p.fillRect(track, g);
    p.setPen(QColor(70, 70, 70));
    p.drawRect(track.adjusted(0, 0, -1, 0));

    /* Handles: small house/triangle markers hanging below the track, like Lightroom. */
    p.setRenderHint(QPainter::Antialiasing, true);
    const int top = kTrackH + 1;
    for (int i = 0; i < 3; ++i) {
        const int x = qBound(kHandleW / 2, xOf(pos[i]), width() - 1 - kHandleW / 2);
        QPolygon tri;
        tri << QPoint(x, top)
            << QPoint(x - kHandleW / 2, top + kHandleH)
            << QPoint(x + kHandleW / 2, top + kHandleH);
        const bool active = (i == dragHandle);
        p.setPen(QPen(QColor(30, 30, 30), 1));
        p.setBrush(active ? QColor(120, 200, 195) : QColor(190, 190, 190));
        p.drawPolygon(tri);
    }
}
