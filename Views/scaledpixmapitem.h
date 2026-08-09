#ifndef SCALEDPIXMAPITEM_H
#define SCALEDPIXMAPITEM_H

#include <QGraphicsPixmapItem>
#include <QPainter>
#include <QPixmap>
#include <QSize>
#include <QStyleOptionGraphicsItem>

/*
    A QGraphicsPixmapItem whose LOGICAL size can be larger than the pixmap it holds: it
    keeps presenting the full image dimensions to the scene and stretches the smaller
    pixmap into them at paint time.

    WHY. The Develop interactive tick renders a screen-resolution PROXY. Putting that on
    the loupe used to mean upscaling it to the full image dimensions first -- on a 50 MP
    file a ~200 MB QImage::scaled plus an equally large QPixmap::fromImage, on EVERY tick
    of a slider or mask drag -- purely so the item's coordinate system did not move.
    That coordinate system is load-bearing: ~37 places (every mask overlay, every hit
    test, zoom, fit and pan calculation) read image geometry off pmItem->boundingRect()
    and map it with pmItem->mapToScene().

    Keeping the logical size fixed means none of that has to change. Item coordinates ARE
    image coordinates, exactly as before; only paint() knows the pixmap is smaller, and
    there QPainter stretches just the visible region straight to the final resolution.

    setPixmap() is deliberately SHADOWED (not overridden -- the base version is not
    virtual) so that setting a plain pixmap always clears a stale logical size. Callers
    hold this type, not QGraphicsPixmapItem*, so the invariant cannot be bypassed through
    the pointer the app actually uses.
*/
class ScaledPixmapItem : public QGraphicsPixmapItem
{
public:
    using QGraphicsPixmapItem::QGraphicsPixmapItem;

    /* Set the pixmap and, optionally, the size it should present to the scene. An invalid
       or matching `logical` means "the pixmap IS the image" -- ordinary behaviour. */
    void setPixmapScaled(const QPixmap &pm, QSize logical = QSize())
    {
        prepareGeometryChange();
        logicalSize = (logical.isValid() && !logical.isEmpty()) ? logical : QSize();
        QGraphicsPixmapItem::setPixmap(pm);
    }

    /* Plain setPixmap: the pixmap is the image, so any previous logical size is stale. */
    void setPixmap(const QPixmap &pm) { setPixmapScaled(pm); }

    /* The size this item presents to the scene, in image pixels. */
    QSize displaySize() const
    { return logicalSize.isValid() ? logicalSize : pixmap().size(); }

    /* True while the held pixmap is smaller than what the item presents. */
    bool isScaled() const
    { return logicalSize.isValid() && logicalSize != pixmap().size(); }

    QRectF boundingRect() const override
    {
        if (!isScaled()) return QGraphicsPixmapItem::boundingRect();
        return QRectF(offset(), QSizeF(logicalSize));
    }

    QPainterPath shape() const override
    {
        if (!isScaled()) return QGraphicsPixmapItem::shape();
        QPainterPath p;
        p.addRect(boundingRect());
        return p;
    }

    bool contains(const QPointF &point) const override
    {
        if (!isScaled()) return QGraphicsPixmapItem::contains(point);
        return boundingRect().contains(point);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override
    {
        if (!isScaled()) { QGraphicsPixmapItem::paint(painter, option, widget); return; }
        /* One stretched blit. The view transform is already on the painter, so QPainter
           resolves proxy -> screen in a single step -- there is no full-resolution
           intermediate anywhere. transformationMode() picks fast (mid-drag) vs smooth
           (settled), mirroring what the old explicit upscale chose. */
        painter->setRenderHint(QPainter::SmoothPixmapTransform,
                               transformationMode() == Qt::SmoothTransformation);
        painter->drawPixmap(boundingRect(), pixmap(), QRectF(pixmap().rect()));
    }

private:
    QSize logicalSize;      // invalid => the pixmap's own size
};

#endif // SCALEDPIXMAPITEM_H
