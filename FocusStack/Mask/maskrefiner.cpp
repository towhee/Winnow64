#include "maskrefiner.h"

MaskRefiner::MaskRefiner()
{
}

bool MaskRefiner::refine(const QString &inputMaskPath,
                         const QString &refinedMaskPath,
                         int radius,
                         float feather)
{
    Q_UNUSED(inputMaskPath)
    Q_UNUSED(refinedMaskPath)
    Q_UNUSED(radius)
    Q_UNUSED(feather)
    return true;
}
