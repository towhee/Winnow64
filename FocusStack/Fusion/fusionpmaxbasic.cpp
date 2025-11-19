#include "fusionpmaxbasic.h"
// #include "Petteri/..."   // when you wire it in

FusionPMaxBasic::FusionPMaxBasic(QObject *parent)
    : FusionBase(parent)
{
}

bool FusionPMaxBasic::fuse(const QStringList &imagePaths,
                           const QString &outputFolder,
                           QString &outputImagePath,
                           QString &generatedMaskPath)
{
    const QString src = name();
    emit updateStatus(false, "Starting FusionPMaxBasic...", src);

    QString alignedFolder;
    QString focusFolder;
    QString depthFolder;

    // TODO: ensure outputFolder exists (QDir::mkpath)

    // TODO: call runAlignment(imagePaths, alignedFolder)
    // TODO: call runFocusMaps(alignedFolder, focusFolder)
    // TODO: call runDepthMap(alignedFolder, focusFolder, depthFolder)
    // TODO: call runFusion(alignedFolder, depthFolder, outputFolder, outputImagePath)

    // For now, FusionPMaxBasic does not generate a mask explicitly.
    generatedMaskPath.clear();

    return true;
}

bool FusionPMaxBasic::runAlignment(const QStringList &imagePaths, QString &alignedFolder)
{
    Q_UNUSED(imagePaths)
    Q_UNUSED(alignedFolder)
    // TODO: bridge to Petteri alignment with paths
    return true;
}

bool FusionPMaxBasic::runFocusMaps(const QString &alignedFolder, QString &focusFolder)
{
    Q_UNUSED(alignedFolder)
    Q_UNUSED(focusFolder)
    // TODO: bridge to Petteri focus map computation
    return true;
}

bool FusionPMaxBasic::runDepthMap(const QString &alignedFolder,
                                  const QString &focusFolder,
                                  QString &depthFolder)
{
    Q_UNUSED(alignedFolder)
    Q_UNUSED(focusFolder)
    Q_UNUSED(depthFolder)
    // TODO: bridge to Petteri depth map
    return true;
}

bool FusionPMaxBasic::runFusion(const QString &alignedFolder,
                                const QString &depthFolder,
                                const QString &outputFolder,
                                QString &outputImagePath)
{
    Q_UNUSED(alignedFolder)
    Q_UNUSED(depthFolder)
    Q_UNUSED(outputFolder)
    Q_UNUSED(outputImagePath)
    // TODO: bridge to Petteri PMax fusion
    return true;
}
