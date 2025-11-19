#ifndef MASKGENERATOR_H
#define MASKGENERATOR_H

#include <QString>
#include <QStringList>

class MaskGenerator
{
public:
    MaskGenerator();

    // Use per-slice focus maps (paths to images representing focus response)
    bool generateFromFocusMaps(const QStringList &focusMapPaths,
                               const QString &outputMaskPath);

    // Use multi-scale confidence or wavelet detail maps
    bool generateFromWaveletConfidence(const QStringList &confidenceMapPaths,
                                       const QString &outputMaskPath);
};

#endif // MASKGENERATOR_H
