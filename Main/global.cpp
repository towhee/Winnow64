#include "Main/global.h"

namespace G
{
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

    int availableMemoryMB;
    int winnowMemoryBeforeCacheMB;
    int metaCacheMB;
    bool memTest;

    QHash<QString, WinScreen> winScreenHash;    // record icc profiles for each monitor
    QString winOutProfilePath;

    QString mode;                       // In MW: Loupe, Grid, Table or Compare
    QString fileSelectionChangeSource;  // GridMouseClick, ThumbMouseClick, TableMouseClick

    int maxIconSize;
    int minIconSize = 40;
    int iconWMax;                       // widest icon found in datamodel
    int iconHMax;                       // highest icon found in datamodel

    QString fontSize;                   // app font point size
    qreal dpi;                          // current logical screen dots per inch
    qreal ptToPx;                       // font points to pixels conversion factor
    int textShade = 190;                // text default luminousity
    int disabledShade = textShade - 64; // disabled text default luminousity
    int backgroundShade;                // app background luminousity
    QString css;                        // app stylesheet;

    int actualDevicePixelRatio;
    bool allMetadataLoaded;
    bool ignoreScrollSignal;
    bool isSlideShow;

    bool isEmbel;

    bool colorManage;

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

    void track(QString functionName, QString comment)
    {
        QString time = QString("%L1").arg(t.nsecsElapsed());
        t.restart();
        qDebug() << time.rightJustified(15, ' ') << " "
                 << functionName.leftJustified(50, '.') << " "
                 << comment;
    }

    PopUp *popUp;
    void newPopUp(QWidget *widget)
    {
        popUp = new PopUp(widget);
    }
}
