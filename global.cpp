

#include "global.h"

namespace G
{
    QSettings *setting;
    QMap<QString, QAction *> actionKeys;
    QMap<QString, QString> externalApps;
    QSet<QString> bookmarkPaths;

    // preferences: files
    bool includeSubFolders;
    bool showHiddenFiles;
    bool rememberLastDir;

    // preferences: thumbs
//    unsigned int thumbSpacing;
//    int thumbPadding;
//    int thumbWidth = 160;
//    int thumbHeight = 160;
//    int labelFontSize;
//    bool showThumbLabels;

    // preferences: slideshow
    int slideShowDelay;
    bool slideShowRandom;

    // preferences: cache
    int cacheSizeMB;
    bool showCacheStatus;
    int cacheStatusWidth;
    int cacheWtAhead;

    // persistant: files
    QString lastDir;

    bool shootingInfoVisible;
    bool isIconView;

    // not persistent
    bool isThreadTrackingOn;
    bool slideShowActive;
    qreal devicePixelRatio;
    QModelIndexList copyCutIdxList;
    QStringList copyCutFileList;
    bool copyOp;
    QString currentViewDir;
    QElapsedTimer t;
    QString appName;
    bool isTimer;

}

