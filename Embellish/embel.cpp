#include "embel.h"

Embel::Embel(ImageView *iv, EmbelProperties *p)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->iv = iv;
    this->p = p;
}

void Embel::exportImage()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__;
    iv->scene->clearSelection();
    iv->scene->setSceneRect(iv->scene->itemsBoundingRect());                          // Re-shrink the scene to it's bounding contents
    QImage image(iv->scene->sceneRect().size().toSize(), QImage::Format_ARGB32);  // Create the image with the exact size of the shrunk scene
    image.fill(Qt::transparent);                                              // Start all pixels transparent

    QPainter painter(&image);
    iv->scene->render(&painter);
    image.save("d:/pictures/test/out/test.tif");
}

void Embel::test()
{
    iv->scene->clear();
    return;
}

void Embel::doNotEmbellish()
{
    clear();
    iv->loadImage(iv->currentImagePath);
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
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    iv->resetFitZoom();
//    diagnostic();
}

void Embel::fitAspect(double aspect, Hole &size)
{
    if (aspect > 1) size.h = static_cast<int>(qRound(size.w / aspect));
    else size.w = static_cast<int>(size.h * aspect);
}

void Embel::borderImageCoordinates()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    //
    int hFit = p->f.horizontalFitPx;
    int vFit = p->f.verticalFitPx;
    hFit > vFit ? ls = hFit : ls = vFit;
    // the hole is the inside of the borders = the image area
    hole.w = hFit;
    hole.h = vFit;
    hole.wb = 0;
    hole.hb = 0;
    // determine frame hole by iterating borders, starting with hole = fit dimensions
    for (int i = 0; i < b.size(); ++i) {
        int horBorders = static_cast<int>(qRound((p->b[i].left + p->b[i].right) * ls / 100));
        int verBorders = static_cast<int>(qRound((p->b[i].top + p->b[i].bottom) * ls / 100));
        hole.w -= horBorders;
        hole.h -= verBorders;
        hole.wb += horBorders;
        hole.hb += verBorders;
    }
    fitAspect(iv->imAspect, hole);
    /*
    qDebug() << __FUNCTION__
             << "hole =" << a
             << "hole.w =" << hole.w
             << "hole.h =" << hole.h
             << "hole.wb =" << hole.wb
             << "hole.hb =" << hole.hb
             << "w =" << hole.w + hole.wb
             << "h =" << hole.h + hole.hb
             ;
//                 */
    // work back from hole dimensions to get scene w (width) and h (height)
    w = hole.w + hole.wb;
    h = hole.h + hole.hb;
    // the hole = image dimensions
    image.w = hole.w;
    image.h = hole.h;

    // canvas coordinates: pixels
    QPoint tl(0,0), br(0,0);
    // iterate borders
    for (int i = 0; i < b.size(); ++i) {
        // top left coord for this border
        b[i].tl = tl;
        // update image width based on subtracting the border margins
        b[i].l = static_cast<int>(qRound(p->b[i].left * ls / 100));
        b[i].r = static_cast<int>(qRound(p->b[i].right * ls / 100));
        b[i].t = static_cast<int>(qRound(p->b[i].top * ls / 100));
        b[i].b = static_cast<int>(qRound(p->b[i].bottom * ls / 100));
        // top left for next border or the image
        tl = tl + QPoint(b[i].l, b[i].t);
    }
    image.tl = tl;
    image.x = tl.x();
    image.y = tl.y();
    image.br = tl + QPoint(image.w, image.h);
    // now that we have the image coord from the aspect ratio finish calc border coord
    br = image.br;
    for (int i = b.size() - 1; i >= 0; --i) {
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
    // other anchor points on image
    image.tc = QPoint(image.x + image.w / 2, image.y);
    image.tr = QPoint(image.x + image.w, image.y);
    image.cl = QPoint(image.x, image.y + image.h / 2);
    image.cc = QPoint(image.x + image.w / 2, image.y + image.h / 2);
    image.cr = QPoint(image.x + image.w, image.y + image.h / 2);
    image.bl = QPoint(image.x, image.y + image.h);
    image.bc = QPoint(image.x + image.w / 2, image.y + image.h);
}

QPoint Embel::textCanvasCoord(int n)
{
/*

*/
    int x0 = 0, y0 = 0;
    // put text in the Image object area
    if (p->t[n].anchorObject == "Image") {
        x0 = image.x + static_cast<int>(p->t[n].x / 100 * image.w);
        y0 = image.y + static_cast<int>(p->t[n].y / 100 * image.h);
        return QPoint(x0, y0);
    }
    // not Image so must be a Border object
    else {
        for (int i = 0; i < p->b.size(); ++i) {
            // position and size of container
            if (p->b[i].name == p->t[n].anchorObject) {
                int x1, y1, w, h;
                if (p->t[n].anchorContainer == "Top") {
                    x1 = b[i].x;
                    y1 = b[i].y;
                    w = b[i].w;
                    h = b[i].t;
                }
                else if (p->t[n].anchorContainer  == "Left") {
                    x1 = b[i].x;
                    y1 = b[i].y + b[i].t;
                    w = b[i].l;
                    h = b[i].h - b[i].t - b[i].b;
                }
                else if (p->t[n].anchorContainer  == "Right") {
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
                x0 = x1 + static_cast<int>(p->t[n].x / 100 * w);
                y0 = y1 + static_cast<int>(p->t[n].y / 100 * h);
            }
        }
        // if align to a border corner
        /*
        qDebug() << __FUNCTION__
                 << "p->t[n].alignToCorner =" << p->t[n].alignToCorner
                 << "x0 =" << x0 << "y0 =" << y0;
//                 */
        if (p->t[n].alignToCorner != "Do not align") {
            // border number
            int bId = p->t[n].alignTo_BorderId;
            // is alignment horizontal or vertical
            bool isHorAlignment = true;
            if (p->t[n].anchorContainer == "Left" || p->t[n].anchorContainer == "Right")
                isHorAlignment = false;
            /*
            qDebug() << __FUNCTION__
                     << "n =" << n
                     << "bId =" << bId
                     << "p->t[n].alignToCorner =" << p->t[n].alignToCorner
                     << "isHorAlignment =" << isHorAlignment
                     << "p->t[n].alignTo_CornerId =" << p->t[n].alignTo_CornerId
                        ;
//                        */
            switch (p->t[n].alignTo_CornerId) {
            // TopLeft
            case 0:
                if (isHorAlignment) x0 = b[bId].tl.x();
                    else y0 = b[bId].tl.y();
                    break;
            // TopRight
            case 1:
                if (isHorAlignment) x0 = b[bId].tr.x();
                else y0 = b[bId].tr.y();
                break;
            // BottomLeft
            case 2:
                if (isHorAlignment) x0 = b[bId].bl.x();
                else y0 = b[bId].bl.y();
//                qDebug() << __FUNCTION__
//                         << "b[bId].bl =" << b[bId].bl;
                break;
            // BottomRight
            case 3:
                if (isHorAlignment) x0 = b[bId].br.x();
                else y0 = b[bId].br.y();
                break;
            }
        }
    }
//    qDebug() << __FUNCTION__ << "QPoint(x0,y0) =" << QPoint(x0,y0);
    return QPoint(x0,y0);
}

QPoint Embel::anchorPointOffset(QString anchorPoint, int w, int h)
{
//    qDebug() << __FUNCTION__ << anchorPoint << w << h;
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
        pen.setWidth(0);
        pen.setColor(Qt::transparent);
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

        // graphics effects
        if (p->b[i].style != "No style" && p->b[i].style != "") {
            GraphicsEffect *effect = new GraphicsEffect();
            effect->set(p->styleMap[p->b[i].style],
                    p->globalLightDirection,
                    0,  /* rotation */
                    bItems[i]->boundingRect());
            bItems[i]->setGraphicsEffect(effect);
        }

        /*
        qDebug() << __FUNCTION__
                 << i
                 << "bItems[i]->boundingRect() ="
                 << bItems[i]->boundingRect();
//                 */
    }
}

void Embel::createTexts()
{
    for (int i = 0; i < p->t.size(); ++i) {
        QGraphicsTextItem *item = new QGraphicsTextItem;
        tItems << item;
    }
}

void Embel::addTextsToScene()
{
    for (int i = 0; i < p->t.size(); ++i) {
        // if a text entry
        if (p->t[i].source == "Text") {
            tItems[i]->setPlainText(p->t[i].text);
        }
        // if a metadata template used to build the text string
        else {
            tItems[i]->setPlainText(p->metaString(p->t[i].metadataTemplate));
        }
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

        // position text
        /*
        qDebug() << __FUNCTION__ << "Getting canvas coord for Text" << i
                 << "p->t[i].anchorObject =" << p->t[i].anchorObject
                 << "p->t[i].anchorContainer =" << p->t[i].anchorContainer
                 << "p->t[i].x =" << p->t[i].x
                 << "p->t[i].y =" << p->t[i].y
                    ;
//                    */
        int w = static_cast<int>(tItems[i]->boundingRect().width());
        int h = static_cast<int>(tItems[i]->boundingRect().height());
//        qDebug() << __FUNCTION__ << "tItems[i]->boundingRect() =" << tItems[i]->boundingRect();
        QPoint canvas = textCanvasCoord(i);
        QPoint offset = anchorPointOffset(p->t[i].anchorPoint, w, h);
        tItems[i]->setTransformOriginPoint(offset);
        tItems[i]->setPos(canvas - offset);

        // if style then rotate in GraphicsEffect, else rotate text here
        double rotation = p->t[i].rotation;

        // graphics effects
        if (p->t[i].style != "No style" && p->t[i].style != "") {
            GraphicsEffect *effect = new GraphicsEffect();
            effect->set(p->styleMap[p->t[i].style],
                    p->globalLightDirection,
                    rotation,
                    tItems[i]->boundingRect());
            tItems[i]->setGraphicsEffect(effect);
        }
        else tItems[i]->setRotation(rotation);

        iv->scene->addItem(tItems[i]);
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

    // graphics effects
    if (p->image.style != "No style" && p->image.style != "") {
        GraphicsEffect *effect = new GraphicsEffect();
        effect->set(p->styleMap[p->image.style],
                p->globalLightDirection,
                0,
                pm.rect());
        iv->pmItem->setGraphicsEffect(effect);
    }

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
