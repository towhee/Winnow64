#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QtWidgets>
#include <QObject>
#include "Cache/cachedata.h"
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

    quint64 getImCacheSize();         // add up total MB cached
    void removeFromCache(QStringList &pathList);
    void rename(QString oldPath, QString newPath);

    bool getAutoMaxMB();
    quint64 getMaxMB();
    bool getShowCacheStatus();

    void updateStatus(int instruction, QString source); // update cached send signal
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

    enum StatusAction {
        Clear,
        All,
        Size
    };
    QStringList statusAction {"Clear", "All", "Size"};

    bool debugCaching = false;
    bool debugLog = false;

signals:
    void stopped(QString src);
    void waitingForRow(int sfRow);
    void setCached(int sfRow, bool isCached, int instance);
    void decode(int sfRow, int instance);
    void setValSf(int sfRow, int sfCol, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft); // not used
    void showCacheStatus(int instruction, bool isAutoSize,
                         quint64 currMB, quint64 maxMB, int targetFirst, int targetLast,
                         QString source = "");
    void centralMsg(QString msg);       // not being used
    void updateIsRunning(bool, bool);   // (isRunning, showCacheLabel)
    void refreshViews(QString fPath, bool isCached, QString src);

public slots:
    void start();
    void stop();
    // void newInstance();
    void abortProcessing();
    void initialize();

    void updateImageCacheParam(quint64 cacheSizeMB, quint64 cacheMinMB,
                               bool isShowCacheStatus);
    void setAutoMaxMB(bool autoSize);
    void setMaxMB(quint64 mb);
    void setShowCacheStatus(bool isShowCacheStatus);

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
    QVector<int> pressureHistory;   // holds last 50 pressure readings

    int currRow;                // current image
    int prevRow;                // used to establish direction of travel

    bool isForward;             // direction of travel for caching algorithm
    int step;                   // difference between key and prevKey
    int sumStep;                // sum of step until threshold
    int directionChangeThreshold;//number of steps before change direction of cache
    bool autoMaxMB;             // use releavePressure() to set maxMB
    quint64 maxMB;              // maximum MB available to cache
    quint64 minMB = 500;        // minimum MB available to cache
    int targetFirst;            // beginning of target range to cache
    int targetLast;             // end of the target range to cache
    int decodeImageCount;
    quint64 decodeImageMsTot;
    quint64 decodeImageMBTot;
    bool isShowCacheStatus;     // show in app status bar
    bool firstDispatchNewDM;

    // --- Cache pressure section ---
    bool firstAdjustFreePass = false;     // bypass rapid/cooldown once after folder change
    qint64 lastAdjustMs = 0;              // last time we changed maxMB
    qint64 lastMoveMs   = 0;              // last time setTargetRange saw a move
    int    lastKeyForMotion = -1;         // last row key we saw

    // motion heuristics (EMA = Exponential Moving Average)
    double emaStepMs = -1.0;              // EMA of time between successive forward steps
    int    forwardStreak = 0;             // count of consecutive forward steps

    // pressure bookkeeping
    int    pressure = INT_MAX;     // rows ahead to first queued target; INT_MAX if none
    int    cushion = 0;

    // tuning knobs (feel free to expose via settings)
    int    pressureHigh  = 10;             // “need more cache” when pressure <= this
    int    pressureLow   = 20;            // “too much cache” when pressure > this
    int    adjustCooldownMs = 100;        // min delay between cache-size adjustments
    int    rapidStepMsThreshold = 70;     // user is “rapid” if EMA step ≤ this (≈14 FPS)
    int    rapidMinStreak = 5;            // need at least N consecutive forward steps

    // step sizing (we try to resize in chunks ≈ a few images)
    float  emaItemMB = -1.0f;             // EMA of item size seen while queuing
    int    minStepMB = 128;               // never resize by less than this
    int    maxStepMB = 1024;              // never resize by more than this

    // optional ceiling; if you already track available mem, use that instead
    quint64 maxMBCeiling = G::availableMemoryMB * 0.9;          // soft cap (MB) to prevent runaway growth

    // pressure helpers
    static qint64 nowMs();
    inline void noteItemSizeMB(float szMB);
    inline void updateMotion(int key, bool isForwardNow);
    inline bool isRapidForward() const;
    inline int calcResizeStepMB() const;

    void releavePressure();
    bool isCushion();

    // --- End Cache pressure section ---


    void launchDecoders(QString src);
    bool okToCache(int id, int sfRow);
    bool nullInImCache();
    void cacheImage(int id, int cacheKey);  // make room and add image to imageCache
    bool cacheUpToDate();           // target range all cached
    void decodeNextImage(int id, int sfRow);   // launch decoder for the next image in cacheItemList
    void trimOutsideTargetRange();// define start and end key in the target range to cache
    void adjustCacheMem(quint64 ms, quint64 mb);
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
                       int &amount, bool &isDone, quint64 &sumMB);
    void setTargetRange(int key);
    bool waitForMetaRead(int sfRow, int ms);
    void log(const QString function, const QString comment = "");
};

#endif // IMAGECACHE_H
