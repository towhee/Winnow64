#ifndef GLOBAL_H
#define GLOBAL_H

// --- Qt core + widgets (base includes) ------------------------
#include <QtCore>
#include <QtWidgets>
#include <QtConcurrent/QtConcurrent>

// --- Standard + Qt utilities ---------------------------------
#include <QColor>
#include <QModelIndexList>
#include <QStringList>
#include <QElapsedTimer>
#include <QMetaEnum>
#include <QMetaType>
#include <QMutex>
#include <type_traits>
#include <iostream>
#include <iomanip>

// --- Project headers ------------------------------------------
#include "popup.h"
#include "Utilities/utilities.h"
#include "Log/issue.h"
#include "Log/log.h"

// Before precompile header
// #include <QtWidgets>
// #include <QtConcurrent/QtConcurrent>
// #include <QColor>
// #include <QModelIndexList>
// #include <QStringList>
// #include <QElapsedTimer>
// #include <QtCore/QMetaType>
// #include <type_traits>
// #include <QMetaEnum>
// #include <QtCore/qobjectdefs.h> //
// #include <iostream>
// #include <iomanip>
// #include <QMutex>
// #include "popup.h"
// #include "Utilities/utilities.h"
// #include "Log/issue.h"
// #include "Log/log.h"

#define ICON_MIN	60
#define ICON_MAX	480  // 256 is default
#define EXISTS if (p.file.exists())

//#define "CLASSFUNCTION" QString::fromUtf8(metaObject()->className()) + "::" + __func__

namespace G
{
Q_NAMESPACE

    enum UserRoles {
        PathRole = Qt::UserRole + 1,    // path to image file
        IconRectRole,                   // used in IconView
        // CachedRole,                     // used in ImageView, IconViewDelegate
        DupIsJpgRole,                   // manage raw/jpg pairs
        DupOtherIdxRole,                // manage raw/jpg pairs
        DupHideRawRole,                 // manage raw/jpg pairs
        DupRawTypeRole,                 // manage raw/jpg pairs
        ColumnRole,                     // used by Filters
        GeekRole                        // used in TableView display of columns
    };

    // Per-row metadata read outcome, stored in MetadataStatusColumn.
    // NotAttempted -> MetaRead has not yet tried this row.
    // Failed       -> read was attempted but did not succeed (terminal).
    // Loaded       -> metadata successfully read into the model.
    enum MetaStatus {
        MetaNotAttempted = 0,
        MetaFailed       = 1,
        MetaLoaded       = 2
    };

    /*
    dataModel setHorizontalHeaderItem in DataModel::DataModel must include all prior enum but
    not in the same order!
    */
    enum dataModelColumns {
        // items available for TableView in order
        // items read when new folder (core fields)
        PathColumn,
        RowNumberColumn,
        NameColumn,
        FolderNameColumn,
        NSThumbColumn,
        NSImageColumn,
        PickColumn,
        IngestedColumn,
        LabelColumn,
        RatingColumn,
        SearchColumn,
        TypeColumn,
        VideoColumn,
        SidecarColumn,
        ApertureColumn,
        ShutterspeedColumn,
        ISOColumn,
        ExposureCompensationColumn,
        DurationColumn,
        CameraMakeColumn,
        CameraModelColumn,
        LensColumn,
        FocalLengthColumn,
        FocusXColumn,
        FocusYColumn,
        GPSCoordColumn,
        ByteSizeColumn,
        WidthColumn,
        HeightColumn,
        ModifiedColumn,
        CreatedColumn,
        // items read on demand (secondary metadata fields)
        YearColumn,
        DayColumn,
        CreatorColumn,
        MegaPixelsColumn,
        LoadMsecPerMpColumn,
        DimensionsColumn,
        AspectRatioColumn,
        IconAspectRatioColumn,
        OrientationColumn,
        RotationColumn,

        CopyrightColumn,
        TitleColumn,
        EmailColumn,
        UrlColumn,
        KeywordsColumn,
        MetadataReadingColumn,
        MetadataStatusColumn,       // tri-state G::MetaStatus: NotAttempted/Failed/Loaded
        IconLoadedColumn,
        RawRenderColumn,
        CompareColumn,
        // original values
        _RatingColumn,
        _LabelColumn,
        _CreatorColumn,
        _TitleColumn,
        _CopyrightColumn,
        _EmailColumn,
        _UrlColumn,
//        _OrientationColumn,
//        _RotationColumn,
        PermissionsColumn,
        ReadWriteColumn,

        // binary helpers
        OffsetFullColumn,
        LengthFullColumn,
        WidthPreviewColumn,
        HeightPreviewColumn,
        OffsetThumbColumn,
        LengthThumbColumn,
        samplesPerPixelColumn,
        isBigEndianColumn,
        ifd0OffsetColumn,
        ifdOffsetsColumn,
        XmpSegmentOffsetColumn,
        XmpSegmentLengthColumn,
        IsXMPColumn,
        ICCSegmentOffsetColumn,
        ICCSegmentLengthColumn,
        ICCBufColumn,
        ICCSpaceColumn,

        // image cache helpers
        CacheSizeColumn,
        IsCachingColumn,
        IsCachedColumn,
        AttemptsColumn,
        DecoderIdColumn,
        DecoderReturnStatusColumn,
        DecoderErrMsgColumn,

        OrientationOffsetColumn,
        RotationDegreesColumn,
        ShootingInfoColumn,
        SearchTextColumn,
        ErrColumn,
        TotalColumns    // insert additional columns before this
    };

    // Errors
    extern QStringList issueList;

    enum ImageFormat {
        UseQt,
        Jpg,
        Tif,
        Heic
    };

    // used to pass externalApps from MW to AppDlg
    struct App {
        QString name;
        QString path;
        QString args;
    };

    // FolderOp used by MW, FSTree and DataModel
    enum class FolderOp : quint8 { Add, Remove, Toggle };
    Q_ENUM_NS(FolderOp)    // optional, enables nice string conversion via QMetaEnum

    /* Which engine decodes a RAW file to the shared WorkingImage (demosaic + global colour /
       baseline luminance NR + start WB / black / white). Both converge on WorkingImage, so
       everything downstream (Develop, masked luminance NR, OutputTransform) is engine-agnostic.

         winnowDecodeRawEngine  in-house decoder (RawFormat::UnpackCfa -> Demosaic -> RawColor).
                                The portable baseline -- the ONLY engine on Windows, and the
                                canonical engine for focus stacking (cross-platform-deterministic,
                                CFA-level control).
         appleDecodeRawEngine   macOS Core Image (CIRAWFilter) front-end. High-quality GPU
                                demosaic + NR. macOS-ONLY: callers MUST fall back to
                                winnowDecodeRawEngine on non-mac (see G::decodeRawEngine). */
    enum class DecodeRawEngine : quint8 { winnowDecodeRawEngine, appleDecodeRawEngine };
    Q_ENUM_NS(DecodeRawEngine)

    /* Winnow's top-level operation mode (more may follow).
         Preview  fast image review: uses the embedded preview JPGs and keeps a large forward
                  cache of upcoming images.
         Develop  view/edit a SINGLE image at best quality (scene-linear raw decode); maintaining
                  a large forward cache is a low priority here.
       Toggled by the D shortcut and the status-bar Operation Mode dropdown (extreme left). */
    enum class OperationMode : quint8 { Preview, Develop };
    Q_ENUM_NS(OperationMode)

    // Generic stringify function
    template <typename Enum>
    inline QString enumClassToString(Enum value)
    {
        static_assert(std::is_enum_v<Enum>, "enumToString requires an enum type");
        const QMetaEnum me = QMetaEnum::fromType<Enum>();
        if (!me.isValid()) return {};
        if (const char* key = me.valueToKey(static_cast<int>(value)))
            return QString::fromLatin1(key);
        return {};
    }

    // mutex
    extern QWaitCondition waitCondition;
    extern QMutex gMutex;

    extern QThread* guiThread;

    // flow
    // extern bool stop;
    // extern bool removingFolderFromDM;
    // extern bool removingRowsFromDM;
    extern std::atomic<bool> stop;
    extern std::atomic<bool> removingFolderFromDM;
    extern std::atomic<bool> removingRowsFromDM;
    extern bool isInitializing;

    // datamodel
    // extern bool iconChunkLoaded;
    // extern int dmInstance;
    extern std::atomic<bool> isModifyingDatamodel;
    // "Has MetaRead finished?" — true when every row has been attempted (== model
    // ready). A LIVE value: republished on every DataModel::addMetadataForItem as
    // isMetaReadFinished(), and reset false the instant a row is inserted or a new
    // folder loads, so it self-corrects (a fresh insert makes it false at once).
    // Read by the app-wide gates (sort/filter/status/cache) and polled by
    // MetaRead::allMetaIconLoaded. Prefer dm->isMetaReadFinished() where a
    // race-free point-in-time check is needed (e.g. MW::refresh).
    extern std::atomic<bool> allMetadataAttempted;
    extern std::atomic<bool> iconChunkLoaded;
    extern std::atomic<int> dmInstance;

    extern bool useMyTiff;
    extern bool useMissingThumbs;
    extern bool suppressTiffWarnings;   // silence libtiff warning messages to the console

    // limit functionality for testing
    extern bool useApplicationStateChanged;
    extern bool useZoomWindow;
    extern bool useImageCache;
    extern bool useImageView;
    extern bool useInfoView;
    extern bool useDWCollapse;   // master switch for dock collapse/expand/solo mode
    extern bool useDockTitleGraphic;   // master switch: show a graphic instead of text on dock tabs
    extern bool useMultimedia;
    extern bool useUpdateStatus ;
    extern bool useFilterView;      // not finished
    extern bool useReadIcons;
    extern bool useReadMeta;
    extern bool useFSTreeCount;
    extern bool useProcessEvents;

    extern QSettings *settings;

    extern bool isLogger;
    extern bool isFlowLogger;
    extern bool sendLogToFile;
    extern bool isRunByExtern;
    extern QFile logFile;

    extern bool isIssueLogger;
    extern bool showIssueInConsole;
    extern QFile issueLogFile;

    extern bool sendLogToConsole;
    extern bool FSLog;              // Focus Stack remote log
    extern bool embelLog;           // Embellish remote

    extern bool showAllEvents;
    extern bool isDev;
    extern bool isRemote;

    // Rory version
    extern bool isRory;


    extern bool loadOnlyVisibleIcons;
    extern std::atomic<quint64> availableMemoryMB;
    extern int winnowMemoryBeforeCacheMB;
    extern int metaCacheMB;

    /* Memory-overrun guardrail.
       memoryAbortMB: hard cap on the process's resident footprint (MB).
                      When MetaRead's periodic check sees the footprint
                      exceed this, it aborts the load and surfaces a
                      dialog. Tunable; default 6000.
       memoryOverrunFlag: latched true when the cap is breached so other
                          subsystems (MetaRead readers, ImageCache,
                          DataModel slots) can short-circuit cheaply
                          without re-probing the OS. */
    extern quint64 memoryAbortMB;
    extern std::atomic<bool> memoryOverrunFlag;
    quint64 processFootprintMB();
    quint64 computeMemoryAbortMB(quint64 totalRamMB);

    struct WinScreen {
        QString adaptor;
        QString device;
        QString profile;
    };
    extern QHash<QString, WinScreen> winScreenHash;
    extern QString winOutProfilePath;
    extern int displayPhysicalHorizontalPixels; // current monitor
    extern int displayPhysicalVerticalPixels;   // current monitor
    extern int displayVirtualHorizontalPixels;  // current monitor
    extern int displayVirtualVerticalPixels;    // current monitor
    extern qreal actDevicePixelRatio;           // current monitor
    extern qreal sysDevicePixelRatio;           // current monitor

    extern QString trash;

    extern int maxIconSize;
    extern int minIconSize;
    extern int maxIconChunk;

    /* Just-in-time thumbnail caching (testing flag). When false, icons are cached
       brute-force for the whole folder. When true, a folder is cached fully only if its
       projected thumbnail footprint fits a memory budget; otherwise it degrades to a
       bounded sliding window. The budget is the free memory remaining after a safety
       reserve and the image cache's own claim (imageCacheHeadroomMB), times
       jitIconCacheMemFraction. See DataModel::resolveIconChunkSize / refineIconChunkSize. */
    extern bool   useJitIconCache;
    extern double jitIconCacheMemFraction;

    /* Single flag gating both ImageCache and MetaRead caching progress. When false,
       progress is neither calculated nor displayed (see ImageCache::updateStatus,
       MetaRead::dispatch and the Progress widget). */
    extern bool   showCacheProgress;

    /* Published by ImageCache::memChk = the memory (MB) the image cache still intends to
       claim on top of what it already holds (maxMBCeiling - current). DataModel subtracts
       this from the thumbnail budget so the two caches don't fight over the same memory.
       0 = not yet known (e.g. before the image cache has run). */
    extern std::atomic<qint64> imageCacheHeadroomMB;

    /* Test override for Layer 3, so the shrink / evict / hysteresis path can be validated
       deterministically without starving the machine. See DataModel::memoryPressureLevel
       and applyIconCachePressure.
         -1 = use the real signal (availableMemoryMB)
          0 = normal AND memory recovered     -> latch releases after cooldown
          1 = warn                            -> window halved, latched
          2 = critical                        -> window clamped to visible page, latched
          3 = normal BUT memory not recovered -> reaches release branch, held by high-water
       (3 reports level 0 but forces the release path's roomy check false, so the
        "stays latched because memory hasn't recovered" branch is testable.) */
    extern int iconPressureTestLevel;

    /* When true, DataModel::setIcon1 / setValDm emit dataChanged to the views only when the
       affected row is currently visible. Off-screen rows are stored without notification and
       paint correctly when scrolled into view — avoiding the per-icon dataChanged -> proxy
       + 3-view slot propagations during a bulk load. Set false to restore always-emit. */
    extern bool useVisibleOnlyIconEmit;

    /* When true, DataModel::addFolder adds a folder's rows with a single setRowCount insert
       and fills their data with the model's signals blocked, emitting one dataChanged for
       the range — instead of one rowsInserted + ~20 dataChanged per file.

       DEFAULT OFF: that single wide dataChanged over the whole inserted block makes the
       sorting-enabled proxy/tableView re-sort the block descending (folders loaded Z-A
       instead of A-Z). The per-row loop preserves A-Z, and measured load latency is
       unchanged — the per-icon win comes from useVisibleOnlyIconEmit, not from this.
       Leave false unless the reordering is fixed first. */
    extern bool useBatchedFolderInsert;

    /* When true, DataModel emits concise [PERF] timing lines for the Phase 1 folder
       load (enumerate+sort vs model/proxy/view insert, plus total wall time). Used to
       A/B load-pipeline changes against the recursive pictures tree. Off in production. */
    extern bool isPerfProbe;

    /* When true, DataModel::addFolder throttles its "Searching for images…" progress
       message (emit centralMsg) to ~50 ms. Each emit drives MW::setCentralMessage, which
       does a synchronous repaint(); firing it once per folder cost ~1.3 s over a 1333-folder
       recursive tree. Set false to restore the per-folder emit (A/B baseline). */
    extern bool throttleFolderLoadMsg;

    /* Selects the RAW decode engine (see DecodeRawEngine). Defaults to the portable in-house
       engine; appleDecodeRawEngine is honoured only on macOS and otherwise falls back to
       winnowDecodeRawEngine. A/B knob for the Core Image vs in-house decode paths. */
    extern DecodeRawEngine decodeRawEngine;
    extern OperationMode operationMode;     // Preview (fast review) vs Develop (best-quality single image)

    /* Develop slider-drag latency probe. When true, MW::developParamsChange logs per-stage
       timings (copy / Apply / ToImage / rotate / preview) for each re-render so the dominant
       cost can be measured before optimising. Default false. */
    extern bool isReportDevelopTime;

    /* Gate for the OPTIONAL debounce-while-editing write of per-image Develop settings to the XMP
       sidecar (a short time after edits settle). Navigate-away / quit / pre-op flushes always run
       regardless of this flag. Default true; turn off to avoid mid-edit disk writes. */
    extern bool isDevelopDebounceWrite;

    /* Phase-2 probe: count of Thumb::loadThumb 100ms retry waits (file-open contention with
       ImageCache) across all reader threads. Reset in MetaRead::initialize, reported in
       MetaRead::allFinished. High count => the retry loop is a real staller. */
    extern std::atomic<int> probeThumbRetryCount;

    extern QColor textColor;
    extern QColor backgroundColor;
    extern QColor disabledColor;
    extern QColor header1Color;
    extern QColor header2Color;
    extern QColor header3Color;
    extern QColor borderColor;
    extern QColor tabWidgetBorderColor;
    extern QColor pushButtonBackgroundColor;
    extern QColor scrollBarHandleBackgroundColor;
    extern QColor helpColor;
    extern QColor selectionColor;
    extern QColor mouseOverColor;
    extern QColor appleBlue;
    extern QString lightgray;
    extern QString darkgray;
    extern QString lightpurple;
    extern QString darkpurple;
    extern QString lightblue;
    extern QString darkblue;
    extern QString lightyellow;
    extern QString darkyellow;
    extern QString lightorange;
    extern QString darkorange;
    extern QString lightred;
    extern QString darkred;
    extern QString lightcyan;
    extern QString darkcyan;
    extern QString lightgreen;
    extern QString darkgreen;
    extern QString lightteal;
    extern QString darkteal;
    extern QString lightmaroon;
    extern QString darkmaroon;
    extern QString lightpink;
    extern QString darkpink;
    extern QString lightmagenta;
    extern QString darkmagenta;

    extern QColor labelNoneColor;
    extern QColor labelRedColor;
    extern QColor labelYellowColor;
    extern QColor labelGreenColor;
    extern QColor labelBlueColor;
    extern QColor labelPurpleColor;

    extern QStringList ratings;
    extern QStringList labelColors;

    extern double iconOpacity;

    extern int wheelSensitivity;
    extern bool wheelSpinning;

    extern QString mode;
    extern QString fileSelectionChangeSource;
    extern bool autoAdvance;

    extern int fontSize;                        // in pixels
    extern QString strFontSize;                 // in pixels
    extern qreal dpi;                           // current screen dots per inch
    extern qreal ptToPx;
    extern int textShade;
    extern int backgroundShade;
    extern QString css;
    // Semantic state stylesheets (not theme-bound). Set via setStyleSheet to
    // signal widget state; for size-sensitive widgets use setFont() so size
    // persists when the stylesheet is replaced.
    extern QString cssError;        // red foreground - error / running / out-of-range
    extern QString cssWarning;      // yellow foreground - warning
    extern QString cssOk;           // green foreground - idle / success
    extern QString cssInactive;     // gray foreground - inactive

    // extern bool isModifyingDatamodel;
    extern bool isFirstImageNewInstance;
    extern bool ignoreScrollSignal;
    extern bool resizingIcons;
    extern bool isSlideShow;
    extern bool isRunningColorAnalysis;
    extern bool isRunningStackOperation;
    extern bool isProcessingExportedImages;
    extern bool isEmbellish;
    extern bool includeSidecars;
    extern bool colorManage;
    extern bool modifySourceFiles;
    extern bool backupBeforeModifying;
    extern bool autoAddMissingThumbnails;
    extern bool renderVideoThumb;
    extern bool combineRawJpg;
    extern bool useRaw;         // decode raw sensor data (true) vs embedded preview/jpg (false)
    extern bool isFilter;

    // focus stack
    extern QStringList fsFusedPaths;

    // training
    extern bool isTraining;

    extern bool isRunningBackgroundIngest;
    extern int ingestCount;
    extern QDate ingestLastSeqDate;

    // copying files
    extern bool isCopyingFiles;
    extern bool stopCopyingFiles;

    extern bool isThreadTrackingOn;
    extern bool showAllTableColumns;

    extern int scrollBarThickness;
    extern int propertyWidgetMarginLeft;
    extern int propertyWidgetMarginRight;
    extern QModelIndexList copyCutIdxList;  // req'd?
    extern QStringList copyCutFileList;     // req'd?

    extern QString tiffData;

    extern QElapsedTimer t;
    extern QElapsedTimer t1;
    extern bool isTimer;
    extern bool isTest;
    extern bool isStressTest;

    // Signal relay class for global communication
    class SignalRelay : public QObject {
        Q_OBJECT
    public:
        explicit SignalRelay(QObject *parent = nullptr) : QObject(parent) {}

    signals:
        void updateStatus(bool keepBase, QString s, QString source);
        void showPopUp(QString msg, int duration, bool isAutosize,
                       float opacity, Qt::Alignment alignment);
    };

    extern SignalRelay *relay;

    void setDM(QObject *dm);

    extern Logger logger;
    extern void track(QString functionName = "", QString comment = "", bool hideTime = false);
    extern void log(QString functionName = "",
                    QString comment = "",
                    bool zeroElapsedTime = false);
    extern IssueLog *issueLog;
    extern void newIssueLog();
    extern QMutex issueListMutex;

    // Severity threshold — issues at or below this level are dropped before
    // any allocation. Defaults to Info: Debug + Comment are filtered.
    // Bumped temporarily via G::isVerboseIssues for diagnostic sessions.
    extern int issueThreshold;
    extern bool isVerboseIssues;

    // Cap on G::issueList growth (ring buffer). The full record still goes
    // to IssueLog::log() on disk; issueList is the in-memory tail.
    extern int issueListMaxSize;

    extern void issue(QString type, QString msg = "", QString src = "",
                      int sfRow = -1, QString fPath = "");

    // Coalesce duplicate issues per (src, type, msg) — caller passes a
    // hint, the function counts repeats and logs only first + summary.
    extern void issueDedup(QString type, QString msg, QString src,
                           int sfRow = -1, QString fPath = "");
    extern QStringList issueDedupReport();
    extern void issueDedupReset();
    extern void issueBeginSession();

    extern void wait(int ms);
    extern QString s(QVariant x);
    extern QString sj(QString s, int x);
    extern bool instanceClash(int instance, QString src);

    extern bool isGuiThread();

    extern int popUpProgressCount;
    extern int popUpLoadFolderStep;
    extern Popup *popup;
    extern void newPopUp(QWidget *widget, QWidget *centralWidget);
}
#endif // GLOBAL_H

