#include "FocusStack/Stages/Align/PetteriAlign.h"
#include "FocusStack/PetteriModular/Core/focusstack.h"   // FStack::FocusStack

#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <thread>     // std::thread::hardware_concurrency

using namespace FStack;

PetteriAlign::PetteriAlign(QObject *parent)
    : IAlign(parent)
{
}

QString PetteriAlign::name() const
{
    return "PetteriAlign";
}

bool PetteriAlign::run(const QStringList &imagePaths,
                       QString &alignedFolderOut)
{
    const QString src = "PetteriAlign::run";

    if (imagePaths.isEmpty()) {
        emit updateStatus(true,
                          "PetteriAlign: No input images provided.",
                          src);
        return false;
    }

    // Decide output folder if caller left it empty
    if (alignedFolderOut.isEmpty()) {
        QFileInfo last(imagePaths.last());
        QDir srcDir = last.dir();

        // Example: /Stack/Mouse/2025-11-01_0238/Aligned
        QString projRoot = srcDir.filePath(last.baseName());
        alignedFolderOut = projRoot + "/Aligned";
    }

    QDir outDir(alignedFolderOut);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        emit updateStatus(true,
                          QString("PetteriAlign: Failed to create aligned folder: %1")
                              .arg(alignedFolderOut),
                          src);
        return false;
    }

    emit updateStatus(false,
                      QString("PetteriAlign: Aligning %1 images into %2")
                          .arg(imagePaths.size())
                          .arg(alignedFolderOut),
                      src);

    // Convert QStringList -> std::vector<std::string>
    std::vector<std::string> inputFiles;
    inputFiles.reserve(imagePaths.size());
    for (const QString &p : imagePaths)
        inputFiles.emplace_back(p.toStdString());

    // Configure PetteriModular FocusStack engine in "align-only" mode
    FocusStack fs;

    fs.set_inputs(inputFiles);
    fs.set_output(alignedFolderOut.toStdString());

    fs.set_align_only(true);          // <<< only alignment
    fs.set_save_steps(true);          // ensure aligned_*.tif get written

    fs.set_align_flags(FocusStack::ALIGN_DEFAULT);
    fs.set_threads(std::thread::hardware_concurrency());

    const bool ok = fs.run();

    if (!ok) {
        emit updateStatus(true,
                          "PetteriAlign: Petteri backend reported failure.",
                          src);
        return false;
    }

    emit updateStatus(false,
                      QString("PetteriAlign: Alignment completed. Output: %1")
                          .arg(alignedFolderOut),
                      src);

    return true;
}
