#ifndef EMBELEXPORT_H
#define EMBELEXPORT_H

#include <QtWidgets>
#include "Datamodel/datamodel.h"
#include "Cache/imagecache.h"
#include "Properties/embelproperties.h"
#include "Embellish/embel.h"

class EmbelExport : public QGraphicsView
{
    Q_OBJECT

public:
    EmbelExport(Metadata *metadata,
                DataModel *dm,
                ImageCache *imageCacheThread,
                EmbelProperties *embelProperties,
                QWidget *parent = nullptr);
    ~EmbelExport();

    void exportImages(const QStringList &fPathList);
    QString exportRemoteFiles(QString templateName, QStringList &pathList);
    void exportImage(const QString &fPath);
    QString exportFolderPath(QString folderPath = "");

private:
    bool loadImage(QString fPath);
    Metadata *metadata;
    DataModel *dm;
    ImageCache *imageCacheThread;
    EmbelProperties *embelProperties;
    Embel *embellish;
    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    QString lastFileExportedPath;
};

#endif // EMBELEXPORT_H
