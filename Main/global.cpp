#include "Main/global.h"

namespace G
{
    QSettings *settings;//

    // system messaging
    bool isLogger = false;              // Writes log messages to file or console
    bool isFlowLogger = false;          // Writes key program flow points to file or console
    bool isFlowLogger2 = false;         // QDebug key program flow points
    bool isWarningLogger = false;       // Writes warnings to qDebug
    bool isFileLogger = false;          // Writes log messages to file (debug executable ie remote embellish ops)
    bool isErrorLogger = true;         // Writes error log messages to file or console
    bool isTestLogger = false;          // Writes test points to file or console
    bool sendLogToConsole = true;       // true: console, false: WinnowLog.txt
    QFile logFile;                      // MW::openLog(), MW::closeLog()
    QFile errlogFile;                   // MW::openErrLog(), MW::closeErrLog()
    bool isDev;                         // Running from within Winnow Project/Winnow64

    // Errors
    QMap<QString,QStringList> err;

    // flow
    bool isInitializing;                // flag program starting / initializing
    bool stop = false;                  // flag to stop everything involving DM loading
                                        // new dataset
    bool dmEmpty;                       // DM is stopped and/or empty.  Flag to abort
                                        // all new folder processes.
    bool isLinearLoadDone;
    bool allMetadataLoaded;
    bool allIconsLoaded;

    int dmInstance;
    int metadataInstance;
    int imageCacheInstance;

    // limit functionality for testing
    bool useReadMetadata = true;
    bool useReadIcons = true;
    bool useImageCache = true;
    bool useImageView = true;
    bool useInfoView = true;
    bool useMultimedia = true;
    bool useUpdateStatus = true;
    bool useFilterView = true;          // not finished

    // metadata/icon read method
    QString metaReadInUse;              // used in tooltip

    // system display
    QHash<QString, WinScreen> winScreenHash;    // record icc profiles for each monitoriconLoaded
    QString winOutProfilePath;
    int displayPhysicalHorizontalPixels;// current monitor
    int displayPhysicalVerticalPixels;  // current monitor
    int displayVirtualHorizontalPixels; // current monitor
    int displayVirtualVerticalPixels;   // current monitor
    qreal actDevicePixelRatio;          // current monitor
    qreal sysDevicePixelRatio;          // current monitor

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
    QColor appleBlue = QColor(21,113,211);
    QColor selectionColor = QColor(68,95,118);
    QColor mouseOverColor = QColor(40,54,66);

    QString css;                        // app stylesheet;

    static int transparency = 150;
    QColor labelNoneColor(85,85,85,transparency);                // Background Gray
    QColor labelRedColor(QColor(128,0,0,transparency));          // Dark red
    QColor labelYellowColor(QColor(255,255,0,transparency));     // Dark yellow
    QColor labelGreenColor(QColor(0,128,0,transparency));        // Dark green
    QColor labelBlueColor(QColor(0,0,200,transparency));         // Dark blue
    QColor labelPurpleColor(QColor(128,0,128,transparency));     // Dark magenta

    QStringList ratings, labelColors;

    double iconOpacity = 0.4;

    // ui
    int wheelSensitivity = 40;

    // caching
    bool loadOnlyVisibleIcons;          // not used
    int availableMemoryMB;
    int winnowMemoryBeforeCacheMB;
    int metaCacheMB;

    // view
    QString mode;                       // In MW: Loupe, Grid, Table or Compare
    QString fileSelectionChangeSource;  // GridMouseClick, ThumbMouseClick, TableMouseClick

    // icons
    int maxIconSize = 256;
    int minIconSize = 40;
    int iconWMax;                       // widest icon found in datamodel
    int iconHMax;                       // highest icon found in datamodel

    // status
    QString currRootFolder;
    bool ignoreScrollSignal;
    bool isSlideShow;
    bool isRunningColorAnalysis;
    bool isRunningStackOperation;
    bool isProcessingExportedImages;
    bool isEmbellish;
    bool colorManage;
    bool modifySourceFiles;
    bool backupBeforeModifying;
    bool autoAddMissingThumbnails;
    bool useSidecar;
//    bool embedTifJpgThumb;
    bool isLoadLinear;
    bool isLoadConcurrent;
    bool renderVideoThumb;
    bool includeSubfolders;

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

    QElapsedTimer t;
    bool isTimer;
    bool isTest;

//    bool empty()
//    {
//        return currRootFolder.isEmpty();
//    }

    QString s(QVariant x)
    // helper function to convert variable values to a string for reporting
    {
//        qDebug() << "Global::s" << x.typeName() << x.typeId();
        if (x.typeId() == QMetaType::QStringList)
            return Utilities::stringListToString(x.toStringList());
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
        static int duration = 0;
        if (ms == 0) {
            duration = 0;
            return 0;
        }
        QTime t = QTime::currentTime().addMSecs(ms);
        while (QTime::currentTime() < t) {
            qApp->processEvents(QEventLoop::AllEvents/*, 10*/);
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

    QStringList doNotLog =
    {
        "ImageCache",
        "ImageDecoder",
        "MetadataCache"
    };

    void log(QString functionName, QString comment, bool zeroElapsedTime)
    {
        /*
        for (int i = 0; i < doNotLog.length(); ++i) {
             if (functionName.contains(doNotLog.at(i))) return;
        }
        //*/
        QMutex mutex;
        mutex.lock();
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
//            if (G::stop) stop = "STOP ";
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
//        else {
//            qDebug().noquote() << "";
//        }
        prevFunctionName = functionName;
        prevComment = comment;
        t.restart();
        mutex.unlock();
    }

    void errlog(QString err, QString functionName, QString fPath)
    {
        if (!isErrorLogger) return;
        if (G::errlogFile.open(QIODevice::WriteOnly  | QIODevice::Append)) {
            QString d = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ");
            QString f = functionName.leftJustified(40, '.') + " ";
            QString p = fPath;
            QString e = err.leftJustified(75, '.') + " ";
            QString msg = d + e + f + p + "\n";
//            if (errlogFile.isOpen()) {
//                quint32 eof = errlogFile.size();
//                errlogFile.seek(eof);
//                errlogFile.write(msg.toUtf8());
//                errlogFile.flush();
//            }
            QTextStream stream(&G::errlogFile);
            stream << msg;
            G::errlogFile.close();
        }
    }

    void error(QString err, QString functionName, QString fPath)
    {
        QString errMsg = functionName + " Error: " + err;
        if (fPath.length()) G::err[fPath].append(errMsg);

        // add to errorLog ...
        errlog(err, functionName, fPath);
    }

    bool instanceClash(int instance, QString src)
    {
        bool clash = (dmInstance != instance);
        if (clash) {
            if (isWarningLogger)
            qWarning() << "WARNING G::instanceClash"
                       << "instance =" << instance
                       << "DM instance =" << dmInstance
                       << "src =" << src
                          ;
        }
        return clash;
    }

    int popUpProgressCount = 0;
    int popUpLoadFolderStep = 100;
    PopUp *popUp;
    void newPopUp(QWidget *widget, QWidget *centralWidget)
    {
        popUp = new PopUp(widget, centralWidget);
    }
}
