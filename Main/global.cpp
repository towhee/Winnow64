#include "Main/global.h"

namespace G
{
    // system messaging
    bool isLogger = false;               // Writes log messages to file or console
    bool sendLogToConsole = true;       // true: console, false: WinnowLog.txt
    QFile logFile;                      // MW::openLog(), MW::closeLog()
    bool isDev;                         // Running from within Winnow Project/Winnow64

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
    QString css;                        // app stylesheet;

    int transparency = 150;
    QColor labelNoneColor(85,85,85,transparency);                // Background Gray
    QColor labelRedColor(QColor(128,0,0,transparency));          // Dark red
    QColor labelYellowColor(QColor(255,255,0,transparency));     // Dark yellow
    QColor labelGreenColor(QColor(0,128,0,transparency));        // Dark green
    QColor labelBlueColor(QColor(0,0,200,transparency));         // Dark blue
    QColor labelPurpleColor(QColor(128,0,128,transparency));     // Dark magenta

    QStringList ratings, labelColors;

    QColor progressCurrentColor = QColor(158,200,158);           // Light green
    QColor progressBgColor = QColor(150,150,150);                // Light gray
    QColor progressAppBgColor = QColor(85,85,85);                // Darker gray
    QColor progressAddFileInfoColor = QColor(85,100,115);        // Slate blue
    QColor progressAddMetadataColor = QColor(100,100,150);       // Purple
    QColor progressBuildFiltersColor = QColor(75,75,125);        // Darker purple
    QColor progressTargetColor = QColor(125,125,125);            // Gray
    QColor progressImageCacheColor = QColor(108,150,108);        // Green

    double iconOpacity = 0.4;

    // caching
    int availableMemoryMB;
    int winnowMemoryBeforeCacheMB;
    int metaCacheMB;
    bool showCacheStatus;

    // view
    QString mode;                       // In MW: Loupe, Grid, Table or Compare
    QString fileSelectionChangeSource;  // GridMouseClick, ThumbMouseClick, TableMouseClick

    // icons
    int maxIconSize;
    int minIconSize = 40;
    int iconWMax;                       // widest icon found in datamodel
    int iconHMax;                       // highest icon found in datamodel

    // status
    bool allMetadataLoaded;
    bool ignoreScrollSignal;
    bool isSlideShow;
    bool isProcessingExportedImages;
    bool isRunningColorAnalysis;
    bool isEmbellish;
    bool colorManage;

    // ingest
    int ingestCount = 0;
    QDate ingestLastDate;

//    bool colorManage;

    // not persistent
    bool isThreadTrackingOn;
    bool showAllTableColumns;
    bool isNewFolderLoaded;
    bool isInitializing;
    int scrollBarThickness = 14;        // Also set in winnowstyle.css for vertical and horizontal
    int propertyWidgetMarginLeft = 5;
    int propertyWidgetMarginRight = 2;
    QModelIndexList copyCutIdxList;
    QStringList copyCutFileList;

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
        QString time = QString("%L1").arg(t.nsecsElapsed());
        if (hideTime) time = "";
        t.restart();
        qDebug().noquote()
                 << time.rightJustified(15, ' ') << " "
                 << functionName.leftJustified(50, '.') << " "
                 << comment;
    }

    void log(QString functionName, QString comment, bool hideElapsedTime)
    {
        QString time = QString("%L1").arg(t.nsecsElapsed() / 1000);
        if (hideElapsedTime) time = "";
        QString d = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " ";
        QString e = time.rightJustified(11, ' ') + " ";
        QString f = functionName.leftJustified(50, ' ') + " ";
        QString c = comment;
        if (sendLogToConsole && isDev) {
            QString msg = e + f + c;
            qDebug().noquote() << msg;
        }
        else {
            QString msg = d + e + f + c + "\n";
            logFile.write(msg.toUtf8());
        }
        t.restart();
    }

    PopUp *popUp;
    void newPopUp(QWidget *widget)
    {
        popUp = new PopUp(widget);
    }
}
