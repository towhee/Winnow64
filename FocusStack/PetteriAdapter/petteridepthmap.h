#ifndef PETTERI_DEPTHMAP_H
#define PETTERI_DEPTHMAP_H

#include <QString>

class PetteriDepthMap
{
public:
    PetteriDepthMap() = default;

    bool run(const QString &alignedFolder,
             const QString &focusFolder,
             const QString &depthOutputFolder);
};

#endif
