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

    QString mode;                       // In MW: Loupe, Grid, Table or Compare
    QString source;                     // GridMouseClick, ThumbMouseClick, TableMouseClick

    int maxIconSize;

    QString fontSize;

    int actualDevicePixelRatio;
    bool allMetadataLoaded;

    int cores;
    bool aSync;

    // not persistent
    bool isThreadTrackingOn;
    bool isNewFolderLoaded;
    bool isInitializing;
    int scrollBarThickness = 14;        // Also set in winnowstyle.css for vertical and horizontal
    QModelIndexList copyCutIdxList;
    QStringList copyCutFileList;
    QElapsedTimer t;
    bool isTimer;

    void track(QString functionName, QString comment)
    {
//        QString time = QString::number(G::t.nsecsElapsed());
        QString time = QString("%L1").arg(t.nsecsElapsed());
        t.restart();

//        QString initializing = isInitializing ? "initalizing = true" : "initializing = false";

        qDebug() << time.rightJustified(15, ' ') << " "
                 << functionName.leftJustified(50, '.') << " "
//                 << initializing.leftJustified(25)
                 << comment;
    }

}


