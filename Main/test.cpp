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
    G::popUp->hide();

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
        if (isForward && currSfRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && currSfRow == 0) isForward = true;
        if (isForward) keyRight();
        else keyLeft();
        qApp->processEvents();
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
    G::popUp->hide();


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

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{
    qDebug() << CLASSFUNCTION;
    bounceFoldersStressTest(50, 10000);
    return;
    QString fPath = "D:/Pictures/favourites/2013-09-17_0033.jpg";   // pos = 889
    metadata->testNewFileFormat(fPath);
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
    QModelIndex idx = fsTree->getCurrentIndex();
    qDebug() << fsTree->currentIndex()
             << fsTree->getCurrentIndex()
             << idx.data(Qt::ToolTipRole)
//             << fsTree->fsModel->filePath(fsTree->getCurrentIndex())
                ;
    return;
    fsTree->refreshModel();
    return;

    imageCacheThread->debugCaching = true;
    return;

    QString s = "-a -v3 -u -g1 -w txt";
    QStringList sl = s.split(" ");
    qDebug() << sl;
    return;
    setting->beginGroup("ExternalAppArgs");
    QStringList args = setting->childKeys();
    qDebug() << args;
    setting->endGroup();
    return;
    QProcess *process = new QProcess();
    process->setProgram("/Users/roryhill/Projects/ExifTool/exiftool");
    process->setArguments({"-a", "-v3", "u", "g1", "-w", "txt", "/Volumes/OM SYSTEM/DCIM/100OMSYS/_8270001.ORF"});
//    process->setArguments({"-w", "txt", "/Volumes/OM SYSTEM/DCIM/100OMSYS/_8270002.JPG"});
    process->setWorkingDirectory("/Volumes/OM SYSTEM/DCIM/100OMSYS");
    process->start();
}
