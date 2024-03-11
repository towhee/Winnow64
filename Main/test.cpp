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
    G::popUp->reset();

    int msPerFolder = secPerFolder * 1000;
    //qDebug() << "MW::traverseFolderStressTest" << msPerImage << msPerFolder;

    if (!msPerImage) {
        msPerImage = QInputDialog::getInt(this,
                                  "Enter ms delay between images",
                                  "Delay (1-1000 ms) ",
                                  50, 1, 1000);
    }

//    G::wait(1000);        // time to release modifier keys for shortcut (otherwise select many)
    isStressTest = true;
    bool isForward = true;
    //slideCount = 0;
    int uturnCounter = 0;
    int uturnAmount = QRandomGenerator::global()->bounded(1, 301);
    QElapsedTimer t;
    t.start();
    while (isStressTest) {
        if (msPerFolder && t.elapsed() > msPerFolder) return;
        if (uturn && ++uturnCounter > uturnAmount) {
            isForward = !isForward;
            uturnAmount = QRandomGenerator::global()->bounded(1, 301);
            uturnCounter = 0;
        }
        ++slideCount;
        if (!uturn && slideCount >= dm->sf->rowCount()) {
            return;
        }
        G::wait(msPerImage);
        if (isForward && dm->currentSfRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && dm->currentSfRow == 0) isForward = true;
        if (isForward) keyRight();
        else keyLeft();
        //qApp->processEvents();
//        if (dm->sf->rowCount() < 2) return;
//        QString status = " Stress test: " + QString::number(slideCount) + " images.";
//        updateStatus(true, status, "StressTest");
    }
    qint64 msElapsed = t.elapsed();
    double seconds = msElapsed * 0.001;
    double elapsedMsPerImage = msElapsed * 1.0 / slideCount;
    int imagePerSec = slideCount * 1.0 / seconds;
    QString msg = "" + QString::number(slideCount) + " images.<br>" +
                  QString::number(msElapsed) + " ms elapsed.<br>" +
                  QString::number(elapsedMsPerImage) + " ms delay.<br>" +
                  QString::number(imagePerSec) + " images per second.<br>" +
                  QString::number(elapsedMsPerImage) + " ms per image."
//                  + "<br><br>Press <font color=\"red\"><b>Spacebar</b></font> to cancel this popup."
                  ;
    G::popUp->showPopup(msg, 0);
    qDebug() << "MW::traverseFolderStressTest" << "Executed stress test" << slideCount << "times.  "
             << msElapsed << "ms elapsed  "
             << msPerImage << "ms delay  "
             << imagePerSec << "images per second  "
             << elapsedMsPerImage << "ms per image."
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
    qDebug() << "MW::bounceFoldersStressTest" << "ms =" << msPerImage << "duration =" << secPerFolder;
    G::popUp->reset();

    if (!msPerImage) {
        msPerImage = QInputDialog::getInt(this,
              "Enter ms delay between images",
              "Delay (1-1000 ms) ",
              100, 1, 1000);
    }

    if (secPerFolder < 0) {
        secPerFolder = QInputDialog::getInt(this,
                                            "Enter seconds per folder tested",
                                            "Duration (0 - 1000 sec)  -1 sec = random 1 - 20 sec",
                                            -1, -1, 1000);
    }
    bool isRandomSecPerFolder = secPerFolder == -1;

    slideCount = 0;
    isStressTest = true;
    QList<QString>bookMarkPaths = bookmarks->bookmarkPaths.values();
    int n = bookMarkPaths.count();
    while (isStressTest) {
        uint randomIdx = QRandomGenerator::global()->generate() % static_cast<uint>(n);
        if (isRandomSecPerFolder)
            secPerFolder = QRandomGenerator::global()->bounded(1, 21);
        QString path = bookMarkPaths.at(randomIdx);
        qDebug() << "MW::bounceFoldersStressTest"
            << "secInFolder =" << secPerFolder << path;
        bookmarks->select(path);
        fsTree->select(path);
        folderSelectionChange();
        traverseFolderStressTest(msPerImage, secPerFolder);
    }
}

void MW::scrollImageViewStressTest(int ms, int pauseCount, int msPauseDelay)
{
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

    sel->recover();
    return;

    bounceFoldersStressTest(100, 1);

    setCursor(QCursor(Qt::BlankCursor));
    return;
    QString fPath = "D:/Pictures/favourites/2013-09-17_0033.jpg";   // pos = 889
    metadata->testNewFileFormat(fPath);
    if (G::isFlowLogger2) qDebug() << "";
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
//    char strings[128000];
//    char* s[4096];
//    int i = 0;
//    int x = (int)((char)i);
//    qDebug () << "x =" << x;
//    for (int i = 0 ; i != 10 ; i++ ) {
//        strings[i] = (char)i;
//        s[i] = &strings[i];
//        int x = (int)(*s[i]);
//        char c = *s[i];
//        qDebug() << "i =" << i
//                 << "x =" << x
//                 << "c =" << c
////                 << "s[i] =" << s[i]
////                 << "*s[i] =" << *s[i]
////                 << "(int)(*s[i]) =" << (int)(*s[i])
//            ;
//    }

    qDebug() << "click";
    QString path = "C:/Users/hillr/Downloads/click.wav";

    QMediaPlayer *player = new QMediaPlayer;
    QAudioOutput *audioOutput = new QAudioOutput;

    player->setAudioOutput(audioOutput);
    //player->setSource(QUrl::fromLocalFile(":/Sounds/ingest.wav"));
    player->setSource(QUrl::fromLocalFile(path));
    //player->setSource(QUrl::fromLocalFile("/Users/roryhill/Downloads/ingest2b.wav"));
    audioOutput->setVolume(50);
    player->play();
    return;

//    pickClick->play();
//    return;

//    QByteArray ba = "thequickbrownfoxjumpedoverthelazydogs";
//    int len = ba.size();
//    char* c1[37];
//    for (int i = 0; i < len; i++) {
//        c1[i] = &ba[i];
//    }
//    char* c = &ba[0];
//    uint offset = 3;
//    char ch = *(c + offset);
//    qDebug()
//            << " len =" << len
//        << "ba[3] =" << ba[3] << "\n"
//        << "*c1[3] =" << *c1[offset] << "\n"
//        << "ch =" << ch << "\n"
//        << "(uint8_t)ch =" << (uint8_t)ch << "\n"
//                ;
}
/*
   Performance
        Big max number of thumbnails
        Turn color management off
        Hide caching progress
*/
