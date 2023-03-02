#ifndef METAREAD_H
#define METAREAD_H

#include <QtWidgets>
#include <QObject>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
//#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"
#include "Cache/thumbdecoder.h"
#include "Cache/imagecache.h"

class MetaRead : public QThread
{
    Q_OBJECT

public:
    MetaRead(QObject *parent,
             DataModel *dm,
             Metadata *metadata,
             FrameDecoder *frameDecoder,
             ImageCache *imageCache);
    ~MetaRead() override;

    void stop();
    QString diagnostics();
    QString reportMetaCache();
    void cleanupIcons();

    int iconChunkSize;
    int firstIconRow;
    int lastIconRow;

signals:
    void stopped(QString src);
    void updateScroll();
    void runStatus(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void centralMsg(QString message);
    void updateProgress(int progress);
    void addToDatamodel(ImageMetadata m, QString src);// decoder
    void addToImageCache(ImageMetadata m);// decoder
    void setIcon(QModelIndex dmIdx, const QPixmap pm, int fromInstance, QString src);  // decoder
    void triggerImageCache(QString src);

    void updateIconBestFit();  // req'd?
    void loaded();

public slots:
    void initialize();
    void decodeThumbs(int id);
    void setCurrentRow(int row = 0, QString src = "");
    int interrupt();

protected:
    void run() Q_DECL_OVERRIDE;

private:
    void read(int startRow = 0, QString src = "");// decoder
    void readRow(int sfRow);// decoder
    bool readMetadata(QModelIndex sfIdx, QString fPath);// decoder
    void readIcon(QModelIndex sfIdx, QString fPath);// decoder

    void buildQueue();
    void startThumbDecoders();
    void done();

//    void iconMax(QPixmap &thumb);
//    bool isNotLoaded(int sfRow);
//    bool inIconRange(int sfRow);

    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    bool abortCleanup;
    bool interrupted;
    int interruptedRow;
    bool decodeThumbsTerminated;

    DataModel *dm;
    Metadata *metadata;
    FrameDecoder *frameDecoder; // decoder
    ImageCache *imageCache;
    Thumb *thumb;               // decoder
    int instance;
    QVector<ThumbDecoder*> decoder;     // all the decoders
    ThumbDecoder *d;        // short ptr for current decoder
    int decoderCount;
    int sfRowCount;
    int dmRowCount;
    int metaReadCount;
    double expansionFactor = 1.2;
    int iconLimit;                  // iconChunkSize * expansionFactor
    int thumbDecoderTriggerCount = 20;
    int imageCacheTriggerCount = 200;

    QList<QString> queue;

    int startRow = 0;
    QString src;
    QString folderPath;

    bool imageCachingStarted = false;

    QList<int> rowsWithIcon;

    bool isDebug = false;
    QElapsedTimer t;
    QElapsedTimer tAbort;
    quint32 ms;
};

#endif // METAREAD_H
