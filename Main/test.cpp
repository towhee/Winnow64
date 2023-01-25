#include "Main/mainwindow.h"

void MW::traverseFolderStressTestFromMenu()
{
    qDebug() << "MW::traverseFolderStressTestFromMenu";
    traverseFolderStressTest();
}

void MW::traverseFolderStressTest(int ms, int duration)
{
    if (G::isLogger) G::log("MW::traverseFolderStressTest");
    qDebug() << "MW::traverseFolderStressTest" << ms;
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
    qDebug() << "MW::traverseFolderStressTest" << "Executed stress test" << slideCount << "times.  "
             << msElapsed << "ms elapsed  "
             << ms << "ms delay  "
             << imagePerSec << "images per second  "
             << msPerImage << "ms per image."
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
    bounceFoldersStressTest(0, 0);
}

void MW::bounceFoldersStressTest(int ms, int duration)
{
    if (G::isLogger) G::log("MW::bounceFoldersStressTest");
    qDebug() << "MW::bounceFoldersStressTest" << "ms =" << ms << "duration =" << duration;
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
        qDebug() << "MW::bounceFoldersStressTest" << randomIdx << path;
        bookmarks->select(path);
        fsTree->select(path);
        folderSelectionChange();
        traverseFolderStressTest(ms, duration);
    }
}

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{

    loadConcurrent(dm->currentSfRow);
    return;
    centralLayout->setCurrentIndex(GridTab);
//    gridView->setVisible(!gridView->isVisible());
    return;

    qDebug() << "MW::testNewFileFormat";
    bounceFoldersStressTest(50, 10000);
    return;
    QString fPath = "D:/Pictures/favourites/2013-09-17_0033.jpg";   // pos = 889
    metadata->testNewFileFormat(fPath);
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
    thumbView->scrollToRow(53-1, "temp");
//    diagnosticsMetadataCache();
    return;

    QMediaMetaData mediaMetadata = videoView->video->mediaPlayer->metaData();
    QList<QMediaMetaData::Key> keys = mediaMetadata.keys();
    qDebug() << keys;
    return;
    QVariant v = mediaMetadata[QMediaMetaData::ThumbnailImage];
    QPixmap pm = v.value<QPixmap>();
    ImageDlg imageDlg(pm, "test");
    imageDlg.exec();
    return;

    QString s = "🚫";  // "❓❌ 🇨🇦 🪰 🦬🐎🪰🪳🦟🪲🦥🦞🦤🐻‍❄️🦩🪱🦨"
                       // "🦦🦫🐫🐪🦎🦋🕷️🐌🦭🦧🦣"
    qDebug() << s;
}
