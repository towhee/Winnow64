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
    iv->scene->clear();
    return;

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
    // remove texts
    for (int i = 0; i < tItems.size(); ++i) {
        iv->scene->removeItem(tItems[i]);
        delete tItems[i];
    }
    tItems.clear();
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
    createTexts();
    borderImageCoordinates();
    iv->setSceneRect(0, 0, w, h);
    addBordersToScene();
    addImageToScene();
    addTextsToScene();
//    QGraphicsTextItem *tt = new QGraphicsTextItem;
//    tt->setPlainText("Test");
//    iv->scene->addItem(tt);
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
        b[i].tc = QPoint(b[i].x + b[i].w / 2, b[i].y);
        b[i].tr = QPoint(b[i].x + b[i].w, b[i].y);
        b[i].cl = QPoint(b[i].x, b[i].y + b[i].h / 2);
        b[i].cc = QPoint(b[i].x + b[i].w / 2, b[i].y + b[i].h / 2);
        b[i].cr = QPoint(b[i].x + b[i].w, b[i].y + b[i].h / 2);
        b[i].bl = QPoint(b[i].x, b[i].y + b[i].h);
        b[i].bc = QPoint(b[i].x + b[i].w / 2, b[i].y + b[i].h);
    }
    // canvas 2nd dimension, depending on aspect
    if (iv->imAspect > 1) h = br.y();
    else w = br.x();
    // other anchor points on image
    image.tc = QPoint(image.x + image.w / 2, image.y);
    image.tr = QPoint(image.x + image.w, image.y);
    image.cl = QPoint(image.x, image.y + image.h / 2);
    image.cc = QPoint(image.x + image.w / 2, image.y + image.h / 2);
    image.cr = QPoint(image.x + image.w, image.y + image.h / 2);
    image.bl = QPoint(image.x, image.y + image.h);
    image.bc = QPoint(image.x + image.w / 2, image.y + image.h);
}

QPoint Embel::canvasCoord(QString object, QString container, double x, double y)
{
    qDebug() << __FUNCTION__ << "object=" << object
             << "container =" << container
             << "x =" << x << "y =" << y;

    if (object == "Image") {
        int x0 = image.x + static_cast<int>(x / 100 * image.w);
        int y0 = image.y + static_cast<int>(y / 100 * image.h);
        return QPoint(x0, y0);
    }
    // not Image so must be a Border object
    else {
        for (int i = 0; i < p->b.size(); ++i) {
            // position and size of container
            if (p->b[i].name == object) {
                qDebug() << __FUNCTION__ << object << container << x << y;
                int x1, y1, w, h;
                if (container == "Top") {
                    x1 = b[i].x;
                    y1 = b[i].y;
                    w = b[i].w;
                    h = b[i].t;
                    qDebug() << __FUNCTION__ << "Top";
                }
                else if (container == "Left") {
                    x1 = b[i].x;
                    y1 = b[i].y + b[i].t;
                    w = b[i].l;
                    h = b[i].h - b[i].t - b[i].b;
                }
                else if (container == "Right") {
                    x1 = b[i].x + b[i].w - b[i].r;
                    y1 = b[i].y + b[i].t;
                    w = b[i].r;
                    h = b[i].h - b[i].t - b[i].b;
                }
                // must be Bottom
                else {
                    x1 = b[i].x;
                    y1 = b[i].y + b[i].h - b[i].b;
                    w = b[i].w;
                    h = b[i].b;
                }
                int x0 = x1 + static_cast<int>(x / 100 * w);
                int y0 = y1 + static_cast<int>(y / 100 * h);
                qDebug() << __FUNCTION__
                         << x1 << y1 << w << h << x0 << y0;
                return QPoint(x0, y0);
            }
        }
    }
    // error - object not found
    return QPoint(0,0);
}

QPoint Embel::anchorPointOffset(QString anchorPoint, int w, int h)
{
    qDebug() << __FUNCTION__ << anchorPoint << w << h;
    int w2 = static_cast<int>(w/2);
    int h2 = static_cast<int>(h/2);
    if (anchorPoint == "Top Left") return QPoint(0, 0);
    else if (anchorPoint == "Top Center") return QPoint(w2, 0);
    else if (anchorPoint == "Top Right") return QPoint(w, 0);
    else if (anchorPoint == "Middle Left") return QPoint(0, h2);
    else if (anchorPoint == "Middle Center") return QPoint(w2, h2);
    else if (anchorPoint == "Middle Right") return QPoint(w, h2);
    else if (anchorPoint == "Bottom Left") return QPoint(0, h);
    else if (anchorPoint == "Bottom Center") return QPoint(w/2, h);
    else if (anchorPoint == "Bottom Right") return QPoint(w, h);
    // error anchor point not found
    else return QPoint(0, 0);
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
    for (int i = 0; i < b.size(); ++i) {
        bItems[i]->setRect(0, 0, b[i].w, b[i].h);
        QColor color;
        QPen pen;
        pen.setWidth(static_cast<int>(p->b[i].outlineWidth * ls / 100));
        color.setNamedColor(p->b[i].outlineColor);
        pen.setColor(color);
        bItems[i]->setPen(pen);
        color.setNamedColor(p->b[i].color);
        // tile or color background
        if (p->b[i].tile.isEmpty()) {
            bItems[i]->setBrush(color);
        }
        else {
            QImage imTile;
            imTile.loadFromData(p->b[i].tile);
            bItems[i]->setBrush(imTile);
        }
        bItems[i]->setOpacity(p->b[i].opacity/100);
        iv->scene->addItem(bItems[i]);
        bItems[i]->setPos(b[i].x, b[i].y);
    }
}

void Embel::createTexts()
{
    for (int i = 0; i < p->t.size(); ++i) {
        Text x;
//        t << x;
        QGraphicsTextItem *item = new QGraphicsTextItem;
        tItems << item;
    }
}

void Embel::addTextsToScene()
{
    for (int i = 0; i < p->t.size(); ++i) {
        tItems[i]->setPlainText(p->t[i].text);
        QFont font(p->t[i].font);
        int fontSize = static_cast<int>(p->t[i].size / 100 * ls);
        font.setPixelSize(fontSize);
        font.setItalic(p->t[i].isItalic);
        font.setBold(p->t[i].isBold);
        tItems[i]->setFont(font);
        tItems[i]->setDefaultTextColor(QColor(p->t[i].color));
        double opacity = static_cast<double>(p->t[i].opacity)/100;
        tItems[i]->setOpacity(opacity);
        tItems[i]->setZValue(20);
        iv->scene->addItem(tItems[i]);
        // position text
        qDebug() << __FUNCTION__ << "Getting canvas coord for Text" << i
                 << "p->t[i].anchorObject =" << p->t[i].anchorObject
                 << "p->t[i].anchorContainer =" << p->t[i].anchorContainer;
        QPoint canvas = canvasCoord(p->t[i].anchorObject, p->t[i].anchorContainer,
                                    p->t[i].x, p->t[i].y);
        QPoint offset = anchorPointOffset(p->t[i].anchorPoint,
                                    static_cast<int>(tItems[i]->boundingRect().width()),
                                    static_cast<int>(tItems[i]->boundingRect().height()));
        tItems[i]->setPos(canvas - offset);
        qDebug() << __FUNCTION__ << i << tItems[i] << p->t[i].text
                 << tItems[i]->boundingRect()
                 << canvas << offset;
    }
}

void Embel::addImageToScene()
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
            ;

}
