#ifndef MASKREFINER_H
#define MASKREFINER_H

#include <QString>

class MaskRefiner
{
public:
    MaskRefiner();

    // inputMaskPath -> refinedMaskPath
    bool refine(const QString &inputMaskPath,
                const QString &refinedMaskPath,
                int radius = 5,
                float feather = 2.0f);
};

#endif // MASKREFINER_H
