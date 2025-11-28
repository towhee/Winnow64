#include "PetteriPMaxFusionWorker.h"

#include <QDir>
#include <QDebug>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include "PetteriModular/Core/logger.h"
#include "PetteriModular/Core/worker.h"
#include "PetteriModular/IO/loadimg.h"
#include "PetteriModular/Wave/wavelet.h"
#include "PetteriModular/Wave/wavelet_opencl.h"
#include "PetteriModular/Fusion/merge.h"
#include "PetteriModular/Fusion/reassign.h"

using namespace FStack;

PetteriPMaxFusionWorker::PetteriPMaxFusionWorker(QObject *parent)
    : QObject(parent)
{
}

void PetteriPMaxFusionWorker::setInput(const QStringList &alignedColorPaths,
                                       const QStringList &alignedGrayPaths,
                                       const QString &fusionOutputPath,
                                       bool useOpenCL,
                                       int consistency)
{
    m_alignedColorPaths = alignedColorPaths;
    m_alignedGrayPaths  = alignedGrayPaths;
    m_fusionOutputPath  = fusionOutputPath;
    m_useOpenCL         = useOpenCL;
    m_consistency       = consistency;
}

bool PetteriPMaxFusionWorker::run(const std::function<bool()> &abortFn)
{
    const QString src = "PetteriPMaxFusionWorker::run";

    if (m_alignedColorPaths.isEmpty() ||
        m_alignedGrayPaths.isEmpty() ||
        m_alignedColorPaths.size() != m_alignedGrayPaths.size())
    {
        emit updateStatus(false,
                          "Invalid input for PMax fusion",
                          src);
        return false;
    }

    QDir().mkpath(QFileInfo(m_fusionOutputPath).absolutePath());

    auto logger = std::make_shared<Logger>();
    logger->set_level(Logger::LOG_INFO);

    Worker worker(std::thread::hardware_concurrency(), logger);

    const int total = m_alignedColorPaths.size();

    std::vector<std::shared_ptr<Task_LoadImg>> colorLoads;
    std::vector<std::shared_ptr<Task_LoadImg>> grayLoads;
    std::vector<std::shared_ptr<ImgTask>>      waveletTasks;  // can be Task_Wavelet or Task_Wavelet_OpenCL

    colorLoads.reserve(total);
    grayLoads.reserve(total);
    waveletTasks.reserve(total);

    QString msg = "Loading aligned color and grayscale images for fusion...";
    emit updateStatus(false, msg, src);

    for (int i = 0; i < total; ++i) {
        auto c = std::make_shared<Task_LoadImg>(m_alignedColorPaths[i].toStdString());
        auto g = std::make_shared<Task_LoadImg>(m_alignedGrayPaths[i].toStdString());

        colorLoads.push_back(c);
        grayLoads.push_back(g);

        worker.add(c);
        worker.add(g);
    }

    msg = "Building wavelet decomposition tasks...";
    // emit updateStatus(false, msg, src);

    for (int i = 0; i < total; ++i) {
        std::shared_ptr<ImgTask> w;

        if (m_useOpenCL) {
            w = std::make_shared<Task_Wavelet_OpenCL>(grayLoads[i], false);
        } else {
            w = std::make_shared<Task_Wavelet>(grayLoads[i], false);
        }

        waveletTasks.push_back(w);
        worker.add(w);
    }

    msg = "Merging images ...";
    emit updateStatus(false, msg, src);

    // Single merge over all wavelet images
    auto mergeTask = std::make_shared<Task_Merge>(
        nullptr,                // prev_merge
        waveletTasks,
        m_consistency);

    worker.add(mergeTask);

    // Wavelet inverse composition
    std::shared_ptr<ImgTask> invWave;
    if (m_useOpenCL) {
        invWave = std::make_shared<Task_Wavelet_OpenCL>(mergeTask, true);
    } else {
        invWave = std::make_shared<Task_Wavelet>(mergeTask, true);
    }
    worker.add(invWave);

    std::vector<std::shared_ptr<ImgTask>> grayImgs;
    std::vector<std::shared_ptr<ImgTask>> colorImgs;

    grayImgs.reserve(grayLoads.size());
    colorImgs.reserve(colorLoads.size());

    for (auto &g : grayLoads)
        grayImgs.push_back(g);      // Task_Grayscale IS an ImgTask

    for (auto &c : colorLoads)
        colorImgs.push_back(c);     // Task_LoadImg IS an ImgTask

    // Color reassignment map
    auto mapTask = std::make_shared<Task_Reassign_Map>(
        grayImgs,
        colorImgs,
        nullptr     // no old map
        );

    worker.add(mapTask);

    // Final reassignment to color
    auto reassignTask = std::make_shared<Task_Reassign>(mapTask, invWave);
    worker.add(reassignTask);

    worker.progress_callback = [this](int p, int t) {
        step();  // emit progress
    };

    // emit updateStatus(false, "Running PMax fusion tasks...", src);

    if (!worker.wait_all(-1)) {
        msg = QString("PMax fusion failed: %1")
                  .arg(QString::fromStdString(worker.error()));
        emit updateStatus(false, msg, src);
        return false;
    }

    cv::Mat fused = reassignTask->img();
    if (fused.empty()) {
        qWarning() << src << "empty fused image";
        return false;
    }

    cv::imwrite(m_fusionOutputPath.toStdString(), fused);

    emit updateStatus(false, "Finished fusion", src);
    return true;
}
