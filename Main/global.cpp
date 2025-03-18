#include "Main/global.h"

namespace G
{

QSettings *settings;

// system messaging
bool isTestLogger = false;
bool isLogger = false;              // Writes log messages to file or console
bool isFlowLogger = false;          // Writes key program flow points to file or console
bool isFlowLogger2 = false;         // QDebug key program flow points
bool showIssueInConsole = false;    // Writes warnings to qDebug
bool isFileLogger = false;          // Writes log messages to file (debug executable ie remote embellish ops)
bool isErrorLogger = false;         // Writes error log messages to file or console
bool isIssueLogger = true;         // Writes issue log messages to file or console
bool sendLogToConsole = true;       // true: console, false: WinnowLog.txt

bool showAllEvents = false;
QFile logFile;                      // MW::openLog(), MW::closeLog()
QFile issueLogFile;                 // MW::openErrLog(), MW::closeErrLog()
// Errors
QStringList issueList;


// Rory version (expanded cache pref)
bool isRory = false;
ShowProgress showProgress = MetaCache;  // None, MetaCache, ImageCache
// ShowProgress showProgress = ImageCache; // None, MetaCache, ImageCache

// mutex
QWaitCondition waitCondition;
QMutex gMutex;

QThread* guiThread;;            // use to check

// flow
bool isInitializing;            // flag program starting / initializing
bool stop = false;              // flag to stop everything involving DM loading new dataset
bool removingFolderFromDM;      // flag when datamodel folder rows are being deleted
                                // all new folder processes.
// datamodel status
bool allMetadataLoaded;         // all metadata attempted
bool iconChunkLoaded;           // all icon chunk loaded

int dmInstance;                 // DataModel instance

// temp while resolving issues, set false to not use
bool useMyTiff = true;
bool useMissingThumbs = true;

// limit functionality for testing
bool useApplicationStateChanged = false;
bool useZoomWindow = false;;
bool useFSTreeCount = true;
bool useReadMeta = true;
bool useReadIcons = true;
bool useImageCache = true;
bool useImageView = true;
bool useInfoView = true;
bool useMultimedia = true;
bool useUpdateStatus = true;
bool useFilterView = true;          // not finished
bool useProcessEvents = true;

// system display
QHash<QString, WinScreen> winScreenHash;    // record icc profiles for each monitoriconLoaded
QString winOutProfilePath;
int displayPhysicalHorizontalPixels;// current monitor
int displayPhysicalVerticalPixels;  // current monitor
int displayVirtualHorizontalPixels; // current monitor
int displayVirtualVerticalPixels;   // current monitor
qreal actDevicePixelRatio;          // current monitor
qreal sysDevicePixelRatio;          // current monitor

// Mac = Trash PC = recycle bin
QString trash;

// application parameters
QString strFontSize;                // app font point size
int fontSize;
qreal dpi;                          // current logical screen dots per inch
qreal ptToPx;                       // font points to pixels conversion factor

// application colors
int textShade = 190;                // text default luminousity
int backgroundShade;                // app background luminousity
QColor textColor = QColor(textShade,textShade,textShade);
QColor backgroundColor;             // define after app stylesheet defined
QColor borderColor;                 // define after app stylesheet defined
QColor disabledColor;               // define after app stylesheet defined
QColor tabWidgetBorderColor;        // define after app stylesheet defined
QColor pushButtonBackgroundColor;   // define after app stylesheet defined
QColor scrollBarHandleBackgroundColor = QColor(90,130,100);
QColor appleBlue = QColor(21,113,211);      // #1571D3
QColor selectionColor = QColor(68,95,118);
QColor mouseOverColor = QColor(40,54,66);

QString css;                        // app stylesheet;

static int transparency = 50;
QColor labelNoneColor(85,85,85,transparency);                // Background Gray
QColor labelRedColor(QColor(60,20,20));          // Dark red
QColor labelYellowColor(QColor(80,55,15));     // Dark yellow
QColor labelGreenColor(QColor(20,40,20));        // Dark green
QColor labelBlueColor(QColor(20,20,60));         // Dark blue
//    QColor labelBlueColor(QColor(20,45,100));         // Dark blue
//    QColor labelBlueColor(QColor(32,58,124));         // Dark blue
QColor labelPurpleColor(QColor(50,30,70));     // Dark purple
//    QColor labelPurpleColor(QColor(60,30,90));     // Dark purple
//    QColor labelPurpleColor(QColor(54,37,95));     // Dark purple

QStringList ratings, labelColors;

double iconOpacity = 0.4;

// ui
int wheelSensitivity = 150;
bool wheelSpinning = false;

// caching
bool loadOnlyVisibleIcons;          // not used
int availableMemoryMB;
int winnowMemoryBeforeCacheMB;
int metaCacheMB;

// view
QString mode;                       // In MW: Loupe, Grid, Table or Compare
QString fileSelectionChangeSource;  // GridMouseClick, ThumbMouseClick, TableMouseClick
bool autoAdvance;

// icons
int maxIconSize = 256;
int minIconSize = 40;
int maxIconChunk = 25000;

// status
bool isLoadingFolder;
bool ignoreScrollSignal;
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
bool useSidecar;
bool renderVideoThumb;
bool isFilter;
bool isRemote;

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

int scrollBarThickness = 14;        // Also set in winnowstyle.css for vertical and horizontal
int propertyWidgetMarginLeft = 5;
int propertyWidgetMarginRight = 2;
QModelIndexList copyCutIdxList;
QStringList copyCutFileList;

QString tiffData;                   // temp for testing tiff decoder performance

// Initialize the global signal relay
SignalRelay *relay = nullptr;

QElapsedTimer t;
bool isTimer;
bool isTest;
bool isStressTest;

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
    return QVariant(x).toString();
}

QString sj(QString s, int x)
// helper function to justify a string with ....... for reporting
{
    s += " ";
    return s.leftJustified(x, '.') + " ";
}

int wait(int ms)
/*
    Reset duration by calling G::wait(0).
*/
{
    // return 0;

    static int duration = 0;
    if (ms == 0) {
        duration = 0;
        return 0;
    }
    QTime t = QTime::currentTime().addMSecs(ms);
    while (QTime::currentTime() < t) {
        // /*if (useProcessEvents)*/ qApp->processEvents(QEventLoop::AllEvents, 10);
    }
    duration += ms;
    return duration;
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
PopUp *popUp = nullptr;
void newPopUp(QWidget *widget, QWidget *centralWidget)
{
    popUp = new PopUp(widget, centralWidget);
}

//*** LOGGER ******************************************************************************

Logger logger;

void log(QString functionName, QString comment, bool zeroElapsedTime)
{
    logger.log(functionName, comment);
    return;
    /*
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    static QString prevFunctionName = "";
    static QString prevComment = "";
    QString stop = "";
    if (zeroElapsedTime) {
        t.restart();
    }
    if (functionName != "skipline") {
        QString microSec = QString("%L1").arg(t.nsecsElapsed() / 1000);
        QString d = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " ";
        QString e = microSec.rightJustified(11, ' ') + " ";
        QString f = prevFunctionName.leftJustified(50, ' ') + " ";
        QString c = prevComment;
        if (sendLogToConsole) {
            QString msg = stop + e + f + c;
            if (prevFunctionName == "skipline") qDebug().noquote() << " ";
            else qDebug().noquote() << msg;
        }
        else {
            QString msg = stop + d + e + f + c + "\n";
            if (logFile.isOpen()) logFile.write(msg.toUtf8());
        }
    }
    prevFunctionName = functionName;
    prevComment = comment;
    t.restart();
    //*/
}

//*** ISSUES ******************************************************************************

IssueLog *issueLog = nullptr;
void newIssueLog()
{
    issueLog = new IssueLog();
}

static QObject *modelInstance = nullptr;
void setDM(QObject *dm)
{
    modelInstance = dm;
}

QMutex issueListMutex;

void issue(QString type, QString msg, QString src, int sfRow,  QString fPath)
{
    if (!isIssueLogger) return;
    QMutexLocker locker(&issueListMutex);

    QSharedPointer<Issue> issue = QSharedPointer<Issue>::create();
    if (issue->TypeDesc.contains(type)) {
        if (type == "Comment") return;
        issue->type = static_cast<Issue::Type>(issue->TypeDesc.indexOf(type));
        issue->msg = msg;
    }
    else {
        issue->type = issue->Type::Undefined;
        issue->msg = type;
    }

    issue->src = src;
    issue->sfRow = sfRow;
    issue->fPath = fPath;
    issue->timeStamp =  QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ");

    if (showIssueInConsole) {
        qDebug().noquote() << issue->toString();
    }

    bool includeInDataModel = sfRow > -1;

    /*
    QString fun = "G::issue";
    qDebug().noquote()
        << fun.leftJustified(30)
        << issue->TypeDesc.at(issue->type).leftJustified(10)
        << QString::number(issue->sfRow).rightJustified(5)
        << issue->msg.leftJustified(40)
        << issue->src.leftJustified(30)
        ;  //*/

    if (modelInstance && includeInDataModel) {
        QMetaObject::invokeMethod(
            modelInstance,
            "issue",
            // Qt::BlockingQueuedConnection,
            Q_ARG(QSharedPointer<Issue>, issue)
        );
    }

    // update current session issue list
    issueList.append(issue->toString());

    // write to issue log
    issueLog->log(issue->toString());
    if (isIssueLogger) {
    }
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

