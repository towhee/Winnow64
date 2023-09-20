#include "Main/mainwindow.h"

void MW::traverseFolderStressTestFromMenu()
{
    qDebug() << "MW::traverseFolderStressTestFromMenu";
    traverseFolderStressTest();
}

void MW::traverseFolderStressTest(int msPerImage, int secPerFolder, bool uturn)
{
/*
    msPerImage      time per image
    secPerFolder    ms test (0 == forever) ESC to stop
    uturn           randomly change direction
*/
    if (G::isLogger) G::log("MW::traverseFolderStressTest");
    qDebug() << "MW::traverseFolderStressTest" << msPerImage;
    G::popUp->end();

    if (!msPerImage) {
        msPerImage = QInputDialog::getInt(this,
                                  "Enter ms delay between images",
                                  "Delay (1-1000 ms) ",
                                  50, 1, 1000);
    }

//    G::wait(1000);        // time to release modifier keys for shortcut (otherwise select many)
    isStressTest = true;
    bool isForward = true;
    slideCount = 0;
    int uturnCounter = 0;
    int uturnAmount = QRandomGenerator::global()->bounded(1, 301);
    QElapsedTimer t;
    t.start();
    while (isStressTest) {
        if (secPerFolder && t.elapsed() > secPerFolder) return;
        if (uturn && ++uturnCounter > uturnAmount) {
            isForward = !isForward;
            uturnAmount = QRandomGenerator::global()->bounded(1, 301);
            uturnCounter = 0;
        }
        ++slideCount;
        G::wait(msPerImage);
        if (isForward && dm->currentSfRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && dm->currentSfRow == 0) isForward = true;
        if (isForward) keyRight();
        else keyLeft();
        //qApp->processEvents();
        if (dm->sf->rowCount() < 2) return;
    }
    qint64 msElapsed = t.elapsed();
    double seconds = msElapsed * 0.001;
    double elapedmsPerImage = msElapsed * 1.0 / slideCount;
    int imagePerSec = slideCount * 1.0 / seconds;
    QString msg = "" + QString::number(slideCount) + " images.<br>" +
                  QString::number(msElapsed) + " ms elapsed.<br>" +
                  QString::number(elapedmsPerImage) + " ms delay.<br>" +
                  QString::number(imagePerSec) + " images per second.<br>" +
                  QString::number(elapedmsPerImage) + " ms per image."
//                  + "<br><br>Press <font color=\"red\"><b>Esc</b></font> to cancel this popup."
                  ;
    G::popUp->showPopup(msg, 0);
    qDebug() << "MW::traverseFolderStressTest" << "Executed stress test" << slideCount << "times.  "
             << msElapsed << "ms elapsed  "
             << msPerImage << "ms delay  "
             << imagePerSec << "images per second  "
             << elapedmsPerImage << "ms per image."
                ;
    return;
    if (G::isLogger) G::log("MW::traverseFolderStressTest");
    getSubfolders("/users/roryhill/pictures");
    QString fPath;
    int r = static_cast<int>(QRandomGenerator::global()->generate());
    fPath = subfolders->at(r % (subfolders->count()));
}

void MW::bounceFoldersStressTestFromMenu()
{
    qDebug() << "MW::bounceFoldersStressTestFromMenu";
    bounceFoldersStressTest();
}

void MW::bounceFoldersStressTest(int msPerImage, int secPerFolder)
{
    if (G::isLogger) G::log("MW::bounceFoldersStressTest");
    //qDebug() << "MW::bounceFoldersStressTest" << "ms =" << msPerImage << "duration =" << secPerFolder;
    G::popUp->end();


    if (!secPerFolder) {
        secPerFolder = QInputDialog::getInt(this,
              "Enter seconds per folder tested",
              "Duration (0 - 1000 sec)",
              5, 0, 86400);
        secPerFolder *= 1000;
    }

    if (!msPerImage) {
        msPerImage = QInputDialog::getInt(this,
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
        qDebug() << "MW::bounceFoldersStressTest" << randomIdx << path;
        bookmarks->select(path);
        fsTree->select(path);
        folderSelectionChange();
        traverseFolderStressTest(msPerImage, secPerFolder);
    }
}

void MW::scrollImageViewStressTest(int ms, int pauseCount, int msPauseDelay)
{
//    imageCacheThread->resume();
//    return;

    // ESC to quit
    isStressTest = true;
    bool isForward = true;
    int i = 0;
    while (isStressTest) {
        if (ms) G::wait(ms);
        if (pauseCount && ++i > pauseCount) {
            G::wait(msPauseDelay);
            i = 0;
        }
        if (isForward && dm->currentSfRow >= dm->sf->rowCount() - 1)
            isForward = false;
        if (!isForward && dm->currentSfRow <= 0)
            isForward = true;
        if (isForward) sel->next();
        else sel->prev();
    }
}

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{
    G::isFlowLogger = !G::isFlowLogger;
    qDebug() << "G::isFlowLogger =" << G::isFlowLogger;
    int count = 0;
    for (int row = 0; row < dm->rowCount(); ++row) {
        if (!(dm->itemFromIndex(dm->index(row, 0))->icon().isNull())) count++;
    }
    qDebug() << "Icon count =" << count
             << "metaReadThread->firstIconRow =" << metaReadThread->firstIconRow
             << "metaReadThread->lastIconRow =" << metaReadThread->lastIconRow
             << "metaReadThread->iconChunkSize =" << metaReadThread->iconChunkSize
                ;
    return;
    QString fPath = "D:/Pictures/favourites/2013-09-17_0033.jpg";   // pos = 889
    metadata->testNewFileFormat(fPath);
    if (G::isFlowLogger2) qDebug() << "";
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
//    QModelIndex rootIdx = fsTree->fsModel->index(0,0);
//    //fsTree->expandAll();
//    QModelIndex firstChildIdx = fsTree->fsModel->index(0,0,rootIdx);
//    QModelIndex usersIdx = fsTree->fsModel->index("/Users");
//    qDebug() << rootIdx.data() << firstChildIdx.data() << usersIdx.data();
//    fsTree->expand(rootIdx);  /Users
//    fsTree->setRootIndex(usersIdx);
    fsTree->setRootIndex(fsTree->model()->index(0,0));
    //fsTree->setRowHidden(0, QModelIndex(), true);

}

/*
   Performance
        Big max number of thumbnails
        Turn color management off
        Hide caching progress
*/
