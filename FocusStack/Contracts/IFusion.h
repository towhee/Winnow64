#ifndef IFUSION_H
#define IFUSION_H

#include "IStage.h"
#include <QString>

class IFusion : public IStage
{
public:
    virtual ~IFusion() = default;

    // Input: aligned images + optional depth map
    // Output: final fused image path
    virtual bool run(const QString &alignedFolder,
                     const QString &depthFolder,
                     QString &outputImagePath) = 0;
};

#endif
