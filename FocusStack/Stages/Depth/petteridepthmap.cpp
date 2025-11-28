#include "FocusStack/Stages/Depth/PetteriDepthMap.h"
#include "FocusStack/PetteriModular/Core/focusstack.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <thread>

using namespace FStack;

PetteriDepthMap::PetteriDepthMap(QObject *parent)
    : IDepthMap(parent)
{
}

QString PetteriDepthMap::name() const
{
    return "PetteriDepthMap";
}

bool PetteriDepthMap::run(const QString &alignedFolder,
                          const QString &focusFolder,
                          QString &depthFolderOut)
{
    const QString src = "PetteriDepthMap::run";

    QDir inDir(alignedFolder);
    if (!inDir.exists()) {
        emit updateStatus(true, "Aligned folder not found.", src);
        return false;
    }

    // Ensure depth output folder
    if (depthFolderOut.isEmpty())
        depthFolderOut = inDir.filePath("Depth");

    QDir outDir(depthFolderOut);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        emit updateStatus(true, "Failed to create depth folder.", src);
        return false;
    }

    // Collect aligned images
    QStringList files = inDir.entryList(
        {"*.tif","*.tiff","*.png","*.jpg"}, QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        emit updateStatus(true, "No aligned images found for depthmap.", src);
        return false;
    }

    std::vector<std::string> inputs;
    for (const QString &f : files)
        inputs.emplace_back(inDir.filePath(f).toStdString());

    emit updateStatus(false, "Building depthmap...", src);

    FocusStack fs;

    fs.set_inputs(inputs);
    fs.set_output(depthFolderOut.toStdString());
    fs.set_depthmap("depth_index.png");  // triggers depthmap mode
    fs.set_align_only(false);
    fs.set_save_steps(true);
    fs.set_threads(std::thread::hardware_concurrency());

    bool ok = fs.run();
    if (!ok) {
        emit updateStatus(true, "Depthmap generation failed.", src);
        return false;
    }

    emit updateStatus(false, "Depthmap created.", src);
    return true;
}
