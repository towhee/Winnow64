#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include <QMessageBox>
#include <QWaitCondition>           // req'd for removeFolder process
#include <atomic>
#include "Metadata/metadata.h"
#include "Datamodel/filters.h"
#include "Cache/framedecoder.h"
#include "selectionorpicksdlg.h"
#include "Log/issue.h"

#include <QAbstractItemModelTester>

class SortFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SortFilter(QObject *parent, Filters *filters, bool &combineRawJpg);
    bool isFinished();
    bool isSuspended();
    bool &combineRawJpg;

public slots:
    void filterChange(QString src = "");
    void suspend(bool suspendFiltering, QString src = "");

private slots:

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

signals:

private:
    Filters *filters;
    mutable bool finished;
    std::atomic<bool> suspendFiltering;
};

class DataModel : public QStandardItemModel
{
    Q_OBJECT
public:
    DataModel(QObject *parent,
              Metadata *metadata,
              Filters *filters,
              bool &combineRawJpg);

    void setModelProperties();
    bool readMetadataForItem(int row, int instance);
    bool refreshMetadataForItem(int sfRow, int instance);
    qint64 rowBytesUsed(int dmRow);
    void clearDataModel();
    void newInstance();
    bool sourceModified(QStringList &added, QStringList &removed, QStringList&modified);
    bool isQueueEmpty();
    bool contains(QString &path);
    void find(QString text);
    ImageMetadata imMetadata(QString fPath, bool updateInMetadata = false);
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
    void resolveIconChunkSize();        // Layer 1: brute-force small folders, JIT window for large
    void refineIconChunkSize();         // Layer 2: re-decide using measured per-icon footprint
    double avgIconMB();                 // measured per-icon footprint, else worst-case estimate
    int    iconBudgetCount();           // icons that fit the thumbnail memory budget
    int    iconChunkFloor();            // hard minimum window (3x visible) — overrides budget
    int    memoryPressureLevel();       // 0 normal / 1 warn / 2 critical, from availableMemoryMB
    void   applyIconCachePressure();    // Layer 3: shrink-only pressure valve with hysteresis
    bool isPath(QString fPath);
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

    QModelIndex instanceParent;         // &index.parent() != &instanceParent means instance clash
    QString firstFolderPathWithImages;
    QString currentFilePath;            // used in caching to update image cache
    int currentSfRow;                   // used in caching to check if new image selected
    int currentDmRow;                   // used in caching to check if new image selected
    QModelIndex currentSfIdx;
    QModelIndex currentDmIdx;
    qint64 bytesUsed = 0;

    const QStringList raw = {"arw", "cr2", "cr3", "dng","nef", "orf", "raf", "sr2", "rw2"};
    const QStringList jpg = {"jpg", "jpeg"};

    int firstVisibleIcon = 0;
    int lastVisibleIcon = 0;
    int visibleIcons = 0;
    int startIconRange;
    int endIconRange;
    int iconChunkSize;                  // max suggested number of icons to cache
    int scrollToIcon = 0;

    /* Layer 2 (measured refinement): running footprint of icons actually loaded this
       folder, accumulated in setIcon1. Used to replace the worst-case per-icon estimate
       with the real average and (one-shot) promote a JIT folder back to brute force when
       the true footprint turns out to fit. Reset per folder in resolveIconChunkSize. */
    std::atomic<qint64> iconBytesSum{0};
    std::atomic<int>    iconSamples{0};
    bool iconChunkRefined = false;

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
    void stop(QString src);
    void folderChange(bool aborted);
    void updateClassification();        // req'd for 1st image, loaded before metadata cached
    void centralMsg(QString message);
    void updateProgress(int progress);
    void rowLoaded();
    void updateStatus(bool keepBase, QString s, QString source);
    void refreshViewsOnCacheChange(QString fPath, bool isCached, QString src);

    /* Emitted when the icon chunk is resized outside the normal scroll/selection flow
       (Layer 2 refineIconChunkSize, Layer 3 applyIconCachePressure). Wired to
       MW::reloadIconChunk so MetaRead re-dispatches and (re)loads the newly in-range
       icons; without it the window grows/shrinks but the new rows wait for a scroll. */
    void iconChunkResized();

    /* Emitted when any DataModel hot path observes the process footprint
       exceeding G::memoryAbortMB. Wired to MW::onMemoryOverrun in
       Main/initialize.cpp. */
    void memoryOverrun(quint64 footprintMB, quint64 capMB);

public slots:
    void enqueueFolderSelection(const QString &folderPath, G::FolderOp op, bool recurse = false,
                                const QStringList &subDirs = QStringList());
    void addAllMetadata();
    void setAllMetadataAttempted(bool isAttempted);
    bool addMetadataForItem(ImageMetadata m, QString src);
    QString primaryFolderPath();
    QVariant valueSf(int row, int column, int role = Qt::DisplayRole);
    void setIcon(QModelIndex dmIdx, const QPixmap &pm, int fromInstance, QString src = "");
    bool iconRowVisible(const QModelIndex &dmIdx);  // true if row in visible range (or flag off)
    void setIcon1(int dmRow, const QImage &im, int fromInstance, QString src = "");
    void setIconFromVideoFrame(int dmRow, QImage im, int fromInstance, qint64 duration,
                               FrameDecoder *frameDecoder);
    void clearVideoReadingFlag(int dmRow, int fromInstance);
    void setValDm(int dmRow, int dmCol, QVariant value, int instance, QString src,
                  int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValSf(int sfRow, int sfCol, QVariant value, int instance, QString src,
                  int role = Qt::EditRole, int align = Qt::AlignLeft);
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
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    void recountLoadFlags();        // full rescan to resync counts after removals
    void updateIconChunkLoaded();   // O(1)-gated refresh of G::iconChunkLoaded
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
    QQueue<QPair<QString, G::FolderOp>> folderQueue;
    QSet<QString> pendingPaths;
    QMutex queueMutex;

    QString prevRawSuffix = "";
    QString prevRawBaseName = "";
    QModelIndex prevRawIdx;

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
    QFileInfo fileInfo;
    QImage emptyImg;

    QItemSelection savedSelection;

    void addFolder(const QString &folderPath);
    // void removeFolder(const QString &folderPath);
    bool endLoad(bool success);
    // bool addFileData();
    void addFileDataForRow(int row, QFileInfo fileInfo);
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
