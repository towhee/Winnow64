#include "Main/global.h"

namespace G
{
    // system messaging
    bool isLogger = false;              // Writes log messages to file or console
    bool isFlowLogger = false;          // Writes key program flow points to file or console
    bool isTestLogger = false;          // Writes test points to file or console
    bool sendLogToConsole = true;       // true: console, false: WinnowLog.txt
    QFile logFile;                      // MW::openLog(), MW::closeLog()
    QFile errlogFile;                   // MW::openErrLog(), MW::closeErrLog()
    bool isDev;                         // Running from within Winnow Project/Winnow64

    // Errors
    QMap<QString,QStringList> err;

    // system display
    QHash<QString, WinScreen> winScreenHash;    // record icc profiles for each monitor
    QString winOutProfilePath;
    int displayPhysicalHorizontalPixels;// current monitor
    int displayPhysicalVerticalPixels;  // current monitor
    int displayVirtualHorizontalPixels; // current monitor
    int displayVirtualVerticalPixels;   // current monitor
    qreal actDevicePixelRatio;          // current monitor
    qreal sysDevicePixelRatio;          // current monitor

    // application parameters
    QPoint mousePos;                    // local mouse position used to locate mouse press custom button in qtreeview
    QString fontSize;                   // app font point size
    qreal dpi;                          // current logical screen dots per inch
    qreal ptToPx;                       // font points to pixels conversion factor
    int textShade = 190;                // text default luminousity
    int backgroundShade;                // app background luminousity

    QColor scrollBarHandleBackgroundColor = QColor(90,130,100);
    QColor selectionColor = QColor(68,95,118);
    QColor mouseOverColor= QColor(40,54,66);

    QString css;                        // app stylesheet;

    int transparency = 150;
    QColor labelNoneColor(85,85,85,transparency);                // Background Gray
    QColor labelRedColor(QColor(128,0,0,transparency));          // Dark red
    QColor labelYellowColor(QColor(255,255,0,transparency));     // Dark yellow
    QColor labelGreenColor(QColor(0,128,0,transparency));        // Dark green
    QColor labelBlueColor(QColor(0,0,200,transparency));         // Dark blue
    QColor labelPurpleColor(QColor(128,0,128,transparency));     // Dark magenta

    QStringList ratings, labelColors;

    double iconOpacity = 0.4;

    // caching
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
    bool allMetadataLoaded;
    bool ignoreScrollSignal;
    bool isSlideShow;
    bool isProcessingExportedImages;
    bool isRunningColorAnalysis;
    bool isRunningStackOperation;
    bool isEmbellish;
    bool colorManage;
    bool useSidecar;
    bool embedTifThumb;

    // ingest
    int ingestCount = 0;
    QDate ingestLastDate;

    // not persistent
    bool isThreadTrackingOn;
    bool showAllTableColumns;
    bool isNewFolderLoaded;
    bool isNewFolderLoadedAndInfoViewUpToDate;
    bool isInitializing;
    bool isNewSelection;
    int scrollBarThickness = 14;        // Also set in winnowstyle.css for vertical and horizontal
    int propertyWidgetMarginLeft = 5;
    int propertyWidgetMarginRight = 2;
    QModelIndexList copyCutIdxList;
    QStringList copyCutFileList;

    QString tiffData;                   // temp for testing tiff decoder performance

    QElapsedTimer t;
    QElapsedTimer t1;
    bool isTimer;
    bool isTest;

    QString s(QVariant x)
    // helper function to convert variable values to a string for reporting
    {
        return QVariant(x).toString();
    }

    QString sj(QString s, int x)
    // helper function to justify a string with ....... for reporting
    {
        s += " ";
        return s.leftJustified(x, '.') + " ";
    }

    void wait(int ms)
    {
        QTime t = QTime::currentTime().addMSecs(ms);
        while (QTime::currentTime() < t) qApp->processEvents(QEventLoop::AllEvents, 10);
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
//        for (int i = 0; i < doNotLog.length(); ++i) {
//             if (functionName.contains(doNotLog.at(i))) return;
//        }
        static QString prevFunctionName = "";
        static QString prevComment = "";
        if (zeroElapsedTime) {
            t.restart();
        }
        if (functionName != "") {
            QString microSec = QString("%L1").arg(t.nsecsElapsed() / 1000);
            QString d = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " ";
            QString e = microSec.rightJustified(11, ' ') + " ";
            QString f = prevFunctionName.leftJustified(50, ' ') + " ";
            QString c = prevComment;
            if (sendLogToConsole) {
                QString msg = e + f + c;
                qDebug().noquote() << msg;
            }
            else {
                QString msg = d + e + f + c + "\n";
                if (logFile.isOpen()) logFile.write(msg.toUtf8());
            }
        }
        prevFunctionName = functionName;
        prevComment = comment;
        t.restart();
    }

    void errlog(QString functionName, QString fPath, QString err)
    {
        return;
        QString d = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " ";
        QString f = functionName.leftJustified(40, '.') + " ";
        QString p = fPath;
        QString e = err.leftJustified(75, '.') + " ";
        QString msg = d + e + f + p + "\n";
//        qDebug() << __FUNCTION__ << msg;
        return;
        if (errlogFile.isOpen()) {
            errlogFile.write(msg.toUtf8());
            errlogFile.flush();
        }
    }

    void error(QString functionName, QString fPath, QString err)
    {
        QString errMsg = functionName + " Error: " + err;
        G::err[fPath].append(errMsg);
        // add to errorLog ...
        errlog(functionName, fPath, err);
    }

    int popUpProgressCount = 0;
    int popUpLoadFolderStep = 100;
    PopUp *popUp;
    void newPopUp(QWidget *widget, QWidget *centralWidget)
    {
        popUp = new PopUp(widget, centralWidget);
    }
}
