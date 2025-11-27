#include "PipelinePMax.h"
#include "Main/global.h"
#include <QDebug>

PipelinePMax::PipelinePMax(QObject *parent)
    : PipelineBase(parent)
{
    m_abort.store(false, std::memory_order_relaxed);
}

void PipelinePMax::setInput(const QStringList &paths)
{
    // Fixed pipeline name for this pipeline
    PipelineBase::setInput(paths, "PMax");

    // Prepare directories and path plans for THIS run
    prepareProjectStructure();
    prepareAlignmentPaths();
    prepareFocusMapPaths();
    prepareDepthPaths();
    prepareFusionPath();

    // set total progress points

}

void PipelinePMax::abort()
{
    m_abort.store(true, std::memory_order_relaxed);
}

void PipelinePMax::run()
{
    QString src = "PipelinePMax::run";
    m_abort.store(false, std::memory_order_relaxed);

    if (m_sourcePaths.isEmpty()) {
        emit finished(false, "No source images");
        return;
    }

    msg = "Starting PMax pipeline...";
    emit updateStatus(false, msg, src);
    G::log(src, msg);

    // Progress total
    int n = sourcePaths().count();
    m_total = 2 * n + 3;
    incrementProgress();

    if (!runAlignment()) {
        if (abortRequested())
            emit finished(false, "Aborted during alignment");
        else
            emit finished(false, "Alignment failed");
        return;
    }

    /* Not req'd for basic Petteri PMax
    if (!abortRequested() && !runFocusMaps()) {
        if (abortRequested())
            emit finished(false, "Aborted during focus maps");
        else
            emit finished(false, "Focus map stage failed");
        return;
    }

    if (!abortRequested() && !runDepthMap()) {
        if (abortRequested())
            emit finished(false, "Aborted during depth map");
        else
            emit finished(false, "Depth map stage failed");
        return;
    }
    */

    if (!abortRequested() && !runFusion()) {
        if (abortRequested())
            emit finished(false, "Aborted during fusion");
        else
            emit finished(false, "Fusion stage failed");
        return;
    }

    if (abortRequested()) {
        emit finished(false, "Aborted");
        return;
    }

    emit finished(true, fusionOutputPath());
}

bool PipelinePMax::runAlignment()
{
    QString src = "PipelinePMax::runAlignment";
    msg = "Running alignment...";
    emit updateStatus(false, "Running alignment...", src);
    G::log(src, msg);

    PetteriAlignWorker worker;
    worker.setInput(m_sourcePaths,
                    m_alignedPaths,
                    m_alignedGrayPaths,
                    m_alignDir,
                    m_grayDir,
                    m_srcExt);

    QObject::connect(&worker, &PetteriAlignWorker::updateStatus,
                     this, &PipelinePMax::updateStatus);

    // progress callback
    worker.setStepCallback([this]() {
        msg = "PipelinePMax::runAlignment worker.setStepCallback";
        G::log(msg);
        this->incrementProgress();
    });

    bool ok = worker.run([this]() { return abortRequested(); });

    return ok && !abortRequested();
}

bool PipelinePMax::runFocusMaps()
{
    QString src = "PipelinePMax::runFocusMaps";
    emit updateStatus(false, "Computing focus measures...", src);

    PetteriFocusMapsWorker worker;
    worker.setInput(m_alignedPaths,
                    m_focusMapPaths,
                    m_focusDir);

    QObject::connect(&worker, &PetteriFocusMapsWorker::updateStatus,
                     this, &PipelinePMax::updateStatus);

    // progress callback
    worker.setStepCallback([this]() {
        msg = "PipelinePMax::runFocusMaps worker.setStepCallback";
        G::log(msg);
        this->incrementProgress();
    });

    bool ok = worker.run([this]() { return abortRequested(); });

    return ok && !abortRequested();
}

bool PipelinePMax::runDepthMap()
{
    QString src = "PipelinePMax::runDepthMap";
    emit updateStatus(false, "Building depth map...", src);

    PetteriDepthMapWorker worker;
    worker.setInput(m_focusMapPaths,
                    m_depthRawPath,
                    m_depthFilteredPath,
                    m_depthDir,
                    /*saveSteps*/ false,
                    /*smoothXY*/ 32,
                    /*smoothZ*/  64,
                    /*haloRadius*/ 30);

    QObject::connect(&worker, &PetteriDepthMapWorker::updateStatus,
                     this, &PipelinePMax::updateStatus);
    // QObject::connect(&worker, &PetteriDepthMapWorker::progress,
    //                  this, &PipelinePMax::progress);
    // progress callback
    worker.setStepCallback([this]() {
        msg = "PipelinePMax::runDepthMap worker.setStepCallback";
        G::log(msg);
        this->incrementProgress();
    });

    bool ok = worker.run([this]() { return abortRequested(); });

    return ok && !abortRequested();
}

bool PipelinePMax::runFusion()
{
    QString src = "PipelinePMax::runFusion";
    emit updateStatus(false, "Running PMax fusion...", src);

    PetteriPMaxFusionWorker worker;
    worker.setInput(m_alignedPaths,
                    m_alignedGrayPaths,
                    fusionOutputPath(),
                    /*useOpenCL*/ true,
                    /*consistency*/ 1);

    QObject::connect(&worker, &PetteriPMaxFusionWorker::updateStatus,
                     this, &PipelinePMax::updateStatus);
    // QObject::connect(&worker, &PetteriPMaxFusionWorker::progress,
    //                  this, &PipelinePMax::progress);
    // progress callback
    worker.setStepCallback([this]() {
        msg = "PipelinePMax::runFusion worker.setStepCallback";
        G::log(msg);
        this->incrementProgress();
    });

    bool ok = worker.run([this]() { return abortRequested(); });

    return ok && !abortRequested();
}
