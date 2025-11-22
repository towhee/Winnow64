#ifndef IDEPTH_H
#define IDEPTH_H

#include "IStage.h"
#include <QString>

class IDepth : public IStage
{
public:
    virtual ~IDepth() = default;

    // Input: focus maps folder
    // Output: folder containing depth map(s)
    virtual bool run(const QString &focusFolder,
                     QString &depthFolder) = 0;
};

#endif
