#ifndef PETTERI_PMAXFUSION_H
#define PETTERI_PMAXFUSION_H

#include <QString>

class PetteriPMaxFusion
{
public:
    PetteriPMaxFusion() = default;

    bool run(const QString &alignedFolder,
             const QString &depthFolder,
             const QString &outputImagePath);
};

#endif
