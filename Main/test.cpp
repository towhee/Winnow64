#include "Main/mainwindow.h"

void MW::stressTest(int ms)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    qDebug() << CLASSFUNCTION << ms;
    ms = QInputDialog::getInt(this, "Enter ms delay between images", "Delay (1-1000 ms) ",
                              50, 1, 1000);

//    G::wait(1000);        // time to release modifier keys for shortcut (otherwise select many)
    isStressTest = true;
    bool isForward = true;
    slideCount = 0;
    QElapsedTimer t;
    t.start();
    qDebug() << CLASSFUNCTION << "-1";
    while (isStressTest) {
        G::wait(ms);
        ++slideCount;
        if (isForward && currSfRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && currSfRow == 0) isForward = true;
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

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{
//    metaRead->restart();
    return;

    QString fPath = "D:/Pictures/favourites/2013-09-17_0033.jpg";   // pos = 889
    metadata->testNewFileFormat(fPath);
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
    QList<int> list;  // force crash
    int a = list.at(17);
    qDebug() << "a =" << a;
    return;
//    QStandardItem *item = dm->itemFromIndex(dm->index(0, 0));
//    item->setData(0, Qt::DecorationRole);
//    dm->setData(dm->index(0,0), 0, Qt::DecorationRole);
//    QVariant v = dm->index(0,0).data(Qt::DecorationRole);
//    QVariant v1 = dm->index(600,0).data(Qt::DecorationRole);
//    qDebug() << v << v1 << v1.isValid() << dm->index(600,0).data(Qt::DecorationRole).isNull();
//    return;

//    thumbView->selectThumb(600);
//    return;

    QPixmap nullPm;
    QModelIndex dmIdx = dm->index(0,0);
    dm->setIcon(dmIdx, nullPm, dm->instance, "test");
    diagnosticsMetadataCache();
    return;

    QByteArray bx = "www";
    quint16 x = Utilities::get16(bx);
    Utilities util;
    quint16 x1 = util.get16(bx);
    qDebug() << x << x1;
    return;


    QElapsedTimer t;
    t.start();

    qDebug() << G::rowsWithIcon.count();
    return;

    dm->clearAllIcons();
    qDebug() << t.nsecsElapsed() << "nanosec";
    return;


    int sfRow = 8;
    bool isCached = icd->cacheItemList.at(sfRow).isCached;
    QString p = icd->cacheItemList.at(sfRow).fPath;
    qDebug() << p << isCached;
//    imageCacheThread->fixOrphans();
//    return;

    QString path = "/Users/roryhill/Pictures/2022/202204/2022-04-11_Nora/2022-04-11_0001.arw";
    emit setValuePath(path, 0, true, G::CachedRole);
    return;

   int quitRow = 400;

//    /*
    int sfRowCount = 10;

    int count = 0;
    int row = 7;
    bool ahead = true;
    int lastRow = sfRowCount - 1;
    bool moreAhead = row < lastRow;
    bool moreBehind = row >= 0;
    int rowAhead = row;
    int rowBehind = row;
    while (count++ < sfRowCount) {
//        count++;
        if (count > quitRow)  break;
        // do something with sfRow

        qDebug() << count << row ;
        // next sfRow to process
        if (ahead) {
            if (moreBehind) ahead = false;
            if (moreAhead) {
                ++rowAhead;
                row = rowAhead;
                moreAhead = rowAhead < lastRow;
            }
        }
        else {
            if (moreAhead) ahead = true;
            if (moreBehind) {
                --rowBehind;
                row = rowBehind;
                moreBehind = row >= 0;
            }
        }
    }
    double ms = t.nsecsElapsed() * 1.0 / 1000000;
    qDebug() << ms << "ms";
    return;
    //*/

/*
    int rows = 100000;
    int row = 7000;
    int behind = row;
    int ahead = row + 1;
    bool abort = false;
    QList<int> queue(rows);
    t.start();
    queue.clear();
    while (behind >= 0 || ahead < rows) {
        if (behind >= 0) queue.append(behind--);
        if (ahead < rows) queue.append(ahead++);
        if (abort) return;
    }
    for (int count = 0; count < rows; count++) {
        if (count > quitRow)  break;
        row = queue.at(count);
        qDebug() << count << row;
    }
    double ms = t.nsecsElapsed() * 1.0 / 1000000;
    qDebug() << ms << "ms";
    return;
//*/



    thumbView->scrollToRow(currSfRow, CLASSFUNCTION);
    return;

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
    G::track(CLASSFUNCTION, "New folder: " + fPath);
    folderSelectionChange();
    b = !b;
}
