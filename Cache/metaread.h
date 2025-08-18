#ifndef METAREAD_H
#define METAREAD_H

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

class MetaRead : public QObject
{
    Q_OBJECT

public:
    MetaRead(QObject *parent,
             DataModel *dm,
             Metadata *metadata,
             // FrameDecoder *frameDecoder,
             ImageCache *imageCache);
    ~MetaRead() override;

    QThread metaReadThread;       // Separate thread for MetaRead

    int readerCount;
    QVector<Reader*> reader;        // all the decoders

    void stop();
    void stopReaders();
    void abortReaders();
    void syncInstance();
    QString reportMetaCache();
    void cleanupIcons();
    QString diagnostics();

    bool isDispatching;

    int firstIconRow;
    int lastIconRow;

    bool showProgressInStatusbar = true;
    bool isDebug = false;

    void test();

signals:
    void stopped(QString src);
    void updateScroll();
    void runStatus(bool/*isRunning*/, bool/*showCacheLabel*/, bool /*success*/, QString/*calledBy*/);
    void centralMsg(QString message);
    void okToSelect(bool ok);
    void select(QModelIndex sfIdx, bool clearSelection);
    void updateProgressInFilter(int progress);
    void updateProgressInStatusbar(int progress, int total);

    void setMsToRead(QModelIndex dmIdx, QVariant value, int instance, QString src = "MetaRead::dispatch",
                     int role = Qt::EditRole, int align = Qt::AlignRight);
    void setIcon(QModelIndex dmIdx, const QPixmap pm, int fromInstance, QString src);

    void fileSelectionChange(QModelIndex sfIdx,
                             QModelIndex idx2 = QModelIndex(),
                             bool clearSelection = false,
                             QString src = "MetaRead::dispatch");

    void done();
    void dispatchIsFinished(QString src);

public slots:
    void initialize(QString src = "");
    void dispatchReaders();
    void dispatch(int id, bool isReturning);
    void setStartRow(int row, bool fileSelectionChanged, QString src = "");
    void dispatchFinished(QString src);

private:
    void read(int startRow = 0, QString src = "");// decoder
    void processReturningReader(int id, Reader *r);
    void quitAfterTimeout();
    bool allMetaIconLoaded();
    void redo();
    int pending();
    inline bool needToRead(int row);
    bool nextA();
    bool nextB();
    bool nextRowToRead();
    void emitFileSelectionChangeWithDelay(const QModelIndex &sfIdx, int msDelay);

    QMutex mutex;
    QWaitCondition condition;
    bool abort;

    QEventLoop runloop;

    QVector<Reader*> readers;
    QVector<QThread*> readerThreads;
    QVector<bool> cycling;                  // all the decoders activity
    bool allReadersCycling();

    DataModel *dm;
    Metadata *metadata;
    FrameDecoder *frameDecoder;     // decoder requires this
    ImageCache *imageCache;
    Thumb *thumb;                   // decoder requires this
    bool fileSelectionChanged;
    int instance;                   // new instance for each folder to detect conflict
    Reader *r;                      // terse ptr for current decoder
    int sfRowCount;
    int dmRowCount;
    int metaReadCount;
    int metaReadItems;
    double expansionFactor = 1.2;
    int iconLimit;                  // iconChunkSize * expansionFactor
    int redoCount = 0;
    int redoMax = 5;
    bool imageCacheTriggered;
    
    // bool isDispatching;
    bool success;
    bool isDone;
    bool aIsDone;
    bool bIsDone;
    bool quitAfterTimeoutInitiated;

    int startRow = 0;
    int prevStartRow = -1;
    int lastRow;
    int nextRow = 0;
    int a = 0;
    int b = -1;
    bool isAhead;
    bool isNewStartRowWhileDispatching;
    bool needMeta;
    bool needIcon;
    QString src;

    QStringList err;

    bool debugLog;
    int col0Width = 40;
    QString msElapsed();

    QElapsedTimer t;
    QElapsedTimer tAbort;
    QTimer *quitTimer;
    quint32 ms;
};

#endif // METAREAD_H

