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

    embellish= new Embel(scene, pmItem, embelProperties, imageCacheThread);
}

EmbelExport::~EmbelExport()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    delete embellish;
//    delete scene;
//    delete pmItem;
}

bool EmbelExport::loadImage(QString fPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (imageCacheThread->imCache.contains(fPath)) {
        pmItem->setPixmap(QPixmap::fromImage(imageCacheThread->imCache.value(fPath)));
        return true;
    }
    // check metadata loaded for image
    int dmRow = dm->fPathRow[fPath];
    if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->m.row = dmRow;
            dm->addMetadataForItem(metadata->m);
        }
        else return false;
    }
    pmItem->setPixmap(QPixmap(fPath));
    return true;
}

void EmbelExport::exportPicks()
{
/*
Images with the current filtration that are picked are rendered according to the current
embellish template and saved to the template output folder.  This is called from
MW::exportEmbel.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString folderPath = embelProperties->exportFolderPath;
    QString suffix = embelProperties->exportFileType;

    // get number of images to export
    int pickCount = 0;
    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        QModelIndex pickIdx = dm->sf->index(row, G::PickColumn);
        // only picks
        if (pickIdx.data(Qt::EditRole).toString() == "true") {
            pickCount++;
        }
    }

    // setup progress popup
    qDebug() << __FUNCTION__
             << "pickCount =" << pickCount
             << "QString::number(pickCount) =" << QString::number(pickCount)
                ;
    G::popUp->setProgressVisible(true);
    G::popUp->setProgressMax(pickCount);
    QString txt = "Exporting " + QString::number(pickCount) + "embellished images to "
            + folderPath;
    G::popUp->showPopup(txt, 0, true, 1);

    for (int row = 0; row < dm->sf->rowCount(); ++row) {
        G::popUp->setProgress(row + 1);
        QModelIndex pickIdx = dm->sf->index(row, G::PickColumn);
        QModelIndex idx = dm->sf->index(row, 0);
        // only picks
        if (pickIdx.data(Qt::EditRole).toString() == "true") {
            // input file path
            QString fPath = idx.data(G::PathRole).toString();
            // build export file path
            QFileInfo fileInfo(fPath);
            QString fName = fileInfo.baseName() + "." + suffix;
            QString exportPath = folderPath + "/" + fName;

            // read the image, add it to the scene and embellish
            if (loadImage(fPath)) {
                // embellish
                embellish->build();
                setSceneRect(scene->itemsBoundingRect());
            }

            // create image to show rendering with embellish
            QImage image(scene->sceneRect().size().toSize(), QImage::Format_ARGB32);  // Create the image with the exact size of the shrunk scene
            image.fill(Qt::transparent);                                              // Start all pixels transparent
            QPainter painter(&image);
            scene->render(&painter);
            // save
            image.save(exportPath);
            qDebug() << __FUNCTION__ << "exporting to" << exportPath;
        }
    }
    G::popUp->setProgressVisible(false);
    G::popUp->hide();
}