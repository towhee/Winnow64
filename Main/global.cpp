#include "Main/global.h"
#include <algorithm>

#ifdef Q_OS_MAC
    #include <mach/mach.h>
    #include <mach/task.h>
#endif
#ifdef Q_OS_WIN
    #include <Windows.h>
    #include <psapi.h>
#endif

namespace G
{

// Rory version (expanded cache pref, focus stack pref, update default workspace)
bool isRory = true;

QSettings *settings;

// LOG
bool isLogger = false;              // Writes all log messages to file or console
bool isFlowLogger = false;          // Writes key program flow points to file or console
bool sendLogToFile = true;         // Writes log messages to file (isLogAllToFileForDebugging)
bool isRunByExtern = false;         // Writes log messages to file (debug executable ie remote embellish ops)
QFile logFile;                      // MW::openLog(), MW::closeLog()

// ISSUES
bool isIssueLogger = true;          // Writes issue log messages to file or console
bool isVerboseIssues = false;       // When true: threshold drops to Debug
bool showIssueInConsole = false;    // Writes issues to qDebug
int issueThreshold = Issue::Info;   // Drop issues below this severity
int issueListMaxSize = 5000;        // Ring-buffer cap for in-memory G::issueList
QFile issueLogFile;                 // MW::openErrLog(), MW::closeErrLog()

bool sendLogToConsole = true;       // true: console, false: WinnowLog.txt

bool FSLog = true;                 // Focus Stack log
bool embelLog = false;               // Embellish log

bool showAllEvents = false;
// Errors
QStringList issueList;



// mutex
QWaitCondition waitCondition;
QMutex gMutex;

QThread* guiThread;;                    // use to check

// flow
bool isInitializing = true;             // flag program starting / initializing
std::atomic<bool> stop{false};
std::atomic<bool> removingFolderFromDM{false};
std::atomic<bool> removingRowsFromDM{false};

// datamodel status
std::atomic<bool> allMetadataAttempted{false};
std::atomic<bool> iconChunkLoaded{false};
std::atomic<int> dmInstance{0};
std::atomic<bool> isModifyingDatamodel{false};
std::atomic<bool> isLoadRunning{false};


// temp while resolving issues, set false to not use
bool useMyTiff = true;
bool useMissingThumbs = true;
bool suppressTiffWarnings = true;   // silence libtiff warning messages to the console

// limit functionality for testing
bool useApplicationStateChanged = false;
bool useZoomWindow = false;;
bool useFSTreeCount = true;
bool useReadMeta = true;
bool useReadIcons = true;
bool useImageCache = true;
bool useImageView = true;
bool useInfoView = true;
bool useDWCollapse = false;         // master switch for dock collapse/expand/solo mode
bool useDockTitleGraphic = true;    // master switch: show a graphic instead of text on dock tabs (TEST: on for folders tab)
bool useMultimedia = true;
bool useLamaSpotFill = true;        // TEST: spot heals with LaMa (GPU); false -> MI-GAN
/* Fill/Object replace modes. Shelved 2026-07-17 (Winnow scope = spot cleanup only; see
   Documentation.txt "WHAT WAS TRIED"): true shows the ReplacePanel whenever the replace
   tool is armed and offers its Fill/Object modes; false hides the panel and arms Spot
   mode only. Stays FALSE until Fill and Object are actually implemented -- the panel is
   only a mode picker for them, so on its own it offers a choice with nothing behind it.
   Briefly flipped true on 2026-09-18 to look at the panel, and turned back off. */
bool useReplaceFillModes = false;
bool useFilterPanel = true;
/* Brush/Object "erase from this stroke" (Opt while painting removes from the stroke)
   CANCELLED 2026-08-08. It existed because the developed effect only appeared on stroke
   release, so erasing inside the submask was the only way to correct a stroke you could
   not yet judge. The effect now re-renders DURING the swipe (brushEmitLiveGeometry), so
   a mis-paint is visible as it happens and Opt is better spent on its other, less
   discoverable meaning: this submask SUBTRACTS from the mask. With this false Opt means
   Subtract everywhere -- one key, one meaning. Set true to restore the erase stroke
   (the whole path is intact: maskBrushErase, the "erase" stroke flag, BrushStamp's erase
   composite, the pink cursor); delete it and the erase path once this proves out. */
bool useBrushEraseStroke = false;
/* Model-path heal correction (Object fills + clone fallbacks; clone heals need none).
   Poisson default: keeps the model's content with the level solved from the boundary. */
int  spotFillCorrectMode = 1;       // A/B: 0 none 1 Poisson 2 low-freq 3 harmonic
bool spotFillGrain = true;          // add surround-matched grain to the heal (N toggles)
bool useUpdateStatus = true;
bool useFilterView = true;          // not finished
bool useProcessEvents = false;

// system display
QHash<QString, WinScreen> winScreenHash;    // record icc profiles for each monitoriconLoaded
QString winOutProfilePath;
int displayPhysicalHorizontalPixels;// current monitor
int displayPhysicalVerticalPixels;  // current monitor
int displayVirtualHorizontalPixels; // current monitor
int displayVirtualVerticalPixels;   // current monitor
qreal actDevicePixelRatio;          // current monitor
qreal sysDevicePixelRatio;          // current monitor

QString trash;                      // Mac = Trash PC = recycle bin

// application parameters
QString strFontSize = "12";         // app font point size
int fontSize = 12;                  // kept in step with strFontSize
qreal dpi;                          // current logical screen dots per inch
qreal ptToPx;                       // font points to pixels conversion factor

/*  Application colours and UI style values. ALL are ASSIGNED in WidgetCSS::styleGlobals
    (Main/widgetcss.cpp) -- the one place to experiment with the app's look. */
int textShade;
int backgroundShade;                // app background luminousity (settings)
QColor textColor;
QColor backgroundColor;             // from backgroundShade (settings / MW)
QColor disabledColor;
QColor header1Color;
QColor header2Color;
QColor header3Color;
QColor scopeSubheaderColor;
QColor borderColor;
QColor tabWidgetBorderColor;
QColor unfiledKeywordColor;
QColor appliedKeywordColor;
QColor frameLineColor;
int frameLineWidth;
int frameLineRadius;
QColor pushButtonBackgroundColor;
QColor scrollBarHandleBackgroundColor;
QColor helpColor;
QColor appleBlue;
QColor selectionColor;
QColor mouseOverColor;
QColor panelContentBg;
QColor groupSeparatorColor;
int groupSeparatorWidth;
int groupSeparatorHeight;
int groupSeparatorInset;
QColor panelSeparatorColor;
QColor headerGradientTop;
QColor headerGradientBottom;
QString lightgray;
QString darkgray;
QString lightpurple;
QString darkpurple;
QString lightblue;
QString darkblue;
QString lightyellow;
QString darkyellow;
QString lightorange;
QString darkorange;
QString lightred;
QString darkred;
QString lightcyan;
QString darkcyan;
QString lightgreen;
QString darkgreen;
QString lightteal;
QString darkteal;
QString lightmaroon;
QString darkmaroon;
QString lightpink;
QString darkpink;
QString lightmagenta;
QString darkmagenta;

QString css;                        // app stylesheet;

/*  Semantic state stylesheets - not theme-bound; used for status indicators,
    error highlights, etc. Color, not font size: widgets that need a specific
    size (e.g. status dots on Windows) should set it via setFont() so it
    persists across setStyleSheet() calls. */
QString cssError    = "QLabel,QLineEdit,QComboBox {color:red;}";
QString cssWarning  = "QLabel,QLineEdit,QComboBox {color:yellow;}";
QString cssOk       = "QLabel,QLineEdit,QComboBox {color:green;}";
QString cssInactive = "QLabel,QLineEdit,QComboBox {color:gray;}";

QColor labelNoneColor;              // pick labels: set in WidgetCSS::styleGlobals
QColor labelRedColor;
QColor labelYellowColor;
QColor labelGreenColor;
QColor labelBlueColor;
QColor labelPurpleColor;
QColor maskOverlayColor(QColor(220,40,40));    // Develop mask overlay (see global.h)
bool maskOverlayGrayscale = false;             // desaturate under the overlay (global.h)

QStringList ratings, labelColors;

double iconOpacity;                 // set in WidgetCSS::styleGlobals

// ui
int wheelSensitivity = 50;          // percent of maximum sensitivity, 1 - 100
bool wheelSpinning = false;

int panelFontSize()
{
    /*  A first run (no settings) used to leave fontSize at 0 while strFontSize was "12",
        which made the panels 6pt.  Fall back to strFontSize, then to the 12pt default,
        so an unset fontSize can never shrink the panels. */
    int base = fontSize;
    if (base <= 0) base = strFontSize.toInt();
    if (base <= 0) base = 12;
    return qMax(8, qRound(base * 0.85));
}

QString panelFontCss(const QString &selector)
{
    // Same unit as WidgetCSS::widget(), where fontSize becomes every widget's size
    return selector + " {font-size:" + QString::number(panelFontSize()) + "pt;}";
}

int wheelSpinThreshold()
/*
    The amount of wheel spinning required before the next/previous image: milliseconds
    between advances in ImageView, accumulated angle delta in VideoView.  Larger = less
    sensitive, so it is the INVERSE of the wheelSensitivity percent: 100% gives 1, 1%
    gives 210.  The 1 - 210 range is the scale those two views were tuned on, before
    the preference itself became a percent.
*/
{
    const int pct = qBound(1, wheelSensitivity, 100);
    return qRound(210.0 - (pct - 1) * 209.0 / 99.0);
}

// caching
bool loadOnlyVisibleIcons;          // not used
std::atomic<quint64> availableMemoryMB{0};
int winnowMemoryBeforeCacheMB;
int metaCacheMB;

// view
QString mode;                       // In MW: Loupe, Grid, Table or Compare
QString fileSelectionChangeSource;  // GridMouseClick, ThumbMouseClick, TableMouseClick
bool autoAdvance;

// icons
int maxIconSize = 256;
int minIconSize = 40;
int maxIconChunk = 10000;
bool   useJitIconCache = false;         // testing flag; see DataModel::resolveIconChunkSize
double jitIconCacheMemFraction = 0.5;   // share of free-remainder memory budgeted for thumbnails
bool   showCacheProgress = true;        // single gate for ImageCache + MetaRead progress display
std::atomic<qint64> imageCacheHeadroomMB{0};  // image cache's remaining intended claim (MB)
int iconPressureTestLevel = -1;         // -1 real; 0 normal+recovered; 1 warn; 2 critical; 3 normal-not-recovered
bool useVisibleOnlyIconEmit = true;     // setIcon1/setValDm/setValSf notify views only for visible rows (set false for prior behavior)

// Mode (Preview / Develop)
OperationMode operationMode = OperationMode::Preview;   // start in fast-review Preview mode
Scope scope = Scope::Folders;           // a folder is what opens at startup
/* Show the developed picture by default: an image the user has edited should look
   edited everywhere. Falls back to the camera render wherever no devPreview exists. */
PreviewSource previewSource = PreviewSource::Developed;
int devPreviewMaxEdge = kDevPreviewSizeFull;
int devPreviewQuality = kDevPreviewQualityDefault;
qint64 devPreviewCacheMaxBytes = 20LL * 1024 * 1024 * 1024;   // 20 GB
/*  0 = NO LIMIT, and that is the default: Catalog scope shows the whole catalog, the way
    picking a folder shows the whole folder. The cap existed because a row cost ~20 KB and
    every row had to be read from its file; the packed row store and Catalog::searchRows
    removed both reasons. It survives as a safety valve the user can set. */
int maxSearchResults = 0;
bool useIndexMetadata = false;   // default off; see global.h
bool useScrollInVerify = true;   // default on; see global.h
bool autoScanCatalog = true;     // default on; see global.h
int autoScanCatalogMinutes = 60; // one automatic scan an hour at most
bool cacheThumbnails = true;                 // write thumbnails to the index as icons load
qint64 thumbCacheMaxBytes = 5LL * 1024 * 1024 * 1024;    // 5 GB ~ 250k thumbnails
bool buildDevPreviewsInBackground = false;
bool autoRunDenoise = true;
bool autoRemoveCA = false;

bool useBatchedFolderInsert = true;    // batched per-folder insert (one rowsInserted + one dataChanged); cuts Phase-1 insert ~34%. Z-A reorder fixed: dynamic sort disabled during load, restored once at end (see DataModel::scheduleProcessing / restoreProxySortAfterLoad)
bool throttleFolderLoadMsg = true;     // throttle addFolder progress message to ~50ms (per-folder centralMsg repaint cost ~1.3s/1333 folders)
// DecodeRawEngine decodeRawEngine = DecodeRawEngine::winnowDecodeRawEngine;  // portable default; appleDecodeRawEngine is macOS-only (callers fall back to winnow off-mac)
DecodeRawEngine decodeRawEngine = DecodeRawEngine::appleDecodeRawEngine;  // portable default; appleDecodeRawEngine is macOS-only (callers fall back to winnow off-mac)
bool isDevelopDebounceWrite = true;     // also flush per-image develop settings to sidecar a short time after edits settle

/*  PROBE SWITCHES: every probe's on/off flag, all in one place (see global.h). All
    default false -- a probe left on ships tracing and its cost into production. The
    counters the probes accumulate follow below.
*/
bool isPerfProbe = false;                // [PERF] Phase 1/2 load timing lines
std::atomic<bool> isIngestProbe{false};  // [INGEST] cull path: selection->loupe, key cost
std::atomic<bool> isPanelProbe{false};   // dock geometry negotiation (panel sizing glitches)
bool isReportDevelopTime = false;        // [DevTime] per-stage Develop re-render timings
bool isCopyPathProbe = false;            // [COPYPATH] context-menu Copy path tracing
bool isWheelProbe = false;               // [WHEEL] IconView wheel/trackpad events

std::atomic<int> probeThumbRetryCount{0};  // count of Thumb::loadThumb 100ms retry waits (Phase-2 probe)
std::atomic<int> probeIndexMetaHits{0};   // rows filled from the local index (Phase-2 probe)
std::atomic<int> probeIndexMetaMisses{0}; // rows the index could not answer (Phase-2 probe)
std::atomic<qint64> probeIconDevThumbNs{0};  // icon decomposition (Phase-2 probe)
std::atomic<qint64> probeIconLoadNs{0};
std::atomic<qint64> probeIconScaleNs{0};
std::atomic<qint64> probeIconOrientNs{0};
std::atomic<qint64> probeIconCacheNs{0};
std::atomic<qint64> probeIconCacheGetNs{0};
std::atomic<int>    probeIconCacheHits{0};
std::atomic<int>    probeIconCacheMisses{0};
std::atomic<qint64> probeThumbStatNs{0};
std::atomic<qint64> probeThumbLockNs{0};
std::atomic<qint64> probeThumbSqlNs{0};
std::atomic<qint64> probeThumbDecodeNs{0};
std::atomic<int>    probeThumbStamps{0};
std::atomic<qint64> probeThumbWriterHoldNs{0};
std::atomic<qint64> probeThumbWriterPrepNs{0};
std::atomic<qint64> probeThumbEvictNs{0};
std::atomic<int>    probeThumbEvicts{0};
std::atomic<int>    probeThumbStampBatches{0};
std::atomic<int>    probeIconCount{0};
std::atomic<int>    probeEmitVisible{0};
std::atomic<int>    probeEmitSuppressed{0};

// status
// bool isModifyingDatamodel;
bool isFirstImageNewInstance;
bool ignoreScrollSignal = false;   // owned by G::ScrollSignalGuard; see global.h
bool resizingIcons;
bool isSlideShow;
bool isRunningColorAnalysis;
bool isRunningStackOperation;
bool isProcessingExportedImages;
bool isEmbellish;
bool includeSidecars;
bool colorManage;
bool modifySourceFiles;
bool backupBeforeModifying;
bool autoAddMissingThumbnails;
bool renderVideoThumb;
bool combineRawJpg;
bool useRaw;
bool isFilter;
bool isRemote;

// focus stack
QStringList fsFusedPaths;

// training
bool isTraining = false;

// ingest
bool isRunningBackgroundIngest;
int ingestCount = 0;
QDate ingestLastSeqDate;

// copying files
bool isCopyingFiles;
bool stopCopyingFiles;

// not persistent
bool isThreadTrackingOn;
bool showAllTableColumns;

// UI layout metrics: set in WidgetCSS::styleGlobals
int scrollBarThickness;
int propertyWidgetMarginLeft;
int propertyWidgetMarginRight;
int decorationTitleGap;
int headerBtnGap;
int headerBtnRightInset;
int subHeaderIndent;
int headerLeftInset;
int panelBorderHeight;
int headerCaptionTrim;
QModelIndexList copyCutIdxList;
QStringList copyCutFileList;

QString tiffData;                   // temp for testing tiff decoder performance

// Initialize the global signal relay
SignalRelay *relay = nullptr;

QElapsedTimer t;
bool isTimer;
bool isTest;
bool isStressTest;
bool isAutomatedRun = false;

// memory overrun guardrail
quint64 memoryAbortMB = 6000;
std::atomic<bool> memoryOverrunFlag{false};

quint64 processFootprintMB()
{
#ifdef Q_OS_MAC
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        return static_cast<quint64>(info.phys_footprint / (1024ull * 1024ull));
    }
    return 0;
#elif defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<quint64>(pmc.WorkingSetSize / (1024ull * 1024ull));
    }
    return 0;
#else
    return 0;
#endif
}

quint64 computeMemoryAbortMB(quint64 totalRamMB)
{
    if (totalRamMB == 0) return memoryAbortMB;
    const quint64 reserve  = std::max<quint64>(4096, totalRamMB / 4);
    const quint64 ceiling  = (totalRamMB * 85) / 100;
    const quint64 floorMB  = std::min<quint64>(2048, totalRamMB / 2);
    const quint64 candidate = (totalRamMB > reserve) ? totalRamMB - reserve : floorMB;
    return std::clamp(candidate, floorMB, ceiling);
}

QString s(QVariant x)
// helper function to convert variable values to a string for reporting
{
//        qDebug() << "Global::s" << x.typeName() << x.typeId();
    if (x.typeId() == QMetaType::QStringList)
        return Utilities::stringListToString(x.toStringList());
    if (x.typeId() == QMetaType::QRect) {
        QRect r = x.toRect();
        return QString::number(r.x()) + ", " +
               QString::number(r.y()) + ", " +
               QString::number(r.width()) + "x" +
               QString::number(r.height());
    }
    if (x.typeId() == QMetaType::QPointF) {
        QPointF p = x.toPointF();
        return QString::number(p.x(),'f', 2) + ", " + QString::number(p.y(),'f', 2);
    }
    return QVariant(x).toString();
}

QString sj(QString s, int x)
// helper function to justify a string with ....... for reporting
{
    s += " ";
    return s.leftJustified(x, '.') + " ";
}

void wait(int ms)
/*
    Use as an alternative to qApp->processEvents().
*/
{
    if (ms <= 0) return;
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

void track(QString functionName, QString comment, bool hideTime)
{
    qint64 nsecsElapsed = t.nsecsElapsed();
    QString time = QString("%L1").arg(nsecsElapsed);
    if (hideTime) time = "";
    qDebug().noquote()
             << time.rightJustified(15, ' ') << " "
             << functionName.leftJustified(50, '.') << " "
             << comment;
    t.restart();
}

//*** POPUP ******************************************************************************

/*
    IF CALLING FROM A NON-GUI THREAD
    Use (example) emit G::relay->showPopUp(msg, 0, true, 0.75, Qt::AlignHCenter);
*/

int popUpProgressCount = 0;
int popUpLoadFolderStep = 100;
Popup *popup = nullptr;
void newPopUp(QWidget *widget, QWidget *centralWidget)
{
    popup = new Popup(widget, centralWidget);
}
QStringList startupWarnings;

//*** LOGGER ******************************************************************************

Logger logger;

void log(QString functionName, QString comment, bool zeroElapsedTime)
{
    if (!sendLogToFile) {
        if (functionName == "") qDebug() << " ";  // empty line
        logger.log(functionName, comment);
        return;
    }

    // save log as per Utilities::log
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Log";
    QDir dir(path);
    if (!dir.exists()) dir.mkpath(path);
    QFile fLog(path + "/WinnowLog.txt");
    // if (fLog.isOpen()) fLog.close();

    // Use Append instead of ReadWrite + readAll()
    if (fLog.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QString t = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString txt = QString("%1  %2  %3\n").arg(t, functionName, comment);

        fLog.write(txt.toUtf8());
        fLog.close();
    }

}

//*** ISSUES ******************************************************************************

IssueLog *issueLog = nullptr;
void newIssueLog()
{
    issueLog = new IssueLog();
}

void deleteIssueLog()
{
/*
    Tear down the issue log during shutdown.  G::issueLog is nulled while holding
    issueListMutex - the same mutex issue() and issueBeginSession() hold when they
    call issueLog->log() - before the object is destroyed.  Without this, queued
    signals still draining from the event loop after MW::closeEvent (eg
    DataModel::setValSf -> G::issue) dereference a freed IssueLog and crash in
    QThread::isRunning().
*/
    IssueLog *log = nullptr;
    {
        QMutexLocker locker(&issueListMutex);
        log = issueLog;
        issueLog = nullptr;
    }
    delete log;     // IssueLog::~IssueLog stops the thread
}

static QObject *modelInstance = nullptr;
void setDM(QObject *dm)
{
    modelInstance = dm;
}

QMutex issueListMutex;

// Per-(src, type, msg) repeat counter for issueDedup().
static QHash<QString, int> issueDedupCounts;
static QMutex issueDedupMutex;

static int effectiveIssueThreshold()
{
    return isVerboseIssues ? Issue::Debug : issueThreshold;
}

void issue(QString type, QString msg, QString src, int sfRow, QString fPath)
{
    if (!isIssueLogger) return;

    // Resolve type up front so we can filter before any allocation.
    Issue::Type resolvedType;
    QString resolvedMsg;
    int idx = Issue::typeDescList().indexOf(type);
    if (idx >= 0) {
        resolvedType = static_cast<Issue::Type>(idx);
        resolvedMsg = msg;
    }
    else {
        // Unknown type string — keep both pieces so the original message is not lost.
        resolvedType = Issue::Undefined;
        resolvedMsg = msg.isEmpty() ? type : (type + ": " + msg);
    }

    // Severity filter (Undefined is never filtered — it represents a caller bug).
    if (resolvedType != Issue::Undefined && resolvedType < effectiveIssueThreshold()) return;

    QMutexLocker locker(&issueListMutex);

    QSharedPointer<Issue> ev = QSharedPointer<Issue>::create();
    ev->type = resolvedType;
    ev->msg = resolvedMsg;
    ev->src = src;
    ev->sfRow = sfRow;
    ev->fPath = fPath;
    ev->timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ");

    QString line = ev->toString();

    if (showIssueInConsole) {
        qDebug().noquote() << line;
    }

    if (modelInstance && sfRow > -1) {
        QMetaObject::invokeMethod(
            modelInstance,
            "issue",
            Q_ARG(QSharedPointer<Issue>, ev)
        );
    }

    // Ring-buffer: keep only the tail in memory. Full history is on disk.
    issueList.append(line);
    if (issueListMaxSize > 0 && issueList.size() > issueListMaxSize) {
        issueList.removeFirst();
    }

    if (issueLog) issueLog->log(line);
}

void issueDedup(QString type, QString msg, QString src, int sfRow, QString fPath)
{
    QString key = src + "\x1f" + type + "\x1f" + msg;
    int count;
    {
        QMutexLocker dlock(&issueDedupMutex);
        count = ++issueDedupCounts[key];
    }
    // Log first occurrence at full severity; subsequent occurrences silently counted.
    if (count == 1) issue(type, msg, src, sfRow, fPath);
}

QStringList issueDedupReport()
{
    QMutexLocker dlock(&issueDedupMutex);
    QStringList out;
    for (auto it = issueDedupCounts.constBegin(); it != issueDedupCounts.constEnd(); ++it) {
        if (it.value() > 1) {
            out.append(QString("repeated %1x  %2").arg(it.value()).arg(it.key()));
        }
    }
    return out;
}

void issueDedupReset()
{
    QMutexLocker dlock(&issueDedupMutex);
    issueDedupCounts.clear();
}

void issueBeginSession()
{
    // Structural marker, not an issue. Bypasses severity gating and the
    // datamodel route, writes a separator straight to the disk log.
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ");
    QString line = ts + "----- New Session -----";
    if (showIssueInConsole) qDebug().noquote() << line;
    QMutexLocker locker(&issueListMutex);
    issueList.append(line);
    if (issueListMaxSize > 0 && issueList.size() > issueListMaxSize) {
        issueList.removeFirst();
    }
    if (issueLog) issueLog->log(line);
}

bool instanceClash(int instance, QString src)
{
    bool clash = (dmInstance != instance);
    if (clash) {
        if (showIssueInConsole)
        qDebug() << "WARNING G::instanceClash"
                   << "instance =" << instance
                   << "DM instance =" << dmInstance
                   << "src =" << src
                      ;
    }
    return clash;
}

bool isGuiThread()
{
    return QThread::currentThread() == guiThread;
}

} // end NameSpace G

