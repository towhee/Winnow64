#include "Main/mainwindow.h"

void MW::traverseFolderStressTestFromMenu()
{
    qDebug() << CLASSFUNCTION;
    traverseFolderStressTest();
}

void MW::traverseFolderStressTest(int ms, int duration)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION << ms;
    G::popUp->end();

    if (!ms) {
        ms = QInputDialog::getInt(this,
                                  "Enter ms delay between images",
                                  "Delay (1-1000 ms) ",
                                  50, 1, 1000);
    }

//    G::wait(1000);        // time to release modifier keys for shortcut (otherwise select many)
    isStressTest = true;
    bool isForward = true;
    slideCount = 0;
    QElapsedTimer t;
    t.start();
    while (isStressTest) {
        if (duration && t.elapsed() > duration) return;
        G::wait(ms);
        ++slideCount;
        if (isForward && dm->currentSfRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && dm->currentSfRow == 0) isForward = true;
        if (isForward) keyRight();
        else keyLeft();
//        qApp->processEvents();
        if (dm->sf->rowCount() < 2) return;
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
    qDebug() << CLASSFUNCTION << "Executed stress test" << slideCount << "times.  "
             << msElapsed << "ms elapsed  "
             << ms << "ms delay  "
             << imagePerSec << "images per second  "
             << msPerImage << "ms per image."
                ;
    return;
    if (G::isLogger) G::log(CLASSFUNCTION);
    getSubfolders("/users/roryhill/pictures");
    QString fPath;
    int r = static_cast<int>(QRandomGenerator::global()->generate());
    fPath = subfolders->at(r % (subfolders->count()));
}

void MW::bounceFoldersStressTestFromMenu()
{
    qDebug() << CLASSFUNCTION;
    bounceFoldersStressTest(0, 0);
}

void MW::bounceFoldersStressTest(int ms, int duration)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION << "ms =" << ms << "duration =" << duration;
    G::popUp->end();


    if (!duration) {
        duration = QInputDialog::getInt(this,
              "Enter time per folder tested",
              "Duration (0 - 1000 sec)",
              5, 0, 86400);
        duration *= 1000;
    }

    if (!ms) {
        ms = QInputDialog::getInt(this,
              "Enter ms delay between images",
              "Delay (1-1000 ms) ",
              50, 1, 1000);
    }

    isStressTest = true;
    QList<QString>bookMarkPaths = bookmarks->bookmarkPaths.values();
    int n = bookMarkPaths.count();
    while (isStressTest) {
        uint randomIdx = QRandomGenerator::global()->generate() % static_cast<uint>(n);
        QString path = bookMarkPaths.at(randomIdx);
        qDebug() << CLASSFUNCTION << randomIdx << path;
        bookmarks->select(path);
        fsTree->select(path);
        folderSelectionChange();
        traverseFolderStressTest(ms, duration);
    }
}

void MW::testMultiThreadDataModel()
{
    // must select folder before running test.
    metaReadThread->testMultiThread = true;
    buildFilters->testMultiThread = true;
    metaReadThread->start();
    buildFilters->start();
}

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{
    centralLayout->setCurrentIndex(GridTab);
//    gridView->setVisible(!gridView->isVisible());
    return;

    qDebug() << CLASSFUNCTION;
    bounceFoldersStressTest(50, 10000);
    return;
    QString fPath = "D:/Pictures/favourites/2013-09-17_0033.jpg";   // pos = 889
    metadata->testNewFileFormat(fPath);
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
//    int a = 2;
//    int b = 4;
//    int c = a | b;
//    qDebug() << "c =" << QString::number(c, 2);
//    return;

    QString f0 = "/Users/roryhill/Pictures/_test0/IMG_5853.heic";
    QString f1 = "/Users/roryhill/Pictures/_test0/A1_02764.ARW";
    QFileDevice::Permissions p0 = QFile::permissions(f0);
    QFileDevice::Permissions p1 = QFile::permissions(f1);
    qDebug() << "p0 =" << p0;
    qDebug() << "p1 =" << p1;
//    bool success = QFile::setPermissions(f1, p0);
//    p1 = QFile::permissions(f1);
//    qDebug() << "p1 =" << p1 << success << QFile(f1).isOpen();
    return;

    bool isReadWrite;
    bool isRead;
    bool isWrite;
    QString fPath = "/Users/roryhill/Pictures/_test0/A1_02764.ARW";
    QFileDevice::Permissions pRW = QFileDevice::ReadUser | QFileDevice::WriteUser;
    QFileDevice::Permissions pOld = QFile::permissions(fPath);
//    int r = p & QFileDevice::ReadUser;
//    int w = p & QFileDevice::WriteUser;
    QFileDevice::Permissions pNew = pOld | pRW;
    QFile(fPath).setPermissions(pNew);
    qDebug() << fPath << pOld << pNew;

    isRead = pNew & QFileDevice::ReadUser;
    isWrite = pNew & QFileDevice::WriteUser;
    isReadWrite = isRead & isWrite;
//    qDebug() << fPath << QFile::permissions(fPath) << p << isRead << isWrite << isReadWrite << rw;
    return;
}

