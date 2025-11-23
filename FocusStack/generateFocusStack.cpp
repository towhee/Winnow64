#include "Main/mainwindow.h"
// #include "FocusStack/Fusion/fusionpmaxbasic.h"


/*
Winnow64/FocusStack/

    Petteri/
        (untouched original sources)

    PetteriModular/
        Core/
        Tasks/
        IO/
        Wrappers/
        Namespace: petteri_mod

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
            PetteriDepthMap.h/.cpp
            ContrastDepthMap.h/.cpp
        Focus/
            PetteriFocusMaps.h/.cpp
            GradientFocusMaps.h/.cpp

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
    /*
    Folder structure:

        Source images
            First source image base name
                Method (Project path)
                    Aligned
                    Focus
                    Depth
                    Fusion

    Pipeline:

        Method
            Method::Fuse
                PetteriAdapterAlign
                    Petteri set options
                    Execute Petteri pipeline
                PetteriAdapterFocusMap
                PetteriAdapterDepthMap
                PetteriAdapterFusion
*/
    if (G::isLogger) G::log("MW::generateFocusStack", "paths " + method);
    QString srcFun = "MW::generateFocusStack";

    bool isLocal = (source == "MW::generateFocusStackFromSelection");

    QFileInfo srcInfo(paths.last());
    QString srcFolder = srcInfo.dir().absolutePath();
    QString base = srcInfo.baseName();


    // PIPELINE: FusionPMaxBasic
    /*
    if (method == "FusionPMaxBasic")
    {
        qDebug() << "Using FusionPMaxBasic pipeline";

        // FusionPMaxBasic *fusion = new FusionPMaxBasic();

        // Run in background thread (similar structure to FocusStackWorker)
        QThread *thread = new QThread;
        fusion->moveToThread(thread);

        connect(thread, &QThread::started, this, [=]() {

            QString ext = ".png";
            QString projDir = srcFolder + "/" + base + "/" + method;
            QString depthDir = projDir + "/Depth";
            QString depthName = "Depth_" + method + "_" + base + ext;
            QString depthMapPath = depthDir + "/" + depthName;
            QString fusedDir = projDir + "/Fused";
            QString fusedName = "Fused_" + method + "_" + base + ext;
            QString fusedImagePath = fusedDir + "/" + fusedName;

            qDebug() << srcFun << "projDir        =" << projDir;

            FusionPMaxBasic::Options options;
            options.align = false;
            options.alignFuse = true;
            options.focusMeasure = false;
            options.depthMap = false;
            options.fuse = false;

            bool ok = fusion->fuse(paths,
                                   projDir,
                                   options);

            // Emit manually since FusionBase doesn't do it
            emit fusionFinished(ok, fusedImagePath, depthMapPath);
        });

        connect(fusion, &FusionBase::updateStatus,
                this, &MW::updateStatus, Qt::QueuedConnection);

        // When done
        connect(this, &MW::fusionFinished, this,
                [=](bool ok, const QString &output, const QString &depthmap)
                {
                    QString success = QString("FusionPMaxBasic finished. Output: %1 Depth: %2").arg(output, depthmap);
                    QString failure = "FusionPMaxBasic failed.";
                    QString msg = ok ? success : failure;

                    updateStatus(false, msg, "MW::generateFocusStack");

                    if (ok) {
                        dm->insert(output);

                        if (!isLocal) {
                            for (const QString &path : paths)
                                QFile(path).moveToTrash();

                            folderAndFileSelectionChange(output,
                                                         "MW::generateFocusStack");

                            // Wait for metadata to finish loading OR timeout
                            const int timeoutMs = 3000;
                            QElapsedTimer timer;
                            timer.start();
                            while (!G::allMetadataLoaded && timer.elapsed() < timeoutMs) {
                                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                                QThread::msleep(50);
                            }
                        }

                        sel->select(output);
                    }

                    // Cleanup
                    thread->quit();
                    thread->wait();
                    fusion->deleteLater();
                    thread->deleteLater();
                });
        // Start processing
        thread->start();
        return;
    }


    // PIPELINE: Petteri PMax Original (Default)
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
}
