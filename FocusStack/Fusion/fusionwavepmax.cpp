#include "fusionwavepmax.h"

FusionWavePMax::FusionWavePMax(QObject *parent)
    : FusionBase(parent)
{
}

bool FusionWavePMax::fuse(const QStringList &imagePaths,
                          const QString &outputFolder,
                          QString &outputImagePath,
                          QString &generatedMaskPath)
{
    const QString src = name();
    emit updateStatus(false, "Starting FusionWavePMax...", src);

    // You might want a subfolder inside outputFolder, e.g. outputFolder + "/wave_work"
    QString workFolder; // TODO: assign and create

    // TODO: prepareData(imagePaths, workFolder);
    // TODO: buildWaveletPyramids(workFolder);
    // TODO: runWaveletFusion(workFolder, outputFolder, outputImagePath);
    // TODO: generateConfidenceMask(workFolder, generatedMaskPath);

    return true;
}

bool FusionWavePMax::prepareData(const QStringList &imagePaths,
                                 const QString &workFolder)
{
    Q_UNUSED(imagePaths)
    Q_UNUSED(workFolder)
    return true;
}

bool FusionWavePMax::buildWaveletPyramids(const QString &workFolder)
{
    Q_UNUSED(workFolder)
    return true;
}

bool FusionWavePMax::runWaveletFusion(const QString &workFolder,
                                      const QString &outputFolder,
                                      QString &outputImagePath)
{
    Q_UNUSED(workFolder)
    Q_UNUSED(outputFolder)
    Q_UNUSED(outputImagePath)
    return true;
}

bool FusionWavePMax::generateConfidenceMask(const QString &workFolder,
                                            QString &maskPath)
{
    Q_UNUSED(workFolder)
    Q_UNUSED(maskPath)
    return true;
}
