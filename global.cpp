#include "global.h"

namespace G
{
    QColor labelNoneColor(65,65,65,255);
    QColor labelRedColor(Qt::red);
    QColor labelYellowColor(Qt::darkYellow);
    QColor labelGreenColor(Qt::darkGreen);
    QColor labelBlueColor(Qt::darkBlue);
    QColor labelPurpleColor(Qt::darkMagenta);

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

