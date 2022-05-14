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
    QApplication::beep();
    qDebug() << __FUNCTION__ << "filterDock->visibleRegion().isNull() ="
             << filterDock->visibleRegion().isNull();
}
