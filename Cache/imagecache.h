#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QtWidgets>
#include <QObject>
#include "Cache/cachedata.h"
#include "Cache/cacheadapt.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/pixmap.h"
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

class ImageCache : public QObject
{
    Q_OBJECT
public:
    ImageCache(QObject *parent, ImageCacheData *icd, DataModel *dm);
    ~ImageCache() override;

    QThread imageCacheThread;  // Separate thread for ImageCache

    bool isRunning() const;

    float getImCacheSize();         // add up total MB cached
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

    bool isIdle();

    int col0Width = 50;

    int decoderCount = 1;

    bool debugCaching = false;
    bool debugLog = false;

signals:
    void stopped(QString src);
    void waitingForRow(int sfRow);
    void setCached(int sfRow, bool isCached, int instance);
    void decode(int sfRow, int instance);
    void setValSf(int sfRow, int sfCol, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft); // not used
    void showCacheStatus(QString instruction,
                         float currMB, int maxMB, int targetFirst, int targetLast,
                         QString source = "");
    void centralMsg(QString msg);       // not being used
    void updateIsRunning(bool, bool);   // (isRunning, showCacheLabel)
    void refreshViews(QString fPath, bool isCached, QString src);

public slots:
    void start();
    void stop();
    // void newInstance();
    void abortProcessing();
    void initialize(int cacheSizeMB, int cacheMinMB,
                    bool isShowCacheStatus, int cacheWtAhead);
    void updateImageCacheParam(int cacheSizeMB, int cacheMinMB,
                               bool isShowCacheStatus, int cacheWtAhead);
    void updateInstance();
    void fillCache(int id);
    void setCurrentPosition(QString path, QString src);
    void filterChange(QString currentImageFullPath, QString source = "");
    void cacheSizeChange();         // flag when cache size is changed in preferences
    void colorManageChange();
    void refreshImageCache();
    void reloadImageCache();
    void removeCachedImage(QString fPath); // remove image from imageCache and update status
    void updateToCache();
    // --- Adaptive buffering public hooks (safe to call via QueuedConnection) ---  // >>> NEW
    void noteUnderrun();          // call when UI asked for next image and it wasn't ready
    void noteSmoothTick();        // call periodically (e.g., every 2s) when no underruns

private slots:
    void dispatch();  // Main processing loop

private:
    // Not being used - research how this works and if it is a good idea
    QAtomicInt running {0};         // 0 = not running, 1 = running

    QMutex gMutex;
    QWaitCondition condition;
    int instance;                   // incremented on every DataModel::load
    bool abort;
    int abortDelay = 1000;
    bool isDecoders = false;

    // rgh retry not being used
    int retry = 0;
    int maxAttemptsToCacheImage = 10;

    bool isInitializing;

    ImageCacheData *icd;                // ptr to all cache data (reentrant)
    DataModel *dm;
    Metadata *metadata;

    QVector<ImageDecoder*> decoders;        // all the decoders
    QVector<QThread*> decoderThreads;       // all the decoder threads
    QVector<bool> cycling;                  // all the decoders activity
    struct CacheItem {
        bool isCaching;
        QString msg;
        int decoderId;
        int instance;
    };
    QList<int> toCache;
    QHash<int,CacheItem> toCacheStatus;
    QList<int> removedFromCache;// items removed in trimOutsideTargetRange
    QVector<float> imageSize;

    int currRow;                // current image
    int prevRow;                // used to establish direction of travel

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
    bool firstDispatchNewDM;

    // --- Adaptive buffering state ----------------------------------------------
    CacheStats cacheStats;
    CacheTargets cacheTargets;
    MemCaps memCaps;
    CacheController cacheCtl;

    // --- NEW Adaptive buffering state (you already added Cache* types earlier) ----
    bool isBootstrap = true;             // >>> NEW
    int bootNavEvents = 0;               // >>> NEW
    int bootDecodeSamples = 0;           // >>> NEW

    // Tunables for bootstrap; adjust if you wish
    static constexpr double vBootFPS = 20.0;  // assume 20 images/sec initially
    static constexpr double HBootSec = 0.60;  // aim to cover ~0.6 s burst
    static constexpr int    bootMinAhead = 10;   // floor
    static constexpr int    bootMinBehind = 5;   // floor; ~2:1 ratio
    static constexpr int    warmNavEventsNeeded = 3;   // to exit bootstrap
    static constexpr int    warmDecodeSamplesNeeded = 8; // to exit bootstrap

    // target counts computed for current position/direction
    int targetAheadImgs{2};
    int targetBehindImgs{1};

    // nav timing helpers
    QElapsedTimer navTimer;
    qint64 lastNavNs{0};
    int lastNavDir{0}; // +1 next, -1 prev

    // sprint detection
    bool sprintActive = false;

    QTimer adaptTimer; // periodic recompute & smooth tick

    void recomputeAdaptiveTargets();             // compute targetAheadImgs/targetBehindImgs
    void recordNavEvent(int dir);                // (+1/-1) updates cacheStats
    void recordDecodeSample(double sec, std::size_t bytes); // updates cacheStats

    void updateTargetWindowForStatus();
    // end adaptive buffering state

    void launchDecoders(QString src);
    bool okToCache(int id, int sfRow);
    bool nullInImCache();
    void cacheImage(int id, int cacheKey);  // make room and add image to imageCache
    bool cacheUpToDate();           // target range all cached
    void decodeNextImage(int id, int sfRow);   // launch decoder for the next image in cacheItemList
    void trimOutsideTargetRange();// define start and end key in the target range to cache
    bool anyDecoderCycling();        // All decoder status is ready
    void setDirection();            // caching direction
    bool okToDecode(int sfRow, int id, QString &msg);
    int nextToCache(int id);        // find highest priority not cached
    void toCacheRemove(int sfRow);
    void toCacheAppend(int sfRow);
    void memChk();                  // still room in system memory for cache?
    bool instanceClash(bool id);
    bool isValidKey(int key);

    void updateTargets(bool dotForward, bool isAhead, int &pos,
                       int &amount, bool &isDone, float &sumMB);
    void setTargetRange(int key);
    bool waitForMetaRead(int sfRow, int ms);
    void log(const QString function, const QString comment = "");
};

#endif // IMAGECACHE_H
