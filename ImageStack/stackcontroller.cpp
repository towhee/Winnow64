#include "stackcontroller.h"
#include <QFileInfo>
#include <QImageReader>

StackController::StackController(QObject *parent)
    : QObject(parent)
{
}

void StackController::setSourceFolder(const QString &path)
{
    srcFolder.setPath(path);
}

void StackController::setInputFiles(const QStringList &filePaths)
{
    inputFiles = filePaths;
    if (inputFiles.isEmpty()) {
        emit updateStatus(false, "No input files provided", "StackController::setInputFiles");
        return;
    }

    QFileInfo firstInfo(inputFiles.first());
    projectBaseName = firstInfo.completeBaseName();

    QString projPath = srcFolder.filePath(QString("Stack_%1").arg(projectBaseName));
    projectFolder.setPath(projPath);
    prepareProjectFolders();
}

bool StackController::prepareProjectFolders()
{
    if (!srcFolder.exists()) {
        emit updateStatus(false, "Source folder does not exist", "StackController::prepareProjectFolders");
        return false;
    }

    if (!projectFolder.exists()) {
        if (!srcFolder.mkpath(projectFolder.dirName())) {
            emit updateStatus(false, "Failed to create project folder", "StackController::prepareProjectFolders");
            return false;
        }
    }

    QStringList subDirs = {"aligned", "focus_maps", "depth_maps", "fused"};
    for (const QString &name : subDirs) {
        QString subPath = projectFolder.filePath(name);
        QDir subDir(subPath);
        if (!subDir.exists())
            projectFolder.mkpath(name);
    }

    alignedFolder.setPath(projectFolder.filePath("aligned"));
    focusMapFolder.setPath(projectFolder.filePath("focus_maps"));
    depthMapFolder.setPath(projectFolder.filePath("depth_maps"));
    fusedFolder.setPath(projectFolder.filePath("fused"));

    emit updateStatus(false, "Project folder ready: " + projectFolder.path(), "StackController::prepareProjectFolders");
    return true;
}

QMap<int, QImage> StackController::loadInputImages()
{
    QMap<int, QImage> stack;
    if (inputFiles.isEmpty()) return stack;

    int idx = 0;
    for (const QString &filePath : inputFiles) {
        QImage img(filePath);
        if (img.isNull()) {
            emit updateStatus(false, "Failed to load " + filePath, "StackController::loadInputImages");
            continue;
        }
        stack.insert(idx++, img);
    }

    emit updateStatus(false, QString("Loaded %1 selected images").arg(stack.size()), "StackController::loadInputImages");
    return stack;
}

bool StackController::runAlignment(bool saveAligned)
{
    if (inputFiles.isEmpty()) {
        emit updateStatus(false, "No input images to align", "StackController::runAlignment");
        return false;
    }

    prepareProjectFolders();
    emit updateStatus(false, "Starting alignment...", "StackController::runAlignment");

    QElapsedTimer timer;
    timer.start();

    QMap<int, QImage> stack = loadInputImages();
    if (stack.isEmpty()) return false;

    // --- StackAligner setup ---
    StackAligner *aligner = new StackAligner(this);
    aligner->setSearchRadius(10);
    aligner->setDownsample(2);
    aligner->setRotationStep(0.0f);
    aligner->setUseEdgeMaskWeighting(true);

    connect(aligner, &StackAligner::progress, this, [this](QString msg, int cur, int total) {
        emit progress(msg, cur, total);
        emit updateStatus(false, QString("%1 %2/%3").arg(msg).arg(cur).arg(total), "StackAligner::align");
    });
    connect(aligner, &StackAligner::updateStatus, this, &StackController::updateStatus);

    QMap<int, QImage> aligned = aligner->align(stack);

    emit updateStatus(false,
                      QString("Alignment complete in %1 s").arg(timer.elapsed() / 1000.0, 0, 'f', 2),
                      "StackController::runAlignment");

    if (saveAligned) {
        int idx = 0;
        for (auto it = aligned.begin(); it != aligned.end(); ++it) {
            QString outPath = alignedFolder.filePath(QString("aligned_%1.jpg").arg(idx++, 3, 10, QChar('0')));
            it.value().save(outPath, "JPEG", 95);
        }
        emit updateStatus(false, "Aligned images saved to " + alignedFolder.path(), "StackController::runAlignment");
    }

    emit finished(projectFolder.path());
    return true;
}
