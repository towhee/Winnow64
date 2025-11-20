#ifndef PETTERI_FOCUSMAPS_H
#define PETTERI_FOCUSMAPS_H

#include <QString>

class PetteriFocusMaps
{
public:
    PetteriFocusMaps() = default;

    bool run(const QString &alignedFolder,
             const QString &focusOutputFolder);
};

#endif
