#include "petteripipeline.h"

#include "petterialign.h"
#include "petterifocusmaps.h"
#include "petteridepthmap.h"
#include "petteripmaxfusion.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>

bool PetteriPipeline::runFull(const QStringList &paths,
                              const QString &outputFolder,
                              QString &outputImagePath,
                              QString &depthMapPath)
{
    if (paths.isEmpty()) {
        qWarning() << "PetteriPipeline::runFull: No input paths.";
        return false;
    }

    // Ensure base output folder exists
    QDir out(outputFolder);
    if (!out.exists() && !out.mkpath(".")) {
        qWarning() << "PetteriPipeline::runFull: Could not create output folder:" << outputFolder;
        return false;
    }

    // Stage folders
    QString alignedFolder = outputFolder + "/Aligned";
    QString focusFolder   = outputFolder + "/Focus";
    QString depthFolder   = outputFolder + "/Depth";

    // Final outputs
    outputImagePath = outputFolder + "/Fused.png";
    depthMapPath    = depthFolder + "/depth_index.png";

    // --- Stage 1: Alignment ---
    if (!runAlignment(paths, alignedFolder)) {
        qWarning() << "PetteriPipeline::runFull: Alignment failed.";
        return false;
    }

    // --- Stage 2: Focus maps ---
    if (!runFocusMaps(alignedFolder, focusFolder)) {
        qWarning() << "PetteriPipeline::runFull: Focus-map stage failed.";
        return false;
    }

    // --- Stage 3: Depth map ---
    if (!runDepthMap(alignedFolder, focusFolder, depthFolder)) {
        qWarning() << "PetteriPipeline::runFull: Depth-map stage failed.";
        return false;
    }

    // --- Stage 4: Fusion ---
    if (!runFusion(alignedFolder, depthFolder, outputImagePath)) {
        qWarning() << "PetteriPipeline::runFull: Fusion stage failed.";
        return false;
    }

    qDebug() << "PetteriPipeline::runFull: Completed successfully.";
    return true;
}

bool PetteriPipeline::runAlignment(const QStringList &paths,
                                   QString &alignedFolder)
{
    if (alignedFolder.isEmpty()) {
        qWarning() << "PetteriPipeline::runAlignment: alignedFolder is empty.";
        return false;
    }

    QDir dir(alignedFolder);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "PetteriPipeline::runAlignment: Could not create aligned folder:" << alignedFolder;
        return false;
    }

    PetteriAlign align;
    return align.run(paths, alignedFolder);
}

bool PetteriPipeline::runFocusMaps(const QString &alignedFolder,
                                   QString &focusFolder)
{
    if (alignedFolder.isEmpty()) {
        qWarning() << "PetteriPipeline::runFocusMaps: alignedFolder is empty.";
        return false;
    }
    if (focusFolder.isEmpty()) {
        qWarning() << "PetteriPipeline::runFocusMaps: focusFolder is empty.";
        return false;
    }

    QDir dir(focusFolder);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "PetteriPipeline::runFocusMaps: Could not create focus folder:" << focusFolder;
        return false;
    }

    PetteriFocusMaps fm;
    return fm.run(alignedFolder, focusFolder);
}

bool PetteriPipeline::runDepthMap(const QString &alignedFolder,
                                  const QString &focusFolder,
                                  QString &depthFolder)
{
    if (alignedFolder.isEmpty()) {
        qWarning() << "PetteriPipeline::runDepthMap: alignedFolder is empty.";
        return false;
    }
    if (focusFolder.isEmpty()) {
        qWarning() << "PetteriPipeline::runDepthMap: focusFolder is empty.";
        return false;
    }
    if (depthFolder.isEmpty()) {
        qWarning() << "PetteriPipeline::runDepthMap: depthFolder is empty.";
        return false;
    }

    QDir dir(depthFolder);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "PetteriPipeline::runDepthMap: Could not create depth folder:" << depthFolder;
        return false;
    }

    PetteriDepthMap dm;
    return dm.run(alignedFolder, focusFolder, depthFolder);
}

bool PetteriPipeline::runFusion(const QString &alignedFolder,
                                const QString &depthFolder,
                                QString &outputImagePath)
{
    if (alignedFolder.isEmpty()) {
        qWarning() << "PetteriPipeline::runFusion: alignedFolder is empty.";
        return false;
    }
    if (depthFolder.isEmpty()) {
        qWarning() << "PetteriPipeline::runFusion: depthFolder is empty.";
        return false;
    }
    if (outputImagePath.isEmpty()) {
        qWarning() << "PetteriPipeline::runFusion: outputImagePath is empty.";
        return false;
    }

    PetteriPMaxFusion fusion;
    return fusion.run(alignedFolder, depthFolder, outputImagePath);
}
