#ifndef IFOCUS_H
#define IFOCUS_H

#include "IStage.h"
#include <QString>

class IFocus : public IStage
{
public:
    virtual ~IFocus() = default;

    // Input: aligned folder
    // Output: folder containing focus maps (one per slice)
    virtual bool run(const QString &alignedFolder,
                     QString &focusFolder) = 0;
};

#endif
