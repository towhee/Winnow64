#ifndef METAREAD_H
#define METAREAD_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
//#include "Metadata/imagemetadata.h"
//#include "Cache/imagecache2.h"
#include "Image/thumb.h"

class MetaRead : public QThread
{
    Q_OBJECT

public:
    MetaRead(QObject *parent, DataModel *dm);
    ~MetaRead() override;
    void stop();
    void read();
    void initialize();
    int iconChunkSize;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void done();
    void addToDatamodel(ImageMetadata m);
    void addToImageCache(ImageMetadata m);
    void setImageCachePosition(QString fPath);
    void delayedStartImageCache();

private:
    void readRow(int sfRow);
    void readMetadata(QModelIndex sfIdx, QString fPath);
    void readIcon(QModelIndex sfIdx, QString fPath);
    void iconMax(QPixmap &thumb);
    void iconCleanup();
    void buildPriorityQueue();
    bool isNotLoaded(int sfRow);

    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
//    ImageCache2 *imageCacheThread2;
    Thumb *thumb;
    int adjIconChunkSize;
    int sfRowCount;
    int visibleIcons;

    QList<int> priorityQueue;
    QList<int> iconsLoaded;
};

#endif // METAREAD_H
