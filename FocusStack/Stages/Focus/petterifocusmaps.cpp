// NOTE: we use the wrapper, not the old FocusStackWorker
#include "FocusStack/Stages/Focus/petterifocusmaps.h"
#include "FocusStack/PetteriModular/Wrappers/PetteriFocusMapsWorker.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <algorithm> // std::sort

using namespace FStack;

PetteriFocusMaps::PetteriFocusMaps(QObject *parent)
    : IFocusMaps(parent)
{
}

bool PetteriFocusMaps::run(const QString &alignedFolder,
                           QString &focusFolderOut)
{
    const QString src = name();

    emit updateStatus(false,
                      QString("Computing focus maps using Petteri for %1")
                          .arg(alignedFolder),
                      src);

    // ---- Validate aligned folder -----------------------------------------
    QDir dir(alignedFolder);
    if (!dir.exists()) {
        emit updateStatus(false,
                          QString("Aligned folder not found: %1")
                              .arg(alignedFolder),
                          src);
        return false;
    }

    // ---- Output folder ----------------------------------------------------
    focusFolderOut = dir.filePath("focus");
    if (!QDir().mkpath(focusFolderOut)) {
        emit updateStatus(false,
                          QString("Failed to create focus output folder: %1")
                              .arg(focusFolderOut),
                          src);
        return false;
    }

    // ---- Collect aligned images ------------------------------------------
    QStringList alignedPaths;
    const QStringList exts = { "*.tif", "*.tiff",
                              "*.png", "*.jpg", "*.jpeg" };

    for (const QString &ext : exts) {
        const QStringList matches =
            dir.entryList({ext}, QDir::Files, QDir::Name);
        for (const QString &m : matches)
            alignedPaths << dir.filePath(m);
    }
    std::sort(alignedPaths.begin(), alignedPaths.end());

    if (alignedPaths.isEmpty()) {
        emit updateStatus(false,
                          "No aligned images found in folder.",
                          src);
        return false;
    }

    // ---- Auto-detect existing focus maps ---------------------------------
    QStringList focusMapPaths;
    {
        QDir fdir(focusFolderOut);
        const QStringList fexts = { "*.png", "*.tif", "*.tiff" };
        for (const QString &ext : fexts) {
            const QStringList matches =
                fdir.entryList({ext}, QDir::Files, QDir::Name);
            for (const QString &m : matches)
                focusMapPaths << fdir.filePath(m);
        }
        std::sort(focusMapPaths.begin(), focusMapPaths.end());
    }

    emit updateStatus(false,
                      QString("Found %1 aligned images, %2 existing focus maps")
                          .arg(alignedPaths.size())
                          .arg(focusMapPaths.size()),
                      src);

    // ---- Run the worker ---------------------------------------------------
    PetteriFocusMapsWorker worker;
    worker.setInput(alignedPaths, focusMapPaths, focusFolderOut);

    // Relay worker signals
    QObject::connect(&worker, &PetteriFocusMapsWorker::updateStatus,
                     this, &PetteriFocusMaps::updateStatus);

    QObject::connect(&worker, &PetteriFocusMapsWorker::progress,
                     this, &PetteriFocusMaps::updateProgress);

    emit updateStatus(false,
                      "Starting Petteri focus map computation...",
                      src);

    bool ok = worker.run(nullptr);  // no abort lambda yet

    if (!ok) {
        emit updateStatus(false,
                          "Petteri focus map computation failed.",
                          src);
        return false;
    }

    emit updateStatus(false,
                      "Focus maps completed successfully.",
                      src);

    return true;
}
