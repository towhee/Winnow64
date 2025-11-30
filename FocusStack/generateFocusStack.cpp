#include "Main/mainwindow.h"
#include "FocusStack/Pipeline/pipelinepmax.h"


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
    bool isClean = false;
    bool isRedo = false;

    // Source images folder (used after pipeline finishes)
    QFileInfo info(paths.first());
    const QString srcFolder = info.absolutePath();

    // Create Thread + Pipeline
    QThread *thread = new QThread(this);
    PipelinePMax *pipeline = new PipelinePMax();   // no parent; lives in thread
    pipeline->moveToThread(thread);

    // Start pipeline when thread begins
    connect(thread, &QThread::started, pipeline, [pipeline, paths, isRedo]() {
        pipeline->setInput(paths, isRedo);
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

    //   Pipeline finished → Handle output
    connect(pipeline, &PipelinePMax::finished,
            this,
            [=](bool ok, const QString &outputFolder)
            {
                QString msg = ok
                                  ? QString("PMax pipeline finished.\nOutput folder: %1").arg(outputFolder)
                                  : QString("PMax pipeline failed.");

                updateStatus(!ok, msg, srcFun);

                if (ok)
                {
                    // The resulting fused image file
                    QString fused = pipeline->fusionOutputPath();

                    // Copy metadata from first source image file
                    ExifTool et;
                    et.setOverWrite(true);
                    et.copyAll(paths.first(), fused);
                    et.close();

                    // Insert into datamodel
                    dm->insert(fused);

                    // Copy fused to source images folder
                    QFileInfo fi(fused);
                    QString destPath = srcFolder + "/" + fi.fileName();
                    QFile::copy(fused, destPath);

                    // Update source folder image counts, filters ...
                    // refresh();

                    if (!isLocal)
                    {
                        //  Delete source images
                        for (const QString &p : paths)
                            QFile(p).moveToTrash();

                        folderAndFileSelectionChange(fused, srcFun);

                        // Wait for metadata to finish loading (max 3000ms)
                        const int timeoutMs = 3000;
                        QElapsedTimer t;
                        t.start();

                        while (!G::allMetadataLoaded && t.elapsed() < timeoutMs)
                        {
                            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                            QThread::msleep(50);
                        }
                    }

                    // Select fused image in UI
                    sel->select(fused);
                    refresh();
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
