#include "fusionpmaxbasic.h"
#include "FocusStack/PetteriAdapter/petteripipeline.h"

#include <QDir>
#include <QDebug>

FusionPMaxBasic::FusionPMaxBasic(QObject *parent)
    : FusionBase(parent)
{
}

bool FusionPMaxBasic::fuse(const QStringList &paths,
                           const QString &projDir,
                           const Options &options)
{
    QString srcFun = "FusionPMaxBasic::fuse";
    QString ext = ".png";
    QString alignedDir = projDir + "/Aligned/";
    QString method = "FusionPMaxBasic";
    QFileInfo srcInfo(paths.last());
    QString srcFolder = srcInfo.dir().absolutePath();
    QString base = srcInfo.baseName();

    qDebug() << srcFun << "projDir         =" << projDir;
    qDebug() << srcFun << "ext             =" << ext;
    qDebug() << srcFun << "alignedDir      =" << alignedDir;
    qDebug() << srcFun << "method          =" << method;
    qDebug() << srcFun << "srcFolder       =" << srcFolder;
    qDebug() << srcFun << "base            =" << base;

    const QString src = name();
    qDebug() << srcFun << "src             =" << src;
    emit updateStatus(false, "Starting FusionPMaxBasic...", src);

    if (paths.isEmpty()) {
        emit updateStatus(false, "FusionPMaxBasic: No input images.", src);
        return false;
    }

    QDir out(projDir);
    if (!out.exists() && !out.mkpath(".")) {
        emit updateStatus(false, "FusionPMaxBasic: Could not create output folder.", src);
        return false;
    }

    PetteriPipeline pipeline;

    // align
    if (options.align) {
        bool ok = pipeline.runAlignment(paths, alignedDir);
        if (!ok) {
            emit updateStatus(false, "FusionPMaxBasic::runAlignment FAILED", src);
            return false;
        }
    }

    // align and fuse
    if (options.alignFuse) {
        QString fusedDir = projDir + "/Fused/";
        QString fusedName = "Fused_" + method + "_" + base + ext;
        QString fusedImagePath = fusedDir + "/" + fusedName;

        bool ok = pipeline.runFusion(alignedDir, fusedDir, fusedImagePath);
        if (!ok) {
            emit updateStatus(false, "FusionPMaxBasic::runAlignment FAILED", src);
            return false;
        }
    }



    // bool ok = pipeline.runFull(paths,
    //                            projDir,
    //                            fusedImagePath,
    //                            depthMapPath);

    // if (!ok) {
    //     emit updateStatus(false, "FusionPMaxBasic FAILED", src);
    //     return false;
    // }

    emit updateStatus(false, "FusionPMaxBasic finished.", src);
    return true;
}
