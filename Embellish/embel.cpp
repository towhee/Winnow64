#include "embel.h"

Embel::Embel(EmbelView *ev, EmbelProperties *p)
{
    this->ev = ev;
    this->p = p;


}

void Embel::test()
{
    qDebug() << __FUNCTION__
             << "ev->sceneRect() =" << ev->sceneRect()
             << "ev->imAspect = " << ev->imAspect;
    resizeItems();
}

void Embel::resizeItems()
{
    QGraphicsRectItem *bItem = new QGraphicsRectItem;
    QImage tile(":/images/icon16/tile.png");
    QBrush tileBrush(tile);
    bItem->setBrush(tileBrush);
    bItem->setZValue(-1);
    ev->scene->addItem(bItem);

    if (ev->imAspect > 1) {
        ls = p->f.horizontalFit;            // long side
        int w = ls;
        sR.setWidth(w);
        b.x = 0;
        b.y = 0;
        b.l = ls * 0.02;
        b.r = ls * 0.02;
        b.t = ls * 0.02;
        b.b = ls * 0.04;

        image.w = ls - b.l - b.r;
        image.h = image.w / ev->imAspect;

        b.w = ls;
        b.h = b.t + b.b + image.h;

        ev->setSceneRect(0, 0, b.w, b.h);
        bItem->setRect(ev->sceneRect());

        qDebug() << __FUNCTION__ << b.w << b.h << image.w << image.h;
        QPixmap pm = ev->pmItem->pixmap().scaledToWidth(image.w);
        ev->pmItem->setPixmap(pm);
        ev->pmItem->moveBy(b.l, b.t);
    }
    else {
        ls = p->f.verticalFit;
    }

}
