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

class MetaRead : public QThread
{
    Q_OBJECT

public:
    MetaRead(QObject *parent,
             DataModel *dm,
             Metadata *metadata,
             FrameDecoder *frameDecoder);
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
    void runStatus(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void centralMsg(QString message);
    void updateProgress(int progress);
    void addToDatamodel(ImageMetadata m, QString src);
    void addToImageCache(ImageMetadata m);
    void setIcon(QModelIndex dmIdx, const QPixmap pm, int fromInstance, QString src);
    void triggerImageCache(QString src);

    void updateIconBestFit();  //r req'd?
    void done();               // not being used - req'd?

public slots:
    void initialize();
    void setCurrentRow(int row = 0, QString src = "");
    int interrupt();

protected:
    void run() Q_DECL_OVERRIDE;

private:
    void read(int startRow = 0, QString src = "");
    void readRow(int sfRow);
    bool readMetadata(QModelIndex sfIdx, QString fPath);
    void readIcon(QModelIndex sfIdx, QString fPath);
//    void iconMax(QPixmap &thumb);
//    bool isNotLoaded(int sfRow);
//    bool inIconRange(int sfRow);

    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    bool changeStartRowWhileRunning;
    bool interrupted;
    int interruptedRow;

    DataModel *dm;
    Metadata *metadata;
    FrameDecoder *frameDecoder;
    Thumb *thumb;
    int instance;
//    int visibleIconCount;
    int sfRowCount;
    int dmRowCount;
    int metaReadCount;
    double expansionFactor = 1.2;
    int iconLimit;                  // iconChunkSize * expansionFactor
    int imageCacheTriggerCount = 50;

    int startRow = 0;
    QString src;
    QString folderPath;

    bool imageCachingStarted = false;

    QList<int> rowsWithIcon;

    bool debugCaching = false;
    QElapsedTimer t;
    QElapsedTimer tAbort;
    quint32 ms;
};

#endif // METAREAD_H
