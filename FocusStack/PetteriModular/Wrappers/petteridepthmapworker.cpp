#include "PetteriDepthMapWorker.h"

#include <QDir>
#include <QDebug>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include "PetteriModular/Core/logger.h"
#include "PetteriModular/Core/worker.h"
#include "PetteriModular/IO/loadimg.h"
#include "PetteriModular/Depth/depthmap.h"
#include "PetteriModular/Depth/depthmap_inpaint.h"

using namespace FStack;

PetteriDepthMapWorker::PetteriDepthMapWorker(QObject *parent)
    : QObject(parent)
{
}

void PetteriDepthMapWorker::setInput(const QStringList &focusMapPaths,
                                     const QString &depthRawPath,
                                     const QString &depthFilteredPath,
                                     const QString &depthDir,
                                     bool saveSteps,
                                     int smoothXY,
                                     int smoothZ,
                                     int haloRadius)
{
    m_focusMapPaths     = focusMapPaths;
    m_depthRawPath      = depthRawPath;
    m_depthFilteredPath = depthFilteredPath;
    m_depthDir          = depthDir;
    m_saveSteps         = saveSteps;
    m_smoothXY          = smoothXY;
    m_smoothZ           = smoothZ;
    m_haloRadius        = haloRadius;
}

bool PetteriDepthMapWorker::run(const std::function<bool()> &abortFn)
{
    const QString src = "PetteriDepthMapWorker::run";

    if (m_focusMapPaths.isEmpty()) {
        emit updateStatus(false,
                          "No focus maps for depth map stage",
                          src);
        return false;
    }

    QDir().mkpath(m_depthDir);

    auto logger = std::make_shared<Logger>();
    logger->set_level(Logger::LOG_INFO);

    Worker worker(std::thread::hardware_concurrency(), logger);

    const int total = m_focusMapPaths.size();

    std::vector<std::shared_ptr<Task_LoadImg>> loadTasks;
    std::vector<std::shared_ptr<Task_Depthmap>> depthTasks;

    loadTasks.reserve(total);
    depthTasks.reserve(total);

    emit updateStatus(false,
                      "Loading focus maps for depth computation...",
                      src);

    std::shared_ptr<Task_Depthmap> prevDepth = nullptr;

    for (int i = 0; i < total; ++i) {
        auto load = std::make_shared<Task_LoadImg>(m_focusMapPaths[i].toStdString());
        loadTasks.push_back(load);
        worker.add(load);

        bool last = (i == total - 1);
        auto depth = std::make_shared<Task_Depthmap>(
            load,
            /*depth*/ i + 1,
            last,
            prevDepth,
            m_saveSteps);

        depthTasks.push_back(depth);
        worker.add(depth);

        prevDepth = depth;
    }

    worker.progress_callback = [this](int p, int t) {
        emit progress(p, t);
    };

    emit updateStatus(false,
                      "Computing depth map (Gaussian fit)...",
                      src);

    if (!worker.wait_all(-1)) {
        emit updateStatus(false, QString("Depth map computation failed: %1")
                          .arg(QString::fromStdString(worker.error())),
                          src);
        return false;
    }

    std::shared_ptr<Task_Depthmap> finalDepth = depthTasks.back();

    // Save raw depth map (Gaussian mean)
    cv::Mat raw = finalDepth->depthmap().clone();
    if (raw.empty()) {
        qWarning() << src << "empty raw depth map";
        return false;
    }

    cv::imwrite(m_depthRawPath.toStdString(), raw);

    emit updateStatus(false,
                      "Inpainting and smoothing depth map...",
                      src);

    // Inpaint / filter stage
    auto depthInpaint = std::make_shared<Task_Depthmap_Inpaint>(
        finalDepth,
        /*threshold*/ 16,
        m_smoothXY,
        m_smoothZ,
        m_haloRadius,
        m_saveSteps);

    Worker worker2(std::thread::hardware_concurrency(), logger);
    worker2.add(depthInpaint);
    worker2.progress_callback = [this](int p, int t) {
        emit progress(p, t);
    };

    if (!worker2.wait_all(-1)) {
        emit updateStatus(false,
                          QString("Depth inpaint failed: %1")
                          .arg(QString::fromStdString(worker2.error())),
                          src);
        return false;
    }

    cv::Mat filtered = depthInpaint->img();
    if (filtered.empty()) {
        qWarning() << src << "empty filtered depth map";
        return false;
    }

    cv::imwrite(m_depthFilteredPath.toStdString(), filtered);

    emit updateStatus(false,"Depth map stage complete", src);
    return true;
}



// #include "petteridepthmapworker.h"
// #include <QDir>

// PetteriDepthMapWorker::PetteriDepthMapWorker(QObject *parent)
//     : QObject(parent)
// {
// }

// void PetteriDepthMapWorker::setInput(const QStringList &focusMapPaths,
//                                      const QString &outputFolder,
//                                      bool saveSteps,
//                                      int smoothXY,
//                                      int smoothZ,
//                                      int haloRadius)
// {
//     m_focusMaps = focusMapPaths;
//     m_outputFolder = outputFolder;
//     m_saveSteps = saveSteps;
//     m_smoothXY = smoothXY;
//     m_smoothZ = smoothZ;
//     m_haloRadius = haloRadius;
// }

// bool PetteriDepthMapWorker::run(const std::function<bool()> &abortFn)
// {
//     if (!abortFn) {
//         qWarning() << "Abort function missing";
//         return false;
//     }

//     const QString src = "PetteriDepthMapWorker::run";

//     if (m_focusMaps.isEmpty()) {
//         emit updateStatus(true, "No focus maps provided", src);
//         return false;
//     }

//     QDir().mkpath(m_outputFolder);

//     auto logger = std::make_shared<FStack::Logger>();
//     logger->set_level(FStack::Logger::LOG_INFO);

//     // Petteri's CPU worker pool
//     FStack::Worker worker(std::thread::hardware_concurrency(), logger);

//     int N = m_focusMaps.size();
//     emit updateStatus(false,
//                       QString("Building depthmap from %1 focus maps...").arg(N), src);

//     // -------------------------------------------------------
//     // 1) LOAD ALL FOCUS MEASURE IMAGES (already 32F)
//     // -------------------------------------------------------
//     std::vector<std::shared_ptr<FStack::Task_LoadImg>> loaders;
//     loaders.reserve(N);

//     for (int i = 0; i < N; i++) {
//         auto lm = std::make_shared<FStack::Task_LoadImg>(m_focusMaps[i].toStdString());
//         loaders.push_back(lm);
//         worker.add(lm);
//     }

//     // -------------------------------------------------------
//     // 2) DEPTHMAP LAYERING: depth 0 → last
//     // -------------------------------------------------------
//     std::shared_ptr<FStack::Task_Depthmap> prev = nullptr;

//     for (int i = 0; i < N; i++) {
//         bool last = (i == N - 1);
//         auto dm = std::make_shared<FStack::Task_Depthmap>(
//             loaders[i],
//             i,          // depth index
//             last,
//             prev,       // previous depthmap
//             m_saveSteps);

//         prev = dm;
//         worker.add(dm);
//     }

//     // -------------------------------------------------------
//     // 3) INPAINT FINAL DEPTHMAP
//     // -------------------------------------------------------
//     emit updateStatus(false, "Inpainting depthmap...", src);

//     auto inpaint = std::make_shared<FStack::Task_Depthmap_Inpaint>(
//         prev,           // last Task_Depthmap
//         16,             // threshold
//         m_smoothXY,
//         m_smoothZ,
//         m_haloRadius,
//         m_saveSteps);

//     worker.add(inpaint);

//     // -------------------------------------------------------
//     // 4) RUN WORKER
//     // -------------------------------------------------------
//     worker.progress_callback = [&](int p, int t) {
//         emit progress(p, t);
//     };

//     if (!worker.wait_all(-1)) {
//         emit updateStatus(true,
//                           QString("Depthmap failed: %1")
//                               .arg(QString::fromStdString(worker.error())),
//                           src);
//         return false;
//     }

//     // -------------------------------------------------------
//     // 5) SAVE OUTPUT DEPTHMAPS
//     // -------------------------------------------------------

//     QString outDepth = m_outputFolder + "/depth_index.png";
//     cv::imwrite(outDepth.toStdString(), prev->depthmap());

//     QString outInpaint = m_outputFolder + "/depth_inpaint.png";
//     cv::imwrite(outInpaint.toStdString(), inpaint->img());

//     emit updateStatus(false, "Depthmap construction complete", src);
//     emit finished(m_outputFolder);

//     return true;
// }
