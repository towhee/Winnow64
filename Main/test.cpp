#include "Main/mainwindow.h"

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{
    QApplication::beep();
    filters->msgFrame->setVisible(true);
    filters->filterLabel->setVisible(true);
    filters->bfProgressBar->setVisible(false);
    return;

    QString fPath = "D:/Pictures/favourites/2013-09-17_0033.jpg";   // pos = 889
    metadata->testNewFileFormat(fPath);
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
    static bool b = true;
//    QString p1 = "/Users/roryhill/Pictures/2022/202204/2022-04-11_Nora";
//    QString p2 = "/Users/roryhill/Pictures/2022/202204/2022-04-12_Nora";
    QString p1 = "D:/Pictures/Coaster";
    QString p2 = "D:/Pictures/Zenfolio/pbase2048";
    QString fPath;
    if (b) fPath = p1;
    else fPath = p2;
    QModelIndex idx = fsTree->fsModel->index(fPath);
    QModelIndex filterIdx = fsTree->fsFilter->mapFromSource(idx);
    fsTree->setCurrentIndex(filterIdx);
    fsTree->scrollTo(filterIdx, QAbstractItemView::PositionAtCenter);
    folderSelectionChange();
    b = !b;
}