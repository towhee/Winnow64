#ifndef FUSIONWAVEPMAX_H
#define FUSIONWAVEPMAX_H

#include "fusionbase.h"

class FusionWavePMax : public FusionBase
{
    Q_OBJECT
public:
    explicit FusionWavePMax(QObject *parent = nullptr);

    QString name() const override { return "FusionWavePMax"; }

    bool fuse(const QStringList &imagePaths,
              const QString &projDir,
              const Options &options,
              QString &generatedMaskPath);

private:
    // High-level pipeline for this fusion method
    bool prepareData(const QStringList &imagePaths,
                     const QString &workFolder);
    bool buildWaveletPyramids(const QString &workFolder);
    bool runWaveletFusion(const QString &workFolder,
                          const QString &outputFolder,
                          QString &outputImagePath);
    bool generateConfidenceMask(const QString &workFolder,
                                QString &maskPath);
};

#endif // FUSIONWAVEPMAX_H
