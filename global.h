
#ifndef GLOBAL_H
#define GLOBAL_H

#include <QSettings>
#include <QModelIndexList>
#include <QStringList>
#include <QColor>
#include <QAction>
#include <QSet>
#include <QtGlobal>

#include <QElapsedTimer>


namespace G
{
    extern QSettings *setting;  // not req'd - move usage in thumbview to MW
    extern QMap<QString, QAction *> actionKeys;
    extern QMap<QString, QString> externalApps;
    extern QSet<QString> bookmarkPaths;

    // preferences: files
    extern bool includeSubFolders;
    extern bool showHiddenFiles;
    extern bool rememberLastDir;

    // preferences: thumbs
//    extern unsigned int thumbSpacing;
//    extern int thumbPadding;
//    extern int thumbWidth;
//    extern int thumbHeight;
//    extern int labelFontSize;
//    extern bool showThumbLabels;

    // preferences: slideshow
    extern int slideShowDelay;
    extern bool slideShowRandom;

    // preferences: cache
    extern int cacheSizeMB;
    extern bool showCacheStatus;
    extern int cacheStatusWidth;
    extern int cacheWtAhead;

    // persistant: files
    extern QString lastDir;

    extern bool shootingInfoVisible;
    extern bool isIconView;

    // make persistent

    // not persistent
    extern bool isThreadTrackingOn;
    extern bool slideShowActive;
    extern qreal devicePixelRatio;
    extern QModelIndexList copyCutIdxList;  // req'd?
    extern QStringList copyCutFileList;     // req'd?
    extern bool copyOp;                     // req'd?
    extern QString currentViewDir;          // make an option to reopen here?
    extern QElapsedTimer t;
    extern QString appName;                 // may be req'd for mult workspaces + be persistent
    extern bool isTimer;

}


#define ISDEBUG        // Uncomment this line to show debugging output

#endif // GLOBAL_H

