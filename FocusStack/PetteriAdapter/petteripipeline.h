#ifndef PETTERI_PIPELINE_H
#define PETTERI_PIPELINE_H

#include <QStringList>
#include <QString>

class PetteriPipeline
{
public:
    PetteriPipeline() = default;

    // Full end-to-end pipeline (alignment → focus → depth → fusion)
    bool runFull(const QStringList &paths,
                 const QString &outputFolder,
                 QString &outputImagePath,
                 QString &depthMapPath);

    // Expose staged execution for modular testing
    bool runAlignment(const QStringList &paths, QString &alignedFolder);
    bool runFocusMaps(const QString &alignedFolder, QString &focusFolder);
    bool runDepthMap(const QString &alignedFolder,
                     const QString &focusFolder,
                     QString &depthFolder);
    bool runFusion(const QString &alignedFolder,
                   const QString &depthFolder,
                   QString &outputImagePath);
};

#endif
