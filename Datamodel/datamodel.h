#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include <QMessageBox>
#include <QWaitCondition>           // req'd for removeFolder process
#include <QElapsedTimer>
#include <atomic>
#include "Metadata/metadata.h"
#include "Datamodel/filters.h"
#include "Datamodel/modelsync.h"
#include "Datamodel/imagerow.h"
#include "Datamodel/iconstore.h"
#include "Datamodel/rowscratch.h"
#include "Datamodel/filterpredicate.h"
#include "Cache/framedecoder.h"
#include "Cache/catalog.h"
#include "selectionorpicksdlg.h"
#include "Log/issue.h"

#include <QAbstractItemModelTester>

/*
    WHAT SET OF IMAGES THE MODEL IS SHOWING, as ONE description.

    Winnow fills the model two ways -- a folder (or a folder tree) picked in the
    Folders/Bookmarks panel, and a catalog search picked in the Find dock -- and until
    now those were two functions with two call sites and nothing that named the thing
    they have in common. They ARE the same thing: a predicate over the catalogue of
    images, differing only in whether the filesystem is also walked to reconcile what
    the index believes against what is actually on disk.

    A folder scope is the query with a folder/subtree predicate PLUS that reconcile; a
    catalog scope is the same query without it. Holding it as one struct is what lets
    the model answer "what am I showing" -- which is what a reload, a refresh and the
    reconcile write-back all need, and none of them could ask before.

    paths IS CARRIED RATHER THAN RE-RESOLVED for catalog scope. The Find dock has
    already run the search (it shows the count), so resolving the query a second time
    here would be a second pass over the index for an answer we were handed.
*/
struct ScopeRequest
{
    G::Scope scope = G::Scope::Folders;

    /* What defines the set. For folder scope, query.folder is the folder and recurse
       says whether its subtree is included; for catalog scope it is what the user
       asked the Find dock for. */
    CatalogQuery query;

    /* Catalog scope only: the result set the dock resolved. paths is the older shape,
       still used by the separate Catalog panel; rows is what the Find dock supplies --
       the same images with everything a datamodel row displays already attached, so the
       fill opens no files. When rows is non-empty it is the set and paths is ignored. */
    QStringList paths;
    QVector<CatalogRow> rows;

    /* Folder scope only. subDirs is the pre-walked subtree (Utilities::subFolderTree),
       consumed rather than re-walked. */
    bool recurse = false;
    G::FolderOp op = G::FolderOp::Add;
    QStringList subDirs;

    /* Add to what is loaded rather than replacing it. */
    bool append = false;

    /* Walk the filesystem to reconcile the set. True for folder scope, where the
       directory listing IS the truth; false for a search result, which is a list of
       files from many folders with no directory to enumerate. */
    bool reconcile = true;
};

class SortFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SortFilter(QObject *parent, Filters *filters, bool &combineRawJpg);
    bool isFinished();
    bool isSuspended();
    bool &combineRawJpg;

    /* Single guard for every sort path (sortThumbs, QTableView::setSortingEnabled, header
       clicks, workspace restore): a persisted/stale sort column can be out of range
       (>= G::TotalColumns, the sentinel past the last real column). Sorting by a phantom
       column silently reverses the model in descending order; treat invalid as -1 (no sort,
       source order). See the column-88 trace. */
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

public slots:
    void filterChange(QString src = "");
    void suspend(bool suspendFiltering, QString src = "");

    /*  Recompile the Filters tree into the plain form filterAcceptsRow reads
        (Datamodel/filterpredicate.h). GUI THREAD ONLY -- it walks the widget.
        Driven by the tree's own change signals rather than by a list of call
        sites, so a path that checks an item, or that adds one as metadata
        arrives, cannot forget to do it. */
    void compileFilters();
    /*  Compile ONCE for a burst of tree mutations. BuildFilters adds items and updates
        counts thousands of times during a rebuild, and every one of those signals used to
        recompile the whole tree -- O(items) work per item, which at 43,000 rows blocked
        the GUI for a measured 56 seconds. Coalescing is what the old comment here
        wrongly believed suspension already did. */
    void scheduleCompileFilters();
    /*  Compile now if a coalesced compile is pending. Called by anything that is about to
        FILTER, so a deferred compile can never leave the predicate a turn behind the
        tree the user just clicked. */
    void flushPendingCompile();

private slots:

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;



signals:

private:
    Filters *filters;
    mutable bool finished;
    bool compilePending = false;
    std::atomic<bool> suspendFiltering;

    /*  The compiled filters, published as a shared_ptr<const> exactly as
        ProxySnapshot is in modelsync.h: filterAcceptsRow takes its own
        reference and evaluates that, so the GUI thread can swap in a rebuilt
        predicate without pulling items out from under a row being tested. */
    FilterPredicatePtr filterPredicate() const;
    mutable QMutex mPredicateMutex;
    FilterPredicatePtr mPredicate;
};

class DataModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    DataModel(QObject *parent,
              Metadata *metadata,
              Filters *filters,
              bool &combineRawJpg);

    /*  QAbstractTableModel, not QStandardItemModel. The stores hold every value
        and every role now, so the only thing the base class still provided was
        a QList<QStandardItem*> of rowCount x 93 -- 744 bytes of mostly null
        pointers per row, 186 MB at 250,000 rows, which is MORE than the whole
        packed row store. G::dataModelColumns is unchanged and so is every
        caller: they address the model through index()/data()/setData(), which
        is why this could be left until last. */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    /*  Empty the model. QStandardItemModel::clear() also DISCARDED the header
        items, which is why every caller followed it with setModelProperties();
        this keeps that contract -- the headers are rebuilt by the same call --
        so the call sites do not change. */
    void clear();

    void setModelProperties();
    bool readMetadataForItem(int row, int instance);
    bool refreshMetadataForItem(int sfRow, int instance);
    qint64 rowBytesUsed(int dmRow);
    void sampleRowBytesUsed(int dmRow);

    /*  THREAD-SAFE VIEWS OF THE MODEL for ImageCache and MetaRead -- see
        Datamodel/modelsync.h. Both are safe to call from any thread; each hands
        back a shared_ptr the caller should hold for the duration of its work
        rather than re-fetching per row.

        rowSync() is LIVE per-row state (metadata status, cache size estimate),
        maintained in the setData override. proxySnapshot() is the proxy's order
        and identity, rebuilt on the GUI thread whenever rows are inserted or
        removed or the proxy is re-sorted or re-filtered. */
    /*  THE PACKED ROW STORE (Datamodel/imagerow.h). Reached only through
        data()/setData(): it is now the ONLY copy of the covered columns -- the
        item writes and the verifyRowStore() shadow comparison that proved them
        are both retired. See "The Row Store" in Documentation.txt. */
    RowStore rowStore;

    /*  Column headers: the name and the geek flag, one entry per column. See
        setModelProperties. */
    std::vector<QString> mHeaderName;
    std::vector<bool> mHeaderGeek;

    /*  THE ONE ROLE NEITHER STORE HOLDS. DataModel::addIssue keeps a
        QList<QSharedPointer<Issue>> on G::ErrColumn at Qt::UserRole, read back
        by rptIssues. It is not a value and not per-column state, it is a
        diagnostic attached to the few rows whose metadata failed, so it gets a
        sparse hash of its own rather than a general "any role" escape hatch --
        a general one would silently absorb the next role somebody adds instead
        of making them decide where it lives. It is the ONLY uncovered-role
        write to this model; every other setData in the codebase targets a
        different model. */
    QHash<int, QVariant> mIssueLists;      // dm row -> the issue list

    /*  Thumbnails, keyed by path rather than held on the row -- see
        Datamodel/iconstore.h. Reached through data()/setData() on
        Qt::DecorationRole exactly as before, so no view or delegate changed. */
    IconStore iconStore;

    /*  THE SCRATCH STORE (Datamodel/rowscratch.h) -- the ~24 columns that only
        matter while a decoder or the ImageCache is on that row. Same seam, same
        shadow verification, but keyed by row in a HASH rather than held on
        every row: a row nothing has touched has no entry at all. */
    ScratchStore scratchStore;

    RowSyncPtr rowSync() const;
    ProxySnapshotPtr proxySnapshot() const;
    void rebuildProxySnapshot();            // GUI thread
    void resizeRowSync(int rows);           // GUI thread

    /*  Raw+JPG pair accessors.

        Every pairing role lives on column 0 (G::PathColumn) of the DATAMODEL.
        Reading one off any other column, or off the proxy, silently yields an
        invalid QVariant -- which reads as "not a pair" rather than as an
        error, so the bug is quiet. Go through these rather than calling
        data(G::Dup...Role) directly. */
    int  dupOtherRow(int dmRow) const;      // the paired row, or -1 if none
    bool isDupJpg(int dmRow) const;         // the jpg half of a pair
    bool isDupHiddenRaw(int dmRow) const;   // the raw half, hidden if combined
    QString dupRawType(int dmRow) const;    // "NEF", "ORF" ... else empty
    void clearDataModel();
    void newInstance();
    bool sourceModified(QStringList &added, QStringList &removed, QStringList&modified);
    bool isQueueEmpty();
    bool contains(QString &path);
    void find(QString text);
    ImageMetadata imMetadata(QString fPath, bool updateInMetadata = false);

    /* Every fully-read row as plain values, for Cache/catalog.h.

       Read on the GUI thread and handed to a pool thread to insert, because the model is
       not thread-safe. Rows whose metadata has not finished loading are skipped: a
       half-read row would be catalogued with empty keywords and then look FRESH to the
       next commit, so the image would stay wrong until its file changed. */
    QVector<CatalogRow> catalogRows() const;

    /*  THE PATHS THIS MODEL HOLDS, GROUPED BY THE FOLDER THEY CAME FROM -- the other
        half of the catalog write-back. catalogRows() says what is here; this says what
        is NOT, by handing Catalog::reconcileFolder a complete listing of each folder to
        compare its live rows against. GUI thread only (it walks the model); the result
        is plain values and travels to the pool. */
    QHash<QString, QSet<QString>> folderPathSets() const;
    /* One row for the catalog; false when it must not be catalogued. Shared by
       the bulk capture and the edit write-back so the two cannot drift. */
    bool catalogRowFor(int row, CatalogRow &r) const;
    bool isAnyPick();
    void clearPicks();
    void remove(QString fPath);
    int insert(QString fPath);
    void refresh();
    QModelIndex indexFromPath(QString fPath);
    QModelIndex proxyIndexFromPath(QString fPath);
    QModelIndex proxyIndexFromModelIndex(QModelIndex dmIdx);
    int proxyRowFromModelRow(int dmRow);
    int modelRowFromProxyRow(int sfRow);
    QModelIndex modelIndexFromProxyIndex(QModelIndex sfIdx);
    int nearestProxyRowFromDmRow(int dmRow);
    QString diagnostics();
    QString diagnosticsForCurrentRow();
    QString diagnosticsAllRows();
    QString reportHealthChecks();
    void getDiagnosticsForRow(int row, QTextStream& rpt);
    bool updateFileData(QFileInfo fileInfo);
    bool metadataLoaded(int dmRow);
    bool isDimensions(int sfRow);
    bool subFolderImagesLoaded = true;
    bool isMetadataAttempted(int sfRow);
    bool isMetadataLoaded(int sfRow);
    bool isAllMetadataLoaded();         // O(1): every row loaded successfully
    bool metaReadHadFailure();          // O(1): some row attempted but not loaded
    QList<int> failedMetadataRows();    // rows with MetaFailed status (reporting)
    int iconCount();
    void clearIconsOutsideChunkRange(int instance);
    bool iconLoaded(int sfRow, int instance);
    bool isIconRangeLoaded();
    void setIconRange(int sfRow);
    void setChunkSize(int chunkSize);
    /* The user's ceiling on icons held at once; rowCount() when unlimited. */
    int maxIconChunkOrAll() const;
    void resolveIconChunkSize();        // Layer 1: brute-force small folders, JIT window for large
    double avgIconMB();                 // measured per-icon footprint, else worst-case estimate
    int    iconBudgetCount();           // icons that fit the thumbnail memory budget
    int    iconChunkFloor();            // hard minimum window (3x visible) — overrides budget
    int    memoryPressureLevel();       // 0 normal / 1 warn / 2 critical, from availableMemoryMB
    void   applyIconCachePressure();    // Layer 3: shrink-only pressure valve with hysteresis
    bool isPath(QString fPath);
    /* Is folderPath one of the folders currently loaded? The single definition of that
       test: folderSet answers the common case in O(1), and a cleaned comparison catches
       the same folder spelled differently (trailing slash, "..", double separator). */
    bool isFolderLoaded(const QString &folderPath) const;
    void rebuildRowFromPathHash();
    int nextPick();
    int prevPick();
    int nearestPick();
    bool getSelectionOrPicks(QStringList &list);
    bool isSelected(int row);
    void saveSelection();
    void restoreSelection();
    int recurseImageCount(QString &parentFolder);

    void removeFolder(const QString &folderPath);

    QMutex dmMutex;
    QReadWriteLock fPathRowLock;
    QReadWriteLock fPathRawInfoLock;

    bool isProcessingFolders = false;

    SortFilter *sf;
    QItemSelectionModel *selectionModel;
    // all folders in the datamodel.  folderSet mirrors folderList for O(1)
    // membership tests (folderList preserves insertion order).
    QStringList folderList;
    QSet<QString> folderSet;
    QHash<QString, int> folderImageCount;
    QDir::SortFlags thumbsSortFlags;

    // fPathRow hash and methods for concurrent access
    QHash<QString, int> fPathRow;
    bool fPathRowContains(const QString &path);
    int  fPathRowValue(const QString &path);
    void fPathRowSet(const QString &path, const int row);
    void fPathRowRemove(const QString &path);
    void fPathRowClear();

    /* RAW sensor unpack info keyed by fPath, populated during metadata read for raw files.
       Lets the RAW decode path (ImageDecoder, cache mode) obtain RawSensorInfo without
       re-walking the file. Only raw files are stored, so this stays small. Concurrent: written
       from Reader/MetaRead threads via addMetadataForItem, read from decoder threads. */
    QHash<QString, RawSensorInfo> fPathRawInfo;
    bool fPathRawInfoGet(const QString &path, RawSensorInfo &info);
    void fPathRawInfoSet(const QString &path, const RawSensorInfo &info);
    void fPathRawInfoClear();

    // track large recursive subfolder trees
    quint32 subFolderTreeCount = 0;
    quint32 subFolderTreeCounter = 0;

    // current status
    // int instance = 0;                   // each new load of DataModel increments the instance
    std::atomic<int> instance;

    /* Backpressure counter for queued Reader → DataModel events.
       Reader increments before each emit of addToDatamodel/setIcon1/setIcon
       (Cache/reader.cpp). DataModel decrements at the end of the matching
       slot (addMetadataForItem, setIcon1, setIcon, setIconFromVideoFrame).
       MetaRead::dispatch reads it to gate further reader dispatches when
       the GUI thread is falling behind, avoiding unbounded queue growth
       on folders that produce events faster than they can be consumed. */
    std::atomic<int> queuedReaderEvents{0};

    /* Running counts maintained in DataModel::setData so the "is the load
       complete?" checks are O(1) instead of O(rows) per row. Previously
       addMetadataForItem rescanned every row (O(N²) over a folder load) and
       setIcon1 rescanned the whole icon chunk on every icon — the dominant
       GUI-thread cost on large folders. Reset in clearDataModel(), recomputed
       by recountLoadFlags() after structural row removals. */
    std::atomic<int> metadataAttemptedCount{0};
    std::atomic<int> metadataLoadedCount{0};
    std::atomic<int> iconLoadedCount{0};
    std::atomic<int> videoRowCount{0};
    /*  ROWS THAT CAN NEVER CARRY AN ICON, for the same reason videoRowCount exists: the
        icon chunk is "complete" when every row in it is loaded OR cannot be. A catalog
        scope can hold rows whose file is on an unplugged drive or gone (see
        Catalog::Availability), and counting those as outstanding made G::iconChunkLoaded
        permanently false -- which put MetaRead into a redo loop that re-dispatched
        readers across 43,000 rows five times per arm and blocked the GUI for a measured
        31 seconds. Maintained on AvailabilityColumn writes in setData and resynced by
        recountLoadFlags, exactly as the other four are. */
    std::atomic<int> iconUnloadableCount{0};

public:
    /*  ONE dataChanged PER VISIBLE ROW PER EVENT-LOOP TURN, instead of one per write.

        The visible-row throttle answers "does this row need repainting"; this answers
        "how often". A row being filled gets a burst of writes -- decoration, icon-loaded,
        status, reading-flag, aspect ratio -- and each one used to be its own notification.
        With 11 rows visible that was 209 notifications, and on macOS each one makes
        QAbstractItemView rebuild the whole accessibility element array: ~27 ms apiece over
        43,000 rows, which is the 5.6 s that survived every other fix.

        Collecting the rows and emitting once on the next turn of the event loop cannot
        lose an update -- the values are already in the model, and the view reads them when
        it paints. */
    void scheduleVisibleEmit(int dmRow);
private:
    void flushVisibleEmits();
    QSet<int> pendingEmitRows;
    bool pendingEmitScheduled = false;
    /*  The deferral for a visible-row repaint. NOT zero -- see scheduleVisibleEmit. */
    static constexpr int kVisibleEmitDeferMs = 20;
public:

    QModelIndex instanceParent;         // &index.parent() != &instanceParent means instance clash
    QString firstFolderPathWithImages;
    QString currentFilePath;            // used in caching to update image cache
    int currentSfRow;                   // used in caching to check if new image selected
    int currentDmRow;                   // used in caching to check if new image selected
    QModelIndex currentSfIdx;
    QModelIndex currentDmIdx;
    /* An ESTIMATE of the bytes the model is holding, for diagnostics only --
       nothing gates on it. Maintained by sampleRowBytesUsed from a 1-in-64
       exact sample, because the exact walk is ~550 data() calls per row.
       Diagnostics::datamodel measures the whole model when an exact figure is
       wanted. */
    qint64 bytesUsed = 0;
    /*  Worker-thread views (Datamodel/modelsync.h). mSyncMutex guards only the
        two shared_ptr slots -- publishing a replacement, or taking a reference.
        The DATA behind them is either lock-free atomics (RowSyncArray) or
        immutable once published (ProxySnapshot), so no worker ever holds this
        lock while it reads. */
    mutable QMutex mSyncMutex;
    RowSyncPtr mRowSync;
    ProxySnapshotPtr mProxySnapshot;

    qint64 bytesUsedSampleTotal = 0;    // exact bytes of the sampled rows
    int    bytesUsedSampleCount = 0;    // how many rows were sampled
    int    bytesUsedSampleTick = 0;     // sample every 64th call

    const QStringList raw = {"arw", "cr2", "cr3", "dng","nef", "orf", "raf", "sr2", "rw2"};
    const QStringList jpg = {"jpg", "jpeg"};

    int firstVisibleIcon = 0;
    int lastVisibleIcon = 0;
    int visibleIcons = 0;
    /* Icon-chunk bounds. Written on the GUI thread (setIconRange) and read on the
       metaReadThread (MetaRead::setStartRow) — std::atomic to remove the TSan-
       confirmed data race on these scalars. The pair is a load hint, so a reader
       briefly seeing a half-updated range is benign and self-corrects next update. */
    std::atomic<int> startIconRange{0};
    std::atomic<int> endIconRange{0};
    /*  Read from metaReadThread (setStartRow) and written by the GUI thread's
        chunk-sizing logic, so it is atomic like startIconRange/endIconRange
        beside it. */
    std::atomic<int> iconChunkSize{0};   // max suggested number of icons to cache
    int scrollToIcon = 0;

    /* Layer 2 (measured refinement): running footprint of icons actually loaded this
       folder, accumulated in setIcon1. Used to replace the worst-case per-icon estimate
       with the real average and (one-shot) promote a JIT folder back to brute force when
       the true footprint turns out to fit. Reset per folder in resolveIconChunkSize. */
    std::atomic<qint64> iconBytesSum{0};
    std::atomic<int>    iconSamples{0};

    /* Layer 3 (defensive pressure valve): a GUI-thread timer polls memory pressure and,
       under warn/critical, shrinks the icon window and evicts — never grows. A cooldown
       plus an available-memory high-water mark (hysteresis) gate when the latch relaxes,
       so the cache can't thrash between shrink and re-grow. Only active when
       G::useJitIconCache. See applyIconCachePressure. */
    QTimer *iconPressureTimer = nullptr;
    QElapsedTimer iconPressureClock;
    qint64 iconPressureCooldownUntil = 0;   // ms on iconPressureClock; latch held until then
    bool iconCachePressureLatched = false;  // true while pressure-reduced (blocks growth)
    int  iconPressureLevel = 0;             // last observed level (diagnostics)
    static constexpr int kIconPressurePollMs   = 1000;   // poll cadence
    static constexpr int kIconPressureCooldownMs = 60000; // hold latch ≥60s after pressure
    static constexpr int kIconPressureClearMB  = 2048;   // high-water to relax latch

    bool hasDupRawJpg;
    bool loadingModel = false;          // do not filter while loading datamodel
    /*  HOW MANY ROWS THE SET WILL HOLD, while a streamed fill is still delivering them.
        0 when nothing is streaming. The status bar reports this rather than rowCount()
        during a load, so a catalog of 43,000 says "1 of 43,000" from the first thumbnail
        instead of counting up -- the batching is the loader's business, not something the
        user should have to watch. */
    int expectedRows = 0;
    bool basicFileInfoLoaded = false;   // not used. do not navigate until basic info loaded in datamodel
    bool folderHasMissingEmbeddedThumb;        // jpg/tiff only

    /* can be set from keyPressEvent in MW to terminate if recursive folder scan or
       building filters too long */
    bool abort;

    bool showThumbNailSymbolHelp = true;
    void setShowThumbNailSymbolHelp(bool showHelp);
    bool okManyImagesWarning();

    struct Fld {
        QString path;
        QString op;
    };

    // QQueue<QPair<QString, G::FolderOp>> folderQueue;
    // QQueue<QPair<QString, bool>> folderQueue;

    // locations of symbols on a thumbnail so can show tooltip
    QHash<QString, QRect>iconSymbolRects;

signals:
    /*  Emitted (queued) when a video row's in-flight frame decode has been
        resolved -- success (setIconFromVideoFrame) or failure
        (clearVideoReadingFlag). MetaRead keeps its own worker-local set of
        dispatched video rows and needs to be told when to drop one; it cannot
        read MetadataReadingColumn off its own thread. The PROXY row is sent
        because that is how MetaRead keys its set, and only the GUI thread may
        map a datamodel row to a proxy row. */
    void videoReadingCleared(int sfRow, int fromInstance);

    void stop(QString src);
    void folderChange(bool aborted);
    void updateClassification();        // req'd for 1st image, loaded before metadata cached
    void centralMsg(QString message);
    void updateProgress(int progress);
    void rowLoaded();
    void updateStatus(bool keepBase, QString s, QString source);
    void refreshViewsOnCacheChange(QString fPath, bool isCached, QString src);

    /* Emitted when the icon chunk is resized outside the normal scroll/selection flow
       (applyIconCachePressure, and the JIT window following the visible page). Wired to
       MW::reloadIconChunk so MetaRead re-dispatches and (re)loads the newly in-range
       icons; without it the window grows/shrinks but the new rows wait for a scroll. */
    void iconChunkResized();

    /* Emitted when any DataModel hot path observes the process footprint
       exceeding G::memoryAbortMB. Wired to MW::onMemoryOverrun in
       Main/initialize.cpp. */
    void memoryOverrun(quint64 footprintMB, quint64 capMB);

public slots:
    /*  THE ONE WAY THE MODEL IS FILLED. Both load paths -- a folder picked in the tree
        and a search resolved by the Find dock -- come through here, so the model always
        knows what set it is showing. See ScopeRequest above and the .cpp. */
    void setScope(const ScopeRequest &req);
    const ScopeRequest &scopeRequest() const { return currentScope; }

    void enqueueFolderSelection(const QString &folderPath, G::FolderOp op, bool recurse = false,
                                const QStringList &subDirs = QStringList());
    void addAllMetadata();
    void setAllMetadataAttempted(bool isAttempted);
    bool addMetadataForItem(ImageMetadata m, QString src);
    /*  Reader's entry point: decrements the backpressure counter Reader incremented,
        then delegates. Every other caller uses addMetadataForItem directly. */
    bool addMetadataForItemFromReader(ImageMetadata m, QString src);
    QString primaryFolderPath();
    QVariant valueSf(int row, int column, int role = Qt::DisplayRole);
    void setIcon(QModelIndex dmIdx, const QPixmap &pm, int fromInstance, QString src = "");
    bool iconRowVisible(const QModelIndex &dmIdx);  // true if row in visible range (or flag off)
    void setIcon1(int dmRow, const QImage &im, int fromInstance, QString src = "");

    /* Replace a row's icon with a freshly rendered develop preview. Unlike setIcon1 this
       DELIBERATELY overwrites an existing icon -- see the implementation for why that is
       safe here and not there. Call after an edit is flushed, on the GUI thread. */
    void setDevelopIcon(int dmRow, const QImage &im);

    /* Forget a row's icon so the normal loader re-reads it. Used when an image's develop
       preview became invalid and no replacement could be rendered. */
    void clearDevelopIcon(int dmRow);
    void setIconFromVideoFrame(int dmRow, QImage im, int fromInstance, qint64 duration,
                               FrameDecoder *frameDecoder);
    void clearVideoReadingFlag(int dmRow, int fromInstance);
    void setValDm(int dmRow, int dmCol, QVariant value, int instance, QString src,
                  int role = Qt::EditRole);
    void setValSf(int sfRow, int sfCol, QVariant value, int instance, QString src,
                  int role = Qt::EditRole);
    void setValuePath(QString fPath, int col, QVariant value, int instance, int role = Qt::EditRole);
    void setCurrent(QModelIndex dmIdx, int instance);
    void setCurrent(QString fPath, int instance);
    bool setCurrentSF(QModelIndex sfIdx, int instance);
    void setCached(int sfRow, bool isCached, int instance);
    void issue(const QSharedPointer<Issue>& issue);
    QStringList rptIssues(int sfRow);
    void rebuildTypeFilter();
    void searchStringChange(QString searchString);
    void imageCacheWaiting(int sfRow, int instance);
    bool isMetaReadFinished();          // O(1): every row attempted (== rowCount)
    bool isAllIconChunkLoaded(int first, int last);
    // Intercepts MetadataStatus/IconLoaded/Video column writes to keep the
    // running counts above accurate; delegates everything to the base class.
    /*  Serves the covered columns from the packed row store -- the seam the
        whole storage change turns on. See the implementation. */
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    void recountLoadFlags();        // full rescan to resync counts after removals
    /*  Refresh G::iconChunkLoaded. Pass the dm row an icon just landed on for the O(1)
        incremental path; the no-argument form does the full authoritative recount. */
    void updateIconChunkLoaded(int dmRow = -1);
    int  countIconChunkMissing(int first, int last);

    /*  THE SCROLL PATH, MEASURED (G::isPerfProbe / WINNOW_PERF_PROBE=1).

        Scrolling a catalog scope is not the load path, and none of the load probes say
        anything about it: the load finishes, then the beachball happens while the user
        drags the scrollbar. Three things the GUI thread does per scroll event are not
        O(1), and which of them costs what cannot be reasoned out -- static analysis has
        named the wrong culprit here twice:

          scan   isAllIconChunkLoaded, called from setIconRange, which runs TWICE per
                 scroll event (MW::updateIconRange and MW::updateChange). It walks the
                 whole icon chunk -- up to G::maxIconChunk rows, 10,000 by default --
                 asking the PROXY for three columns each, and unlike updateIconChunkLoaded
                 it has no O(1) counter gate in front of it.
          evict  clearIconsOutsideChunkRange, queued from MetaRead::dispatchFinished onto
                 the GUI thread. It walks EVERY row outside the chunk -- 33,000 of 43,000
                 -- reading a decoration from each.
          rearm  MetaRead::setStartRow's JIT re-arm loop, O(chunk) on the metaReadThread.
                 Not a beachball itself, but it is what decides which icons get read, so
                 a thumbnail that never appears is diagnosed from its count.

        Reported as one throttled line rather than per call, because a scroll produces
        hundreds of calls a second and a line each would itself stall the GUI. */
    void reportScrollProbe(const QString &src);

/*  Plain data, so it cannot live in the public slots: block this sits in -- moc rejects a
    member declaration there ("Not a signal or slot declaration"). The slots section
    resumes below. */
public:
    qint64 probeScanNs = 0;             // isAllIconChunkLoaded, cumulative
    qint64 probeScanCalls = 0;
    qint64 probeScanRows = 0;
    qint64 probeEvictNs = 0;            // clearIconsOutsideChunkRange, cumulative
    qint64 probeEvictCalls = 0;
    qint64 probeEvictCleared = 0;       // icons actually dropped
    qint64 probeEvictWalked = 0;        // rows the sweep actually looked at
    qint64 probeSetRangeCalls = 0;
    /*  Written on the metaReadThread, read here: atomic for the same reason
        startIconRange is. */
    std::atomic<qint64> probeRearmNs{0};
    std::atomic<qint64> probeRearmCalls{0};
    std::atomic<qint64> probeRearmRows{0};
    qint64 probeIconsSet = 0;           // icons actually stored
    qint64 probeIconSetNs = 0;          // GUI thread time inside setIcon1
    qint64 probeFlushCount = 0;         // visible-repaint flushes
    qint64 probeFlushMaxDelayNs = 0;    // worst wait from icon stored to repaint emitted
    qint64 probeFlushEmitNs = 0;        // time inside the dataChanged emissions themselves
    QElapsedTimer pendingEmitClock;
    qint64 probeIconsRedundant = 0;     // delivered for a row that already had one
    std::atomic<qint64> probeDispatched{0};   // rows MetaRead handed to a Reader
    std::atomic<qint64> probeRedos{0};        // MetaRead::redo passes
    QElapsedTimer probeScrollClock;
    qint64 probeLastReportMs = -1;
    /*  Rows in the CURRENT icon range that still owe an icon. Exact after every
        setIconRange, decremented as icons land. G::iconChunkLoaded == (this == 0).
        Not a probe -- it is live state; it sits here because this is the plain-data
        block (the surrounding section is public slots:, where moc rejects members). */
    int iconChunkMissing = 0;
    /*  The range the last eviction sweep covered, so the next one can walk only what the
        chunk has slid off. -1 means "no sweep yet": do a full one. */
    int lastEvictStart = -1;
    int lastEvictEnd = -1;
    int evictsSinceFullSweep = 0;
    static constexpr int kEvictFullSweepEvery = 64;

public slots:
    int rowFromPath(QString fPath);
    int proxyRowFromPath(QString fPath, QString src = "");
    QString pathFromProxyRow(int sfRow);
    QString folderPathFromProxyRow(int sfRow);
    QString folderPathFromModelRow(int dmRow);

private slots:
    void processNextBatch(); // async pump

private:
    Metadata *metadata;
    Filters *filters;
    bool &combineRawJpg;
    static bool lessThan(const QFileInfo &i1, const QFileInfo &i2);
    static bool lessThanCombineRawJpg(const QFileInfo &i1, const QFileInfo &i2);

    // Pair of folderPath and operation type (true=add, false=remove)
    void enqueueOp(const QString& folderPath, G::FolderOp op);
    void scheduleProcessing();

    /* Re-enable the proxy's dynamic sort/filter after a batched load (idempotent).
       During a batched load (G::useBatchedFolderInsert) dynamic sorting is turned off so
       the one wide dataChanged per folder does not re-sort the inserted block; this
       restores it (one re-sort pass) when the load finishes or aborts. */
    void restoreProxySortAfterLoad();
    bool sfSortDisabledForLoad = false;

    /* Throttle for addFolder's progress message (gated by G::throttleFolderLoadMsg). */
    QElapsedTimer centralMsgTimer;

    /* Phase 1 load perf probe (gated by G::isPerfProbe). */
    QElapsedTimer perfLoadTimer;
    qint64 perfEnumNs   = 0;            // dir.entryInfoList (I/O + name-filter matching)
    qint64 perfSortNs   = 0;            // std::sort of each folder's file list
    qint64 perfMsgNs    = 0;            // progress-string build + emit centralMsg (per folder)
    qint64 perfInsertNs = 0;            // model fill + synchronous proxy/view reaction
    int    perfFolders  = 0;

    QQueue<QPair<QString, G::FolderOp>> folderQueue;
    QSet<QString> pendingPaths;
    QMutex queueMutex;

    QString prevRawSuffix = "";
    QString prevRawBaseName = "";
    int prevRawRow = -1;

    enum ErrorType {
        General,
        DM
    };

    struct Error {
        ErrorType type;
        QString functionName;
        QString msg;
        int sfRow;
        QString fPath;
    };

    bool mLock;

    QStringList *fileFilters;
    QSet<QString> supportedExtSet;      // lowercased supported extensions for addFolder suffix check
    QFileInfo fileInfo;
    QImage emptyImg;

    QItemSelection savedSelection;

    /*  What setScope was last asked for. The model's own answer to "what am I
        showing", which a reload, a refresh and the reconcile write-back all need. */
    ScopeRequest currentScope;

    void addFolder(const QString &folderPath);

    /* Load an arbitrary set of image paths as one virtual folder -- the catalog search
       result. Unlike every other load path this does not start from a directory, because
       the results routinely span many. Reached through setScope, like addFolder. */
    void addPaths(const QStringList &fPaths);
    /*  The same as addPaths for a set the CATALOG resolved, filled from the index rather
        than from the files. Every column a row displays is already in the CatalogRow, so
        no image is opened and no file need be stat'd -- which is what lets a whole-
        catalog scope arrive in the time the query took rather than the time a metadata
        read per image takes. See "The Rows Come Back in the Query That Found Them" in
        notes/Documentation.txt. */
    void addCatalogRows(const QVector<CatalogRow> &rows);

private:
    /*  ONE BATCH of the streamed catalog fill, re-posted to the event loop until the set
        is in. See addCatalogRows for why it is posted rather than looped. */
    void insertCatalogBatch();
    void finishCatalogFill();
    QVector<CatalogRow> pendingCatalogRows;
    int pendingCatalogAt = 0;
    /*  Where the streamed fill's time actually goes, in nanoseconds, accumulated across
        batches and reported once in finishCatalogFill under G::isPerfProbe. The fill is
        the only part of a catalog scope that is per-row, so it is the only part that can
        turn 43,000 rows into minutes -- and which of its four steps costs that is not
        something to reason about from the source. */
    qint64 perfFillInsertNs = 0;
    qint64 perfFillFileDataNs = 0;
    qint64 perfFillIndexFillNs = 0;
    qint64 perfFillAddMetaNs = 0;
    qint64 perfFillEmitNs = 0;
    qint64 perfFillPrepNs = 0;

public:
    // void removeFolder(const QString &folderPath);
    bool endLoad(bool success);
    // bool addFileData();
    /*  cat, when given, supplies the file facts INSTEAD OF THE FILESYSTEM: size, modified
        time, capture date and whether there is a sidecar all come from the index, and the
        QFileInfo is used only for the parts of a path that need no stat (name, folder,
        suffix). It is one function rather than two so that a row means the same thing
        whichever set it arrived in. */
    void addFileDataForRow(int row, QFileInfo fileInfo, const CatalogRow *cat = nullptr);
    // void rawPlusJpg();
    void rawJpgPairing(int row, const QString &ext, const QString &baseName);
    double aspectRatio(int w, int h, int orientation);
    void processErr(Error e);

    bool isDebug = false;

    int imageCacheWaitingForRow = -1;

    QString errMsg;

    void setThumbnailLegend();
    QString thumbnailHelp;
};

#endif // DATAMODEL_H
