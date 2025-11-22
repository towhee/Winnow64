#ifndef PETTERI_ALIGN_H
#define PETTERI_ALIGN_H

#include <QString>
#include <QStringList>

class PetteriAlign
{
public:
    // PetteriAlign();
    PetteriAlign() = default;

    // Run alignment stage of Petteri pipeline.
    // paths: input image paths (TIFF, JPG, etc)
    // alignedDir: directory where aligned images should be written.
    // Returns true on success, false on failure.
    bool run(const QStringList &paths,
             const QString &alignedDir);
};

#endif // PETTERI_ALIGN_H
