#include "FocusStack/Stages/Fusion/PetteriPMaxFusion.h"
#include "FocusStack/PetteriModular/Pipeline/focusstack.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <thread>

using namespace FStack;

PetteriPMaxFusion::PetteriPMaxFusion(QObject *parent)
    : IFusion(parent)
{
}

QString PetteriPMaxFusion::name() const
{
    return "PetteriPMaxFusion";
}

bool PetteriPMaxFusion::run(const QString &alignedFolder,
                            const QString &depthFolder,
                            const QString &outputFolder,
                            QString &outputImagePath,
                            QString &generatedMaskPath)
{
    const QString src = "PetteriPMaxFusion::run";

    Q_UNUSED(depthFolder); // depth-aware fusion comes later
    generatedMaskPath.clear();

    QDir inDir(alignedFolder);
    if (!inDir.exists()) {
        emit updateStatus(true,
                          "Aligned folder does not exist.",
                          src);
        return false;
    }

    QDir outDir(outputFolder);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        emit updateStatus(true,
                          "Failed to create output folder.",
                          src);
        return false;
    }

    QStringList files = inDir.entryList(
        {"*.tif","*.tiff","*.png","*.jpg","*.jpeg"},
        QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        emit updateStatus(true,
                          "No aligned images found for fusion.",
                          src);
        return false;
    }

    // Decide output image path if not set
    if (outputImagePath.isEmpty()) {
        QFileInfo fi(inDir.filePath(files.first()));
        outputImagePath = outDir.filePath(fi.completeBaseName() + "_Fused.png");
    }

    std::vector<std::string> inputs;
    inputs.reserve(files.size());
    for (const QString &f : files)
        inputs.emplace_back(inDir.filePath(f).toStdString());

    emit updateStatus(false,
                      "Starting Petteri PMax fusion...",
                      src);

    FocusStack fs;

    fs.set_inputs(inputs);
    fs.set_output(outputImagePath.toStdString());

    fs.set_align_only(false);
    fs.set_save_steps(true);
    fs.set_nocrop(true);

    fs.set_threads(std::thread::hardware_concurrency());

    bool ok = fs.run();
    if (!ok) {
        emit updateStatus(true,
                          "Fusion failed in Petteri backend.",
                          src);
        return false;
    }

    if (!QFileInfo::exists(outputImagePath)) {
        emit updateStatus(true,
                          "Fusion did not produce an output image.",
                          src);
        return false;
    }

    emit updateStatus(false,
                      QString("Fusion complete: %1").arg(outputImagePath),
                      src);

    return true;
}
