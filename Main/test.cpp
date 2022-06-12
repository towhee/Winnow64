#include "Main/mainwindow.h"

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{
    metaRead->stop();
    return;

    QString fPath = "D:/Pictures/favourites/2013-09-17_0033.jpg";   // pos = 889
    metadata->testNewFileFormat(fPath);
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
    MetaRead2 *metaread2 = new MetaRead2(dm);
    return;

    int dmRow = 0;
    bool isVideo = dm->index(dmRow, G::VideoColumn).data().toBool();
    qDebug() << "MW::updateCachedStatus" << dmRow << isVideo;;

//    FrameDecoder *frameDecoder = new FrameDecoder(dm);
//    connect(frameDecoder, &FrameDecoder::test, this, &MW::testFrameDecoder);
//    qDebug() << __FUNCTION__ << frameDecoder << frameDecoder->testString;
//    frameDecoder->getFrame("test");
//    return;


    fsTree->selectionModel()->clearSelection();
    bookmarks->selectionModel()->clearSelection();
    return;

#ifdef Q_OS_WIN
    QString p1 = "D:/Pictures/Coaster";
    QString p2 = "D:/Pictures/Zenfolio/pbase2048";
#endif

#ifdef Q_OS_MAC
QString p1 = "/Users/roryhill/Pictures/3840x2160";
QString p2 = "/Users/roryhill/Pictures/Test";
#endif

    QString fPath;
    static bool b = true;
    if (b) fPath = p1;
    else fPath = p2;
    QModelIndex idx = fsTree->fsModel->index(fPath);
    QModelIndex filterIdx = fsTree->fsFilter->mapFromSource(idx);
    fsTree->setCurrentIndex(filterIdx);
    fsTree->scrollTo(filterIdx, QAbstractItemView::PositionAtCenter);

    qDebug();
    G::t.restart();
    G::track(__FUNCTION__, "New folder: " + fPath);
    folderSelectionChange();
    b = !b;
}
