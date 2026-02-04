#include "FSRunner.h"
// #include "FocusStack/Pipeline/PipelineBase.h"

#include <QDebug>
#include <opencv2/imgcodecs.hpp>

FSRunner::FSRunner(PipelineBase *pipeline, QObject *parent)
    : QObject(parent),
    m_pipeline(pipeline)
{
}

void FSRunner::emitStatus(bool isError, const QString &msg, const QString &src, bool enabled)
{
    if (enabled)
        emit updateStatus(isError, msg, src);
}

void FSRunner::emitProgress(int current, int total, bool enabled)
{
    if (enabled)
        emit progress(current, total);
}

bool FSRunner::runAlign(const QStringList &paths,
                        const std::vector<QString> &alignedColorPaths,
                        const std::vector<QString> &alignedGrayPaths,
                        bool emitSignals)
{
    const QString srcFun = "FSRunner::runAlign";

    if (paths.isEmpty())
    {
        emitStatus(true, "No input paths", srcFun, emitSignals);
        return false;
    }

    int N = paths.size();
    emitStatus(false,
               QString("Running FS alignment on %1 images...").arg(N),
               srcFun,
               emitSignals);

    // ---------------------------------------------------------------------
    // Load all images with Petteri-compatible padding + validArea
    // ---------------------------------------------------------------------
    std::vector<FSLoader::Image> imgs(N);

    for (int i = 0; i < N; ++i)
    {
        try {
            imgs[i] = FSLoader::load(paths[i].toStdString());
        }
        catch (std::exception &e)
        {
            emitStatus(true, QString("Error loading %1: %2")
                           .arg(paths[i], e.what()),
                           srcFun, emitSignals);
            return false;
        }

        emitProgress(i + 1, N, emitSignals);
    }

    // ---------------------------------------------------------------------
    // Prepare FSAlign global transform chain
    // ---------------------------------------------------------------------
    using namespace FSAlign;

    std::vector<Result> globals(N);
    globals[0] = makeIdentity(imgs[0].validArea); // reference frame

    Options opt;
    opt.matchContrast     = true;
    opt.matchWhiteBalance = true;
    opt.lowRes            = 256;
    opt.maxRes            = 2048;
    opt.fullResolution    = false;

    // ---------------------------------------------------------------------
    // Reference image (index 0): save unmodified aligned + gray copies
    // ---------------------------------------------------------------------
    {
        cv::imwrite(alignedColorPaths[0].toStdString(), imgs[0].color);
        cv::imwrite(alignedGrayPaths[0].toStdString(), imgs[0].gray);
    }

    emitProgress(1, N, emitSignals);

    // ---------------------------------------------------------------------
    // Pairwise + stacked alignment: image i → image (i-1)
    // ---------------------------------------------------------------------
    for (int i = 1; i < N; ++i)
    {
        emitStatus(false,
                   QString("Aligning %1 → %2")
                       .arg(paths[i], paths[i - 1]),
                   srcFun,
                   emitSignals);

        Result local;

        try {
            local = computeLocal(
                imgs[i - 1].gray,
                imgs[i - 1].color,
                imgs[i].gray,
                imgs[i].color,
                imgs[i].validArea,
                opt
                );
        }
        catch (std::exception &e)
        {
            emitStatus(true, QString("Alignment failed for %1: %2")
                           .arg(paths[i], e.what()),
                           srcFun, emitSignals);
            return false;
        }

        // stack with previous global (Petteri-style)
        globals[i] = accumulate(
            globals[i - 1],
            local,
            imgs[i].validArea
            );

        // apply transform → aligned color + gray
        cv::Mat alignedColor, alignedGray;

        applyTransform(imgs[i].color, globals[i].transform, alignedColor);
        applyTransform(imgs[i].gray,  globals[i].transform, alignedGray);

        // write results
        cv::imwrite(alignedColorPaths[i].toStdString(), alignedColor);
        cv::imwrite(alignedGrayPaths[i].toStdString(),  alignedGray);

        emitProgress(i + 1, N, emitSignals);
    }

    emitStatus(false, "Alignment complete.", srcFun, emitSignals);
    return true;
}
