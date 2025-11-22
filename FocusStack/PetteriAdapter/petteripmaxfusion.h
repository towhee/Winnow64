#ifndef PETTERI_PMAXFUSION_H
#define PETTERI_PMAXFUSION_H

#include <QString>

class PetteriPMaxFusion
{
public:
    PetteriPMaxFusion() = default;

    bool run(const QString &alignedDir,
             const QString &fusedFolder,
             const QString &fusedImagePath
             );

private:
    bool collectAlignedImages(const QString &alignedDir,
                              std::vector<std::string> &files);


};

#endif
