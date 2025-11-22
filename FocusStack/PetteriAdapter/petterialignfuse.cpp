#include "petterialignfuse.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// Petteri code
#include "focusstack.h"

using namespace focusstack;

// PetteriPMaxFusion::PetteriPMaxFusion() {}   // NOT = default

bool PetteriAlignFuse::run(const QString &alignedDir,
                            const QString &fusedDir,
                            const QString &fusedImagePath)
{
    QString srcFun = "PetteriPMaxFusion::run";

    // --- Validate inputs --------------------------------------------------
    if (!QDir(alignedDir).exists()) {
        qWarning() << "PetteriPMaxFusion::run: Aligned folder does not exist:" << alignedDir;
        return false;
    }

    qDebug() << srcFun << "alignedDir =" << alignedDir;
    qDebug() << srcFun << "fusedDir =" << fusedDir;
    qDebug() << srcFun << "fusedImagePath =" << fusedImagePath;

    // Ensure align dir exists
    QDir aDir(QFileInfo(alignedDir).absolutePath());
    if (!aDir.exists() && !aDir.mkpath(".")) {
        qWarning() << "PetteriPMaxFusion::run: Could not create align folder.";
        return false;
    }


    // Ensure output dir exists
    QDir out(QFileInfo(fusedDir).absolutePath());
    if (!out.exists() && !out.mkpath(".")) {
        qWarning() << "PetteriPMaxFusion::run: Could not create output folder.";
        return false;
    }

    // --- Collect aligned images ------------------------------------------
    QDir dir(alignedDir);
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
        inputFiles.emplace_back((alignedDir + "/" + f).toStdString());

    // --- Configure Petteri ------------------------------------------------
    FocusStack fs;
    fs.set_inputs(inputFiles);

    // Output fused image
    fs.set_output(fusedImagePath.toStdString());

    // Use existing depth map
    // fs.set_depthmap(depthIdx.toStdString());

    // Prevent alignment
    // fs.set_align_only(false);

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

    if (!QFileInfo::exists(fusedDir)) {
        qWarning() << "PetteriPMaxFusion::run: Output image not created:";
        return false;
    }

    qDebug() << "PetteriPMaxFusion::run: Final fused image written to:" << fusedDir;
    return true;
}
