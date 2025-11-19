#include "maskassessor.h"

MaskAssessor::MaskAssessor()
{
}

bool MaskAssessor::overlay(const QString &maskPath,
                           const QString &baseImagePath,
                           const QString &outputPath,
                           const MaskAssessOptions &options)
{
    Q_UNUSED(maskPath)
    Q_UNUSED(baseImagePath)
    Q_UNUSED(outputPath)
    Q_UNUSED(options)
    // TODO: load base image, optionally convert to grayscale,
    //       tint mask regions, blend, draw boundaries, save PNG.
    return true;
}
