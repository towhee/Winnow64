#ifndef PETTERIALIGNFUSE_H
#define PETTERIALIGNFUSE_H

#include <QString>

class PetteriAlignFuse
{
public:
    PetteriAlignFuse();

    bool run(const QString &alignedDir,
             const QString &fusedFolder,
             const QString &fusedImagePath
             );

};

#endif // PETTERIALIGNFUSE_H
