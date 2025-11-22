#include "fusionfactory.h"
#include "fusionpmaxbasic.h"
#include "fusionwavepmax.h"

FusionBase* FusionFactory::create(const QString &methodName, QObject *parent)
{
    if (methodName == "FusionPMaxBasic")
        return new FusionPMaxBasic(parent);
    // if (methodName == "FusionWavePMax")
    //     return new FusionWavePMax(parent);

    return nullptr;
}
