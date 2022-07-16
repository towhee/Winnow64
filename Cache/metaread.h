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
    void okayToStart(int newRow);
    void runStatus(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void addToDatamodel(ImageMetadata m);
    void addToImageCache(ImageMetadata m);
    void setIcon(QModelIndex dmIdx, QPixmap &pm, int instance);
    void setImageCachePosition(QString fPath);      // not used
    void startImageCache();
    void updateIconBestFit();

public slots:
    void initialize();
    void restart(int newRow);
    void stop();
    void start();
    void read(int sfRow = 0, QString src = "");
//    void scroll(int sfRow, QString src = "");
//    void sizeChange(int sfRow, QString src = "");

private:
    void readRow(int sfRow);
    void readMetadata(QModelIndex sfIdx, QString fPath);
    void readIcon(QModelIndex sfIdx, QString fPath);
    void iconMax(QPixmap &thumb);
    void cleanupIcons();
    void buildMetadataPriorityQueue(int sfRow);
    bool isNotLoaded(int sfRow);
    bool okToLoadIcon(int sfRow);
    void updateIcons();

    QMutex mutex;
//    QWaitCondition condition;
//    bool abort;
//    bool isRunning = false;
    int newRow;
    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;
    int dmInstance;
    int adjIconChunkSize;
    int sfRowCount;
    int visibleIconCount;
    int sfStart;
    int sfRow;

    bool imageCachingStarted = false;
    QList<int> priorityQueue;
    QList<int> iconsLoaded;
    QList<int> visibleIcons;
    QList<int> outsideIconRange;

    bool debugCaching = false;
    QElapsedTimer t;
};
#endif // METAREAD_H
