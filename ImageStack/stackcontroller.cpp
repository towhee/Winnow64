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
    emit updateStatus(false, "Focus stack operation aborted.", "StackController::stop");
}

void StackController::test()
{
    emit updateStatus(false, "Focus stack operation test.", "StackController::stop");
    runAlignment(false);
}

void StackController::prepareProjectFolders()
{
    if (inputPaths.isEmpty()) return;

    QFileInfo fi(inputPaths.first());
    QString baseName = fi.completeBaseName();

    projectRoot = fi.dir().path();
    projectFolder = projectRoot + "/" + baseName;

    QDir dir;
    if (!dir.exists(projectFolder)) {
        dir.mkpath(projectFolder);
        emit updateStatus(false,
                          QString("Created project folder: %1").arg(projectFolder),
                          "StackController::prepareProjectFolders");
    }

    // subfolders for intermediate results
    QStringList subDirs = { "aligned", "depth", "masks", "output" };
    for (const QString &sub : subDirs) {
        QString path = projectFolder + "/" + sub;
        if (!dir.exists(path)) dir.mkpath(path);
    }

    alignedFolder = QDir(projectFolder + "/aligned");
}

void StackController::loadInputImages(const QStringList &paths, const QList<QImage*> &images)
{
    inputPaths = paths;
    inputImages = images;

    prepareProjectFolders();

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

    // Prepare folders (safe to call multiple times)
    prepareProjectFolders();
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

        QList<QImage> aligned = aligner->align(inputImages);

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
                    QString outPath = alignedFolder.filePath(
                        QString("aligned_%1.jpg").arg(idx++, 3, 10, QChar('0')));
                    if (!img.save(outPath, "JPEG", 95)) {
                        emit updateStatus(false,
                                          QString("Failed to save %1").arg(outPath),
                                          "StackController::runAlignment");
                    }
                }
                emit updateStatus(false,
                                  "Aligned images saved to " + alignedFolder.path(),
                                  "StackController::runAlignment");
            }
        }

        // --- Signal completion ---------------------------------------------
        emit finished(projectFolder);

        // Cleanly exit thread
        workerThread->quit();
    });

    // --- Cleanup when the thread finishes ----------------------------------
    connect(workerThread, &QThread::finished, aligner, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    // --- Start alignment ---------------------------------------------------
    isAborted = false;
    workerThread->start();

    return true;
}
