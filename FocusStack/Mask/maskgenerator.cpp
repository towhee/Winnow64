#include "maskgenerator.h"

MaskGenerator::MaskGenerator()
{
}

bool MaskGenerator::generateFromFocusMaps(const QStringList &focusMapPaths,
                                          const QString &outputMaskPath)
{
    Q_UNUSED(focusMapPaths)
    Q_UNUSED(outputMaskPath)
    return true;
}

bool MaskGenerator::generateFromWaveletConfidence(const QStringList &confidenceMapPaths,
                                                  const QString &outputMaskPath)
{
    Q_UNUSED(confidenceMapPaths)
    Q_UNUSED(outputMaskPath)
    return true;
}
