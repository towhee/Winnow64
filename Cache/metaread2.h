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

    void setIcon(QModelIndex dmIdx, const QPixmap pm, int fromInstance, QString src);

    void fileSelectionChange(QModelIndex sfIdx,
                             QModelIndex idx2 = QModelIndex(),
                             bool clearSelection = false,
                             QString src = "MetaRead::triggerCheck");

    void done();

public slots:
    void initialize();
    void dispatch(int id);
    void setStartRow(int row, bool fileSelectionChanged, QString src = "");
    void quitAnyway();

protected:
    void run() Q_DECL_OVERRIDE;

private:
    void read(int startRow = 0, QString src = "");// decoder

    void dispatchReaders();
    bool readersRunning();
    void redo();

//    void iconMax(QPixmap &thumb);
//    bool isNotLoaded(int sfRow);
//    bool inIconRange(int sfRow);

    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    bool abortCleanup;

    DataModel *dm;
    Metadata *metadata;
    FrameDecoder *frameDecoder;     // decoder requires this
    ImageCache *imageCache;
    Thumb *thumb;                   // decoder requires this
    int instance;                   // new instance for each folder to detect conflict
    QVector<Reader*> reader;        // all the decoders
    Reader *d;                      // terse ptr for current decoder
    int readerCount;
    int sfRowCount;
    int dmRowCount;
    int metaReadCount;
    int metaReadItems;
    double expansionFactor = 1.2;
    int iconLimit;                  // iconChunkSize * expansionFactor
    int redoCount = 0;
    int redoMax = 5;
    
    bool isDispatching;
    bool isDone;
    bool aIsDone;
    bool bIsDone;
    bool firstDone;

    QList<int> toRead;

    int startRow = 0;
    int a = 0;
    int b = -1;
    bool isNewStartRowWhileStillReading;
    QString src;

    QList<int> rowsWithIcon;
    QStringList err;

    bool isDebug = false;
    QElapsedTimer t;
    QElapsedTimer tAbort;
    quint32 ms;
};

#endif // METAREAD2_H

