#include "stresstest.h"

StressTest::StressTest(QObject *parent, DataModel *dm, BookMarks *bookmarks, FSTree *fsTree)
{
    this->dm = dm;
    this->bookmarks = bookmarks;
    this->fsTree = fsTree;
    lower = 5000;
    higher = 100000;
}

void StressTest::stop()
{
    requestInterruption();
    wait();
}

void StressTest::begin()
{
    int msDelay = QInputDialog::getInt(nullptr,
                       "Enter ms delay between images",
                       "Delay (1-1000 ms) ",
                       100, 1, 1000);
    double secPerFolder = QInputDialog::getDouble(nullptr,
                       "Enter seconds per folder tested",
                       "Duration (0 - 1000 sec)  -1 sec = random 1 - 300 sec",
                       -1, -1, 1000);
    if (secPerFolder == -1) {
        isRandomFolderTime = true;
        double lower = 0.1;
        double upper = 300;
        lower = QInputDialog::getDouble(nullptr,
                       "Random lower bound",
                       "0.1 - 300 sec",
                       1, 0.1, 300);
        upper = QInputDialog::getDouble(nullptr,
                       "Random uper bound",
                       QString::number(lower) + " - 300 sec",
                       100, lower, 300);
        begin(msDelay, lower * 1000, upper * 1000);
        return;
    }
    else {
        isRandomFolderTime = false;
        quint64 msPerFolder = secPerFolder * 1000;
        begin(msDelay, msPerFolder, msPerFolder);
    }
}

void StressTest::begin(int msDelay, quint64 lower, quint64 higher)
{
    qDebug() << "StressTest::begin";
    G::isStressTest = true;
    bookMarkPaths = bookmarks->bookmarkPaths.values();
    bmCount = bookMarkPaths.count();
    if (isRandomFolderTime)
        folderTime = QRandomGenerator::global()->bounded(lower, higher);
    else
        folderTime = lower;
    t.start();
    delay = msDelay;
    this->lower = lower;
    this->higher = higher;
    isForward = true;
    uturnCounter = 0;
    dm->sf->rowCount() < 300 ? uturnMax = dm->sf->rowCount() : uturnMax = 300;
    uturnAmount = QRandomGenerator::global()->bounded(1, uturnMax);
    start();
}

void StressTest::run(){
    while (G::isStressTest) {
        if (isInterruptionRequested()) break;
        // qDebug() << "StressTest::run uturnCounter =" << uturnCounter;

        if (t.elapsed() > folderTime) {
            int imageCount;
            QString path;
            do {
                uint randomIdx = QRandomGenerator::global()->generate() % static_cast<uint>(bmCount);
                path = bookMarkPaths.at(randomIdx);
                imageCount =  fsTree->imageCount(path);
            } while (imageCount < 2);
            emit selectBookmark(path);
            emit selectFolder(path);
            // bookmarks->select(path);
            // fsTree->select(path);
            if (isRandomFolderTime)
                folderTime = QRandomGenerator::global()->bounded(lower, higher);
            else
                folderTime = lower;
            uturnCounter = 0;
            dm->sf->rowCount() < 300 ? uturnMax = dm->sf->rowCount() : uturnMax = 300;
            t.restart();
        }

        if (++uturnCounter > uturnAmount) {
            isForward = !isForward;
            uturnAmount = QRandomGenerator::global()->bounded(1, uturnMax);
            uturnCounter = 0;
        }

        if (isForward && dm->currentSfRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && dm->currentSfRow == 0) isForward = true;
        if (isForward) emit next(Qt::NoModifier);
        else emit prev(Qt::NoModifier);

        // wait
        msleep(delay);
    }
}
