#include "petterifocusmaps.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// Petteri
#include "focusstack.h"    // Adjust path as needed

using namespace focusstack;

bool PetteriFocusMaps::run(const QString &alignedFolder,
                           const QString &focusOutputFolder)
{
    // --- Validate aligned input ---
    if (!QDir(alignedFolder).exists()) {
        qWarning() << "PetteriFocusMaps::run: Aligned folder does not exist:" << alignedFolder;
        return false;
    }

    // --- Ensure output directory exists ---
    QDir out(focusOutputFolder);
    if (!out.exists() && !out.mkpath(".")) {
        qWarning() << "PetteriFocusMaps::run: Could not create output folder:" << focusOutputFolder;
        return false;
    }

    // --- Enumerate aligned image files ---
    QDir dir(alignedFolder);
    QStringList files = dir.entryList(
        QStringList() << "*.tif" << "*.tiff" << "*.png" << "*.jpg" << "*.jpeg",
        QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        qWarning() << "PetteriFocusMaps::run: No aligned images found in" << alignedFolder;
        return false;
    }

    // Convert to std::vector<std::string>
    std::vector<std::string> inputs;
    inputs.reserve(files.size());
    for (const QString &f : files) {
        inputs.emplace_back((alignedFolder + f).toStdString());
        // inputs.emplace_back((alignedFolder + "/" + f).toStdString());
    }

    // --- Configure Petteri ---
    FocusStack fs;
    fs.set_inputs(inputs);

    // Output location is where focus maps should go
    QString prefix = focusOutputFolder;
    if (!prefix.endsWith('/')) prefix += '/';
    prefix += "focus_";
    fs.set_output(prefix.toStdString());

    // Critical flags
    fs.set_align_only(false);    // This triggers focusmap generation
    fs.set_save_steps(true);     // Forces intermediate results to be written
    fs.set_nocrop(true);         // Keep image geometry consistent

    // Optional safer defaults
    fs.set_threads(std::thread::hardware_concurrency());
    fs.set_disable_opencl(false);

    // --- Run Petteri pipeline ---
    bool ok = fs.run();

    if (!ok) {
        qWarning() << "PetteriFocusMaps::run: Petteri run() returned failure.";
        return false;
    }

    qDebug() << "PetteriFocusMaps::run: Focus maps generated successfully in:" << focusOutputFolder;
    return true;
}
