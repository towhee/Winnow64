#include "Main/mainwindow.h"
#include "FocusStack/fs.h"
// #include "FocusStack/Pipeline/PipelineBase.h"
// #include "FocusStack/Pipeline/pipelinepmax.h"


/*
Winnow64/FocusStack/

    Petteri/
        (untouched original sources)

    PetteriModular/
        Core/
        Tasks/
        IO/
        Wrappers/
        Namespace: FStack

    Contracts/
        IAlign.h
        IFocus.h
        IDepth.h
        IFusion.h

    Stages/
        Align/
            PetteriAlign.h/.cpp
            ECCAlign.h/.cpp
            NoOpAlign.h/.cpp
        Fusion/
            PetteriPMaxFusion.h/.cpp
            WeightedPMax.h/.cpp
            DepthAwareFusion.h/.cpp
        Depth/
            PetteriDepth.h/.cpp
            ContrastDepth.h/.cpp
        Focus/
            PetteriFocus.h/.cpp
            GradientFocus.h/.cpp

    Pipeline/
        PipelineBase.h/.cpp
        PipelinePMax.h/.cpp
        PipelineWavePMax.h/.cpp
        PipelineDepthAware.h/.cpp
        PipelineLegacyPetteri.h/.cpp

    Mask/
        maskoptions.h/.cpp
        maskgenerator.h/.cpp
        maskrefiner.h/.cpp
        maskassessor.h/.cpp

    Utils/
        pyramid.h/.cpp
        blend.h/.cpp
        filehelpers.h/.cpp

    Cache/
        PyramidCache.h/.cpp
        ImageCache.h/.cpp
*/

void MW::generateFocusStackFromSelection()
{

    QString src = "MW::generateFocusStackFromSelection";
    if (G::isLogger) G::log(src);

    QStringList paths;
    if (!dm->getSelection(paths) || paths.isEmpty()) {
        QString msg = "No images selected for focus stacking.";
        updateStatus(false, msg, src);
        return;
    }

    QString method = "FusionPMaxBasic";
    // QString method = "LegacyPetteri";
    generateFocusStack(paths, method, src);
}

void MW::generateFocusStack(const QStringList paths,
                            const QString method,
                            const QString source)
{
    if (G::isLogger) G::log("MW::generateFocusStack", "paths " + method);
    QString srcFun = "MW::generateFocusStack";

    bool isLocal = (source == "MW::generateFocusStackFromSelection");

    // Options
    // clean (send all project folders to the trash)
    bool isClean = false;       // send all project folders to the trash
    bool isRedoAlign = true;    //

    // Source images folder (used after pipeline finishes)
    QFileInfo info(paths.first());
    const QString srcFolder = info.absolutePath();

    // --------------------------------------------------------------------
    // Create worker thread + FS pipeline object
    // --------------------------------------------------------------------
    QThread *thread = new QThread(this);
    FS *pipeline = new FS();           // new unified pipeline
    pipeline->moveToThread(thread);

    // Set input + project root *before* thread runs
    QString projectRoot = srcFolder + "/" + info.completeBaseName() + "_FS";
    pipeline->setProjectRoot(projectRoot);
    pipeline->setInput(paths);

    FS::Options opt;
    opt.enableAlign         = true;
    opt.enableFocusMaps     = false;   // until wired
    opt.enableDepthMap      = false;
    opt.enableFusion        = false;
    opt.overwriteExisting   = true;
    pipeline->setOptions(opt);

    // When the thread starts → run the FS pipeline
    connect(thread, &QThread::started, pipeline, [pipeline, thread]()
            {
                pipeline->run();     // runs synchronously inside worker thread
                QMetaObject::invokeMethod(thread, "quit", Qt::QueuedConnection);
            });

    // Status / progress → UI
    connect(pipeline, &FS::updateStatus,
            this, &MW::updateStatus,
            Qt::QueuedConnection);

    connect(pipeline, &FS::progress, this, [this](int current, int total)
        {
            this->cacheProgressBar->updateUpperProgress(current, total, Qt::darkYellow);
        },
        Qt::QueuedConnection);

    // cleanup when finished
    connect(thread, &QThread::finished, pipeline, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread,   &QObject::deleteLater);

    // When FS finishes (thread quits)
    // We detect success by checking filesystem or pipeline signals (later)
    connect(thread, &QThread::finished, this, [=]()
    {
        // THIS IS NOT RUNNING AFTER FS::run() returns.
        qDebug() << "FINISHED!!!!!!!!";
        // Clear progress
        cacheProgressBar->clearUpperProgress();

        // Example: pick the last aligned color image as “fusedPath”
        // (temporary until focus/depth/fusion implemented)
        QString alignedDir = projectRoot + "/align";
        QDir ad(alignedDir);
        QStringList files = ad.entryList(QStringList() << "*.png",
                                         QDir::Files,
                                         QDir::Name);

        QString fusedPath;
        if (!files.isEmpty())
            fusedPath = alignedDir + "/" + files.last();   // temporarily treat as result

        if (!fusedPath.isEmpty())
        {
            // Copy metadata from first source using your existing logic
            ExifTool et;
            et.setOverWrite(true);
            et.copyAll(paths.first(), fusedPath);
            et.close();

            // Copy into source folder
            QFileInfo fi(fusedPath);
            QString destPath = srcFolder + "/" + fi.fileName();
            QFile::copy(fusedPath, destPath);

            dm->insert(destPath);
            if (isLocal)
                sel->select(destPath);
            else
                folderAndFileSelectionChange(destPath, srcFun);

            waitUntilMetadataLoaded(5000, srcFun);

            setColorClassForRow(dm->currentSfRow, "Red");
            embedThumbnails();
        }

        // thread->quit();
        // pipeline->deleteLater();
    });

    // Start
    thread->start();

    /*
    // --- Create Thread + FSRunner -------------------------------------
    QThread *thread = new QThread(this);
    FSRunner *pipeline = new FSRunner(nullptr);   // temporary test runner
    pipeline->moveToThread(thread);

    // Build output paths for aligned images (minimal testing only)
    std::vector<QString> alignedColor;
    std::vector<QString> alignedGray;
    alignedColor.reserve(paths.size());
    alignedGray.reserve(paths.size());

    for (const QString &p : paths)
    {
        QFileInfo fi(p);
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix();

        QString colorOut = fi.absolutePath() + "/aligned_" + base + "." + ext;
        QString grayOut  = fi.absolutePath() + "/aligned_gray_" + base + "." + ext;

        alignedColor.push_back(colorOut);
        alignedGray.push_back(grayOut);
    }

    // Start pipeline when thread begins
    connect(thread, &QThread::started,
            pipeline,
            [pipeline, paths, alignedColor, alignedGray]()
            {
                pipeline->runAlign(paths, alignedColor, alignedGray, true);
                QMetaObject::invokeMethod(pipeline->thread(), "quit", Qt::QueuedConnection);
            });

    // Cleanup
    connect(thread, &QThread::finished, pipeline, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
    */

    /*
    // Create Thread + Pipeline
    QThread *thread = new QThread(this);
    PipelinePMax *pipeline = new PipelinePMax();   // no parent; lives in thread
    pipeline->moveToThread(thread);

    // Start pipeline when thread begins
    connect(thread, &QThread::started, pipeline, [pipeline, paths, isRedoAlign]() {
        pipeline->setInput(paths, isRedoAlign);
        pipeline->run();      // executes inside worker thread
    });

    // Pipeline status → UI
    connect(pipeline, &PipelinePMax::updateStatus,
            this, &MW::updateStatus, Qt::QueuedConnection);

    connect(pipeline, &PipelinePMax::progress,
            this, [this](int current, int total)
            {
                this->cacheProgressBar->updateUpperProgress(current, total, Qt::darkYellow);
            }, Qt::QueuedConnection);

    // Pipeline finished → Handle output
    connect(pipeline, &PipelinePMax::finished,
        this, [=](bool ok, const QString &outputFolder)
        {
            QString msg = ok
                ? QString("PMax pipeline finished.\nOutput folder: %1").arg(outputFolder)
                : QString("PMax pipeline failed.");
            updateStatus(!ok, msg, srcFun);

            if (ok)
            {
                // The resulting fused image file
                QString fusedPath = pipeline->fusionOutputPath();

                // Copy metadata from first source image file
                ExifTool et;
                et.setOverWrite(true);
                et.copyAll(paths.first(), fusedPath);
                et.close();

                // Copy fused to source images folder
                QFileInfo fi(fusedPath);
                QString destPath = srcFolder + "/" + fi.fileName();
                QFile::copy(fusedPath, destPath);

                // Insert into datamodel
                dm->insert(destPath);

                if (isClean) {
                    //  Delete source images
                    for (const QString &p : paths)
                        QFile(p).moveToTrash();

                }

                // Lightroom etc
                if (isLocal)
                {
                    sel->select(destPath);
                }
                else {

                    folderAndFileSelectionChange(fusedPath, srcFun);
                 }

                waitUntilMetadataLoaded(5000, srcFun);

                // set color and update filters, image counts
                setColorClassForRow(dm->currentSfRow, "Red");

                embedThumbnails();      // on the current selection

                // Update source folder image counts, filters ...
                // refresh();
            }

            // Cleanup after pipeline ends
            if (isClean) pipeline->clean();

            thread->quit();
            thread->wait();
            pipeline->deleteLater();
            thread->deleteLater();
        },
        Qt::QueuedConnection);

    // Start the worker thread
    thread->start();
*/
}


// {
//     if (G::isLogger) G::log("MW::generateFocusStack", "paths " + method);
//     QString srcFun = "MW::generateFocusStack";

//     bool isLocal = (source == "MW::generateFocusStackFromSelection");

//     // Thread + Pipeline object
//     QThread *thread = new QThread;
//     PipelinePMax *pipeline = new PipelinePMax();
//     pipeline->moveToThread(thread);

//     // Start pipeline when thread starts
//     connect(thread, &QThread::started, this, [=]() {
//         pipeline->setInput(paths);
//         pipeline->run();       // synchronous inside worker thread
//     });

//     // Pipeline status → MW status
//     connect(pipeline, &PipelinePMax::updateStatus,
//             this, &MW::updateStatus,
//             Qt::QueuedConnection);

//     connect(pipeline, &PipelinePMax::progress,
//         this, [this](int current, int total) {
//             this->cacheProgressBar->updateUpperProgress(current, total, Qt::blue);
//         }, Qt::QueuedConnection);

//     // Pipeline finished → handle result
//     connect(pipeline, &PipelinePMax::finished,
//             this,
//             [=](bool ok, const QString &outputFolder)
//             {
//                 QString msg = ok
//                                   ? QString("PMax pipeline finished.\nOutput folder: %1")
//                                         .arg(outputFolder)
//                                   : QString("PMax pipeline failed.");

//                 updateStatus(!ok, msg, srcFun);

//                 if (ok) {
//                     // Fusion output file is in: pipeline->fusionOutputPath()
//                     QString fused = pipeline->fusionOutputPath();

//                     dm->insert(fused);

//                     if (!isLocal) {
//                         // Delete originals
//                         for (const QString &p : paths)
//                             QFile(p).moveToTrash();

//                         folderAndFileSelectionChange(fused, srcFun);

//                         // Wait for metadata (3000ms max)
//                         const int timeoutMs = 3000;
//                         QElapsedTimer t;
//                         t.start();
//                         while (!G::allMetadataLoaded &&
//                                t.elapsed() < timeoutMs) {
//                             QCoreApplication::processEvents(
//                                 QEventLoop::AllEvents, 50);
//                             QThread::msleep(50);
//                         }
//                     }

//                     sel->select(fused);
//                 }

//                 // Cleanup thread safely
//                 thread->quit();
//                 thread->wait();
//                 pipeline->deleteLater();
//                 thread->deleteLater();
//             });

//     // Start the thread
//     thread->start();
// }
//*/

/* PIPELINE: Petteri PMax Original (Default)
    {
        qDebug() << "Using legacy Petteri FocusStackWorker";
        QThread *thread = new QThread;
        FocusStackWorker *worker = new FocusStackWorker(paths);
        worker->moveToThread(thread);

        connect(worker, &FocusStackWorker::updateStatus,
                this, &MW::updateStatus, Qt::QueuedConnection);

        connect(worker, &FocusStackWorker::updateProgress,
                cacheProgressBar, &ProgressBar::updateUpperProgress, Qt::QueuedConnection);

        connect(worker, &FocusStackWorker::clearProgress,
                cacheProgressBar, &ProgressBar::clearUpperProgress, Qt::QueuedConnection);

        connect(thread, &QThread::started, worker, &FocusStackWorker::process);

        connect(worker, &FocusStackWorker::finished, this,
                [=](bool ok, const QString &output, const QString &depthmap)
                {
                    QString msg = ok
                                      ? QString("FocusStack finished successfully. Output: %1 DepthMap: %2")
                                            .arg(output, depthmap)
                                      : "FocusStack failed.";

                    updateStatus(false, msg, srcFun);

                    if (ok) {
                        dm->insert(output);
                        if (!isLocal) {
                            for (const QString &path : paths) {
                                QFile(path).moveToTrash();
                            }

                            folderAndFileSelectionChange(output,"MW::generateFocusStack");

                            // wait for folder to be fully loaded
                            const int timeoutMs = 3000;
                            QElapsedTimer timer;
                            timer.start();
                            while (!G::allMetadataLoaded && timer.elapsed() < timeoutMs) {
                                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                                QThread::msleep(50);
                            }
                        }
                        sel->select(output);
                        QTimer::singleShot(50, this, [=]() {
                            setColorClassForRow(dm->currentSfRow, "Red");
                        });
                    }

                    // cleanup
                    thread->quit();
                    thread->wait();
                    worker->deleteLater();
                    thread->deleteLater();
                });

        QString msg = "Starting FocusStack background task (%1 images)...";
        QString src = "MW::generateFocusStack";
        updateStatus(false, msg, src);

        thread->start();
    }
    */
// }
