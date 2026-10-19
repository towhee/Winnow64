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

/*  The file writer for an Embellish export, on its own thread: the image encode, the
    thumbnail, and the ExifTool tag, ICC and thumbnail copies.  ExifTool is a
    stay-open QProcess driven with blocking waits, so it is created on this thread, on
    the first write, and lives for the batch. */
class EmbelWriter : public QObject
{
    Q_OBJECT

public:
    ~EmbelWriter() override;

public slots:
    void write(const QImage &image, const QString &src, const QString &dst,
               const QString &dstThumb, const QString &extension);

signals:
    void written(bool ok, const QString &dst);

private:
    ExifTool *et = nullptr;
};

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

    /* Both return at once; the export runs from the event loop and emits finished.
       They return false, and emit nothing, when there is nothing to export. */
    bool exportImages(const QStringList &srcList, bool isRemote = false);
    bool exportRemoteFiles(QString templateName, const QStringList &pathList);
    QString exportSubfolderPath(QString folderPath = "", bool allowOverride = false);
    bool isRunning() const { return running; }

signals:
    void finished(const QStringList &dstPaths, bool aborted);

public slots:
    void abortEmbelExport();

private:
    void startNext();
    void onPixels(bool ok, const QImage &developed);
    void compose(const QString &fPath);
    void onWritten(bool ok, const QString &dst);
    void finish();
    bool loadImage(QString fPath);
    bool isValidExportFolder();

    PixelSource pixelSource;
    Metadata *metadata;
    DataModel *dm;
    ImageCacheData *icd;
    EmbelProperties *embelProperties;
    Embel *embellish;
    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    QThread *writerThread = nullptr;
    EmbelWriter *writer = nullptr;
    QString exportFolderIfNotSubfolder;
    QStringList srcList;
    QStringList dstPaths;
    QSet<QString> reservedPaths;        // destinations claimed by this batch
    QStringList remoteTempFiles;        // a winnet's temp copies, removed when done
    int index = 0;                      // the image being rendered
    int pendingWrites = 0;              // handed to the writer, not yet written
    bool waitingForWriter = false;      // render paused: the writer is two behind
    bool remote = false;                // a winnet's files: no datamodel
    bool running = false;
    bool abort = false;
};

#endif // EMBELEXPORT_H
