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
        GeekRole,                       // used in TableView display of columns
        /* Filters: true on a keyword item the catalog has seen under more than one
           parent, so the item carries its own ambiguity rather than the panel consulting
           a shared set. Written ONLY on the GUI thread (by
           Filters::refreshAmbiguousKeywords) and read wherever an item is restyled --
           including from the BuildFilters worker thread, which must not touch that set
           while the GUI thread is rebuilding it. */
        AmbiguousKeywordRole
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
        WidthOrigPreviewColumn,
        HeightOrigPreviewColumn,
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
        /* True when the image has a non-identity Develop recipe in its sidecar. Drives the
           develop badge in the icon delegate. Appended (not inserted) so saved table
           column order/width settings, which are keyed by index, still line up. */
        DevelopColumn,
        /* Hash of that recipe, so a decoder thread can look this image's devPreview up in
           the devPreview cache. Appended for the same reason as DevelopColumn. */
        DevPreviewKeyColumn,
        /* Hierarchical keyword paths from lr:hierarchicalSubject ie "Wildlife|Birds|
           Heron", parallel to KeywordsColumn's flat leaf names. Appended for the same
           reason as DevelopColumn. */
        KeywordPathsColumn,
        /* The FLAT keyword vocabulary this image is filtered and searched on: the
           de-duplicated union of KeywordsColumn's dc:subject leaves and every NODE of
           KeywordPathsColumn's hierarchical paths (Metadata/keywordflatten.h). This is
           what the Filters Keywords category and the catalog category both read.

           KeywordsColumn deliberately keeps the LITERAL dc:subject list rather than being
           replaced by this one. The two are different facts and only one of them may ever
           be written back to a file: emitting this column as dc:subject would put every
           ancestor ("Wildlife", "Birds") into a property that never held them.

           Appended for the same reason as DevelopColumn. */
        KeywordsAllColumn,
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
       Entered with D; the status-bar dropdown at the extreme left switches either way
       (see PreviewSource below -- one control covers both). */
    enum class OperationMode : quint8 { Preview, Develop };
    Q_ENUM_NS(OperationMode)

    /*  WHICH SET OF IMAGES THE USER IS LOOKING AT.

        Folders = whatever the Folders/Bookmarks selection loaded. Catalog = the
        whole local index, searched across folders. It is ONE fact with three
        views -- the Catalog row above the Folders tree, the same row above
        Bookmarks, and the Find dock's Folders|Catalog buttons -- so it lives
        here and MW::setScope is the only thing that changes it. Before this the
        Find dock owned the scope privately and the trees knew nothing about it,
        which is why selecting a folder and searching the catalog felt like two
        different applications. */
    enum class Scope : quint8 { Folders, Catalog };
    Q_ENUM_NS(Scope)

    /* Which of an image's two pictures the grid and the loupe show when NOT in Develop
       mode (in Develop the developed picture is the only sensible answer):

         Original   the camera's embedded preview / the plain decode -- as shot.
         Developed  the image rendered through its saved develop recipe (the devPreview),
                    falling back to Original when no devPreview exists.

       Persisted, unlike operationMode. Set through MW::setPreviewSource, which rebuilds
       the icons and the image cache because it changes what a cached decode MEANS. */
    enum class PreviewSource : quint8 { Original, Developed };
    Q_ENUM_NS(PreviewSource)

    /* Long-edge cap, in pixels, for the devPreview written to the devPreview cache.
       kDevPreviewSizeFull (0) means no cap -- encode at full sensor resolution, which is
       what makes a developed image browsable and zoomable without a raw decode. */
    constexpr int kDevPreviewSizeFull = 0;
    constexpr int kDevPreviewSizeLarge = 4096;
    constexpr int kDevPreviewSizeScreen = 2560;

    /* JPEG quality for the devPreview tier, entered as a number. 90 is the default and
       Qt's JPEG handler disables chroma subsampling only ABOVE it, so 91+ writes 4:4:4 at
       roughly double the size. The floor is 40 rather than 1 because below it JPEG starts
       showing blocking on smooth tone at any zoom, which defeats a preview meant to be
       looked at. The sidecar THUMBNAIL tier is not covered by this -- it is a 256 px grid
       icon and stays at its own fixed quality. */
    constexpr int kDevPreviewQualityDefault = 90;
    constexpr int kDevPreviewQualityMin = 40;
    constexpr int kDevPreviewQualityMax = 100;

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

    /* Return and Enter are the same command key everywhere in Winnow: every operation
       bound to one must also fire on the other.  The Enter key (numeric keypad Enter,
       or Fn+Return on a MacBook) reports Qt::Key_Enter and ALSO sets
       Qt::KeypadModifier, so a "bare key" test written as
       `e->modifiers() == Qt::NoModifier` silently rejects it -- use bareModifiers()
       for those gates.  See notes/Documentation.txt "Return and Enter". */
    inline bool isEnterKey(int key) {
        return key == Qt::Key_Return || key == Qt::Key_Enter;
    }
    inline bool isEnterKey(const QKeyEvent *e) {
        return e && isEnterKey(e->key());
    }

    // Modifiers with KeypadModifier removed, for `== Qt::NoModifier` style gates.
    inline Qt::KeyboardModifiers bareModifiers(const QKeyEvent *e) {
        return e->modifiers() & ~Qt::KeypadModifier;
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
    extern bool useLamaSpotFill;   // spot tool heals with LaMa (GPU) instead of MI-GAN
    extern bool useReplaceFillModes;   // Fill/Object modes shelved; false = spots only
    /* The unified Find dock: one panel with a Folders/Catalog scope switch over one
       category vocabulary, replacing the separate Filters and Catalog panels. False
       restores both panels exactly as they were -- an escape hatch while this proves out,
       not a shelved feature. It is read at dock-creation time, so it takes a restart to
       change. */
    extern bool useFindDock;
    extern bool useScopeHeaderLab;     // true = experimental ScopeHeaderLab in dock
    /* Develop Edits layout A/B/C -- where the scope is picked and where the editor (the
       Mask panel + adjustment tree) sits. See global.cpp and Documentation.txt
       "EDITS LAYOUT A/B/C". */
    enum class EditsLayout {
        Nested  = 0,    // scope list; editor inserted under the SELECTED row
        Flat    = 1,    // scope list; editor below the WHOLE list, under its own band
        Minimal = 2     // no list: one "Scope: [combo] [+] [eye] [:]" bar, no rail
    };
    extern EditsLayout developEditsLayout;
    extern bool useBrushEraseStroke;   // false = Opt always means Subtract, never erase
    extern int  spotFillCorrectMode;   // model-path heal correction 0-3, see global.cpp
    extern bool spotFillGrain;         // match surround grain into the heal (N toggles)
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

    /* Test override for Scope 3, so the shrink / evict / hysteresis path can be validated
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
    extern Scope scope;                     // Folders (loaded set) vs Catalog (whole index)
    extern PreviewSource previewSource;     // Original (as shot) vs Developed (devPreview)
    /* Long-edge cap for a written devPreview; one of the kDevPreviewSize* values. */
    extern int devPreviewMaxEdge;
    /* JPEG quality for a written devPreview; one of the kDevPreviewQuality* values. */
    extern int devPreviewQuality;
    /* LRU byte cap for the on-disk devPreview cache. Applied to DevPreviewCache at
       startup and whenever the preference changes. */
    extern qint64 devPreviewCacheMaxBytes;
    /* When true, a folder load queues a background devPreview build for every edited
       image that has no current devPreview. OFF by default: building one means decoding
       and rendering the image, which is exactly the work the byproduct rule avoids. */
    extern bool buildDevPreviewsInBackground;
    /* Run mode for the heavy PMRID raw denoise (the Develop dock's "Auto run" checkbox).
       true: run automatically on select / entering Develop / a denoise-param settle.
       false: only when "Denoise" is ticked by hand. Persisted to QSettings
       Develop/autoRunDenoise.

       Global rather than an MW member because it is part of the RENDER IDENTITY: it decides
       whether PMRID is baked into a default-render devPreview, so Metadata::defaultRenderKey
       must hash it, and that runs on ImageCache decoder threads which cannot reach MW. */
    extern bool autoRunDenoise;

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

    /* The ONE colour the Develop mask overlay is painted in (veil + per-submask
       preview). User-pickable from the Mask panel's swatch row; persisted as
       "Develop/maskOverlayColor" and seeded by the DevelopProperties ctor. */
    extern QColor maskOverlayColor;

    /* Show the image desaturated while the mask overlay is painted, so the coloured
       veil separates from the picture (a red veil over a red subject reads as noise).
       Toggled from the Mask panel's swatch row, persisted as
       "Develop/maskOverlayGrayscale" and seeded by the DevelopProperties ctor. It is a
       VIEW effect only (ImageView::drawForeground) -- the render and the export are
       untouched. */
    extern bool maskOverlayGrayscale;

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
    extern int decorationTitleGap;
    extern int headerBtnGap;            // Develop headers: gap between [eye] and [:]
    extern int headerBtnRightInset;     // ditto: inset from the panel's right edge
    extern int scopeRailX;              // Develop scope containment rail: left edge
    extern int scopeRailW;              // ditto: width (0 = no rail)
    extern int panelBorderHeight;       // Develop panel bottom separator rule height

    /* Stylesheet for a label that needs an EXPLICIT colour (panel captions, scope names,
       hints ...). Use this instead of a bare "color: x" string: a per-widget stylesheet
       OVERRIDES the app stylesheet, including its "QLabel:disabled" rule, so a hand-
       rolled "color: x" leaves the text at full strength when its panel is greyed -- the
       "disabled panel with live-looking captions". This emits both states, so every
       explicitly coloured label greys with its panel (Develop greys wholesale on a video
       selection; see MW::setDevelopPanelEnabled).
       ptSize 0 leaves the font size to the widget. The background is transparent so the
       label sits on whatever its panel paints (gradient band, selection fill, rail). */
    inline QString labelCss(const QColor &color, int ptSize = 0) {
        QString css = "QLabel { color: " + color.name() + "; background: transparent;";
        if (ptSize > 0) css += " font-size: " + QString::number(ptSize) + "pt;";
        css += " } QLabel:disabled { color: " + disabledColor.name() + "; }";
        return css;
    }

    /* A colour blended halfway into the panel background: the DISABLED form of anything
       that carries meaning through colour rather than text (a selection band, a hue
       chip, the scope rail). Still identifiable, visibly dead. Text does not use this --
       it has disabledColor. */
    inline QColor dimmed(const QColor &c) {
        return QColor((c.red()   + backgroundShade) / 2,
                      (c.green() + backgroundShade) / 2,
                      (c.blue()  + backgroundShade) / 2);
    }

    /* Panel captions that ACCENT their shortcut letter ("Crop", "Spot") do it with inline
       HTML, and an inline colour is not a stylesheet: ":disabled" cannot reach it, so the
       letter stays bright on a greyed panel while the rest of the caption dims. The pair
       below fixes that. setAccentCaption remembers the two halves on the label, and
       restyleAccentLabels re-renders every such label in a panel for its CURRENT enabled
       state -- call it from the panel's changeEvent on QEvent::EnabledChange. */
    inline QString accentHtml(const QString &letter, const QString &rest,
                              const QColor &accent) {
        return "<span style=\"color:" + accent.name() + "; font-weight:bold;\">" +
               letter.toHtmlEscaped() + "</span>" + rest.toHtmlEscaped();
    }
    inline void setAccentCaption(QLabel *label, const QString &letter, const QString &rest,
                                 const QColor &accent) {
        if (!label) return;
        label->setProperty("accentLetter", letter);
        label->setProperty("accentRest", rest);
        label->setProperty("accentColor", accent);
        label->setText(accentHtml(letter, rest,
                                  label->isEnabled() ? accent : disabledColor));
    }
    inline void restyleAccentLabels(QWidget *panel) {
        if (!panel) return;
        const QList<QLabel *> labels = panel->findChildren<QLabel *>();
        for (QLabel *l : labels) {
            const QVariant letter = l->property("accentLetter");
            if (!letter.isValid()) continue;
            const QColor accent = l->property("accentColor").value<QColor>();
            l->setText(accentHtml(letter.toString(), l->property("accentRest").toString(),
                                  l->isEnabled() ? accent : disabledColor));
        }
    }

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
    /* Stop + destroy the issue log, nulling G::issueLog under issueListMutex first
       so late queued calls into G::issue() cannot touch a freed IssueLog. */
    extern void deleteIssueLog();
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

    /* Problems found while READING SETTINGS AT STARTUP -- e.g. a saved value that no
       longer names anything this build understands. MW::loadSettings runs long before
       G::newPopUp creates the popup, so these cannot be shown when they are detected:
       append the message here and MW::whenActivated shows them once the window is up.
       See notes/Documentation.txt "Corrupted or changed develop settings". */
    extern QStringList startupWarnings;
}
#endif // GLOBAL_H

