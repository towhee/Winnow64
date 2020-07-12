#include "embel.h"

Embel::Embel(EmbelView *ev, EmbelProperties *p)
{
    this->ev = ev;
    this->p = p;

    connect(ev, &EmbelView::updateEmbel, this, &Embel::build);
}

void Embel::test()
{
    ev->setFitZoom();
//    qDebug() << __FUNCTION__
//             << "ev->sceneRect() =" << ev->sceneRect()
//             << "ev->imAspect = " << ev->imAspect;
//    int n = bItems.size();
//    if (n) {
//        ev->scene->removeItem(bItems[n-1]);
//        bItems.remove(n-1);
//    }
}

void Embel::build()
{
    /*
    QGraphicsRectItem *bItem = new QGraphicsRectItem;
    QImage tile(":/images/icon16/tile.png");
    QBrush tileBrush(tile);
    bItem->setBrush(tileBrush);
    bItem->setZValue(-1);
    */

    createBorders();
    borderImageCoordinates();
    ev->setSceneRect(0, 0, w, h);
    addBordersToScene();
    translateImage();
    ev->resetFitZoom();
//    ev->setSceneRect(ev->scene->itemsBoundingRect());
    diagnostic();
}

//void Embel::readModel()
//{
//    // Borders
//    QModelIndex BordersIdx = p->model->index(0, MH_Borders, QModelIndex());
//    // iterate borders
//    for (int border = 0; border < p->model->rowCount(BordersIdx), ++border) {
//        QModelIndex borderIdx = p->model->index(border, 1, BordersIdx);
//        for (int borderItem = 0)
//        b[b].t = borderIdx.data
//    }
//}

void Embel::borderImageCoordinates()
{
    // canvas coordinates: pixels
    QPoint tl(0,0), br(0,0);
    // long side in pixels (ls)
    if (ev->imAspect > 1) ls = p->f.horizontalFit;
    else ls = p->f.verticalFit;
    // image long side before subtract border margins
    int ils = ls;
    // iterate borders to determine image width
    for (int i = 0; i < b.size(); ++i) {
        // top left coord for this border
        b[i].tl = tl;
        // update image width based on subtracting this border margins
        b[i].l = static_cast<int>(p->b[i].left * ls / 100);
        b[i].r = static_cast<int>(p->b[i].right * ls / 100);
        b[i].t = static_cast<int>(p->b[i].top * ls / 100);
        ils -= (b[i].l + b[i].r);
        // top left for next border or the image
        tl = tl + QPoint(b[i].l, b[i].t);
    }
    if (ev->imAspect > 1) {
        // canvas width
        w = ls;
        // image dimensions
        image.w = ils;
        image.h = static_cast<int>(image.w / ev->imAspect);
    }
    else {
        //canvas height
        h = ls;
        // image dimensions
        image.h = ils;
        image.w = static_cast<int>(image.h * ev->imAspect);
    }
    image.tl = tl;
    image.x = tl.x();
    image.y = tl.y();
    image.br = tl + QPoint(image.w, image.h);
    // now that we have the image coord from the aspect ratio finish calc border coord
    br = image.br;
    for (int i = b.size() - 1; i >= 0; --i) {
        b[i].b = static_cast<int>(p->b[i].bottom * ls / 100);
        br += QPoint(b[i].r, b[i].b);
        b[i].br = br;
        b[i].w = b[i].br.x() - b[i].tl.x();
        b[i].h = b[i].br.y() - b[i].tl.y();
        b[i].x = b[i].tl.x();
        b[i].y = b[i].tl.y();
    }
    // canvas 2nd dimension, depending on aspect
    if (ev->imAspect > 1) h = br.y();
    else w = br.x();
}

void Embel::createBorders()
{
    for (int i = 0; i < bItems.size(); ++i) {
        ev->scene->removeItem(bItems[i]);
    }
    bItems.clear();
    b.clear();
    for (int i = 0; i < p->b.size(); ++i) {
        Border x;
        b << x;
        QGraphicsRectItem *item = new QGraphicsRectItem;
        bItems << item;
    }
}

void Embel::addBordersToScene()
{
    for (int i = 0; i < b.size(); ++i) {
        bItems[i]->setRect(0, 0, b[i].w, b[i].h);
        QColor color;
        color.setNamedColor(p->b[i].color);
        qDebug() << __FUNCTION__ << color << p->b[i].color;
        bItems[i]->setBrush(color);
        ev->scene->addItem(bItems[i]);
        bItems[i]->moveBy(b[i].x, b[i].y);
//            delete item;
    }
}

void Embel::translateImage()
{
    // scale the image to fit inside the borders
    QPixmap pm = ev->pmItem->pixmap().scaledToWidth(image.w);
    // add the image to the scene
    ev->pmItem->setPixmap(pm);
    // move the image to center in the borders
    qDebug() << __FUNCTION__ << "moveBy" << image.x << image.y;
//    ev->pmItem->moveBy(image.x, image.y);   // 336,232
    ev->pmItem->setPos(image.tl);
    ev->pmItem->setZValue(10);
}

void Embel::diagnostic()
{
//    qDebug().noquote()
//            << "rect() =" << p->rect()
//            << "sceneRect() =" << p->sceneRect()
//            << "scene->itemsBoundingRect() =" << p->scene->itemsBoundingRect();

    qDebug().noquote()
            << "Canvas                      "
            << "w =" << QString::number(w).leftJustified(5)
            << "h =" << QString::number(h).leftJustified(5)
               ;
    for (int i = 0; i < b.size(); ++i) {
        qDebug().noquote()
            << "Border" << i
            << "x =" << QString::number(b[i].x).leftJustified(5)
            << "y =" << QString::number(b[i].y).leftJustified(5)
            << "w =" << QString::number(b[i].w).leftJustified(5)
            << "h =" << QString::number(b[i].h).leftJustified(5)
            << "l =" << QString::number(b[i].l).leftJustified(5)
            << "r =" << QString::number(b[i].r).leftJustified(5)
            << "t =" << QString::number(b[i].t).leftJustified(5)
            << "b =" << QString::number(b[i].b).leftJustified(5)
            << "tl.x =" << QString::number(b[i].tl.x()).leftJustified(5)
            << "tl.y =" << QString::number(b[i].tl.y()).leftJustified(5)
            << "br.x =" << QString::number(b[i].br.x()).leftJustified(5)
            << "br.y =" << QString::number(b[i].br.y()).leftJustified(5)
            ;
    }
    qDebug().noquote()
            << "Image   "
            << "x =" << QString::number(image.x).leftJustified(5)
            << "y =" << QString::number(image.y).leftJustified(5)
            << "w =" << QString::number(image.w).leftJustified(5)
            << "h =" << QString::number(image.h).leftJustified(5)
            << "                                       "
            << "tl.x =" << QString::number(image.tl.x()).leftJustified(5)
            << "tl.y =" << QString::number(image.tl.y()).leftJustified(5)
            << "br.x =" << QString::number(image.br.x()).leftJustified(5)
            << "br.y =" << QString::number(image.br.y()).leftJustified(5)
//            << "tl ="  << image.tl
//            << "br =" << image.br
            ;

}
