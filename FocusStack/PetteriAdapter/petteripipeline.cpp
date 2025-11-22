#include "petteripipeline.h"

#include "petterialign.h"
#include "petterifocusmaps.h"
#include "petteridepthmap.h"
#include "petterialignfuse.h"
#include "petteripmaxfusion.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>

bool PetteriPipeline::runFull(const QStringList &paths,
                              const QString &projDir,
                              QString &fusedImagePath,
                              QString &depthMapPath)
{
    QString srcFun = "PetteriPipeline::runFull";


    if (paths.isEmpty()) {
        qWarning() << "PetteriPipeline::runFull: No input paths.";
        return false;
    }

    // Ensure base output folder exists
    QDir out(projDir);
    if (!out.exists() && !out.mkpath(".")) {
        qWarning() << "PetteriPipeline::runFull: Could not create output folder:" << projDir;
        return false;
    }

    // Stage folders
    QString alignedDir = projDir + "/Aligned/";
    QString focusDir   = projDir + "/Focus/";
    QString depthDir   = projDir + "/Depth/";
    QString fusedDir   = projDir + "/Fused";
    qDebug() << srcFun << "projDir        =" << projDir;
    qDebug() << srcFun << "depthMapPath   =" << depthMapPath;
    qDebug() << srcFun << "fusedImagePath =" << fusedImagePath;

    qDebug() << srcFun << "alignedDir    =" << alignedDir;
    qDebug() << srcFun << "depthDir      =" << depthDir;
    qDebug() << srcFun << "focusDir      =" << focusDir;

    // --- Stage 1: Alignment ---
    if (!runAlignment(paths, alignedDir)) {
        qWarning() << "PetteriPipeline::runFull: Alignment failed.";
        return false;
    }

    // // --- Stage 2: Focus maps ---
    // if (!runFocusMaps(alignedFolder, focusFolder)) {
    //     qWarning() << "PetteriPipeline::runFull: Focus-map stage failed.";
    //     return false;
    // }

    // // --- Stage 3: Depth map ---
    // if (!runDepthMap(alignedFolder, focusFolder, depthFolder)) {
    //     qWarning() << "PetteriPipeline::runFull: Depth-map stage failed.";
    //     return false;
    // }

    // --- Stage 4: Fusion ---
    if (!runFusion(alignedDir, fusedDir, fusedImagePath)) {
        qWarning() << "PetteriPipeline::runFull: Fusion stage failed.";
        return false;
    }

    qDebug() << "PetteriPipeline::runFull: Completed successfully.";
    return true;
}

bool PetteriPipeline::runAlignment(const QStringList &paths,
                                   const QString &alignedDir)
{
    QString srcFun = "PetteriPipeline::runAlignment";
    qDebug() << srcFun << "alignedDir         =" << alignedDir;

    QDir dir(alignedDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "PetteriPipeline::runAlignment: Could not create aligned folder:" << alignedDir;
        return false;
    }

    PetteriAlign align;
    return align.run(paths, alignedDir);
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

bool PetteriPipeline::runFusion(const QString &alignedDir,
                                const QString &fusedDir,
                                const QString &fusedImagePath)
{
    QString srcFun = "PetteriPipeline::runFusion";

    qDebug() << srcFun << "alignedDir     =" << alignedDir;
    qDebug() << srcFun << "fusedDir       =" << fusedDir;
    qDebug() << srcFun << "fusedImagePath =" << fusedImagePath;

    if (alignedDir.isEmpty()) {
        qWarning() << "PetteriPipeline::runFusion: alignedFolder is empty.";
        return false;
    }

    QDir dir(fusedDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "PetteriPipeline::runFusion: Could not create Fused folder:" << alignedDir;
        return false;
    }

    qDebug() << srcFun << "Folder exists:" << fusedDir;

    if (fusedImagePath.isEmpty()) {
        qWarning() << "PetteriPipeline::runFusion: fusedImagePath is empty.";
        return false;
    }

    PetteriPMaxFusion fusion;
    return fusion.run(alignedDir, fusedDir, fusedImagePath);
}
