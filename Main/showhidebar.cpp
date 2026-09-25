#include "Main/showhidebar.h"

#include "Main/global.h"

#include <QMouseEvent>
#include <QPainter>

/*
    ShowHideBar (see showhidebar.h): a window-edge strip with one solid triangle.
*/

namespace {
/* Across the short axis. Wide enough to hit without aiming, narrow enough that three of
   them cost the photo almost nothing. */
constexpr int kThickness = 14;
/* The triangle's extent across the bar, leaving a margin either side so the glyph reads
   as sitting IN the strip rather than filling it. */
constexpr int kGlyph = 7;
}

int ShowHideBar::thickness()
{
    return kThickness;
}

ShowHideBar::ShowHideBar(Edge e, QWidget *parent) : QWidget(parent), edge(e)
{
    if (G::isLogger) G::log("ShowHideBar::ShowHideBar");
    setCursor(Qt::PointingHandCursor);
    /* Fixed across the short axis, free along the long one: the bar spans its whole edge
       the way Lightroom's does, so there is no "where exactly do I click" to learn. */
    if (edge == Bottom || edge == Top) {
        setFixedHeight(kThickness);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    else {
        setFixedWidth(kThickness);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
}

void ShowHideBar::setExpanded(bool e)
{
    if (e == expanded) return;
    expanded = e;
    update();
}

QPolygonF ShowHideBar::triangle(const QRectF &r) const
{
    /* Which way it points: the direction the panels travel when clicked. Expanded, they
       are about to leave towards their own edge; collapsed, they are about to come back
       towards the photo. So the glyph is the edge's own direction when expanded and its
       opposite when collapsed. */
    enum Dir { PointLeft, PointRight, PointUp, PointDown } dir;
    switch (edge) {
    case Left:   dir = expanded ? PointLeft  : PointRight; break;
    case Right:  dir = expanded ? PointRight : PointLeft;  break;
    case Top:    dir = expanded ? PointUp    : PointDown;  break;
    default:     dir = expanded ? PointDown  : PointUp;    break;
    }

    const qreal cx = r.center().x(), cy = r.center().y();
    const qreal h = kGlyph / 2.0;        // half the base
    const qreal d = kGlyph / 2.0;        // tip's distance from centre along the axis

    QPolygonF p;
    switch (dir) {
    case PointLeft:  p << QPointF(cx - d, cy) << QPointF(cx + d, cy - h) << QPointF(cx + d, cy + h); break;
    case PointRight: p << QPointF(cx + d, cy) << QPointF(cx - d, cy - h) << QPointF(cx - d, cy + h); break;
    case PointUp:    p << QPointF(cx, cy - d) << QPointF(cx - h, cy + d) << QPointF(cx + h, cy + d); break;
    case PointDown:  p << QPointF(cx, cy + d) << QPointF(cx - h, cy - d) << QPointF(cx + h, cy - d); break;
    }
    return p;
}

void ShowHideBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    /* The window's own background, so the strip reads as part of the frame the panels
       sit in rather than as a thin panel of its own. */
    p.fillRect(rect(), G::backgroundColor);

    /* Dim like every other passive glyph in the chrome, and only fully lit under the
       cursor: three of these are on screen at all times, and at full strength they would
       compete with the photo they exist to uncover. */
    QColor ink = G::textColor;
    ink.setAlphaF(hovered ? 1.0 : G::iconOpacity);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawPolygon(triangle(QRectF(rect())));
}

void ShowHideBar::enterEvent(QEnterEvent *)
{
    hovered = true;
    update();
}

void ShowHideBar::leaveEvent(QEvent *)
{
    hovered = false;
    update();
}

void ShowHideBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    e->accept();
    emit clicked();
}
