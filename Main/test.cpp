#include "Main/mainwindow.h"

void MW::stressTest(int ms)
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__ << ms;
    ms = QInputDialog::getInt(this, "Enter ms delay between images", "Delay (1-1000 ms) ",
                              50, 1, 1000);

//    G::wait(1000);        // time to release modifier keys for shortcut (otherwise select many)
    isStressTest = true;
    bool isForward = true;
    slideCount = 0;
    QElapsedTimer t;
    t.start();
    qDebug() << __FUNCTION__ << "-1";
    while (isStressTest) {
        G::wait(ms);
        ++slideCount;
        if (isForward && currentRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && currentRow == 0) isForward = true;
        if (isForward) keyRight();
        else keyLeft();
        qApp->processEvents();
    }
    qint64 msElapsed = t.elapsed();
    double seconds = msElapsed * 0.001;
    double msPerImage = msElapsed * 1.0 / slideCount;
    int imagePerSec = slideCount * 1.0 / seconds;
    QString msg = "Executed stress test " + QString::number(slideCount) + " times.<br>" +
                  QString::number(msElapsed) + " ms elapsed.<br>" +
                  QString::number(ms) + " ms delay.<br>" +
                  QString::number(imagePerSec) + " images per second.<br>" +
                  QString::number(msPerImage) + " ms per image."
//                  + "<br><br>Press <font color=\"red\"><b>Esc</b></font> to cancel this popup."
                  ;
    G::popUp->showPopup(msg, 0);
    qDebug() << __FUNCTION__ << "Executed stress test" << slideCount << "times.  "
             << msElapsed << "ms elapsed  "
             << ms << "ms delay  "
             << imagePerSec << "images per second  "
             << msPerImage << "ms per image."
                ;
    return;
    if (G::isLogger) G::log(__FUNCTION__);
    getSubfolders("/users/roryhill/pictures");
    QString fPath;
    int r = static_cast<int>(QRandomGenerator::global()->generate());
    fPath = subfolders->at(r % (subfolders->count()));
}

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{
    metaRead->stop();
    return;

    QString fPath = "D:/Pictures/favourites/2013-09-17_0033.jpg";   // pos = 889
    metadata->testNewFileFormat(fPath);
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{

//    return;

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
