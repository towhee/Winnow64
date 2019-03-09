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

    qreal actualDevicePixelRatio;

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
//        std::cout << std::setw(7) << std::setfill(' ') << std::right << G::t.restart()
//                  << "   "
//                  << std::setw(50) << std::setfill(' ') << std::left << functionName.toStdString()
//                  << "   "
//                  << comment.toStdString()
//                  << "\n" << std::flush;

        const QByteArray b = " ";
//        const char *str = b.data();

        QString time = QString::number(G::t.nsecsElapsed());
        G::t.restart();

        QString initializing = isInitializing ? "initalizing = true" : "initializing = false";

        qDebug() << time.rightJustified(10, ' ') << " "
                 << functionName.leftJustified(50, '.') << " "
                 << initializing.leftJustified(25)
                 << comment;

//        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
//        Profile::print(functionName, comment);
    }

//    void Profile::print(QString functionName, QString comment)
//    {
//        m.lock();
//        std::cout << std::setw(7) << std::setfill(' ') << std::right << G::t.restart()
//                  << "   "
//                  << std::setw(50) << std::setfill(' ') << std::left << functionName.toStdString()
//                  << "   "
//                  << comment.toStdString()
//                  << "\n" << std::flush;
//        m.unlock();
//    }
}


