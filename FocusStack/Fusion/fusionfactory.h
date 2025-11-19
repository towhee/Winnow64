#ifndef FUSIONFACTORY_H
#define FUSIONFACTORY_H

#include <QString>
#include "fusionbase.h"

class FusionFactory
{
public:
    static FusionBase* create(const QString &methodName, QObject *parent = nullptr);
};

#endif // FUSIONFACTORY_H
