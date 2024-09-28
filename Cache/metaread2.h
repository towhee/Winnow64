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

    int readerCount;
    QVector<Reader*> reader;        // all the decoders

    void syncInstance();
    bool stop();
    QString diagnostics();
    QString reportMetaCache();
    void cleanupIcons();

    bool isDispatching;

    int firstIconRow;
    int lastIconRow;

    bool showProgressInStatusbar = true;
    bool isDebug = false;

signals:
    void stopped(QString src);
    void updateScroll();
    void runStatus(bool/*isRunning*/, bool/*showCacheLabel*/, bool /*success*/, QString/*calledBy*/);
    void centralMsg(QString message);
    void okToSelect(bool ok);
    void updateProgressInFilter(int progress);
    void updateProgressInStatusbar(int progress, int total);

    void setMsToRead(QModelIndex dmIdx, QVariant value, int instance, QString src = "MetaRead2::dispatch",
                     int role = Qt::EditRole, int align = Qt::AlignRight);
    void setIcon(QModelIndex dmIdx, const QPixmap pm, int fromInstance, QString src);

    void fileSelectionChange(QModelIndex sfIdx,
                             QModelIndex idx2 = QModelIndex(),
                             bool clearSelection = false,
                             QString src = "MetaRead2::dispatch");

    void done();

public slots:
    void initialize();
    void dispatch(int id);
    void setStartRow(int row, bool fileSelectionChanged, QString src = "");
    void quitAfterTimeout();

protected:
    void run() Q_DECL_OVERRIDE;

private:
    void read(int startRow = 0, QString src = "");// decoder

    void dispatchReaders();
    void dispatchFinished(QString src);
    bool allMetaIconLoaded();
    void redo();
    int pending();
    inline bool needToRead(int row);
    bool nextA();
    bool nextB();
    bool nextRowToRead();
    void emitFileSelectionChangeWithDelay(const QModelIndex &sfIdx, int msDelay);
    // void emitFileSelectionChangeWithDelay(const QModelIndex &sfIdx,
    //                                       const QModelIndex &idx2 = QModelIndex(),
    //                                       bool clearSelection = false,
    //                                       const QString &src = "MetaRead2::dispatch");

    QMutex mutex;
    QWaitCondition condition;
    bool abort;

    QEventLoop runloop;

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
    
    // bool isDispatching;
    bool success;
    bool isDone;
    bool aIsDone;
    bool bIsDone;
    bool quitAfterTimeoutInitiated;

    //QList<int> toRead;  // to be removed

    int startRow = 0;
    int lastRow;
    int nextRow = 0;
    int a = 0;
    int b = -1;
    bool isAhead;
    bool isNewStartRowWhileStillReading;
    QString src;

    QStringList err;

    QElapsedTimer t;
    QElapsedTimer tAbort;
    quint32 ms;
};

#endif // METAREAD2_H

