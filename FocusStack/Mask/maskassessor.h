#ifndef MASKASSESSOR_H
#define MASKASSESSOR_H

#include <QString>
#include "maskoptions.h"

class MaskAssessor
{
public:
    MaskAssessor();

    // maskPath: single-channel or RGBA mask image
    // baseImagePath: original/fused image used as background
    // outputPath: path for overlay image (eg "maskname_basename.png")
    bool overlay(const QString &maskPath,
                 const QString &baseImagePath,
                 const QString &outputPath,
                 const MaskAssessOptions &options = MaskAssessOptions());
};

#endif // MASKASSESSOR_H
