#include "embel.h"
//#include "Effects/effect.h"       // temp to test

/*

Embel applies all the settings in EmbelProperties to the ImageView QGraphicScene (scene), which
contains just pmItem (a pixmap of the image) in the base "Do not embellish" mode.  All
embellishments are defined in the PropertyEditor embelProperties.

Export converts the scene, with any borders, texts and graphics, into a QImage.

*/

Embel::Embel(QGraphicsScene *scene, QGraphicsPixmapItem *pmItem,
             EmbelProperties *p, ImageCache *imCache, QString src,
             QObject *)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->scene = scene;
    this->pmItem = pmItem;
    this->p = p;
    this->imCache = imCache;
    this->src = src;
    qDebug() << __FUNCTION__ << "src =" << src;
    flashItem = new QGraphicsRectItem;
    itemEventFilter = new GraphicsItemEventFilter;
//    scene->addItem(itemEventFilter);
}

Embel::~Embel()
{
    clear();
}

void Embel::exportImage()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__;
    scene->clearSelection();
    scene->setSceneRect(scene->itemsBoundingRect());                          // Re-shrink the scene to it's bounding contents
    QImage image(scene->sceneRect().size().toSize(), QImage::Format_ARGB32);  // Create the image with the exact size of the shrunk scene
    image.fill(Qt::transparent);                                              // Start all pixels transparent

    QPainter painter(&image);
    scene->render(&painter);
    image.save("d:/pictures/test/out/test.tif");
}

void Embel::test()
{
//    /* transparentEdgeMap
    QPixmap srcPixmap(tItems[0]->boundingRect().size().toSize());
    QPainter tmpPainter(&srcPixmap);
    tItems[0]->document()->drawContents(&tmpPainter);
    tmpPainter.end();
    QImage img = srcPixmap.toImage();
    QImage edgeMap;
    Effects effect;
//    effect.transparentEdgeMap(img, edgeMap);
    img.save("D:/Pictures/Temp/effect/_transparentEdgeMap.tif");
//    */

    /* Test
    Effects effect;
    QImage img("D:/Pictures/Temp/effect/_border.tif");
    effect.test(img);
    img.save("D:/Pictures/Temp/effect/_borderShaded.tif");
//    */

    /* Convolve example
    int n = 9;
    double matrix[81] =
    {
        9, 9, 9, 9, 9, 9, 9, 9, 9,
        9, 9, 9, 9, 9, 9, 9, 9, 9,
        9, 9, 1, 1, 1, 1, 1, 9, 9,
        9, 9, 1, 1, 1, 1, 1, 9, 9,
        9, 9, 1, 1, 1, 1, 1, 9, 9,
        9, 9, 1, 1, 1, 1, 1, 9, 9,
        9, 9, 1, 1, 1, 1, 1, 9, 9,
        9, 9, 9, 9, 9, 9, 9, 9, 9,
        9, 9, 9, 9, 9, 9, 9, 9, 9
    };
//    double matrix[81] =
//    {
//        9, 9, 9, 9, 9, 9, 9, 9, 9,
//        9, 9, 9, 9, 9, 9, 9, 9, 9,
//        9, 9, 9, 9, 9, 9, 9, 9, 9,
//        9, 9, 9, 9, 9, 9, 9, 9, 9,
//        9, 9, 9, 9, 9, 9, 9, 9, 9,
//        9, 9, 9, 9, 9, 9, 9, 9, 9,
//        9, 9, 9, 9, 9, 9, 9, 9, 9,
//        9, 9, 9, 9, 9, 9, 9, 9, 9,
//        9, 9, 9, 9, 9, 9, 9, 9, 9
//    };
//    double matrix[25] =
//    {
//        0.003765, 0.015019, 0.023792, 0.015019, 0.003765,
//        0.015019, 0.059912, 0.094907, 0.059912, 0.015019,
//        0.023792, 0.094907, 0.150342, 0.094907, 0.023792,
//        0.015019, 0.059912, 0.094907, 0.059912, 0.015019,
//        0.003765, 0.015019, 0.023792, 0.015019, 0.003765
//    };
    QImage img("D:/Pictures/Temp/effect/goose.jpg");
    Effects effect;
    QImage gooseConvolved = effect.convolve(img, n, matrix);
    gooseConvolved.save("D:/Pictures/Temp/effect/gooseconvolved.tif");
//    */

    /* raise example
    QImage img("D:/Pictures/Temp/effect/goose.jpg");
    Effects effect;
    effect.raise(img, 20, 0.0, 0, false);
    img.save("D:/Pictures/Temp/effect/gooseraise.tif");
    return;
//    */

    /* blur example
    QImage img("D:/Pictures/Temp/effect/text.tif");
    Effects effect;
    effect.blur(img, 1);
    img.save("d:/pictures/temp/effect/textblurred.tif");
//    */

    /* brighten test
    Effects effect;
    QRgb p = qRgba(110, 150, 65, 255);
    effect.brightenPixel(p, 1.2);
//    */
}

void Embel::doNotEmbellish()
{
    clear();
}

void Embel::clear()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // remove borders
    for (int i = bItems.size() - 1; i >= 0; --i) {
        removeBorder(i);
    }
    bItems.clear();
    b.clear();

    // remove texts
    for (int i = tItems.size() - 1; i >= 0; --i) {
        removeText(i);
    }
    tItems.clear();

    // remove graphics
    for (int i = gItems.size() - 1; i >= 0; --i) {
        removeGraphic(i);
    }
    graphicPixmaps.clear();
    gItems.clear();

    // remove flashItem
    if (scene->items().contains(flashItem)) scene->removeItem(flashItem);
}

void Embel::build(QString path, QString src)
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    QString s = "path = " + path + " src = " + src;
    Utilities::log(__FUNCTION__, s);
    #endif
    }
    /*
    qDebug() << __FUNCTION__ << QTime::currentTime()
             << "path =" << path
             << "src =" << src;
//             */

    QString msg = "src = " + src + "  path = " + path;
//    Utilities::log(__FUNCTION__, msg);
    /*
    qDebug() << __FUNCTION__
             << "src =" << src
             << "path =" << path;
//           */

    QElapsedTimer t;
    t.start();

    if (G::mode != "Loupe") return;

    if (p->templateId == 0) {
        doNotEmbellish();
        return;
    }
    /* file path for the current image (req'd to update styles applied to pmItem).  If it is
       blank then source is main Winnow program, otherwise called from EmbelExport, which will
       not have loaded the folder, and therefore no datamodel.  */
    isRemote = false;
    if (path != "") {
        isRemote = true;
        fPath = path;
    }
//    qDebug() << __FUNCTION__ << "isRemote =" << isRemote << "path =" << path << "src =" << src;
    clear();
    createBorders();
    createTexts();
    createGraphics();
    borderImageCoordinates();
    scene->setSceneRect(0, 0, w, h);
    addBordersToScene();
    addImageToScene();
    addTextsToScene();
    addGraphicsToScene();
    addFlashToScene();

    emit done();    // call imageView->resetFitZoom() (not req'd when exporting)
//    qDebug() << __FUNCTION__
//             << "Elapsed time (ms) =" << QString("%L1").arg(t.elapsed());
//    diagnostic();
}

void Embel::fitAspect(double aspect, Hole &size)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (aspect > 1) size.h = static_cast<int>(qRound(size.w / aspect));
    else size.w = static_cast<int>(size.h * aspect);
}

void Embel::borderImageCoordinates()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    //
    int hFit = p->horizontalFitPx;
    int vFit = p->verticalFitPx;
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
    imageAspect = static_cast<double>(pmItem->pixmap().width()) / pmItem->pixmap().height();
    fitAspect(imageAspect, hole);
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
    /*
    qDebug() << __FUNCTION__
             << "w =" << w << "h =" << h
             << "image.w =" << image.w << "image.h =" << image.h
                ;
//    */
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

QPoint Embel::canvasCoord(double x, double y,
                          QString anchorObject,
                          QString anchorContainer,
                          bool alignWithImageEdge)
{
/*
    Returns a QPoint canvas coordinate for the anchor point of a text or graphic.  The input
    x,y are in the container coordinates (0-100%).
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    /*
    qDebug() << __FUNCTION__
             << "x =" << x
             << "y =" << y
             << "anchorObject =" << anchorObject
             << "anchorContainer =" << anchorContainer
             << "align =" << align
             << "alignTo_BorderId =" << alignTo_BorderId
                ;
//    */
    // range check
    if (p->b.size() > b.size()) {
        qDebug() << __FUNCTION__ << "$$$$$$$$$$$$$$$  p->b.size() > b.size())   $$$$$$$$$$$$$";
    }
    int x0 = 0, y0 = 0;
    // put text in the Image object area
    if (anchorObject == "Image" || b.size() == 0 || p->b.size() > b.size()) {
        x0 = image.x + static_cast<int>(x / 100 * image.w);
        y0 = image.y + static_cast<int>(y / 100 * image.h);
        /*
        qDebug() << __FUNCTION__
                 << "image.x =" << image.x
                 << "image.y =" << image.y
                 << "image.w =" << image.w
                 << "image.h =" << image.h
                 << "x =" << x
                 << "y =" << y
                 << "x0 =" << x0
                 << "y0 =" << y0
                    ;
//                    */
        return QPoint(x0, y0);
    }
    // not Image so must be a Border object unless no border objects
    else {
        for (int i = 0; i < p->b.size(); ++i) {
            int x1, y1, w, h;
            // position and size of container
            if (p->b[i].name == anchorObject) {
                if (anchorContainer == "Top") {
                    x1 = b[i].x;
                    y1 = b[i].y;
                    w = b[i].w;
                    h = b[i].t;
                }
                else if (anchorContainer  == "Left") {
                    x1 = b[i].x;
                    y1 = b[i].y + b[i].t;
                    w = b[i].l;
                    h = b[i].h - b[i].t - b[i].b;
                }
                else if (anchorContainer  == "Right") {
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
                // container offset + container coordinate
                x0 = static_cast<int>(x1 + x / 100 * w);
                y0 = static_cast<int>(y1 + y / 100 * h);
            }
        }
    }

    if (alignWithImageEdge) {
        if (anchorContainer == "Top" || anchorContainer == "Bottom") {
            if (x < 50) x0 = image.tl.x();
            else x0 = image.tr.x();
        }
        if (anchorContainer == "Left" || anchorContainer == "Right") {
            if (y < 50) y0 = image.tl.y();
            else y0 = image.bl.y();
        }
    }

    /*
    qDebug() << __FUNCTION__
             << "x =" << x
             << "y =" << y
             << "x0 =" << x0
             << "y0 =" << y0
             << "anchorObject =" << anchorObject
             << "anchorContainer =" << anchorContainer
             << "alignWithImageEdge =" << alignWithImageEdge
                ;
//    */

    return QPoint(x0, y0);
}

QPoint Embel::anchorPointOffset(QString anchorPoint, int w, int h, double rotation)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
//    qDebug() << __FUNCTION__ << anchorPoint << w << h;
    int w2 = static_cast<int>(w/2);
    int h2 = static_cast<int>(h/2);
    anchorPoint = anchorPointRotationEquivalent(anchorPoint, rotation);
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

QString Embel::anchorPointRotationEquivalent(QString anchorPoint, double rotation)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (anchorPoint == "Middle Center") return anchorPoint;
    int rot = static_cast<int>(rotation);
    // East
    if (rot == 0) {
        qDebug() << __FUNCTION__ << "East";
        return anchorPoint;
    }
    // north
    if ((rot >= -135 && rot <= -45 ) ||
        (rot >=  225 && rot <=  315)) {
        qDebug() << __FUNCTION__ << "North";
        if      (anchorPoint == "Top Left") return "Top Right";
        else if (anchorPoint == "Top Center") return "Middle Right";
        else if (anchorPoint == "Top Right") return "Bottom Right";
        else if (anchorPoint == "Middle Left") return "Top Center";
        else if (anchorPoint == "Middle Right") return "Bottom Center";
        else if (anchorPoint == "Bottom Left") return "Top Left";
        else if (anchorPoint == "Bottom Center") return "Middle Left";
        else if (anchorPoint == "Bottom Right") return "Bottom Left";
    }
    // west
    if ((rot >= -225 && rot <= -135) ||
        (rot >=  135 && rot <=  225)) {
        qDebug() << __FUNCTION__ << "West";
        if      (anchorPoint == "Top Left") return "Bottom Right";
        else if (anchorPoint == "Top Center") return "Bottom Center";
        else if (anchorPoint == "Top Right") return "Bottom Left";
        else if (anchorPoint == "Middle Left") return "Middle Right";
        else if (anchorPoint == "Middle Right") return "Middle Left";
        else if (anchorPoint == "Bottom Left") return "Top Right";
        else if (anchorPoint == "Bottom Center") return "Top Center";
        else if (anchorPoint == "Bottom Right") return "Top Left";
    }
    // south
    if ((rot >= -315 && rot <= -225) ||
        (rot >=   45 && rot <=  315)) {
        qDebug() << __FUNCTION__ << "South";
        if      (anchorPoint == "Top Left") return "Bottom Left";
        else if (anchorPoint == "Top Center") return "Middle Left";
        else if (anchorPoint == "Top Right") return "Top Left";
        else if (anchorPoint == "Middle Left") return "Bottom Center";
        else if (anchorPoint == "Middle Right") return "Top Center";
        else if (anchorPoint == "Bottom Left") return "Bottom Right";
        else if (anchorPoint == "Bottom Center") return "Middle Right";
        else if (anchorPoint == "Bottom Right") return "Top Right";
    }
    // east
    return anchorPoint;
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
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    for (int i = 0; i < p->b.size(); ++i) {
        Border x;
        b << x;
        QGraphicsRectItem *item = new QGraphicsRectItem;
        item->setToolTip(p->b[i].name);
//        item->setToolTip("Border" + QString::number(i));
        bItems << item;
    }
}

void Embel::createTexts()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    for (int i = 0; i < p->t.size(); ++i) {
        QGraphicsTextItem *item = new QGraphicsTextItem;
        item->setToolTip(p->t[i].name);
        tItems << item;
    }
}

void Embel::createGraphics()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    for (int i = 0; i < p->g.size(); ++i) {
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem;
        item->setToolTip(p->g[i].name);
        graphicPixmaps << QPixmap(p->g[i].filePath);
        item->setPixmap(graphicPixmaps.at(i));
        gItems << item;
    }
}

void Embel::addBordersToScene()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    for (int i = 0; i < b.size(); ++i) {
        updateBorder(i);
        scene->addItem(bItems[i]);
//        bItems[i]->installSceneEventFilter(itemEventFilter);
        /*
        qDebug() << __FUNCTION__
                 << i
                 << "bItems[i]->boundingRect() ="
                 << bItems[i]->boundingRect();
//                 */
    }
}

void Embel::addTextsToScene()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    for (int i = 0; i < p->t.size(); ++i) {
        updateText(i);
        scene->addItem(tItems[i]);
//        tItems[i]->installSceneEventFilter(itemEventFilter);
    }
}

void Embel::addGraphicsToScene()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    for (int i = 0; i < p->g.size(); ++i) {
        updateGraphic(i);
        scene->addItem(gItems[i]);
    }
}

void Embel::addImageToScene()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    // scale the image to fit inside the borders
    QPixmap pm;
    if (imCache->imCache.contains(fPath))
        pm = QPixmap::fromImage(imCache->imCache.value(fPath)).scaledToWidth(image.w);
    else
        pm = pmItem->pixmap().scaledToWidth(image.w);
    // add the image to the scene
    pmItem->setPixmap(pm);
    // move the image to center in the borders
    pmItem->setPos(image.tl);
    pmItem->setZValue(ZImage);
    pmItem->setToolTip("Image");
//    pmItem->installSceneEventFilter(itemEventFilter);

    updateImage();
}

void Embel::addFlashToScene()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    scene->addItem(flashItem);
}

void Embel::updateBorder(int i)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (G::mode != "Loupe") return;
    // index guard
    if (bItems.count() < i + 1) return;
    bItems[i]->setRect(0, 0, b[i].w, b[i].h);
    QColor color;
    QPen pen;
    pen.setWidth(0);
    pen.setColor(Qt::transparent);
    bItems[i]->setPen(pen);
    color.setNamedColor(p->b[i].color);
    // index guard
    if (p->b.count() < i + 1) return;
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
    bItems[i]->setPos(b[i].x, b[i].y);
    bItems[i]->setZValue(ZBorder);

    bool isEffects = (p->styleMap[p->b[i].style].size() > 0);
    bool legalStyle = (p->b[i].style != "No style" && p->b[i].style != "");
    bool hasStyle = p->styleMap.contains(p->b[i].style);

    // graphics effects
    if (hasStyle && legalStyle && isEffects) {
        GraphicsEffect *effect = new GraphicsEffect(src);
        effect->set(p->styleMap[p->b[i].style],
                    p->lightDirection,
                    0,  /* rotation */
                    bItems[i]->boundingRect());
        bItems[i]->setGraphicsEffect(effect);
    }
    else bItems[i]->setGraphicsEffect(nullptr);
}

void Embel::updateText(int i)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (G::mode != "Loupe") return;

    // index guard
    if (p->t.count() < i + 1) return;
    if (tItems.count() < i + 1) return;

    // if a text entry
    if (p->t[i].source == "Text") {
        tItems[i]->setPlainText(p->t[i].text);
    }
    // if a metadata template used to build the text string
    else {
        if (isRemote) // from Embel::Export
            tItems[i]->setPlainText(p->metaString(p->t[i].metadataTemplate, fPath));
        else
            tItems[i]->setPlainText(p->metaString(p->t[i].metadataTemplate));
    }
    // font
    QFont font(p->t[i].font);
    int fontSize = static_cast<int>(p->t[i].size / 100 * ls);
    font.setPixelSize(fontSize);
    font.setItalic(p->t[i].isItalic);
    font.setBold(p->t[i].isBold);
    tItems[i]->setFont(font);
    tItems[i]->setDefaultTextColor(QColor(p->t[i].color));
    // opacity
    double opacity = static_cast<double>(p->t[i].opacity)/100;
    tItems[i]->setOpacity(opacity);
    tItems[i]->setZValue(ZText);
    double rotation = p->t[i].rotation;
    tItems[i]->setRotation(rotation);

    // position text
    /*
    qDebug() << __FUNCTION__ << "Getting canvas coord for Text" << i
             << "p->t[i].anchorObject =" << p->t[i].anchorObject
             << "p->t[i].anchorContainer =" << p->t[i].anchorContainer
             << "p->t[i].x =" << p->t[i].x
             << "p->t[i].y =" << p->t[i].y
                ;
//                    */

    /* Range check - make sure embel borders synced with embelProperties borders.  A text
       update could be triggered before a new border has been processed.  */
    if (p->b.size() == b.size()) {
        int w = static_cast<int>(tItems[i]->boundingRect().width());
        int h = static_cast<int>(tItems[i]->boundingRect().height());
        QPoint canvas = canvasCoord(p->t[i].x,
                                    p->t[i].y,
                                    p->t[i].anchorObject,
                                    p->t[i].anchorContainer,
                                    p->t[i].align);
        QPoint offset = anchorPointOffset(p->t[i].anchorPoint, w, h, rotation);
        tItems[i]->setTransformOriginPoint(offset);
        tItems[i]->setPos(canvas - offset);
    }

    // if style then rotate in GraphicsEffect, else rotate text here

    bool isEffects = (p->styleMap[p->t[i].style].size() > 0);
    bool legalStyle = (p->t[i].style != "No style" && p->t[i].style != "");
    bool hasStyle = p->styleMap.contains(p->t[i].style);
//    tItems[i]->setRotation(rotation);

    // graphics effects
    tItems[i]->setGraphicsEffect(nullptr);
    if (hasStyle && legalStyle && isEffects) {
        GraphicsEffect *effect = new GraphicsEffect(src);
//            effect->setObjectName("Text" + QString::number(i));
        effect->set(p->styleMap[p->t[i].style],
                p->lightDirection,
                rotation,
                tItems[i]->boundingRect());
        tItems[i]->setGraphicsEffect(effect);
    }
    else {
//        tItems[i]->setRotation(rotation);
    }
}

void Embel::updateGraphic(int i)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (G::mode != "Loupe") return;

    // index guard
    if (p->g.count() < i + 1) return;
    if (gItems.count() < i + 1) return;

    gItems[i]->setZValue(ZGraphic);
    double opacity = static_cast<double>(p->g[i].opacity)/100;
    gItems[i]->setOpacity(opacity);
    // workaround until add shapes and use line
    if (p->templateName == "Zen2048") {
        if (p->g[i].anchorContainer == "Left" || p->g[i].anchorContainer == "Right") stu = h;
        else stu = w;
    }
    else stu = ls;
    int dim = static_cast<int>(static_cast<double>(p->g[i].size) / 100 * stu);

    gItems[i]->setPixmap(graphicPixmaps.at(i).scaled(QSize(dim, dim), Qt::KeepAspectRatio));
    // check if squished to zero height (to make line work in Zen2048 template - real fix is
    // to add shapes (oval, arc, rectangle, polygon and line)
    if (gItems[i]->pixmap().height() < 2) {
        gItems[i]->setPixmap(graphicPixmaps.at(i).scaled(dim, 2));
        /*
        gItems[i]->setPixmap(graphicPixmaps.at(i).scaledToWidth(dim));
        gItems[i]->setPixmap(graphicPixmaps.at(i).scaledToHeight(2));
        gItems[i]->setPixmap(graphicPixmaps.at(i).scaledToWidth(dim));
        */
    }
    double rotation = p->g[i].rotation;
    gItems[i]->setRotation(rotation);

    /* Range check - make sure embel borders synced with embelProperties borders.  A graphic
       update could be triggered before a new border has been processed.  */
    if (p->b.size() == b.size()) {
        int w = static_cast<int>(gItems[i]->pixmap().width());
        int h = static_cast<int>(gItems[i]->boundingRect().height());
        /*
        qDebug() << __FUNCTION__ << "Adding graphic #" << i
                 << "p->g[i].size =" << p->g[i].size
                 << "dim =" << dim << "w =" << w << "h =" << h;
    //             */
        QPoint canvas = canvasCoord(p->g[i].x,
                                    p->g[i].y,
                                    p->g[i].anchorObject,
                                    p->g[i].anchorContainer,
                                    p->g[i].align);
        QPoint offset = anchorPointOffset(p->g[i].anchorPoint, w, h, rotation);
        gItems[i]->setTransformOriginPoint(offset);
        gItems[i]->setPos(canvas - offset);
    }

    // if style then rotate in GraphicsEffect, else rotate text here

    bool isEffects = (p->styleMap[p->g[i].style].size() > 0);
    bool legalStyle = (p->g[i].style != "No style" && p->g[i].style != "");
    bool hasStyle = p->styleMap.contains(p->g[i].style);

    // graphics effects
    if (hasStyle && legalStyle && isEffects) {
        GraphicsEffect *effect = new GraphicsEffect(src);
//            effect->setObjectName("Graphic" + QString::number(i));
        effect->set(p->styleMap[p->g[i].style],
                p->lightDirection,
                rotation,
                gItems[i]->boundingRect());
        gItems[i]->setGraphicsEffect(effect);
    }
    else {
        gItems[i]->setGraphicsEffect(nullptr);
//        gItems[i]->setRotation(rotation);
    }
}

void Embel::updateImage()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (G::mode != "Loupe") return;

    bool isEffects = (p->styleMap[p->image.style].size() > 0);
    bool legalStyle = (p->image.style != "No style" && p->image.style != "");
    bool hasStyle = p->styleMap.contains(p->image.style);

    // graphics effects
    if (hasStyle && legalStyle && isEffects) {
        // start with a fresh image from the ImageCache
        if (imCache->imCache.contains(fPath)) {
            pmItem->setPixmap(QPixmap::fromImage(imCache->imCache.value(fPath)).scaledToWidth(image.w));
        }
        GraphicsEffect *effect = new GraphicsEffect(src);
        effect->setObjectName("EmbelImageEffect");
        /*
        qDebug() << __FUNCTION__
                 << "effect =" << effect
                 << "p->image.style =" << p->image.style;
//                     */
        effect->set(p->styleMap[p->image.style],
                p->lightDirection,
                0,
                pmItem->pixmap().rect()
                );
        pmItem->setGraphicsEffect(effect);
    }
    else pmItem->setGraphicsEffect(nullptr);
}

void Embel::refreshTexts()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    for (int i = 0; i < tItems.size(); ++i) {
        updateText(i);
    }
}



void Embel::removeBorder(int i)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "i = " + QString::number(i));
    #endif
    }
//    qDebug() << __FUNCTION__ << "i =" << i << "bItems.size() =" << bItems.size();
    // index guard
    if (b.count() < i + 1) return;
    if (bItems.count() < i + 1) return;

    if (i >= bItems.size() || i >= b.size()) return;
    if (scene->items().contains(bItems[i]))
        scene->removeItem(bItems[i]);
    if (bItems.contains(bItems[i])) {
        delete bItems[i];
        bItems.removeAt(i);
        b.removeAt(i);
    }
}

void Embel::removeText(int i)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "i = " + QString::number(i));
    #endif
    }
    // index guard
    if (tItems.count() < i + 1) return;

    if (i >= tItems.size()) return;
    if (scene->items().contains(tItems[i]))
        scene->removeItem(tItems[i]);
    if (tItems.contains(tItems[i])) {
        delete tItems[i];
        tItems.removeAt(i);
    }
}

void Embel::removeGraphic(int i)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "i = " + QString::number(i));
    #endif
    }
    // index guard
    if (gItems.count() < i + 1) return;

    if (i >= gItems.size()) return;
    if (scene->items().contains(gItems[i]))
        scene->removeItem(gItems[i]);
    if (gItems.contains(gItems[i])) {
        delete gItems[i];
        gItems.removeAt(i);
    }
    if (graphicPixmaps.count() > i)
        graphicPixmaps.removeAt(i);
}

void Embel::updateStyle(QString style)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "style = " + style);
    #endif
    }
    if (G::mode != "Loupe") return;

    // update any borders with this style
    for (int i = 0; i < bItems.size(); ++i) {
        if (p->b[i].style == style) updateBorder(i);
    }

    // update any texts with this style
    for (int i = 0; i < tItems.size(); ++i) {
        if (p->t[i].style == style) updateText(i);
    }

    // update any graphics with this style
    for (int i = 0; i < gItems.size(); ++i) {
        if (p->g[i].style == style) updateGraphic(i);
    }

    // update image with this style
    if (p->image.style == style) updateImage();
}

void Embel::flashObject(QString type, int index, bool show)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    #ifdef ISLOGGER
    Utilities::log(__FUNCTION__, "");
    #endif
    }
    if (G::mode != "Loupe") return;

    flashItem->setVisible(show);
    if (!show) return;

    if (type == "border") {
        // index guard (triggered if no image has been selected in loupe view
        if (bItems.count() < index + 1) return;
        flashItem->setRect(bItems[index]->boundingRect());
        flashItem->setPos(bItems[index]->pos());
//        qDebug() << __FUNCTION__ << bItems[index]->boundingRect();
    }
    if (type == "text") {
        // index guard (triggered if no image has been selected in loupe view
        if (tItems.count() < index + 1) return;
        flashItem->setRect(tItems[index]->boundingRect());
        int w = static_cast<int>(tItems[index]->boundingRect().width());
        int h = static_cast<int>(tItems[index]->boundingRect().height());
        QPoint offset = anchorPointOffset(p->t[index].anchorPoint, w, h, p->t[index].rotation);
        flashItem->setTransformOriginPoint(offset);
        flashItem->setPos(tItems[index]->pos());
        flashItem->setRotation(tItems[index]->rotation());
    }
    if (type == "graphic") {
        // index guard (triggered if no image has been selected in loupe view
        if (gItems.count() < index + 1) return;
        flashItem->setRect(gItems[index]->boundingRect());
        int w = static_cast<int>(gItems[index]->boundingRect().width());
        int h = static_cast<int>(gItems[index]->boundingRect().height());
        QPoint offset = anchorPointOffset(p->g[index].anchorPoint, w, h, p->g[index].rotation);
        flashItem->setTransformOriginPoint(offset);
        flashItem->setPos(gItems[index]->pos());
        flashItem->setRotation(gItems[index]->rotation());
    }
    /*
    qDebug() << __FUNCTION__
             << "type =" << type
             << "index =" << index
             << "flashItem->rect() =" << flashItem->rect()
             << "flashItem->pos() =" << flashItem->pos()
                ;
//                */
    flashItem->setZValue(ZFlash);
    flashItem->setPen(QPen(Qt::red, 2));
    flashItem->setBrush(QBrush(Qt::transparent));

    QTimer::singleShot(100, this, SLOT(flashObject()));
}

bool Embel::eventFilter(QObject *object, QEvent *event)
{
    qDebug() << __FUNCTION__ << object << event;
    return true;// QWidget::eventFilter(object, event);
}

void Embel::diagnostics(QTextStream &rpt)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    rpt << "\n" << "\nEmbel:";

    rpt << "\nScene total items = " << scene->items().count() << "\n";
    for (int i = 0; i < scene->items().count(); ++i) {
        rpt << QString::number(i)
             << ". tooltip = " << scene->items().at(i)->toolTip()
             << ", type = " << scene->items().at(i)->type()
             << ", pos = ("
             << QString::number(scene->items().at(i)->pos().x()) << ","
             << QString::number(scene->items().at(i)->pos().y()) << ")"
             << ", iv->scene->items().at(i) = " << scene->items().at(i)
             << "\n"
                ;
    }

    rpt << "\nTotal border items = " << bItems.count();
    for (int i = 0; i < bItems.count(); ++i) {
        rpt  << i
             << ". tooltip = " << bItems.at(i)->toolTip()
             << ", type = " << bItems.at(i)->type()
             << ", pos = ("
             << QString::number(bItems.at(i)->pos().x()) << ","
             << QString::number(bItems.at(i)->pos().y()) << ")"
             << ", bItems.at(i) =" << bItems.at(i)
             << "\n"
                ;
            }

    rpt << "\nTotal text items = " << tItems.count();
    for (int i = 0; i < tItems.count(); ++i) {
        rpt << i
            << ". tooltip = " << tItems.at(i)->toolTip()
            << ", type = " << tItems.at(i)->type()
            << ", pos = ("
            << QString::number(tItems.at(i)->pos().x()) << ","
            << QString::number(tItems.at(i)->pos().y()) << ")"
            << ", tItems.at(i) = " << tItems.at(i)
            << "\n"
              ;
    }

    rpt << "\nTotal graphic items = " << gItems.count();
    for (int i = 0; i < gItems.count(); ++i) {
        rpt << i
            << ". tooltip =" << gItems.at(i)->toolTip()
            << ", type =" << gItems.at(i)->type()
            << ", pos = ("
            << QString::number(gItems.at(i)->pos().x()) << ","
            << QString::number(gItems.at(i)->pos().y()) << ")"
            << ", gItems.at(i) =" << gItems.at(i)
            << "\n"
               ;
    }

    rpt << "\nCanvas                     "
        << " w = " << QString::number(w).leftJustified(5)
        << " h = " << QString::number(h).leftJustified(5)
        << "\n"
               ;
    for (int i = 0; i < b.size(); ++i) {
        rpt << "Border" << i
        << " x = " << QString::number(b[i].x).leftJustified(5)
        << " y = " << QString::number(b[i].y).leftJustified(5)
        << " w = " << QString::number(b[i].w).leftJustified(5)
        << " h = " << QString::number(b[i].h).leftJustified(5)
        << " l = " << QString::number(b[i].l).leftJustified(5)
        << " r = " << QString::number(b[i].r).leftJustified(5)
        << " t = " << QString::number(b[i].t).leftJustified(5)
        << " b = " << QString::number(b[i].b).leftJustified(5)
        << " tl.x = " << QString::number(b[i].tl.x()).leftJustified(5)
        << " tl.y = " << QString::number(b[i].tl.y()).leftJustified(5)
        << " br.x = " << QString::number(b[i].br.x()).leftJustified(5)
        << " br.y = " << QString::number(b[i].br.y()).leftJustified(5)
        << "\n"
        ;
    }
    rpt << "Image  "
        << " x = " << QString::number(image.x).leftJustified(5)
        << " y = " << QString::number(image.y).leftJustified(5)
        << " w = " << QString::number(image.w).leftJustified(5)
        << " h = " << QString::number(image.h).leftJustified(5)
        << "                                        "
        << " tl.x = " << QString::number(image.tl.x()).leftJustified(5)
        << " tl.y = " << QString::number(image.tl.y()).leftJustified(5)
        << " br.x = " << QString::number(image.br.x()).leftJustified(5)
        << " br.y = " << QString::number(image.br.y()).leftJustified(5)
        ;
}
