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

    void dmRowRemoved(int dmRow);
    QString diagnostics();
    QString reportMetaCache();

    bool isRunning = false;

    int iconChunkSize;
    int firstIconRow;
    int lastIconRow;

signals:
    void runStatus(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void addToDatamodel(ImageMetadata m);
    void addToImageCache(ImageMetadata m);
    void startImageCache();

    void updateIconBestFit();  // req'd?
    void done();               // not being used - req'd?

public slots:
    void initialize();
    void stop();
    void start(int row = 0, QString src = "");

private:
    void read(int startRow = 0, QString src = "");
    void readRow(int sfRow);
    void readMetadata(QModelIndex sfIdx, QString fPath);
    void readIcon(QModelIndex sfIdx, QString fPath);
    void iconMax(QPixmap &thumb);
    void cleanupIcons();
    bool isNotLoaded(int sfRow);
    bool inIconRange(int sfRow);

    QMutex mutex;
//    QWaitCondition condition;
    bool abort;

    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;
    int dmInstance;
    int visibleIconCount;
    int sfRowCount;
    int iconLimit;                  // iconChunkSize * expansionFactor

    int newStartRow = -1;

    bool imageCachingStarted = false;

    QList<int> rowsWithIcon;

    // lists not req'd
    QList<int> visibleIcons;
    QList<int> outsideIconRange;

    bool debugCaching = false;
    QElapsedTimer t;
};
#endif // METAREAD_H
