#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QtWidgets>
#include <QObject>
#include "Cache/cachedata.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/pixmap.h"
//#include "Image/cacheimage.h"
#include "Cache/imagedecoder.h"
#include <algorithm>         // reqd to sort cache
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <QGradient>
#include <vector>

#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif
#include "Utilities/icc.h"

#ifdef Q_OS_MAC
#include "Utilities/mac.h"
#endif

class ImageCache : public QThread
{
    Q_OBJECT
public:
    ImageCache(QObject *parent, ImageCacheData *icd, DataModel *dm);
    ~ImageCache() override;

    void initImageCache(int &cacheSizeMB, int &cacheMinMB,
                        bool &isShowCacheStatus, int &cacheWtAhead);
    void updateImageCacheParam(int &cacheSizeMB, int &cacheMinMB, bool &isShowCacheStatus,
                               int &cacheWtAhead);
    void filterChange(QString &currentImageFullPath, QString source = "");
    void stop(QString src);
    bool cacheUpToDate();           // target range all cached
    float getImCacheSize();         // add up total MB cached
    void addToCache(QStringList &pathList);
    void removeFromCache(QStringList &pathList);
    void rename(QString oldPath, QString newPath);

    void updateStatus(QString instruction, QString source); // update cached send signal
    QString reportToCache();
    QString diagnostics();
    QString reportCacheParameters();
    QString reportCacheDecoders();
    QString reportCacheItemList(QString title = "");
    QString reportImCache();
    QString reportImCacheRows();
    QString reportToCacheRows();
    int col0Width = 50;

    int decoderCount = 1;

    bool debugCaching = false;
    bool debugLog = false;
    QString source;                 // temp for debugging

signals:
    void setValueSf(QModelIndex sfIdx, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft); // not used
    void showCacheStatus(QString instruction,
                         float currMB, int maxMB, int targetFirst, int targetLast,
                         QString source = "");
    void centralMsg(QString msg);
    void updateIsRunning(bool, bool);   // (isRunning, showCacheLabel)
    void refreshViews(QString fPath, bool isCached, QString src);

protected:
    void run() Q_DECL_OVERRIDE;

public slots:
    void fillCache(int id);
    void returningDecoder(int id);
    void setCurrentPosition(QString path, QString src);
    void datamodelFolderCountChange(QString src);
    void cacheSizeChange();         // flag when cache size is changed in preferences
    void colorManageChange();
    void refreshImageCache();
    void reloadImageCache();
    void removeCachedImage(QString fPath); // remove image from imageCache and update status
    void updateToCache();

private:
    QMutex gMutex;
    QWaitCondition condition;
    int instance;                   // incremented on every DataModel::load
    bool abort;
    bool isDecoders = false;

    // rgh retry not being used
    int retry = 0;
    int maxAttemptsToCacheImage = 10;

    bool isInitializing;

    ImageCacheData *icd;                // ptr to all cache data (reentrant)
    DataModel *dm;
    Metadata *metadata;

    QVector<ImageDecoder*> decoder;     // all the decoders
    enum Status {NotCached, Caching, Cached, Failed};
    QStringList statusText{"NotCached", "Caching  ", "Cached   ", "Failed   "};
    struct CacheStatus {
        Status status;
        int decoderId;
        int instance;
    };
    QList<int> toCache;
    QHash<int,CacheStatus> toCacheStatus;

    int key;                    // current image
    int prevKey;                // used to establish direction of travel
    // int toCacheKey;             // next file to cache
    bool isForward;             // direction of travel for caching algorithm
    int step;                   // difference between key and prevKey
    int sumStep;                // sum of step until threshold
    int directionChangeThreshold;//number of steps before change direction of cache
    int wtAhead;                // ratio cache ahead vs behind * 10 (ie 7 = ratio 7/10)
    int maxMB;                  // maximum MB available to cache
    int minMB;                  // minimum MB available to cache
    int targetFirst;            // beginning of target range to cache
    int targetLast;             // end of the target range to cache
    bool isShowCacheStatus;     // show in app status bar

    void launchDecoders(QString src);
    void cacheImage(int id, int cacheKey);  // make room and add image to imageCache
    void decodeNextImage(int id, int sfRow);   // launch decoder for the next image in cacheItemList
    void trimOutsideTargetRange();// define start and end key in the target range to cache
    bool allDecodersReady();        // All decoder status is ready
    void setDirection();            // caching direction
    int nextToCache(int id);        // find highest priority not cached
    void toCacheRemove(int sfRow);
    void toCacheAppend(int sfRow);
    void memChk();                  // still room in system memory for cache?
    bool isValidKey(int key);

    void updateTargets(bool dotForward, bool isAhead, int &pos,
                       int &amount, bool &isDone, float &sumMB);
    void setTargetRange(int key);
    void log(const QString function, const QString comment = "");
};

#endif // IMAGECACHE_H
