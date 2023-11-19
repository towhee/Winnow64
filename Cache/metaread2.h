#ifndef METAREAD2_H
#define METAREAD2_H

#include <QtWidgets>
#include <QObject>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"
#include "Cache/reader.h"
#include "Cache/imagecache.h"

class MetaRead2 : public QThread
{
    Q_OBJECT

public:
    MetaRead2(QObject *parent,
              DataModel *dm,
              Metadata *metadata,
              FrameDecoder *frameDecoder,
              ImageCache *imageCache);
    ~MetaRead2() override;

    bool stop();
    QString diagnostics();
    QString reportMetaCache();
    void cleanupIcons();
    void resetTrigger();

    int iconChunkSize;
    int firstIconRow;
    int lastIconRow;

    bool showProgressInStatusbar = true;

signals:
    void stopped(QString src);
    void updateScroll();
    void runStatus(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void centralMsg(QString message);
    void updateProgressInFilter(int progress);
    void updateProgressInStatusbar(int progress, int total);

    void addToDatamodel(ImageMetadata m, QString src);// decoder
    void addToImageCache(ImageMetadata m);// decoder
    void setIcon(QModelIndex dmIdx, const QPixmap pm, int fromInstance, QString src);  // decoder

    void fileSelectionChange(QModelIndex sfIdx,
                             QModelIndex idx2 = QModelIndex(),
                             bool clearSelection = false,
                             QString src = "MetaRead::triggerCheck");

    void done();

public slots:
    void initialize();
    void dispatch(int id);
    void setStartRow(int row, bool fileSelectionChanged, QString src = "");

protected:
    void run() Q_DECL_OVERRIDE;

private:
    void read(int startRow = 0, QString src = "");// decoder

    void buildQueue();
    void dispatchReaders();
    void weAreDone();
    bool readersRunning();
    void redoFailed();

//    void iconMax(QPixmap &thumb);
//    bool isNotLoaded(int sfRow);
//    bool inIconRange(int sfRow);

    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    bool abortCleanup;
    bool interrupted;
    int interruptedRow;

    DataModel *dm;
    Metadata *metadata;
    FrameDecoder *frameDecoder; // decoder
    ImageCache *imageCache;
    Thumb *thumb;               // decoder
    int instance;
    QVector<Reader*> reader;     // all the decoders
    Reader *d;        // short ptr for current decoder
    int readerCount;
    int sfRowCount;
    int dmRowCount;
    int metaReadCount;
    double expansionFactor = 1.2;
    int iconLimit;                  // iconChunkSize * expansionFactor
    //bool hasBeenTriggered;
    bool okToTrigger;                       // signal MW::fileSelectionChange
    int thumbDecoderTriggerCount = 20;
    int imageCacheTriggerCount = 200;
    int redoCount = 0;
    int redoMax = 5;
    bool isDone;

    //QList<QString> queue;
    QList<int> queueN;

    int startRow = 0;
    int targetRow = 0;
    int a = 0;
    int b = -1;
    QString src;

    QList<int> rowsWithIcon;

    bool isDebug = false;
    QElapsedTimer t;
    QElapsedTimer tAbort;
    quint32 ms;
};

#endif // METAREAD2_H

