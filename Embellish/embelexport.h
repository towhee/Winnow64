#ifndef EMBELEXPORT_H
#define EMBELEXPORT_H

#include <QtWidgets>
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
#include <functional>

class EmbelExport : public QGraphicsView
{
    Q_OBJECT

public:
    EmbelExport(Metadata *metadata,
                DataModel *dm,
                ImageCacheData *icd,
                EmbelProperties *embelProperties,
                QWidget *parent = nullptr);
    ~EmbelExport() override;

    /* The image each export embellishes: MW::developPixelSource -- the develop recipe
       (or default settings) rendered from the raw SENSOR data, not the embedded preview
       the image cache holds while browsing. Unset, or for a remote (winnet) export, the
       old path is used: the image cache, else the file decoded by Qt. */
    using PixelDone = std::function<void(bool ok, const QImage &img)>;
    using PixelSource = std::function<void(const QString &fPath, PixelDone done)>;
    void setPixelSource(PixelSource source) { pixelSource = std::move(source); }

    void exportImages(const QStringList &srcList, bool isRemote = false);
    QStringList exportRemoteFiles(QString templateName, QStringList &pathList);
    bool exportImage(const QString &fPath);
    QString exportSubfolderPath(QString folderPath = "", bool allowOverride = false);

    bool exportingEmbellishedImages = false;

public slots:
    void abortEmbelExport();

private:
    bool loadImage(QString fPath);
    bool renderDeveloped(const QString &fPath, QImage &image);
    PixelSource pixelSource;
    bool isValidExportFolder();
    Metadata *metadata;
    DataModel *dm;
    ImageCacheData *icd;
    ImageCache *imageCacheThread;
    EmbelProperties *embelProperties;
    Embel *embellish;
    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    QString exportFolderIfNotSubfolder;
    QString lastFileExportedPath;
    QString lastFileExportedThumbPath;
    QStringList dstPaths;
    bool abort = false;
};

#endif // EMBELEXPORT_H
