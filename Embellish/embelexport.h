#ifndef EMBELEXPORT_H
#define EMBELEXPORT_H

#include <QtWidgets>
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Cache/cachedata.h"
#include "Properties/embelproperties.h"
#include "Embellish/embel.h"
#include "Metadata/ExifTool.h"
//#ifdef Q_OS_WIN
#include "Utilities/icc.h"
//#endif

class EmbelExport : public QGraphicsView
{
    Q_OBJECT

public:
    EmbelExport(Metadata *metadata,
                DataModel *dm,
                ImageCacheData *icd,
                EmbelProperties *embelProperties,
                QWidget *parent = nullptr);
    ~EmbelExport();

    void exportImages(const QStringList &srcList);
    QString exportRemoteFiles(QString templateName, QStringList &pathList);
    void exportImage(const QString &fPath);
    QString exportFolderPath(QString folderPath = "");

    bool exportingEmbellishedImages = false;

public slots:
    void abortEmbelExport();

private:
    bool loadImage(QString fPath);
    Metadata *metadata;
    DataModel *dm;
    ImageCacheData *icd;
    ImageCache *imageCacheThread;
    EmbelProperties *embelProperties;
    Embel *embellish;
    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    QString lastFileExportedPath;
    QString lastFileExportedThumbPath;
    bool abort = false;
};

#endif // EMBELEXPORT_H
