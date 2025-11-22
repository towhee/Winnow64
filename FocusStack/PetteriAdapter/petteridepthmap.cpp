#include "petteridepthmap.h"
#include <QDir>
#include <QDebug>
#include <QFileInfo>

// Petteri pipeline
#include "focusstack.h"

using namespace Petteri;

// PetteriDepthMap::PetteriDepthMap() {}   // default set in header

bool PetteriDepthMap::run(const QString &alignedFolder,
                          const QString &focusFolder,
                          const QString &depthOutputFolder)
{
    // --- Validation -------------------------------------------------------
    if (!QDir(alignedFolder).exists()) {
        qWarning() << "PetteriDepthMap::run: Aligned folder does not exist:" << alignedFolder;
        return false;
    }

    if (!QDir(focusFolder).exists()) {
        qWarning() << "PetteriDepthMap::run: Focus folder does not exist:" << focusFolder;
        return false;
    }

    // Create output dir
    QDir out(depthOutputFolder);
    if (!out.exists() && !out.mkpath(".")) {
        qWarning() << "PetteriDepthMap::run: Failed to create depth output folder:" << depthOutputFolder;
        return false;
    }

    // --- Collect aligned inputs ------------------------------------------
    QDir dir(alignedFolder);
    QStringList files = dir.entryList(
        QStringList() << "*.tif" << "*.tiff" << "*.png" << "*.jpg" << "*.jpeg",
        QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        qWarning() << "PetteriDepthMap::run: No aligned images found in" << alignedFolder;
        return false;
    }

    std::vector<std::string> inputFiles;
    inputFiles.reserve(files.size());
    for (const QString &f : files) {
        inputFiles.emplace_back((alignedFolder + "/" + f).toStdString());
    }

    // --- Configure Petteri -----------------------------------------------
    FocusStack fs;
    fs.set_inputs(inputFiles);
    fs.set_output(depthOutputFolder.toStdString());
    fs.set_align_only(false);
    fs.set_save_steps(true);   // ensures depthmap files are written

    // Force depthmap output
    fs.set_depthmap((depthOutputFolder + "depth_").toStdString());
    // fs.set_depthmap((depthOutputFolder + "/depth_index.png").toStdString());

    // Avoid cropping
    fs.set_nocrop(true);

    // Optional performance tweaks
    fs.set_threads(std::thread::hardware_concurrency());

    // --- Run the pipeline -------------------------------------------------
    bool ok = fs.run();

    if (!ok) {
        qWarning() << "PetteriDepthMap::run: Depthmap generation failed.";
        return false;
    }

    // --- Verify output ----------------------------------------------------
    QString depthIdx = depthOutputFolder + "/depth_index.png";
    if (!QFileInfo::exists(depthIdx)) {
        qWarning() << "PetteriDepthMap::run: depth_index.png not found after run.";
    }

    qDebug() << "PetteriDepthMap::run: Depthmap created at:" << depthIdx;
    return ok;
}
