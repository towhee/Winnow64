#include "Develop/Properties/detailpreview.h"

#include <QPainter>
#include <QMouseEvent>

DetailPreview::DetailPreview(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(120);
    setCursor(Qt::OpenHandCursor);      // the whole surface slides the sample point
    setMouseTracking(false);
    message = tr("Pick a location in the image");
}

int DetailPreview::roiSize() const
{
    /* IMAGE pixels, and the widget paints one per LOGICAL pixel, so the ROI side is the
       widget's logical side -- no scaling anywhere in the path. Square: the shorter side
       wins, because a rect wider than it is tall would show a letterboxed strip. */
    return qMax(1, qMin(width(), height()));
}

void DetailPreview::setImage(const QImage &img)
{
    roi = img;
    update();
}

void DetailPreview::setMessage(const QString &text)
{
    if (text == message) return;
    message = text;
    if (roi.isNull()) update();          // only visible while there is nothing to show
}

void DetailPreview::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QRect r = rect();

    /* The well, matching the curve plot's (curveeditor.cpp): darker than the panel so the
       patch reads as a window onto the image rather than as another control. */
    p.fillRect(r, QColor(20, 20, 20));

    if (roi.isNull()) {
        p.setPen(QColor(150, 150, 150));
        p.drawText(r.adjusted(8, 8, -8, -8),
                   Qt::AlignCenter | Qt::TextWordWrap, message);
    }
    else {
        /* Centred, unscaled: one image pixel per logical pixel. If the ROI is smaller
           than the widget (a small image, or the window was just resized and the next
           render has not landed) it simply sits in the middle rather than stretching --
           stretching would be a lie about what the sharpening looks like, which is the
           one thing this widget exists to tell the truth about. */
        const QSize sz = roi.size() / roi.devicePixelRatio();
        const QPoint at(r.x() + (r.width()  - sz.width())  / 2,
                        r.y() + (r.height() - sz.height()) / 2);
        p.drawImage(at, roi);
    }

    /* A hairline frame, so the well has an edge against the panel background. */
    p.setPen(QColor(70, 70, 70));
    p.drawRect(r.adjusted(0, 0, -1, -1));
}

void DetailPreview::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) { QWidget::mousePressEvent(event); return; }
    dragging = true;
    dragFrom = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
}

void DetailPreview::mouseMoveEvent(QMouseEvent *event)
{
    if (!dragging) { QWidget::mouseMoveEvent(event); return; }
    const QPoint d = event->pos() - dragFrom;
    if (d.isNull()) { event->accept(); return; }
    dragFrom = event->pos();
    /* Drag the IMAGE, not the window: pulling right shows what is to the LEFT, so the
       sample point moves against the cursor -- the same convention as panning the loupe.
       One logical pixel of drag is one image pixel, since that is the preview's scale. */
    emit pointNudged(-d.x(), -d.y());
    event->accept();
}

void DetailPreview::mouseReleaseEvent(QMouseEvent *event)
{
    if (!dragging) { QWidget::mouseReleaseEvent(event); return; }
    dragging = false;
    setCursor(Qt::OpenHandCursor);
    event->accept();
}

void DetailPreview::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();                     // swallow: an unhandled one floats the dock
}
