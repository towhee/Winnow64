#include "Main/global.h"

namespace G
{
int transparency = 255;
QColor labelNoneColor(85,85,85,transparency);                // Background Gray
QColor labelRedColor(QColor(128,0,0,transparency));          // Dark red
QColor labelYellowColor(QColor(128,128,0,transparency));     // Dark yellow
QColor labelGreenColor(QColor(0,128,0,transparency));        // Dark green
QColor labelBlueColor(QColor(0,0,128,transparency));         // Dark blue
QColor labelPurpleColor(QColor(128,0,128,transparency));     // Dark magenta

//    QColor labelNoneColor(85,85,85,128);                // Background Gray
//    QColor labelRedColor(QColor(128,0,0,128));          // Dark red
//    QColor labelYellowColor(QColor(128,128,0,128));     // Dark yellow
//    QColor labelGreenColor(QColor(0,128,0,128));        // Dark green
//    QColor labelBlueColor(QColor(0,0,128,128));         // Dark blue
//    QColor labelPurpleColor(QColor(128,0,128,128));     // Dark magenta

    QStringList ratings, labelColors;

    QString mode;                       // In MW: Loupe, Grid, Table or Compare
    QString lastThumbChangeEvent;

    qreal actualDevicePixelRatio;

    // not persistent
    bool isThreadTrackingOn;
    bool isNewFolderLoaded;
    int scrollBarThickness = 12;        // Also set in winnowstyle.css
    QModelIndexList copyCutIdxList;
    QStringList copyCutFileList;
    QElapsedTimer t;
    bool isTimer;
}

