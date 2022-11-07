#ifndef METAREADLINEAR_H
#define METAREADLINEAR_H

#include <QObject>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"
#include "Main/global.h"

class MetaReadLinear
{
    Q_OBJECT
public:
    MetaReadLinear(QObject *parent,
                   DataModel *dm,
                   Metadata *metadata,
                   FrameDecoder *frameDecoder
                  );
    virtual ~MetaReadLinear(void);
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    FrameDecoder *frameDecoder;
    Thumb *thumb;

    int startRow;
    int endRow;
    int tpp;                                    // thumbsPerPage;
    int countInterval = 100;                    // report progress interval

    // icon caching
    QList<int> iconsCached;

    bool foundItemsToLoad;
    bool allIconsLoaded;
    int instance;
    int metadataChunkSize = 250;
    int defaultMetadataChunkSize = 250;
    bool cacheAllIcons = false;
    bool isRefreshFolder = false;

    bool cacheIcons;
    bool cacheAllMetadata = false;

    int firstIconVisible;
//    int midIconVisible;
    int lastIconVisible;
    int visibleIcons;

    int metadataTry = 3;
    int iconTry = 3;

    void stop();
    bool loadIcon(int sfRow);
    void iconMax(QPixmap &thumb);
    void updateIconLoadingProgress(int count, int end);
//    void createCacheStatus();
//    void updateCacheStatus(int row);
//    void readAllMetadata();
    void readIconChunk();
//    void readMetadataChunk();
//    void iconCleanup();
//    bool anyItemsToLoad();

signals:
    void updateIsRunning(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void showCacheStatus(QString);            // row, clear progress bar
//    void addToDatamodel(ImageMetadata m, QString src);
//    void setIcon(QModelIndex dmIdx, QPixmap &pm, int instance, QString src);
//    void setIconCaching(int sfRow, bool state);
    void updateIconBestFit();
    void selectFirst();

};

#endif // METAREADLINEAR_H
