#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QtWidgets>
#include <QObject>
#include <atomic>
#include "Cache/cachedata.h"
#include "Cache/deferredrows.h"
#include "Cache/decodeverdict.h"
#include <functional>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/pixmap.h"
#include "Cache/imagedecoder.h"
#include "Utilities/movingavg.h"
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
    /*  ANY THREAD. Re-key a renamed file's decoded image and bookkeeping. icd and the
        per-path state are both locked, and nothing row-keyed is touched. */
    void rename(QString oldPath, QString newPath);

    /*  THE ONE WAY TO CHANGE WHAT THE CACHE HOLDS FROM OUTSIDE IT (Documentation
        4.4.13). Any thread.

        invalidate: these images no longer depict their files (edited outside Winnow, a
        new Develop recipe or devPreview, rewritten in place, deleted). They leave icd
        NOW, so a loupe load right after this misses instead of showing the old picture,
        and a decode already running for one of them is superseded (okToCache discards
        it). On the ImageCache thread the bookkeeping follows: per-path state reset, the
        model's cache badge set to CacheNone, and -- with refill -- a dispatch, which
        queues again whatever is still in the target range, the current image first.
        refill = false when re-decoding now would be waste: the image being edited in
        Develop, whose loupe shows renders, not the cache.

        replace: swap the cached image for one already derived from it (a rotation). Only
        if it is still cached; a decode in flight is superseded, because it reads the
        old orientation. */
    void invalidate(const QStringList &keys, const QString &src, bool refill = true);
    void replace(const QString &key, const QImage &image);

    /*  Run fn on the ImageCache thread and return its result: the diagnostic reports
        read state only that thread may touch (toCache, toCacheStatus, the target
        range). Runs fn directly when already there or when the thread is not running.
        Waits at most timeoutMs; a busy thread yields a note instead of a hang. */
    QString fromCacheThread(std::function<QString()> fn, int timeoutMs = 3000);

    bool getAutoMaxMB();
    QString getAutoStrategy();
    quint64 getMaxMB();
    quint64 getMaxMBCeiling();

    /* Thread-safe throttle flag set from the GUI-thread memory watchdog. Honored at the
       single decode choke point (decodeNextImage) so no new full-resolution decode starts
       while under memory pressure. */
    void setMemoryThrottled(bool on);
    bool isMemoryThrottled() const;
    /* Thread-safe append to the memory-warning ring buffer surfaced in diagnostics(). */
    void noteMemoryWarning(const QString &msg);

    void updateStatus(int instruction, QString source); // update cached send signal
    QString reportToCache();
    QString diagnostics();
    QString reportHealthChecks();
    QString reportLifetimeCounters();
    QString reportCacheParameters();
    QString reportCacheDecoders();
    QString reportPressureItemList();
    QString reportCacheItemList(QString title = "");
    QString reportImCache();
    QString reportImCacheRows();
    QString reportToCacheRows();
    QString reportMemoryWarnings();
    void debugRunStatus();

    bool isIdle();

    int col0Width = 50;

    int decoderCount = 1;

    enum StatusAction {Clear, All, Size, InfoOnly};
    QStringList statusAction {"Clear", "All", "Size", "InfoOnly"};

    enum AutoStrategy {Frugal, Moderate, Greedy, Ignore};
    QStringList autoStrategyStr {"Frugal", "Moderate", "Greedy", "Ignore"};

    bool debugCaching = false;
    bool debugLog = false;
    bool autoLogStalls = false;          // dump diagnostics() on stall (throttled)
    qint64 lastStallSnapshotMs = 0;      // last time we wrote a stall snapshot

signals:
    void stopped(QString src);
    void setCached(int sfRow, QString fPath, int cacheSource, int instance);  // G::CacheSource
    void setIcon(int dmRow, const QImage &im, int fromInstance, QString src);
    void decode(int sfRow, int instance);
    /* By row key, never sfRow: see DataModel::setValKey. */
    void setValKey(QString key, int col, QVariant value, int instance, QString src,
                   int role = Qt::EditRole);
    void showCacheStatus(int instruction, bool isAutoSize,
                         quint64 currMB, quint64 maxMB, int targetFirst, int targetLast,
                         QString source = "");
    void updateIsRunning(bool, bool);   // (isRunning, showCacheLabel)
    void refreshViews(QString fPath, bool isCached, QString src);
    /* Relayed from a decoder thread: per-tile RAW demosaic progress for MW's status-bar
       "Demosaic" row (see MW::onDemosaicProgress). */
    void demosaicProgress(QString fPath, int done, int total);

public slots:
    void start();
    void stop();
    // void newInstance();
    void abortProcessing();
    void initialize();

    void setAutoMaxMB(bool autoSize, AutoStrategy strategy = AutoStrategy::Ignore);
    void setMaxMB(quint64 mb);

    void updateInstance();
    // doneStatus/doneSfRow/doneImage/doneFPath/doneMsToDecode are the snapshot
    // carried by ImageDecoder::done; defaulted so non-signal callers still work.
    void fillCache(int id,
                   int doneStatus = -1,
                   int doneSfRow = -1,
                   QImage doneImage = QImage(),
                   QString doneFPath = QString(),
                   qint64 doneMsToDecode = 0,
                   int doneCacheSource = -1);
    void setCurrentPosition(QString path, QString src);
    void filterChange(QString currentImageFullPath, QString source = "");
    void cacheSizeChange();         // flag when cache size is changed in preferences
    void colorManageChange();
    void refreshImageCache();
    void reloadImageCache();
    /* reloadImageCache, with fPath's entry supplied instead of decoded (leaving Develop:
       MW::setOperationMode). sRGB pixels; colorManage = transform to the display as the
       decoder would (ImageDecoder::colorManage). */
    void reloadImageCacheSeeded(QString fPath, QImage image, bool colorManage);
    void updateToCache();

    /* Memory pressure throttle (driven by MW::memoryWatchdogTick). memoryPause parks
       decoders and shrinks/trims the cache to free memory; memoryResume relaunches
       decoders once the footprint recovers. Queued from the GUI thread so the heavy
       work runs on the ImageCache thread. Inert when Decode Raw is off — small
       JPEG/HEIC decodes never push the footprint to the pause threshold. */
    void memoryPause(quint64 footprintMB, quint64 capMB);
    void memoryResume(quint64 footprintMB);

    /*  DataModel::rowsChanged: rows this cache deferred (metadata not loaded yet) have
        had their metadata written. Dispatches if any of them is still wanted. */
    void onRowsChanged(QStringList keys, int aspects);
    /*  DataModel::proxySnapshotPublished: a pending position or rows deferred because
        the snapshot was behind the model can go now (Documentation 4.4.15). */
    void onSnapshotPublished();

private slots:
    void dispatch();  // Main processing loop

private:
    // Not being used - research how this works and if it is a good idea
    QAtomicInt running {0};         // 0 = not running, 1 = running

    QMutex gMutex;
    /*  Rows waiting on something, never on a blocked thread: see Cache/deferredrows.h.
        Replaces waitForMetaRead, whose QWaitCondition could only be woken by a slot
        queued to the very thread it blocked (Documentation 4.4.10). ImageCache thread
        only. */
    DeferredRows deferred;
    void armIdleBackstop();
    void idleBackstop();
    /*  setCurrentPosition waiting for a proxy snapshot that knows the current path and
        the model's instance (a filter or insert republishes it on the next GUI turn). */
    QString pendingPositionPath;
    QString pendingPositionSrc;
    int positionRetries = 0;
    bool positionRetryArmed = false;
    static constexpr int kPositionRetryMs = 10;
    static constexpr int kMaxPositionRetries = 200;     // 2 s
    int instance;                   // incremented on every DataModel::load
    bool abort;
    int abortDelay = 1000;
    bool isDecoders = false;

    int maxAttemptsToCacheImage = 10;
    /*  Paths whose attempts cap was lifted once by setCurrentPosition (the current image
        always gets one more try). Cleared with the state it overrides. ImageCache thread
        only. */
    QSet<QString> capLifted;
    void readmitCurrent(const QString &fPath);

    /* Memory pressure throttle state. memoryThrottled is set by the GUI-thread watchdog
       (setMemoryThrottled) and read on the ImageCache thread at the decode choke point.
       memoryWarnings is a small ring buffer of timestamped pause/resume/critical events
       printed by diagnostics(); guarded by memWarnMutex since it is written from both
       threads. */
    std::atomic<bool> memoryThrottled{false};
    QMutex memWarnMutex;
    QStringList memoryWarnings;
    static constexpr int maxMemoryWarnings = 50;

    std::atomic<bool> isInitializing;

    ImageCacheData *icd;                // ptr to all cache data (reentrant)
    DataModel *dm;
    // One Metadata per decoder: sharing one across threads raced on ifd->ifdDataHash (crashed 2025-03-08).
    QVector<Metadata*> decoderMetadatas;

    QVector<ImageDecoder*> decoders;        // all the decoders
    QVector<QThread*> decoderThreads;       // all the decoder threads
    QVector<bool> cycling;                  // all the decoders activity

    /* Raw-decode concurrency cap. A full-sensor RAW decode (UnpackCfa->Demosaic->RawColor)
       holds a large float WorkingImage plus demosaic scratch — ~8x the cached 8-bit QImage
       — so fanning all decoderCount threads onto RAWs at once can blow the memory cap even
       though the image cache itself is small. decoderIsRaw[id] marks a decoder currently on
       the sensor path; activeRawDecodes counts them and is bounded by rawDecodeLimit().
       Only engaged when Decode Raw is on for a sensor-decodable file — JPEG/HEIC and the
       Decode-Raw-off embedded-JPG path run at full concurrency, unchanged. */
    QVector<bool> decoderIsRaw;
    std::atomic<int> activeRawDecodes{0};
    int peakActiveRawDecodes = 0;           // instrumentation (diagnostics report)
    bool willUseSensorDecode(int sfRow) const;
    int  rawDecodeLimit(int sfRow) const;
    QString reportMemoryFootprint();        // WorkingImageCache + in-flight raw decode stats
    /*  PER-IMAGE CACHE BOOKKEEPING, OWNED HERE.

        isCaching / isCached / attempts / decoderId / decoder status and error
        message used to live in the DATAMODEL, written through queued setValSf
        signals to the GUI thread and then read straight back off this thread --
        so every read raced the GUI thread's write, and the value read was
        whatever had been applied so far rather than what this thread had just
        decided. A snapshot cannot fix that: these change continuously while the
        cache runs, so any point-in-time copy is stale by construction. They are
        state, and state belongs to whoever owns the decision.

        KEYED BY FILE PATH, not by row. Proxy rows shift under filtering and
        sorting, and icd->imCache is already keyed by path, so a path key means
        the two can never disagree about the same image.

        The model still carries these columns and is still updated through the
        queued signals -- the icon delegate draws the cached badge from
        G::IsCachedColumn and the table shows the rest -- but that is now a
        one-way MIRROR for display, never a source of truth.

        Written on the ImageCache thread; invalidate(), replace() and rename() (any
        thread) also reach it, hence the mutex. */
    struct RowCache {
        bool isCaching = false;
        bool isCached  = false;
        int  attempts  = 0;
        int  decoderId = -1;
        int  status    = 0;             // ImageDecoder::Status
        QString errMsg;
    };
    mutable QMutex cacheStateMutex;
    QHash<QString, RowCache> cacheState;
    /* Keys whose decode was IN FLIGHT when invalidate() or replace() changed them: the
       result depicts what the image used to be, so okToCache discards it and re-queues
       the row. Guarded by cacheStateMutex. */
    QSet<QString> supersededDecodes;
    bool takeSupersededDecode(const QString &fPath);
    void markSuperseded(const QStringList &keys);          // any thread
    /*  ImageCache thread. A supersede mark with no decode running for its key would
        refuse the NEXT, legitimate decode; drop those. */
    void settleSuperseded(const QStringList &keys);
    void onInvalidated(const QStringList &keys, const QString &src, bool refill);

    /*  WHAT EACH DECODER IS WORKING ON, as this thread assigned it (decodeNextImage) --
        the key okToCache files the result under when the decoder reports none, and the
        in-flight test for superseding and resetStaleIsCaching. ImageCache thread only. */
    QVector<QString> decoderPath;
    QSet<QString> inFlightKeys() const;

    /*  THE CACHE'S GENERATION. Bumped by every reload (operation mode, Decode Raw,
        preview source, colour management): a decode started under an older generation
        chose its pixels by the old settings, so okToCache supersedes it (C26).
        decoderGeneration[id] is the generation a decoder was dispatched under.
        ImageCache thread only. */
    int generation = 0;
    QVector<int> decoderGeneration;
    void bumpGeneration(const QString &src);
    void clearForReload(const QString &src);

    /*  Debug aid: records a session issue (once per function) when a function that
        owns ImageCache-thread state runs on another thread (C4). */
    void checkCacheThread(const char *fun);

    /*  THE MODEL IS READ ONLY THROUGH THESE on the ImageCache thread. Each
        takes its own reference to the published view (Datamodel/modelsync.h)
        and answers from that, so nothing here touches dm or dm->sf. Structural
        facts (path, row count, proxy row for a path) come from the snapshot;
        live facts (metadata loaded, cache size estimate, video) from the
        lock-free per-row array. */
    QString pathAt(int sfRow) const;
    QVariant sfVal(int sfRow, int col, int role = Qt::DisplayRole) const;   // reports
    /*  THE QUEUE'S SNAPSHOT (Documentation 4.4.18). Every row number this cache holds
        (toCache, toCacheStatus, currRow, the target range) means a row of queueSnap,
        and the helpers here answer from it, never from a newer one. adoptSnapshot moves
        to the model's latest at the two entry points (setCurrentPosition,
        updateToCache): if any queued row now names another image, the row-numbered
        queue is dropped and rebuilt from the current path. ImageCache thread only. */
    ProxySnapshotPtr queueSnap;
    QString currPath;
    ProxySnapshotPtr qsnap() const { return queueSnap ? queueSnap : dm->proxySnapshot(); }
    void adoptSnapshot();
    int     sfRowOf(const QString &fPath) const;
    int     rowCountSf() const;
    bool    isVideoAt(int sfRow) const;
    bool    metaLoadedAt(int sfRow) const;
    float   cacheMBAt(int sfRow) const;

    RowCache stateOf(const QString &fPath) const;
    void setStateCaching(const QString &fPath, bool on, int decoderId);
    void setStateCached(const QString &fPath, bool on);
    int  bumpStateAttempts(const QString &fPath);
    void unbumpStateAttempts(const QString &fPath);   // a discarded decode is no attempt
    void setStateStatus(const QString &fPath, int status, const QString &errMsg = QString());
    void clearStateFor(const QString &fPath);
    void clearAllState();

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
    struct PressureItem {
        int sfRow;
        bool isJustDoIt;
        bool isRapidForward;
        bool isCooldown;
        qint64 elapsedMs;
        qint64 tSinceMoveMs;    // ms since last forward step at the time of this check
        int cushion;
        bool highChk;
        bool lowChk;
        int stepMB;
        quint64 ceilMB;
        quint64 cacheMB;
        quint64 maxMB;
    } pressureItem;

    QVector<PressureItem> pressureHistory;   // holds last 50 pressure readings

    int currRow;                // current image
    int prevRow;                // used to establish direction of travel

    bool isForward;             // direction of travel for caching algorithm
    int step;                   // difference between key and prevKey
    int sumStep;                // sum of step until threshold
    int directionChangeThreshold;//number of steps before change direction of cache
    bool isAutoMaxMB;             // use releavePressure() to set maxMB
    AutoStrategy autoStrategy;  // Frugal, Moderate, Greedy or Ignore
    qint64 maxMB;               // maximum MB available to cache
    qint64 minMB = 500;         // minimum MB available to cache
    int targetFirst;            // beginning of target range to cache
    int targetLast;             // end of the target range to cache
    int targetCount;
    int decodeImageCount;
    quint64 decodeImageMsTot;
    quint64 decodeImageMBTot;
    bool firstDispatchNewDM;

    // --- Cache pressure section Req'd when autoMaxMB == true ---
    bool ignorePressureRestraints = false;// bypass rapid/cooldown once after folder change
    qint64 lastAdjustMs = 0;              // last time we changed maxMB
    qint64 lastMoveMs   = 0;              // last time setTargetRange saw a move
    int lastKeyForMotion = -1;            // last row key we saw
    float memThrottle = 1.0;              // factor to adjust maxMB

    // motion heuristics (EMA = Exponential Moving Average)
    double emaStepMs = -1.0;              // EMA of time between successive forward steps
    int forwardStreak = 0;                // count of consecutive forward steps

    // cushion bookkeeping
    int cushion = INT_MAX;     // rows ahead to first queued target; INT_MAX if none

    // tuning knobs (feel free to expose via settings)
    int cushionLow  = 5;                  // “need more cache” when pressure <= this
    int cushionHigh = 15;                 // “too much cache” when pressure > this
    int adjustCooldownMs = 100;           // min delay between cache-size adjustments
    int rapidStepMsThreshold = 70;        // user is “rapid” if EMA step ≤ this (≈14 FPS)
    int rapidMinStreak = 2;               // need at least N consecutive forward steps

    // step sizing (we try to resize in chunks ≈ a few images)
    float emaItemMB = -1.0f;              // EMA of item size seen while queuing
    quint64 minStepMB = 256;              // never resize by less than this
    quint64 maxStepMB = 1024;             // never resize by more than this

    // decoder performance.
    // NOTE: ImageDecoder::msToDecode is actually nanoseconds (qint64 from
    // QElapsedTimer::nsecsElapsed) — name is historical. We convert to ms at
    // the boundary in decodeHistory() so decoderMs / decodeMsAvg are genuine ms.
    int decoderMs = 250;                  // moving-average decode time, ms
    int lastNDecoders = 10;               // window size for decodeMsAvg
    Winnow::Util::MovingAvg decodeMsAvg { lastNDecoders };
    inline void decodeHistory(qint64 nsToDecode);

    // lifetime counters since last initImageCache/instance reset.
    // Used by diagnostics() to surface trim/discard races and health invariants.
    std::atomic<quint64> cachedCount{0};            // successful icd->insert in cacheImage
    std::atomic<quint64> trimmedCount{0};           // keys removed in trimOutsideTargetRange
    std::atomic<quint64> lateDecodeCount{0};        // okToCache: decode finished after row trimmed from toCache
    std::atomic<quint64> attemptCapHitCount{0};     // okToDecode: "Max attempts reached"

    // cache size ceiling with default start amount (MB) to prevent runaway growth
    qint64 maxMBCeiling = G::availableMemoryMB * 0.9;

    // pressure helpers
    static qint64 nowMs();
    inline void noteItemSizeMB(float szMB);
    inline void updateMotion(int key, bool isForwardNow);
    inline bool isRapidForward() const;
    inline quint64 calcResizeStepMB() const;
    
    void relievePressure();

    // --- End Cache pressure section ---

    void launchDecoders(QString src);
    bool okToCache(int id, int sfRow, int doneStatus, const QString &doneFPath);
    void cacheImage(int id, int cacheKey,
                    const QImage &doneImage,
                    const QString &doneFPath,
                    int cacheSource);  // make room and add image to imageCache
    bool cacheUpToDate();           // target range all cached
    void resetStaleIsCaching();
    void decodeNextImage(int id, int sfRow);   // launch decoder for the next image in cacheItemList
    void trimOutsideTargetRange();// define start and end key in the target range to cache
    bool anyDecoderCycling();        // All decoder status is ready
    void setDirection();            // caching direction
    bool okToDecode(int sfRow, int id, QString &msg);
    int nextToCache(int id);        // find highest priority not cached
    void toCacheRemove(int sfRow);
    void toCacheAppend(int sfRow);
    void memChk();                  // still room in system memory for cache?
    /*  id is a DECODER INDEX. It was declared `bool id`, so every id above 1 collapsed
        to 1 and okToCache tested decoder 1's instance instead of the returning decoder's
        -- with decoderCount = idealThreadCount that is most of the pool checking the
        wrong thread. A stale decode could be accepted and a current one refused,
        at random, which is the worst possible behaviour for a guard whose whole job is
        to keep a previous folder's pixels out of this one's cache. */
    bool instanceClash(int id);
    bool isValidKey(int key);

    void updateTargets(bool dotForward, bool isAhead, int &pos,
                       int &amount, bool &isDone, quint64 &sumMB);
    void setTargetRange(int key);
    void log(const QString function, const QString comment = "");
};

#endif // IMAGECACHE_H
