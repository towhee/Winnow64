#include "fusionpmaxbasic.h"
#include "FocusStack/PetteriAdapter/petteripipeline.h"

#include <QDir>
#include <QDebug>

FusionPMaxBasic::FusionPMaxBasic(QObject *parent)
    : FusionBase(parent)
{
}

bool FusionPMaxBasic::fuse(const QStringList &paths,
                           const QString &outputFolder,
                           QString &outputImagePath,
                           QString &generatedMaskPath)
{
    const QString src = name();
    emit updateStatus(false, "Starting FusionPMaxBasic...", src);

    if (paths.isEmpty()) {
        emit updateStatus(false, "FusionPMaxBasic: No input images.", src);
        return false;
    }

    QDir out(outputFolder);
    if (!out.exists() && !out.mkpath(".")) {
        emit updateStatus(false, "FusionPMaxBasic: Could not create output folder.", src);
        return false;
    }

    PetteriPipeline pipeline;
    bool ok = pipeline.runFull(paths,
                               outputFolder,
                               outputImagePath,
                               generatedMaskPath);

    if (!ok) {
        emit updateStatus(false, "FusionPMaxBasic FAILED", src);
        return false;
    }

    emit updateStatus(false, "FusionPMaxBasic finished.", src);
    return true;
}
