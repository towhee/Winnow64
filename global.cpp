#include "global.h"

namespace G
{
    QColor labelNoneColor(65,65,65,128);                // Gray
    QColor labelRedColor(QColor(128,0,0,128));          // Dark red
    QColor labelYellowColor(QColor(128,128,0,128));     // Dark yellow
    QColor labelGreenColor(QColor(0,128,0,128));        // Dark green
    QColor labelBlueColor(QColor(0,0,128,128));         // Dark blue
    QColor labelPurpleColor(QColor(128,0,128,128));     // Dark magenta

    QString mode;                       // In MW: Loupe, Grid, Table or Compare
    QString lastThumbChangeEvent;

    qreal refScaleAdjustment;

    // not persistent
    bool isThreadTrackingOn;
    bool isNewFolderLoaded;
    int scrollBarThickness = 10;        // Also set in winnowstyle.css
    qreal devicePixelRatio;
    QModelIndexList copyCutIdxList;
    QStringList copyCutFileList;
    QElapsedTimer t;
    QString appName;
    bool isTimer;
}

