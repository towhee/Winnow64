#ifndef IALIGN_H
#define IALIGN_H

#include "IStage.h"
#include <QStringList>

class IAlign : public IStage
{
public:
    virtual ~IAlign() = default;

    // Input: raw source file paths
    // Output: directory containing aligned images
    virtual bool run(const QStringList &inputPaths,
                     QString &alignedFolder) = 0;
};

#endif
