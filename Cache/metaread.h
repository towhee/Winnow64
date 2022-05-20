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
    enum Action {
        FileSelection,
        Scroll,
        SizeChange
    } action;
    void read(Action action = Action::FileSelection, int sfRow = 0, QString src = "");
    void initialize();
    void dmRowRemoved(int dmRow);
    int iconChunkSize;
    int firstVisible;
    int lastVisible;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void done();
    void addToDatamodel(ImageMetadata m);
    void addToImageCache(ImageMetadata m);
    void setIcon(QModelIndex dmIdx, QPixmap &pm);
    void setImageCachePosition(QString fPath);      // not used
    void delayedStartImageCache();
    void updateIconBestFit();

private:
    void readRow(int sfRow);
    void readMetadata(QModelIndex sfIdx, QString fPath);
    void readIcon(QModelIndex sfIdx, QString fPath);
    void iconMax(QPixmap &thumb);
    void cleanupIcons();
    void updateIcons();
    void buildMetadataPriorityQueue(int sfRow);
    bool isNotLoaded(int sfRow);
    bool isVisible(int sfRow);

    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;
    int adjIconChunkSize;
    int sfRowCount;
    int visibleIconCount;
    int sfStart;

    bool imageCachingStarted = false;
    QList<int> priorityQueue;
    QList<int> iconsLoaded;
    QList<int> visibleIcons;

};

#endif // METAREAD_H
