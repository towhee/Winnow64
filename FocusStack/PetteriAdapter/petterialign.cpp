#include "petterialign.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>

// If you have a namespace for the Petteri code, include it here.
// For example:
#include "FocusStack/petteri/focusstack.h"
using namespace Petteri;

// no constructor because header uses default

bool PetteriAlign::run(const QStringList &paths,
                       const QString &alignedDir)
{
    QString srcFun = "PetteriAlign::ru";
    qDebug() << srcFun << "alignedDir         =" << alignedDir;

    if (paths.isEmpty()) {
        qWarning() << "PetteriAlign::run: No input paths.";
        return false;
    }

    // Ensure output directory exists
    QDir dir(alignedDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "PetteriAlign::run: Failed to create output folder:" << alignedDir;
        return false;
    }

    // Convert paths -> std::vector<string>
    std::vector<std::string> inputFiles;
    inputFiles.reserve(paths.size());
    for (const QString &p : paths)
        inputFiles.emplace_back(p.toStdString());

    // --- Configure Petteri pipeline ---
    FocusStack fs;

    fs.set_inputs(inputFiles);
    fs.set_output(alignedDir.toStdString());
    fs.set_align_only(true);
    fs.set_save_steps(true);   // ensures intermediate imgs get saved

    // Optional settings (you may customize)
    fs.set_align_flags(FocusStack::ALIGN_DEFAULT);
    fs.set_threads(std::thread::hardware_concurrency());

    // --- Execute pipeline ---
    bool ok = fs.run();

    if (!ok) {
        qWarning() << "PetteriAlign::run: Petteri run() returned failure.";
        return false;
    }

    qDebug() << "PetteriAlign::run: Alignment completed successfully.";
    return true;
}
