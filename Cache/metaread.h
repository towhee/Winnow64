#ifndef METAREAD_H
#define METAREAD_H

#include <QObject>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"

class MetaRead : public QObject
{
    Q_OBJECT

public:
    MetaRead(QObject *parent, DataModel *dm, Metadata *metadata);
    ~MetaRead() override;
    enum Action {
        FileSelection,
        Scroll,
        SizeChange
    } action;
    void dmRowRemoved(int dmRow);
    QString diagnostics();
    QString reportMetaCache();

    QWaitCondition condition;
    bool abort;
    bool isRunning = false;
    bool isRestart;

    int iconChunkSize;
    int firstIconRow;
    int lastIconRow;

signals:
    void done();
    void stopped();
    void startAtRow(int row, QString src);
    void okayToStart(int newRow);
    void runStatus(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void addToDatamodel(ImageMetadata m);
    void addToImageCache(ImageMetadata m);
    void setIcon(QModelIndex dmIdx, const QPixmap &pm, int instance, QString src);
    void clearOutOfRangeIcons(int startRow);
    void setImageCachePosition(QString fPath);      // not used
    void startImageCache();
    void updateIconBestFit();

public slots:
    void initialize();
    void stop();
    void start(int row = 0, QString src = "");
    void read(int startRow = 0, QString src = "");
//    void scroll(int sfRow, QString src = "");
//    void sizeChange(int sfRow, QString src = "");

private:
    void readRow(int sfRow);
    void readMetadata(QModelIndex sfIdx, QString fPath);
    void readIcon(QModelIndex sfIdx, QString fPath);
    void iconMax(QPixmap &thumb);
    void cleanupIcons();
    bool isNotLoaded(int sfRow);
    bool inIconRange(int sfRow);

    QMutex mutex;
//    QWaitCondition condition;
//    bool abort;
//    bool isRunning = false;
    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;
    int dmInstance;
    int adjIconChunkSize;
    int sfRowCount;
    int visibleIconCount;
    int sfStart;

    int sfRow;
    int newStartRow = -1;
    int newRow;

    const QPixmap nullPm;

    bool imageCachingStarted = false;
    QList<int> priorityQueue;

    // lists not req'd
    QList<int> iconsLoaded;
    QList<int> visibleIcons;
    QList<int> outsideIconRange;

    bool debugCaching = false;
    QElapsedTimer t;
};
#endif // METAREAD_H
