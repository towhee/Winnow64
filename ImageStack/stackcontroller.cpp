#include "stackcontroller.h"
#include <QFileInfo>
#include <QImageReader>

StackController::StackController(QObject *parent)
    : QObject(parent)
{
}

void StackController::stop()
{
    isAborted = true;
    if (workerThread && workerThread->isRunning()) {
        workerThread->requestInterruption();
    }
    // Also interrupt any other workers (focus stage)
    const auto threads = this->findChildren<QThread*>();
    for (QThread* th : threads) if (th->isRunning()) th->requestInterruption();

    emit updateStatus(false, "Focus stack operation aborted.", "StackController::stop");
}

void StackController::test()
{
    emit updateStatus(false, "Focus stack operation test.", "StackController::test");
    qDebug() << "StackController::test";
    daisyChain = false;
    // runAlignment(/*saveAligned=*/true, /*useGpu=*/false);
    // runFocusMaps(alignedPath);
    // runDepthMap(alignedPath);
    // runFusion();
    QString imagePath = "/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0227/output/fused_pmax.png";
    runHaloReduction(imagePath);
}

void StackController::initialize()
{
    if (inputPaths.isEmpty()) return;

    QFileInfo fi(inputPaths.first());
    QString baseName = fi.completeBaseName();

    projectRoot = fi.dir().path();
    projectFolder = projectRoot + "/" + baseName;
    projectDir = QDir(projectFolder);

    QDir dir;
    if (!dir.exists(projectFolder)) {
        dir.mkpath(projectFolder);
        emit updateStatus(false,
                          QString("Created project folder: %1").arg(projectFolder),
                          "StackController::prepareProjectFolders");
    }

    // Define subfolder structure
    alignedPath     = projectFolder + "/aligned";
    masksPath       = projectFolder + "/masks";
    depthPath       = projectFolder + "/depth";
    haloPath        = projectFolder + "/halo";
    outputPath      = projectFolder + "/output";

    QStringList subDirs = { alignedPath, masksPath, depthPath, outputPath };
    for (const QString &sub : subDirs) {
        if (!dir.exists(sub)) dir.mkpath(sub);
    }

    // Cache QDir handles
    alignedDir     = QDir(alignedPath);
    masksDir       = QDir(masksPath);
    depthDir       = QDir(depthPath);
    haloDir        = QDir(haloPath);
    outputDir      = QDir(outputPath);

    // Files
    depthMapPath    = depthPath + "/depth_index.png";
    // depthMapPath    = depthPath + "/depth_idx_from_zerene.png.png";

    // Halo parameters
    haloStrength = 0.7;
    haloRadius = 5;

    // Connections
    connect(this, &StackController::finishedAlign,
            this, &StackController::runFocusMaps);

    connect(this, &StackController::finishedFocus,
            this, &StackController::runDepthMap);

    connect(this, &StackController::finishedDepthMap,
            this, &StackController::runFusion);

    // connect(this, &StackController::finishedHaloReduction,
    //         this, &StackController::runFusion);

    emit updateStatus(false,
                      QString("Project structure ready in %1").arg(projectFolder),
                      "StackController::prepareProjectFolders");
}

void StackController::loadInputImages(const QStringList &paths, const QList<QImage*> &images)
{
    inputPaths = paths;
    inputImages = images;

    qDebug() << "StackController::loadInputImages" << inputImages.count();

    // if (inputPaths.isEmpty()) return;

    // QMap<int, QImage> stack;

    // int idx = 0;
    // for (const QString &filePath : inputPaths) {
    //     QImage img(filePath);
    //     if (img.isNull()) {
    //         emit updateStatus(false, "Failed to load " + filePath, "StackController::loadInputImages");
    //         continue;
    //     }
    //     stack.insert(idx++, img);
    // }

    initialize();

    emit updateStatus(false,
          QString("Loaded %1 images for stacking.").arg(images.size()),
          "StackController::loadInputImages");
}

bool StackController::runAlignment(bool saveAligned, bool useGpu)
{
    if (inputImages.isEmpty()) {
        emit updateStatus(false, "No input images to align", "StackController::runAlignment");
        return false;
    }

    emit updateStatus(false, "Starting threaded alignment...", "StackController::runAlignment");

    // --- Create a dedicated worker thread and aligner ----------------------
    workerThread = new QThread(this);
    auto *aligner = new StackAligner();
    aligner->moveToThread(workerThread);

    // --- Pass configuration ------------------------------------------------
    aligner->setSearchRadius(10);
    aligner->setDownsample(2);
    aligner->setRotationStep(0.0f);
    aligner->setUseEdgeMaskWeighting(true);
    aligner->setUseGpu(useGpu);

    // --- Forward signals to StackController (queued, cross-thread) ---------
    connect(aligner, &StackAligner::updateStatus,
            this, &StackController::updateStatus,
            Qt::QueuedConnection);

    connect(aligner, &StackAligner::progress,
            this, &StackController::progress,
            Qt::QueuedConnection);

    // --- Run alignment inside the worker thread ----------------------------
    connect(workerThread, &QThread::started, aligner, [=]() {
        QElapsedTimer timer;
        timer.start();

        QList<QImage> aligned = aligner->alignECC(inputImages);  // OpenCV
        // QList<QImage> aligned = aligner->align(inputImages);

        if (isAborted) {
            emit updateStatus(false, "Alignment aborted.", "StackController::runAlignment");
        }
        else {
            emit updateStatus(false,
                              QString("Alignment complete in %1 s")
                                  .arg(timer.elapsed() / 1000.0, 0, 'f', 2),
                              "StackController::runAlignment");

            // --- Save aligned results if requested --------------------------
            if (saveAligned && !aligned.isEmpty()) {
                int idx = 0;
                for (const QImage &img : aligned) {
                    QString outPath = alignedDir.filePath(
                        QString("aligned_%1.jpg").arg(idx++, 3, 10, QChar('0')));
                    if (!img.save(outPath, "JPEG", 95)) {
                        emit updateStatus(false,
                                          QString("Failed to save %1").arg(outPath),
                                          "StackController::runAlignment");
                    }
                }
                emit updateStatus(false,
                                  "Aligned images saved to " + alignedDir.path(),
                                  "StackController::runAlignment");
            }
        }

        // --- Signal completion ---------------------------------------------
        if (daisyChain) emit finishedAlign(projectFolder);

        // Cleanly exit thread
        workerThread->quit();
    });

    // --- Cleanup when the thread finishes ----------------------------------
    connect(workerThread, &QThread::finished, aligner, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    // --- Start alignment ---------------------------------------------------
    isAborted = false;
    workerThread->start();

    // --- Connect pipeline continuation -----------------------------------------
    connect(this, &StackController::finishedAlign,
            this, &StackController::runFocusMaps,
            Qt::QueuedConnection);

    return true;
}

bool StackController::runFocusMaps(const QString &alignedFolderPath)
{
    const QString src = "StackController::runFocusMaps";
    qDebug() << src << "starting";

    const QString folder = alignedFolderPath.isEmpty() ? alignedPath : alignedFolderPath;
    if (!QDir(folder).exists()) {
        emit updateStatus(false, QString("Aligned folder not found: %1").arg(folder), src);
        return false;
    }

    const QString pf = projectFolder.isEmpty() ? projectRoot : projectFolder;
    if (pf.isEmpty()) {
        emit updateStatus(false, "No project folder set for focus-map stage.", src);
        return false;
    }

    emit updateStatus(false,
                      QString("Preparing focus-map computation for: %1").arg(folder),
                      src);

    // --- Worker thread -----------------------------------------------------
    QThread *worker = new QThread(this);
    auto *fm = new FocusMeasure();
    fm->moveToThread(worker);

    // Signal forwarding
    connect(fm, &FocusMeasure::updateStatus, this, &StackController::updateStatus, Qt::QueuedConnection);
    connect(fm, &FocusMeasure::progress,     this, &StackController::progress,     Qt::QueuedConnection);

    connect(worker, &QThread::finished, fm,     &QObject::deleteLater);
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);

    // --- Prepare image stack (in main thread) ------------------------------
    QDir alignedDir(folder);
    QStringList filters{ "*.jpg", "*.jpeg", "*.png", "*.tif", "*.tiff", "*.bmp" };
    QStringList files = alignedDir.entryList(filters, QDir::Files, QDir::Name);

    qDebug() << "StackController::runFocusMaps  files.count() =" << files.count();

    if (files.isEmpty()) {
        emit updateStatus(false, "No aligned images found.", src);
        return false;
    }

    QMap<int, QImage> stack;
    QImageReader reader;
    reader.setAutoTransform(false);

    for (int i = 0; i < files.size(); ++i) {
        const QString fpath = alignedDir.filePath(files[i]);
        reader.setFileName(fpath);
        QImage img = reader.read();
        if (img.isNull()) continue;
        if (img.format() != QImage::Format_RGB32 &&
            img.format() != QImage::Format_ARGB32 &&
            img.format() != QImage::Format_RGB888)
            img = img.convertToFormat(QImage::Format_RGB32);
        stack.insert(i, std::move(img));
    }

    if (stack.isEmpty()) {
        emit updateStatus(false, "No readable images for focus-map computation.", src);
        return false;
    }

    // Configure before starting
    fm->setMethod(FocusMeasure::Tenengrad);
    fm->setDownsample(1);
    fm->setSaveResults(true);
    fm->setOutputFolder(masksPath);

    // --- Connect start signal properly to FocusMeasure ---------------------
    connect(worker, &QThread::started, fm, [fm, stack, this, pf, src]() {
            QElapsedTimer t; t.start();
            fm->computeFocusMaps(stack);  // now runs in fm's own thread
            emit updateStatus(false,
                              QString("Focus-map computation complete in %1 s")
                                  .arg(t.elapsed() / 1000.0, 0, 'f', 2),
                              src);
            if (daisyChain) emit finishedFocus(pf);
            QThread::currentThread()->quit();
        }, Qt::QueuedConnection);  // <--- key line

    worker->start();
    return true;
}

bool StackController::runDepthMap(const QString &alignedFolderPath)
{
    QString src = "StackController::runDepthMap";
    emit updateStatus(false, "Launching threaded depth-map generation...", src);

    QThread *thread = new QThread;
    DepthMap *dm = new DepthMap();

    dm->moveToThread(thread);

    // --- Real-time status forwarding ----------------------------------------
    connect(dm, &DepthMap::updateStatus,
            this, &StackController::updateStatus,
            Qt::QueuedConnection);

    // --- When the thread starts, invoke generate() *in that thread* ---------
    connect(thread, &QThread::started, dm, [this, dm, alignedFolderPath, src]() {
        emit updateStatus(false, "Generating depth map...", src);
        bool ok = dm->generate(alignedFolderPath, true);
        emit updateStatus(false,
                          ok ? "Depth map complete." : "Depth map failed.",
                          src);
        if (daisyChain) emit finishedDepthMap(ok);
    });

    // --- Cleanup after finishing --------------------------------------------
    connect(this, &StackController::finishedDepthMap, thread, [dm, thread]() {
            thread->quit();
            thread->wait();
            dm->deleteLater();
            thread->deleteLater();
        }, Qt::QueuedConnection);

    thread->start();
    return true;
}

bool StackController::runFusion()
{
    QString src = "StackController::runFusion";
    emit updateStatus(false, "Launching threaded fusion stage...", src);

    // --- Sanity checks -------------------------------------------------------
    if (alignedPath.isEmpty() || depthPath.isEmpty()) {
        emit updateStatus(false, "Aligned or depth path not set.", src);
        return false;
    }

    QString mDepthMapPath = depthMapPath;
    if (!QFile::exists(depthMapPath)) {
        emit updateStatus(false,
                          QString("No depth map found in %1").arg(depthPath),
                          src);
        return false;
    }

    QDir dir;
    if (!dir.exists(outputPath))
        dir.mkpath(outputPath);

    // --- Create and move worker to thread -----------------------------------
    QThread *thread = new QThread;
    StackFusion *fusion = new StackFusion();
    fusion->moveToThread(thread);

    // --- Real-time updates (queued connections ensure main-thread delivery) --
    connect(fusion, &StackFusion::updateStatus,
            this, &StackController::updateStatus,
            Qt::QueuedConnection);
    connect(fusion, &StackFusion::progress,
            this, &StackController::progress,
            Qt::QueuedConnection);

    // --- Start threaded work -------------------------------------------------
    connect(thread, &QThread::started, fusion, [this, fusion, src, mDepthMapPath]()
    {
        emit updateStatus(false, "Starting fusion stage...", src);

        // Do not run naive for now
        // bool ok1 = fusion->fuse(StackFusion::Naive,
        //                         alignedPath,
        //                         depthMapPath,
        //                         outputPath);
        bool ok1 = true;

        bool ok2 = fusion->fuse(StackFusion::PMax,
                                alignedPath,
                                depthMapPath,
                                outputPath);

        bool success = ok1 && ok2;

        emit updateStatus(false,
                          success ? "Fusion stage complete." : "Fusion stage failed.",
                          src);
        emit finishedFusion(success ? outputPath : QString());
    });

    // --- Cleanup after completion -------------------------------------------
    connect(this, &StackController::finishedFusion, thread, [fusion, thread]() {
            thread->quit();
            thread->wait();
            fusion->deleteLater();
            thread->deleteLater();
        }, Qt::QueuedConnection);

    thread->start();
    return true;
}

bool StackController::runHaloReduction(const QString &imagePath)
{
    const QString src = "StackController::runHaloReduction";
    emit updateStatus(false, QString("Launching threaded halo reduction..."), src);

    if (imagePath.isEmpty() || !QFile::exists(imagePath)) {
        emit updateStatus(false, QString("Input image not found: %1").arg(imagePath), src);
        return false;
    }

    // Ensure halo output directory exists
    QDir dir;
    if (!dir.exists(haloPath))
        dir.mkpath(haloPath);

    // --- Thread setup --------------------------------------------------------
    QThread *thread = new QThread;
    FocusHalo *halo = new FocusHalo();
    halo->setOutputDir(haloDir);
    halo->setStrength(haloStrength);
    halo->setRadius(haloRadius);
    halo->moveToThread(thread);

    // Connect signals for progress/status
    connect(halo, &FocusHalo::updateStatus, this, &StackController::updateStatus);

    // When thread starts, begin halo processing
    connect(thread, &QThread::started, [this, halo, imagePath, src]() {
        emit updateStatus(false, QString("Running halo reduction on %1").arg(imagePath), src);
        bool ok = halo->removeHalos(imagePath, "halo_reduced.png");
        emit updateStatus(false,
                          ok ? "Halo reduction complete." : "Halo reduction failed.",
                          src);
        emit finishedHaloReduction(QString("Halo reduction done."));
        halo->deleteLater();
        QThread::currentThread()->quit();
    });

    // Cleanup after thread finishes
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
    return true;
}
