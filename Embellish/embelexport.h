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
    void exportRemoteFile(QString fPath, QString templateName);
    void exportImage(const QString &fPath);

private:
    bool loadImage(QString fPath);
    QString exportFolderPath(QString folderPath = "");
    Metadata *metadata;
    DataModel *dm;
    ImageCache *imageCacheThread;
    EmbelProperties *embelProperties;
    Embel *embellish;
    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
};

#endif // EMBELEXPORT_H
