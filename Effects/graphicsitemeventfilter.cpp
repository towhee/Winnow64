#include "graphicsitemeventfilter.h"

GraphicsItemEventFilter::GraphicsItemEventFilter(/*QObject *parent*/)
{

}

bool GraphicsItemEventFilter::sceneEventFilter(QGraphicsItem *watched, QEvent *event)
{
    qDebug() << __PRETTY_FUNCTION__ << watched << event;
    return QGraphicsItem::sceneEventFilter(watched, event);
}

QRectF GraphicsItemEventFilter::boundingRect() const
{
    return QRectF();
}

void GraphicsItemEventFilter::paint(QPainter */*painter*/, const QStyleOptionGraphicsItem */*option*/, QWidget */*widget*/)
{

}

