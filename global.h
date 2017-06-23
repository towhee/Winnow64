
#ifndef GLOBAL_H
#define GLOBAL_H

#include <QModelIndexList>
#include <QStringList>
#include <QtGlobal>
#include <QElapsedTimer>


namespace G
{
    // not persistent
    extern bool isThreadTrackingOn;
    extern qreal devicePixelRatio;
    extern QModelIndexList copyCutIdxList;  // req'd?
    extern QStringList copyCutFileList;     // req'd?
    extern QElapsedTimer t;
    extern QString appName;                 // may be req'd for mult workspaces + be persistent
    extern bool isTimer;

}


//#define ISDEBUG        // Uncomment this line to show debugging output

#endif // GLOBAL_H

