#include "embel.h"

Embel::Embel(ImageView *ev, EmbelProperties *p)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->iv = ev;
    this->p = p;
}

void Embel::test()
{
    iv->scene->removeItem(bItems[0]);
    delete bItems[0];
    bItems.clear();
    b.clear();

    QGraphicsRectItem *x = new QGraphicsRectItem;
    x->setRect(0,0,w,h);
    x->setPen(QPen(Qt::red));
    x->setBrush(QBrush(Qt::blue));
    iv->scene->addItem(x);

    iv->scene->removeItem(x);
//    delete x;

//    QGraphicsRectItem *x = new QGraphicsRectItem;
    x->setRect(0,0,w,h);
    x->setPen(QPen(Qt::green));
    x->setBrush(QBrush(Qt::yellow));
    iv->scene->addItem(x);

    return;
//    iv->scene->clear();
//    iv->pmItem->setPixmap(iv->displayPixmap);
    qDebug() << __FUNCTION__ << iv->displayPixmap;
    doNotEmbellish();
}

void Embel::doNotEmbellish()
{
    clear();
    w = iv->displayPixmap.width();
    h = iv->displayPixmap.height();
    qDebug() << __FUNCTION__ << w << h;
    iv->setSceneRect(0, 0, w, h);
    iv->pmItem->setPos(0, 0);
    iv->pmItem->setPixmap(iv->displayPixmap);
    iv->resetFitZoom();
}

void Embel::clear()
{
//    iv->scene->clear();
//    iv->scene->addItem(iv->pmItem);
    // remove borders
    for (int i = 0; i < bItems.size(); ++i) {
        iv->scene->removeItem(bItems[i]);
        delete bItems[i];
    }
    bItems.clear();
    b.clear();
}

void Embel::build()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    /*
    QGraphicsRectItem *bItem = new QGraphicsRectItem;
    QImage tile(":/images/icon16/tile.png");
    QBrush tileBrush(tile);
    bItem->setBrush(tileBrush);
    bItem->setZValue(-1);
    */
    qDebug() << __FUNCTION__ << "p->templateId =" << p->templateId;
    if (p->templateId == 0) {
        doNotEmbellish();
        return;
    }
    clear();
    createBorders();
    borderImageCoordinates();
    iv->setSceneRect(0, 0, w, h);
    addBordersToScene();
    translateImage();
    iv->resetFitZoom();
    diagnostic();
}

void Embel::borderImageCoordinates()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // canvas coordinates: pixels
    QPoint tl(0,0), br(0,0);
    // long side in pixels (ls)
    if (iv->imAspect >= 1) ls = p->f.horizontalFit;
    else ls = p->f.verticalFit;
    // image long side before subtract border margins
    int ils = ls;
    // iterate borders to determine image width
    for (int i = 0; i < b.size(); ++i) {
        // top left coord for this border
        b[i].tl = tl;
        // update image width based on subtracting this border margins
        b[i].l = static_cast<int>(qRound(p->b[i].left * ls / 100));
        b[i].r = static_cast<int>(qRound(p->b[i].right * ls / 100));
        b[i].t = static_cast<int>(qRound(p->b[i].top * ls / 100));
        ils -= (b[i].l + b[i].r);
        // top left for next border or the image
        tl = tl + QPoint(b[i].l, b[i].t);
    }
    if (iv->imAspect > 1) {
        // canvas width
        w = ls;
        // image dimensions
        image.w = ils;
        image.h = static_cast<int>(qRound(image.w / iv->imAspect));
    }
    else {
        //canvas height
        h = ls;
        // image dimensions
        image.h = ils;
        image.w = static_cast<int>(image.h * iv->imAspect);
    }
    image.tl = tl;
    image.x = tl.x();
    image.y = tl.y();
    image.br = tl + QPoint(image.w, image.h);
    // now that we have the image coord from the aspect ratio finish calc border coord
    br = image.br;
    for (int i = b.size() - 1; i >= 0; --i) {
        b[i].b = static_cast<int>(qRound(p->b[i].bottom * ls / 100));
        br += QPoint(b[i].r, b[i].b);
        b[i].br = br;
        b[i].w = b[i].br.x() - b[i].tl.x();
        b[i].h = b[i].br.y() - b[i].tl.y();
        b[i].x = b[i].tl.x();
        b[i].y = b[i].tl.y();
    }
    // canvas 2nd dimension, depending on aspect
    if (iv->imAspect > 1) h = br.y();
    else w = br.x();
}

void Embel::createBorders()
{
/*
Create the QGraphicsRectItem for each border.  Overlapping rectangles and the image create the
illusion of border margins using the painters algorithm.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for (int i = 0; i < p->b.size(); ++i) {
        Border x;
        b << x;
        QGraphicsRectItem *item = new QGraphicsRectItem;
        bItems << item;
    }
}

void Embel::addBordersToScene()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    QMessageBox::information(iv, "addBordersToScene", "Continue");
    for (int i = 0; i < b.size(); ++i) {
        bItems[i]->setRect(0, 0, b[i].w, b[i].h);
        QColor color;
        QPen pen;
        pen.setWidth(static_cast<int>(p->b[i].outlineWidth * ls / 100));
        color.setNamedColor(p->b[i].outlineColor);
        pen.setColor(color);
        bItems[i]->setPen(pen);
        color.setNamedColor(p->b[i].color);
        bItems[i]->setBrush(color);
        iv->scene->addItem(bItems[i]);
        bItems[i]->setPos(b[i].x, b[i].y);
//        QString s = "Border " + QString::number(i);
//        QMessageBox::information(iv, s, "Continue");
    }
}

void Embel::translateImage()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // scale the image to fit inside the borders
    QPixmap pm = iv->pmItem->pixmap().scaledToWidth(image.w);
    // add the image to the scene
    iv->pmItem->setPixmap(pm);
    // move the image to center in the borders
    iv->pmItem->setPos(image.tl);
    iv->pmItem->setZValue(10);
}

void Embel::diagnostic()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
