#include "Embellish/embelcompose.h"
#include "Embellish/embel.h"
#include "Main/global.h"

EmbelCompose::EmbelCompose(EmbelProperties *p, DataModel *dm, ImageCacheData *icd)
{
    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
    scene->addItem(pmItem);
    embel = new Embel(scene, pmItem, p, dm, icd, "Export");
}

EmbelCompose::~EmbelCompose()
{
    delete embel;
    delete scene;                       // and pmItem with it
}

QImage EmbelCompose::compose(const QImage &src, const QString &fPath)
{
/*
    The same steps as EmbelExport::compose: build the embellishment around the image on
    pmItem, then render the scene, from a transparent start, at exactly its size.
*/
    QString srcFun = "EmbelCompose::compose";
    if (G::isLogger) G::log(srcFun, fPath);
    if (src.isNull()) return QImage();

    pmItem->setPixmap(QPixmap::fromImage(src));
    embel->build(fPath, srcFun);
    const QRectF rect = scene->itemsBoundingRect();
    scene->setSceneRect(rect);

    QImage image(rect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    scene->render(&painter);
    painter.end();
    pmItem->setPixmap(QPixmap());       // release the full-size source
    embel->clear();

    /* Embel draws in sRGB; the exporter converts from this tag to the output space. */
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}
