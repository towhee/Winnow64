#include "Main/mainwindow.h"
#include "ImageFormats/Video/mov.h"

#include <cstdlib>          // std::_Exit (soak fast-exit path)
#ifdef Q_OS_MAC
#include <malloc/malloc.h>  // malloc_zone_pressure_relief (soak anchor baseline)
#include <mach/mach.h>      // task_threads (live thread count for soak probe)
#endif
#include <atomic>

namespace {
/* Live OS thread count for this process. A monotonic climb across bounces is a
   thread leak (the QMediaPlayer/FFmpeg video-thumbnail fallback is the prime
   suspect); flat means the EAGAIN crash is stack-VM exhaustion instead.
   Returns -1 where unsupported. */
int processThreadCount()
{
#ifdef Q_OS_MAC
    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &threads, &count) != KERN_SUCCESS)
        return -1;
    // task_threads allocates the array and the per-thread send rights; free both.
    for (mach_msg_type_number_t i = 0; i < count; ++i)
        mach_port_deallocate(mach_task_self(), threads[i]);
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads),
                  count * sizeof(thread_act_t));
    return static_cast<int>(count);
#else
    return -1;
#endif
}
} // namespace
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <sys/stat.h>
#include <thread>

void MW::traverseFolderStressTestFromMenu()
{
    qDebug() << "MW::traverseFolderStressTestFromMenu";
    traverseFolderStressTest();
}

/*
void MW::traverseFolderStressTest(int msPerImage, int secPerFolder, bool uturn)
{
    // ChatGPT version
    // if no images in folder then return
    if (dm->sf->rowCount() == 0) return;

    bool isBounceFolders = secPerFolder > 0;
    bool bounceNextFolder = false;

    int msPerFolder = secPerFolder * 1000;
    if (!msPerImage) {
        msPerImage = QInputDialog::getInt(this,
                                          "Enter ms delay between images",
                                          "Delay (1-1000 ms) ",
                                          50, 1, 1000);
    }

    G::popUp->reset();
    QString stopMsg = "Press ESC to stop stress test";
    G::popUp->showPopup(stopMsg, 0);

    G::isStressTest = true;
    bool isForward = true;
    int uturnCounter = 0;
    int uturnMax = qMin(dm->sf->rowCount(), 300);
    int uturnAmount = QRandomGenerator::global()->bounded(1, uturnMax);

    QElapsedTimer elapsedTimer;
    elapsedTimer.start();

    qint64 elapsedMsInFolder = 0;
    int slideCount = 0;

    QTimer *imageTimer = new QTimer(this);
    imageTimer->setInterval(msPerImage);

    connect(imageTimer, &QTimer::timeout,
            this, [=, &isBounceFolders, &bounceNextFolder, &elapsedMsInFolder,
                      &slideCount, &uturnCounter, &isForward]() mutable
    {
        if (!isStressTest) {
            imageTimer->stop();
            return;
        }

        elapsedMsInFolder += msPerImage;

        // Countdown folder time limit reached
        if (isBounceFolders && elapsedMsInFolder >= msPerFolder) {
            imageTimer->stop();
            bounceNextFolder = true;
            return;
        }

        // U-turn logic
        if (uturn && ++uturnCounter > uturnAmount) {
            isForward = !isForward;
            uturnAmount = QRandomGenerator::global()->bounded(1, uturnMax);
            uturnCounter = 0;
        }

        // Slide count and folder end logic
        ++slideCount;
        if (!uturn && slideCount >= dm->sf->rowCount()) {
            G::isStressTest = false;
            imageTimer->stop();
            return;
        }

        // Update stress test status
        double seconds = elapsedTimer.elapsed() * 0.001;
        if (secPerFolder) stressSecToGoInFolder = secPerFolder - seconds;

        // Navigate to the next image
        if (isForward && dm->currentSfRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && dm->currentSfRow == 0) isForward = true;
        if (isForward) sel->next();
        else sel->prev();
    });

    imageTimer->start();

    // Block until G::isStressTest becomes false
    while (G::isStressTest || !bounceNextFolder) {
        if (G::useProcessEvents) qApp->processEvents(); // Process events to keep the UI responsive
    }

    qDebug() << "MW::traverseFolderStressTest bounceNextFolder";
    if (bounceNextFolder) return;

    // Cleanup and final reporting
    imageTimer->deleteLater();
    double elapsedSeconds = elapsedTimer.elapsed() * 0.001;
    double elapsedMsPerImage = elapsedTimer.elapsed() * 1.0 / slideCount;
    int imagesPerSecond = slideCount / elapsedSeconds;

    QString msg = QString("%1 images.<br>%2 secPerFolder.<br>%3 stressSecToGoInFolder.<br>%4 ms elapsed.<br>%5 ms delay.<br>%6 images per second.<br>%7 ms per image.")
                      .arg(slideCount)
                      .arg(secPerFolder)
                      .arg(secPerFolder - elapsedSeconds)
                      .arg(elapsedTimer.elapsed())
                      .arg(elapsedMsPerImage)
                      .arg(imagesPerSecond)
                      .arg(elapsedMsPerImage);

    G::popUp->showPopup(msg, 0);

    qDebug() << "MW::traverseFolderStressTest"
             << "Executed stress test" << slideCount << "times."
             << elapsedTimer.elapsed() << "ms elapsed"
             << msPerImage << "ms delay"
             << imagesPerSecond << "images per second"
             << elapsedMsPerImage << "ms per image.";
}
//*/

// void MW::traverseFolderStressTest(int msPerImage, double secPerFolder, bool uturn)
// {

// }

// /*
void MW::traverseFolderStressTest(int msPerImage, double secPerFolder, bool uturn)
{

    // msPerImage      time per image
    // secPerFolder    ms test (0 == forever) ESC to stop
    // uturn           randomly change direction

    G::popup->reset();

    // if no images in folder then return
    if (dm->sf->rowCount() == 0) return;

    int msPerFolder = secPerFolder * 1000;
    //qDebug() << "MW::traverseFolderStressTest" << msPerImage << msPerFolder;

    if (!msPerImage) {
        msPerImage = QInputDialog::getInt(this,
                                  "Enter ms delay between images",
                                  "Delay (1-1000 ms) ",
                                  50, 1, 1000);
    }

    bool randomChangeDirection = false;
    if (secPerFolder < 0.1) {
        QStringList options;
        options << "No" << "Yes";

        // Show the input dialog
        bool ok;
        QString choice = QInputDialog::getItem(
            nullptr,                                // Parent widget
            "Folder Stress Test",            // Dialog title
            "Randomly Change Direction:", // Label text
            options,                               // Options
            0,                                     // Initial selection index
            false,                                 // Editable (false for a dropdown)
            &ok                                    // To check if the user confirmed
            );

        // If the user confirmed, update the value
        if (ok) {
            randomChangeDirection = (choice == "Yes");
        }
    }
    // qDebug() << "MW::traverseFolderStressTest randomChangeDirection =" << randomChangeDirection;

    QString stopMsg = "Press <font color=\"red\"><b>ESC</b></font> to stop stress test";
    if (!secPerFolder) G::popup->showPopup(stopMsg, 0);
    G::isStressTest = true;
    bool isForward = true;
    // Per-folder counter — must reset each call or the !uturn early-exit
    // below trips immediately on the second folder when bouncing.
    slideCount = 0;
    int uturnCounter = 0;
    int uturnMax;
    dm->sf->rowCount() < 300 ? uturnMax = dm->sf->rowCount() : uturnMax = 300;
    // qDebug() << "uturnMax =" << uturnMax;
    int uturnAmount;
    if (uturnMax > 10) uturnAmount = QRandomGenerator::global()->bounded(1, uturnMax);
    else uturnAmount = dm->sf->rowCount();
    QElapsedTimer folderTime;
    folderTime.start();
    QElapsedTimer t;
    t.start();
    qint64 elapsedMsInFolder = 0;
    while (G::isStressTest) {
        elapsedMsInFolder += t.elapsed();
        t.restart();
        // countdown time limit reached
        if (secPerFolder && (elapsedMsInFolder > msPerFolder)) return;

        // uturn
        if (randomChangeDirection && ++uturnCounter > uturnAmount) {
            isForward = !isForward;
            uturnAmount = QRandomGenerator::global()->bounded(1, uturnMax);
            uturnCounter = 0;
        }

        // no uturn and end of images in folder
        ++slideCount;
        if (!uturn && slideCount >= dm->sf->rowCount()) return;


        // pause
        while (t.elapsed() < msPerImage){
            qApp->processEvents(QEventLoop::WaitForMoreEvents, 5);
        }
        double seconds = elapsedMsInFolder * 0.001;

        // report countdown time left in this folder
        if (secPerFolder) stressSecToGoInFolder = secPerFolder - seconds;

        // next image
        if (isForward && dm->currentSfRow == dm->sf->rowCount() - 1) isForward = false;
        if (!isForward && dm->currentSfRow == 0) isForward = true;
        if (isForward) sel->next();
        else sel->prev();
    }
    qint64 msElapsed = folderTime.elapsed();
    double seconds = msElapsed * 0.001;
    stressSecToGoInFolder = secPerFolder - seconds;
    double elapsedMsPerImage = msElapsed * 1.0 / slideCount;
    int imagePerSec = slideCount * 1.0 / seconds;
    // Skip the sticky end-of-folder popup when bouncing — it would overlay
    // the UI after every folder switch and never auto-clear (msDuration=0).
    if (!secPerFolder) {
        QString msg = "" + QString::number(slideCount) + " images.<br>" +
                      QString::number(seconds) + " seconds elapsed."
                      + "<br>Press <font color=\"red\"><b>ESC</b></font> to cancel this popup."
                      ;
        G::popup->showPopup(msg, 0);
    }
    /*
    qDebug() << "MW::traverseFolderStressTest" << "Executed stress test" << slideCount << "times.  "
             << msElapsed << "ms elapsed  "
             << msPerImage << "ms delay  "
             << imagePerSec << "images per second  "
             << elapsedMsPerImage << "ms per image."
                ; //*/
    return;
}
//*/

void MW::bounceFoldersStressTestFromMenu()
{
    qDebug() << "MW::bounceFoldersStressTestFromMenu";
    bounceFoldersStressTest();
}

void MW::bounceFoldersStressTest(int msPerImage, double secPerFolder)
{
    if (G::isLogger) G::log("MW::bounceFoldersStressTest");
    // qDebug() << "MW::bounceFoldersStressTest" << "ms =" << msPerImage << "duration =" << secPerFolder;
    G::popup->reset();

    if (!msPerImage) {
        msPerImage = QInputDialog::getInt(this,
              "Enter ms delay between images",
              "Delay (1-1000 ms) ",
              100, 1, 1000);
    }

    if (secPerFolder < 0) {
        secPerFolder = QInputDialog::getDouble(this,
                            "Enter seconds per folder tested",
                            "Duration (0 - 1000 sec)  -1 sec = random 1 - 300 sec",
                            -1, -1, 1000);
    }
    bool isRandomSecPerFolder = secPerFolder == -1;

    double lower = 0.1;
    double upper = 300;
    if (isRandomSecPerFolder) {
        lower = QInputDialog::getDouble(this,
                                "Random lower bound",
                                "0.1 - 300 sec",
                                0.1, 0.1, 300);
        upper = QInputDialog::getDouble(this,
                                "Random uper bound",
                                QString::number(lower) + " - 300 sec",
                                5, lower, 300);
    }

    slideCount = 0;
    G::isStressTest = true;
    QList<QString>bookMarkPaths = bookmarks->bookmarksWithImages();
    int n = bookMarkPaths.count();
    if (n == 0) {
        G::popup->showPopup("Stress test needs at least one bookmark.", 2000);
        G::isStressTest = false;
        return;
    }
    /* Memory-leak probe: footprint and cache size are sampled at the tail of
       each folder bounce so a slow climb can be localized. If footprintMB
       climbs while imCacheMB and dm rows stay flat, the leak is outside the
       image cache; if footprintMB tracks imCacheMB and never recovers, the
       cache trim is the issue. nonCacheMB = footprintMB - imCacheMB isolates
       the non-cache residency (the volatile part).

       Anchor sampling (REVERSIBLE — set anchorPath to "" to disable, or delete
       this block to revert): per-bounce footprint is swamped by folder-size
       variance (1.5–11 GB), so a slow leak is invisible. After each random
       bounce we load the same fixed anchor folder and sample footprint THERE —
       constant content means the only thing that can move ANCHOR footprintMB
       across a long run is a true leak. Point this at any small stable folder. */
    const QString anchorPath = "";
    // const QString anchorPath = "/Users/roryhill/Pictures/_heic";
    const bool useAnchor = false;
    // const bool useAnchor = !anchorPath.isEmpty() && QFileInfo(anchorPath).isDir();
    if (!anchorPath.isEmpty() && !useAnchor)
        qDebug() << "MW::bounceFoldersStressTest anchorPath is not a folder:"
                 << anchorPath;
    qint64 bounceCount = 0;
    while (G::isStressTest) {
        uint randomIdx = QRandomGenerator::global()->generate() % static_cast<uint>(n);
        if (isRandomSecPerFolder) {
            int msLower = static_cast<quint64>(lower * 1000);
            int msUpper = static_cast<quint64>(upper * 1000);
            quint64 msPerFolder = QRandomGenerator::global()->bounded(msLower, msUpper);
            secPerFolder = msPerFolder * 1.0 / 1000;
            // qDebug() << "MW::bounceFoldersStressTest RANDOM"
            //          << "msLower =" << msLower
            //          << "msUpper =" << msUpper
            //          << "msPerFolder =" << msPerFolder
            //          << "secInFolder =" << secPerFolder
            //          ;
        }
        QString path = bookMarkPaths.at(randomIdx);
        // qDebug() << "MW::bounceFoldersStressTest"
        //          << "secInFolder =" << secPerFolder
        //          << path;
        bookmarks->select(path);
        fsTree->select(path);
        // Folder load is async — wait until dm->sf is populated before
        // navigating, otherwise traverseFolderStressTest races against
        // the in-flight folder change and can crash.
        waitUntilMetadataLoaded(30000, "bounceFoldersStressTest");
        if (!G::isStressTest) break;
        traverseFolderStressTest(msPerImage, secPerFolder);

        /* Footprint probe (see note above loop). sizeMB() is the lock-free
           atomic byte total; dm rows reflect the current folder. */
        ++bounceCount;
        // {
        //     const qint64 footMB  = static_cast<qint64>(G::processFootprintMB());
        //     const qint64 cacheMB = qRound(icd->sizeMB());
        //     qDebug().nospace()
        //         << "MW::bounceFoldersStressTest PROBE"
        //         << "  bounce=" << bounceCount
        //         << "  footprintMB=" << footMB
        //         << "  imCacheMB=" << cacheMB
        //         << "  nonCacheMB=" << (footMB - cacheMB)
        //         << "  threads=" << processThreadCount()
        //         << "  availMB=" << G::availableMemoryMB.load()
        //         << "  dmRows=" << dm->rowCount()
        //         << "  sfRows=" << dm->sf->rowCount()
        //         << "  folder=" << path;
        // }

        /* Fixed-content baseline (only if anchorPath set). Load the anchor,
           settle its cache with a short fixed traverse, then force libmalloc to
           return free pages to the OS before sampling. macOS phys_footprint
           counts dirty pages the allocator hasn't handed back, so without the
           pressure relief the anchor footprint just tracks the previous folder's
           high-water mark. After relief, footprintMB is the TRUE resident
           baseline: flat across the run = retention only (no leak); climbing =
           a real leak. preReliefMB shows how much was merely retained.
           Not counted as a bounce. */
        if (useAnchor && G::isStressTest) {
            fsTree->select(anchorPath);
            waitUntilMetadataLoaded(30000, "bounceFoldersStressTest anchor");
            if (!G::isStressTest) break;
            traverseFolderStressTest(msPerImage, 0.5);   // fixed 0.5 s dwell
            const qint64 preReliefMB = static_cast<qint64>(G::processFootprintMB());
#ifdef Q_OS_MAC
            /* Ask every malloc zone to return free pages to the OS. Diagnostic
               only — drives the footprint to its true resident floor. */
            malloc_zone_pressure_relief(nullptr, 0);
#endif
            const qint64 footMB  = static_cast<qint64>(G::processFootprintMB());
            const qint64 cacheMB = qRound(icd->sizeMB());
            qDebug().nospace()
                << "MW::bounceFoldersStressTest ANCHOR"
                << "  bounce=" << bounceCount
                << "  footprintMB=" << footMB
                << "  preReliefMB=" << preReliefMB
                << "  imCacheMB=" << cacheMB
                << "  nonCacheMB=" << (footMB - cacheMB)
                << "  threads=" << processThreadCount()
                << "  availMB=" << G::availableMemoryMB.load()
                << "  dmRows=" << dm->rowCount();
        }
    }
}

void MW::runSoakTest(const QStringList &folders, int durationMs,
                     int msPerImage, uint seed)
{
/*
    Headless race/leak soak (see tests/soak). Reuses the interactive bounce
    algorithm without its dialogs: pick a random folder, reload it via the normal
    startup path, ping-pong through its images for a random dwell, repeat until
    durationMs elapses. Seeded so a failure can be replayed.

    Exit drives an orderly window close (closeEvent → stop all worker threads) so
    AddressSanitizer/LeakSanitizer's atexit report flags only TRUE leaks; set
    WINNOW_SOAK_FAST_EXIT=1 to std::_Exit instead (TSan race runs, where the leak
    check is off and orderly-teardown noise is unwanted).

    Probe line per bounce: footprint vs cache MB vs row counts localizes a climb
    (footprint up while imCacheMB/rows flat → leak outside the cache). threads is
    the live OS thread count; the "SOAK: done" summary reports threadsGrowth
    (threadsMax - threadsBaseline), which run_soak.sh gates on to catch thread
    leaks (the video-thumbnail decoder-thread leak).
*/
    if (G::isLogger) G::log("MW::runSoakTest");
    centralLayout->setCurrentIndex(LoupeTab);

    QStringList paths;
    for (const QString &f : folders)
        if (QFileInfo(f).isDir()) paths << f;
    if (paths.isEmpty()) {
        fprintf(stderr, "SOAK: no valid folders given\n");
        fflush(stderr);
        std::_Exit(2);
    }

    if (msPerImage < 1) msPerImage = 50;
    QRandomGenerator rng(seed);
    fprintf(stderr, "SOAK: start folders=%lld durationMs=%d msPerImage=%d seed=%u\n",
            (long long)paths.size(), durationMs, msPerImage, seed);
    fflush(stderr);

    G::isStressTest = true;
    qint64 bounceCount = 0;
    int threadsBaseline = -1;   // first bounce's thread count
    int threadsMax = -1;        // peak thread count over the run
    QElapsedTimer total;
    total.start();

    while (G::isStressTest && total.elapsed() < durationMs) {
        const int idx = paths.size() == 1 ? 0
                                          : int(rng.bounded(uint(paths.size())));
        const QString path = paths.at(idx);

        /* Random dwell 0.1–3.0 s, never overshooting the remaining budget.
           Kept >= 0.1 s so traverseFolderStressTest does not fall into its
           interactive QInputDialog branch. */
        double secPerFolder = 0.1 + rng.generateDouble() * 2.9;
        const double remainingSec = (durationMs - total.elapsed()) / 1000.0;
        if (remainingSec < 0.1) break;
        if (secPerFolder > remainingSec) secPerFolder = remainingSec;

        if (fsTree->select(path))
            folderSelectionChange(path, G::FolderOp::Add, /*resetDataModel*/true,
                                  /*recurse*/false);
        waitUntilMetadataLoaded(30000, "runSoakTest");
        if (!G::isStressTest) break;

        traverseFolderStressTest(msPerImage, secPerFolder);

        ++bounceCount;
        const int threads = processThreadCount();
        if (bounceCount == 1) threadsBaseline = threads;
        if (threads > threadsMax) threadsMax = threads;
        fprintf(stderr,
            "SOAK: bounce=%lld elapsedMs=%lld footprintMB=%llu imCacheMB=%lld "
            "threads=%d availMB=%llu dmRows=%d sfRows=%d folder=%s\n",
            (long long)bounceCount, (long long)total.elapsed(),
            (unsigned long long)G::processFootprintMB(),
            (long long)qRound(icd->sizeMB()),
            threads,
            (unsigned long long)G::availableMemoryMB.load(),
            dm->rowCount(), dm->sf->rowCount(),
            path.toLocal8Bit().constData());
        fflush(stderr);
    }

    G::isStressTest = false;
    /* Summary line the soak runner gates on: threadsGrowth = threadsMax -
       threadsBaseline. A bounded value means no thread leak; a large value is
       the QMediaPlayer/AVFoundation video-thumbnail leak resurfacing. */
    const int threadsGrowth = (threadsBaseline >= 0 && threadsMax >= 0)
                                  ? threadsMax - threadsBaseline : -1;
    fprintf(stderr,
        "SOAK: done bounces=%lld elapsedMs=%lld footprintMB=%llu "
        "threadsBaseline=%d threadsMax=%d threadsGrowth=%d\n",
        (long long)bounceCount, (long long)total.elapsed(),
        (unsigned long long)G::processFootprintMB(),
        threadsBaseline, threadsMax, threadsGrowth);
    fflush(stderr);

    if (qEnvironmentVariableIntValue("WINNOW_SOAK_FAST_EXIT") == 1) {
        std::_Exit(0);
    }
    /* Orderly teardown: closeEvent stops the reader/cache/decoder threads, then
       quitting unwinds exec() so the stack-allocated MW destructs and LSan runs
       its leak check against a fully-released app. */
    close();
    qApp->quit();
}

void MW::scrollImageViewStressTest(int ms, int pauseCount, int msPauseDelay)
{
    // ESC to quit
    G::isStressTest = true;
    bool isForward = true;
    int i = 0;
    while (G::isStressTest) {
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

void listChildren(const QObject *parent, int depth = 0) {
    const QObjectList &children = parent->children();
    for (QObject *child : children) {
        QWidget *widget = qobject_cast<QWidget *>(child);
        QString indent(depth * 2, ' '); // Indentation for readability
        if (widget) {
            qDebug() << indent << "Class name:" << widget->metaObject()->className() << ", Object name:" << widget->objectName();
        } else {
            qDebug() << indent << "Class name:" << child->metaObject()->className() << ", Object name:" << child->objectName();
        }
        listChildren(child, depth + 1); // Recursively list children
    }
}

QString MW::readSvgFileToString(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Unable to open file:" << filePath;
        return QString();
    }

    QTextStream in(&file);
    QString svgContent = in.readAll();
    file.close();
    return svgContent;
}

void MW::ingestTest(QWidget* target)
{
    for(int i = 0; i < 10; i++) {
        togglePick();
        sel->next();
    }
    QKeyEvent pressEvent(QEvent::KeyPress, Qt::Key_Q, Qt::NoModifier, "Q");
    QKeyEvent releaseEvent(QEvent::KeyRelease, Qt::Key_Q, Qt::NoModifier, "Q");
    QApplication::sendEvent(target, &pressEvent);
    QApplication::sendEvent(target, &releaseEvent);
}

void MW::mergeProjectFiles() {

    QString destinationDir = "/Users/roryhill/Temp";
    QStringList files;

    // /*
    // FOCUS STACK
    // files << "/Users/roryhill/Projects/Winnow64/Main/mainwindow.h";
    // files << "/Users/roryhill/Projects/Winnow64/Main/mainwindow.cpp";

    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/generateFocusStack.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fs.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fs.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fs.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsalign_types.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsalign.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsalign.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsdepth.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsdepth.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfocus.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfocus.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfusion.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfusion.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfusiondmap.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfusiondmap.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfusionpmax.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfusionpmax.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfusionreassign.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfusionreassign.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfusionwavelet.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsfusionwavelet.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/FSFusionWaveletTemplates.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsloader.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsloader.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsmerge.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsmerge.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsphotometric.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsphotometric.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsutilities.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fsutilities.cpp";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fusionpyr.h";
    // files << "/Users/roryhill/Projects/Winnow64/FocusStack/fusionpyr.cpp";
    //*/

    // /*
    // WINNOW CORE
    files << "/Users/roryhill/Projects/Winnow64/Main/mainwindow.h";
    files << "/Users/roryhill/Projects/Winnow64/Main/mainwindow.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Main/global.h";
    files << "/Users/roryhill/Projects/Winnow64/Main/global.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Main/initialize.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Main/navigate.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Main/settings.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Main/status.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Main/viewmodes.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Datamodel/datamodel.h";
    files << "/Users/roryhill/Projects/Winnow64/Datamodel/datamodel.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Cache/cachedata.h";
    files << "/Users/roryhill/Projects/Winnow64/Cache/cachedata.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Cache/framedecoder.h";
    files << "/Users/roryhill/Projects/Winnow64/Cache/framedecoder.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Cache/imagecache.h";
    files << "/Users/roryhill/Projects/Winnow64/Cache/imagecache.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Cache/imagedecoder.h";
    files << "/Users/roryhill/Projects/Winnow64/Cache/imagedecoder.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Cache/metaread.h";
    files << "/Users/roryhill/Projects/Winnow64/Cache/metaread.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Datamodel/buildfilters.h";
    files << "/Users/roryhill/Projects/Winnow64/Datamodel/buildfilters.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Datamodel/filters.h";
    files << "/Users/roryhill/Projects/Winnow64/Datamodel/filters.cpp";

    files << "/Users/roryhill/Projects/Winnow64/Datamodel/selection.h";
    files << "/Users/roryhill/Projects/Winnow64/Datamodel/selection.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Views/iconview.h";
    files << "/Users/roryhill/Projects/Winnow64/Views/iconview.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Views/iconviewdelegate.h";
    files << "/Users/roryhill/Projects/Winnow64/Views/iconviewdelegate.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Views/imageview.h";
    files << "/Users/roryhill/Projects/Winnow64/Views/imageview.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Views/infostring.h";
    files << "/Users/roryhill/Projects/Winnow64/Views/infostring.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Views/infoview.h";
    files << "/Users/roryhill/Projects/Winnow64/Views/infoview.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Views/tableview.h";
    files << "/Users/roryhill/Projects/Winnow64/Views/tableview.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Views/videoview.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Views/videowidget.cpp";

    files << "/Users/roryhill/Projects/Winnow64/Main/widgetcss.h";
    files << "/Users/roryhill/Projects/Winnow64/Main/widgetcss.cpp";
    files << "/Users/roryhill/Projects/Winnow64/Main/workspaces.cpp";

    files << "/Users/roryhill/Projects/Winnow64/Cache/tiffthumbdecoder.h";
    files << "/Users/roryhill/Projects/Winnow64/Cache/tiffthumbdecoder.cpp";

    files << "/Users/roryhill/Projects/Winnow64/File/fstree.h";
    files << "/Users/roryhill/Projects/Winnow64/File/fstree.cpp";
    files << "/Users/roryhill/Projects/Winnow64/File/bookmarks.h";
    files << "/Users/roryhill/Projects/Winnow64/File/bookmarks.cpp";


    // */

    // 1. Ensure the destination directory exists
    QDir dir(destinationDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 2. Setup the output file path
    QString outputFilePath = dir.filePath("merged_code.txt");
    QFile outputFile(outputFilePath);

    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Could not open output file for writing:" << outputFilePath;
        return;
    }

    QTextStream out(&outputFile);

    // 3. Iterate through each file path in the list
    for (const QString &filePath : files) {
        QFile inputFile(filePath);
        QFileInfo fileInfo(filePath);

        if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&inputFile);

            // Add a clear header for the AI to identify the file
            out << "\n/* =========================================\n";
            out << "   File: " << fileInfo.fileName() << "\n";
            out << "   Path: " << filePath << "\n";
            out << "   ========================================= */\n\n";

            // Stream the content
            out << in.readAll();
            out << "\n\n";

            inputFile.close();
        } else {
            qDebug() << "Warning: Could not open file for reading:" << filePath;
        }
    }

    outputFile.close();
    qDebug() << "Successfully merged" << files.size() << "files into" << outputFilePath;
}

void MW::testNewFileFormat()    // shortcut = "Shift+Ctrl+Alt+F"
{

    QString fusedPath = "/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0238_PMax/fusion/2025-11-01_0238_pmax_FocusStack.tif";
    QFileInfo fi(fusedPath);
    QString destPath = dm->primaryFolderPath() + "/" + fi.fileName();
    qDebug() << destPath;
    QFile::copy(fusedPath, destPath);
    dm->insert(destPath);
    sel->select(destPath);
    qDebug() << "G::allMetadataAttempted =" << G::allMetadataAttempted;
    waitUntilMetadataLoaded(3000);
    setColorClassForRow(dm->currentSfRow, "Red");
}

void MW::test() // shortcut = "Shift+Ctrl+Alt+T"
{
    // Bounce between random bookmarked folders, cycling images in each,
    // until ESC is pressed or something crashes. Prompts for delay/duration.
    // bounceFoldersStressTest();
    developDock->raise();
    developDockVisibleAction->setChecked(true);

}
// Shift Cmd G: /Users/roryhill/Library/Preferences/com.winnow.winnow_101.plist
/*
   Performance
        Big max number of thumbnails
        Turn color management off
        Hide caching progress
*/
