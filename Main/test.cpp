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
    WId wId = winId();
    Win::darkMode(winId());
    return;


    qDebug() << "G::modifySourceFiles =" << G::modifySourceFiles;
    bounceFoldersStressTest(50, 10000);
    return;
    QString fPath = "D:/Pictures/favourites/2013-09-17_0033.jpg";   // pos = 889
    metadata->testNewFileFormat(fPath);
    if (G::isFlowLogger2) qDebug() << "";
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
    QList<QTabBar *> tabList = findChildren<QTabBar *>();
    for (int i = 0; i < tabList.count(); i++) {
        QTabBar* tabBar = tabList.at(i);
        for (int j = 0; j < tabBar->count(); j++) {
            qDebug() << "tabBar" << i
                     << "tab" << j
                     << "text" << tabBar->tabText(j)
                ;
        }
    }
    return;

    QList<QDockWidget*> dockWidgets = tabifiedDockWidgets(metadataDock);
    for (int i = 0; i < dockWidgets.size(); ++i) {
        DockWidget *dw = qobject_cast<DockWidget*>(dockWidgets.at(i));
        if (dw) {
            // Do something with dw
            qDebug() << dw->objectName();
        }
    }
    return;

    qDebug() << "IconView::updateVisibleCells"
             << thumbView->objectName()
             << "current row =" << thumbView->currentIndex().row()
             << "firstVisibleCell =" << thumbView->firstVisibleCell
             << "lastVisibleCell =" << thumbView->lastVisibleCell
             << "midVisibleCell =" << thumbView->midVisibleCell
             << "visibleCellCount =" << thumbView->visibleCellCount
             << "bestAspectRatio =" << thumbView->bestAspectRatio;
        ;
    return;


    qDebug() << "pos() =" << pos();
    qDebug() << "QApplication::desktop ()->screenNumber (pos)) =" << QGuiApplication::screenAt(pos());
    qDebug() << "geometry() =" << geometry();
    qDebug() << "screen()->geometry() =" << screen()->geometry();
    qDebug() << "screen()->availableGeometry() =" << screen()->availableGeometry();
    qDebug() << "screen()->availableVirtualGeometry() ="<<  screen()->availableVirtualGeometry();

//    Mac::spaces();
    QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QRect screenGeometry = screens[i]->geometry();
        qDebug() << "Screen" << i << "geometry:" << screenGeometry;
    }}
