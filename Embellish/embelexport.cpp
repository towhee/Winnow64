#include "embelexport.h"

EmbelExport::EmbelExport(Metadata *metadata,
                         DataModel *dm,
                         ImageCache *imageCacheThread,
                         EmbelProperties *embelProperties,
                         QWidget *parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->metadata = metadata;
    this->dm = dm;
    this->imageCacheThread = imageCacheThread;
    this->embelProperties = embelProperties;

    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
    scene->addItem(pmItem);
    setScene(scene);

    embellish= new Embel(scene, pmItem, embelProperties);
}

EmbelExport::~EmbelExport()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    delete embellish;
    delete scene;
    delete pmItem;
}

bool EmbelExport::loadImage(QString fPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // rgh add error trapping
    if (imageCacheThread->imCache.contains(fPath)) {
        pmItem->setPixmap(QPixmap::fromImage(imageCacheThread->imCache.value(fPath)));
    }
    else {
        // check metadata loaded for image
        int dmRow = dm->fPathRow[fPath];
        if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
            QFileInfo fileInfo(fPath);
            if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
                metadata->m.row = dmRow;
                dm->addMetadataForItem(metadata->m);
            }
        }
        pmItem->setPixmap(QPixmap(fPath));
    }
    embellish->build();
    setSceneRect(scene->itemsBoundingRect());

    return true;
}

void EmbelExport::exportPicks()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString folderPath = embelProperties->exportFolderPath;
    QString suffix = embelProperties->exportFileType;
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QModelIndex pickIdx = dm->sf->index(row, G::PickColumn);
        QModelIndex idx = dm->sf->index(row, 0);
        // only picks
        if (pickIdx.data(Qt::EditRole).toString() == "true") {
            QString fPath = idx.data(G::PathRole).toString();
            QFileInfo fileInfo(fPath);
            QString fName = fileInfo.baseName() + "." + suffix;
            QString exportPath = folderPath + "/" + fName;
            loadImage(fPath);
            QImage image(scene->sceneRect().size().toSize(), QImage::Format_ARGB32);  // Create the image with the exact size of the shrunk scene
            image.fill(Qt::transparent);                                              // Start all pixels transparent

            QPainter painter(&image);
            scene->render(&painter);
            image.save(exportPath);
        }
    }
}
