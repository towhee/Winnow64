#include "petteripmaxfusion.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// Petteri code
#include "focusstack.h"

using namespace focusstack;

// PetteriPMaxFusion::PetteriPMaxFusion() {}   // NOT = default

bool PetteriPMaxFusion::run(const QString &alignedFolder,
                            const QString &depthFolder,
                            const QString &outputImagePath)
{
    // --- Validate inputs --------------------------------------------------
    if (!QDir(alignedFolder).exists()) {
        qWarning() << "PetteriPMaxFusion::run: Aligned folder does not exist:" << alignedFolder;
        return false;
    }

    QString depthIdx = depthFolder + "/depth_index.png";
    if (!QFileInfo::exists(depthIdx)) {
        qWarning() << "PetteriPMaxFusion::run: depth_index.png not found in" << depthFolder;
        return false;
    }

    // Ensure output dir exists
    QDir out(QFileInfo(outputImagePath).absolutePath());
    if (!out.exists() && !out.mkpath(".")) {
        qWarning() << "PetteriPMaxFusion::run: Could not create output folder.";
        return false;
    }

    // --- Collect aligned images ------------------------------------------
    QDir dir(alignedFolder);
    QStringList files = dir.entryList(
        QStringList() << "*.tif" << "*.tiff" << "*.png" << "*.jpg" << "*.jpeg",
        QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        qWarning() << "PetteriPMaxFusion::run: No aligned images found.";
        return false;
    }

    std::vector<std::string> inputFiles;
    inputFiles.reserve(files.size());
    for (const QString &f : files)
        inputFiles.emplace_back((alignedFolder + "/" + f).toStdString());

    // --- Configure Petteri ------------------------------------------------
    FocusStack fs;
    fs.set_inputs(inputFiles);

    // Output fused image
    fs.set_output(outputImagePath.toStdString());

    // Use existing depth map
    fs.set_depthmap(depthIdx.toStdString());

    // Prevent alignment
    fs.set_align_only(false);

    // Preserve geometry
    fs.set_nocrop(true);

    // Required to save final output
    fs.set_save_steps(true);

    fs.set_threads(std::thread::hardware_concurrency());

    // --- Run pipeline -----------------------------------------------------
    bool ok = fs.run();

    if (!ok) {
        qWarning() << "PetteriPMaxFusion::run: Fusion process failed.";
        return false;
    }

    if (!QFileInfo::exists(outputImagePath)) {
        qWarning() << "PetteriPMaxFusion::run: Output image not created:";
        return false;
    }

    qDebug() << "PetteriPMaxFusion::run: Final fused image written to:" << outputImagePath;
    return true;
}
