#include "global.h"

namespace G
{
    QColor labelNoneColor(65,65,65,128);
    QColor labelRedColor(QColor(128,0,0,128));
    QColor labelYellowColor(QColor(128,128,0,128));
    QColor labelGreenColor(QColor(0,128,0,128));
    QColor labelBlueColor(QColor(0,0,128,128));
    QColor labelPurpleColor(QColor(128,0,128,128));
//    QColor labelRedColor(Qt::red);
//    QColor labelYellowColor(Qt::darkYellow);
//    QColor labelGreenColor(Qt::darkGreen);
//    QColor labelBlueColor(Qt::darkBlue);
//    QColor labelPurpleColor(Qt::darkMagenta);

    // not persistent
    bool isThreadTrackingOn;
    bool isNewFolderLoaded;
    qreal devicePixelRatio;
    QModelIndexList copyCutIdxList;
    QStringList copyCutFileList;
    QElapsedTimer t;
    QString appName;
    bool isTimer;
}

