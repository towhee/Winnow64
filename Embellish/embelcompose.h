#ifndef EMBELCOMPOSE_H
#define EMBELCOMPOSE_H

#include <QImage>
#include <QString>

class QGraphicsScene;
class QGraphicsPixmapItem;
class Embel;
class EmbelProperties;
class DataModel;
class ImageCacheData;

/*
    Embellish one image: put it on a private scene, build the CURRENT template's borders,
    texts and graphics around it, and render the scene to a QImage. The export stage of
    ImageExporter (MW::embellishedPixelSource) uses it, so any pixel source -- a develop
    render, a camera preview, a plain file -- can go out embellished.

    The Embel instance is an "Export" one (Embel::isExport): it renders whatever mode the
    loupe is in, keeps the image it is given rather than the image cache's copy, and reads
    text tokens from the exported image's own row (EmbelProperties::metaStringOf).

    GUI THREAD ONLY: the scene is a QGraphicsScene of QPixmap items. The template is read
    from embelProperties, so the caller makes the wanted template current first.
*/
class EmbelCompose
{
public:
    EmbelCompose(EmbelProperties *p, DataModel *dm, ImageCacheData *icd);
    ~EmbelCompose();

    /* The embellished image for src, the pixels of fPath (a row key: a version key
       finds the version's row). Null if src is null. */
    QImage compose(const QImage &src, const QString &fPath);

private:
    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    Embel *embel;
};

#endif // EMBELCOMPOSE_H
