#include "Main/global.h"

namespace G
{
    int transparency = 255;
    QColor labelNoneColor(85,85,85,transparency);                // Background Gray
    QColor labelRedColor(QColor(128,0,0,transparency));          // Dark red
    QColor labelYellowColor(QColor(255,255,0,transparency));     // Dark yellow
    QColor labelGreenColor(QColor(0,128,0,transparency));        // Dark green
    QColor labelBlueColor(QColor(0,0,200,transparency));         // Dark blue
    QColor labelPurpleColor(QColor(128,0,128,transparency));     // Dark magenta

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

    void track(QString functionName, QString comment)
    {
//        if (functionName == "MW::folderSelectionChange") qDebug() << "\n";
//        qDebug() << G::t.restart() << "\t" << functionName << "\t" << comment;

        if (functionName == "MW::folderSelectionChange") std::cout << "\n";
        std::cout << std::setw(7) << std::setfill(' ') << std::right << G::t.restart()
                  << "   "
                  << std::setw(50) << std::setfill(' ') << std::left << functionName.toStdString()
                  << "   "
                  << comment.toStdString()
                  << "\n" << std::flush;
    }
}

