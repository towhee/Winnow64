#ifndef ISTAGE_H
#define ISTAGE_H

#include <QString>

class IStage
{
public:
    virtual ~IStage() = default;

    // Human-readable name, for logging and UI
    virtual QString name() const = 0;
};

#endif
