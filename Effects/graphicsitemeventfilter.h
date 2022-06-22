#ifndef GRAPHICSITEMEVENTFILTER_H
#define GRAPHICSITEMEVENTFILTER_H

#include <QtWidgets>
#include "Main/global.h"

class GraphicsItemEventFilter : public QGraphicsItem
{
//    Q_OBJECT
public:
    GraphicsItemEventFilter(/*QObject *parent = nullptr*/);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

protected:
    bool sceneEventFilter(QGraphicsItem *watched, QEvent *event) override;
};

#endif // GRAPHICSITEMEVENTFILTER_H
