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
    setWindowOpacity(0);
    return;

    QPalette pal = QToolTip::palette();
    pal.setColor(QPalette::ToolTipText, Qt::red);
//    QFont font;
//    font.setPointSize(18);
//    pal.setBrush(QPalette::ToolTipText, QBrush(Qt::red));
    QToolTip::setPalette(pal);
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
    //this->windowHandle()->setPosition(4291,361);
    qDebug() << "test";
    //restoreLastSessionGeometryState();

//    restoreGeometry(setting->value("Geometry").toByteArray());
//    restoreState(setting->value("WindowState").toByteArray());
//    setVisible(true);
//    setWindowOpacity(100);
//    show();
    return;

    QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *screen = screens.at(1);
    QRect availableGeometry = screen->availableGeometry();
    int x = qBound(availableGeometry.left(), 4291, availableGeometry.right() - 855);
    int y = qBound(availableGeometry.top(), 361, availableGeometry.bottom() - 493);
    qDebug() << "availableGeometry:" << availableGeometry
             << "x =" << x
             << "y =" << y
        ;
    setGeometry(QRect(x, y, 855, 493));
    //setGeometry(QRect(4291,361, 855, 493));
    return;

//    QList<QScreen *> screens = QGuiApplication::screens();
//    qDebug() << "Number of screens:" << screens.count();

//    for (int i = 0; i < screens.count(); ++i) {
//        QScreen *screen = screens.at(i);
//        qDebug() << "Screen" << i << screen << "geometry:" << screen->geometry();
//    }
//    qDebug() << "Winnow Screen" << this->screen();
//    QWindow *window = windowHandle();
//    QScreen *screen = screens.at(1);
    //window->setScreen(screen);
//    this->move(QPoint(4500,500));

//    qDebug() << "pos() =" << pos();
//    qDebug() << "QApplication::desktop ()->screenNumber (pos)) =" << QGuiApplication::screenAt(pos());
//    qDebug() << "geometry() =" << geometry();
//    qDebug() << "screen()->geometry() =" << screen()->geometry();
//    qDebug() << "screen()->availableGeometry() =" << screen()->availableGeometry();
//    qDebug() << "screen()->availableVirtualGeometry() ="<<  screen()->availableVirtualGeometry();

//    Mac::spaces();
//    QList<QScreen *> screens = QGuiApplication::screens();
//    for (int i = 0; i < screens.size(); ++i) {
//        QRect screenGeometry = screens[i]->geometry();
//        qDebug() << "Screen" << i << "geometry:" << screenGeometry;
//    }
}
