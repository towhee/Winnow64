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
        m_alignedColorPaths.size() != m_alignedGrayPaths.size()) {
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

    emit updateStatus(false,
                      "Building wavelet decomposition tasks...",
                      src);

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

    emit updateStatus(false,
                      "Building merge (PMax) task...",
                      src);

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
        step();  // emit progress(p, t);
    };

    emit updateStatus(false, "Running PMax fusion tasks...", src);

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

    emit updateStatus(false, "PMax fusion complete", src);
    return true;
}



// #include "petteripmaxfusionworker.h"
// #include <QDir>

// PetteriPMaxFusionWorker::PetteriPMaxFusionWorker(QObject *parent)
//     : QObject(parent)
// {
// }

// void PetteriPMaxFusionWorker::setInput(const QStringList &alignedPaths,
//                                        const QStringList &alignedGrayPaths,
//                                        const QString &outputFolder,
//                                        bool useOpenCL,
//                                        int consistency)
// {
//     m_colorPaths = alignedPaths;
//     m_grayPaths = alignedGrayPaths;
//     m_outputFolder = outputFolder;
//     m_useOpenCL = useOpenCL;
//     m_consistency = consistency;
// }

// bool PetteriPMaxFusionWorker::run(const std::function<bool()> &abortFn)
// {
//     if (!abortFn) {
//         qWarning() << "Abort function missing";
//         return false;
//     }


//     const QString src = "PetteriPMaxFusionWorker::run";

//     if (m_colorPaths.size() != m_grayPaths.size()) {
//         emit updateStatus(true, "Color and grayscale lists differ in length", src);
//         return false;
//     }
//     if (m_colorPaths.isEmpty()) {
//         emit updateStatus(true, "No aligned images provided", src);
//         return false;
//     }

//     QDir().mkpath(m_outputFolder);

//     auto logger = std::make_shared<FStack::Logger>();
//     logger->set_level(FStack::Logger::LOG_INFO);

//     FStack::Worker worker(std::thread::hardware_concurrency(), logger);

//     int N = m_colorPaths.size();
//     emit updateStatus(false,
//                       QString("PMax fusion on %1 images...").arg(N), src);

//     // ---------------------------------------------------------
//     // 1) LOAD IMAGES
//     // ---------------------------------------------------------
//     std::vector<std::shared_ptr<FStack::Task_LoadImg>> colorLoad;
//     std::vector<std::shared_ptr<FStack::Task_LoadImg>> grayLoad;

//     colorLoad.reserve(N);
//     grayLoad.reserve(N);

//     for (int i = 0; i < N; i++) {
//         auto c = std::make_shared<FStack::Task_LoadImg>(m_colorPaths[i].toStdString());
//         auto g = std::make_shared<FStack::Task_LoadImg>(m_grayPaths[i].toStdString());
//         colorLoad.push_back(c);
//         grayLoad.push_back(g);
//         worker.add(c);
//         worker.add(g);
//     }

//     // ---------------------------------------------------------
//     // 2) WAVELET DECOMPOSITION (GPU or CPU)
//     // ---------------------------------------------------------
//     std::vector<std::shared_ptr<FStack::Task_Wavelet>> waveColor;
//     std::vector<std::shared_ptr<FStack::Task_Wavelet>> waveGray;

//     waveColor.reserve(N);
//     waveGray.reserve(N);

//     for (int i = 0; i < N; i++) {
//         std::shared_ptr<FStack::Task_Wavelet> wc;
//         std::shared_ptr<FStack::Task_Wavelet> wg;

//         if (m_useOpenCL) {
//             wc = std::make_shared<FStack::Task_Wavelet_OpenCL>(colorLoad[i], false);
//             wg = std::make_shared<FStack::Task_Wavelet_OpenCL>(grayLoad[i], false);
//         } else {
//             wc = std::make_shared<FStack::Task_Wavelet>(colorLoad[i], false);
//             wg = std::make_shared<FStack::Task_Wavelet>(grayLoad[i], false);
//         }

//         waveColor.push_back(wc);
//         waveGray.push_back(wg);

//         worker.add(wc);
//         worker.add(wg);
//     }

//     // ---------------------------------------------------------
//     // 3) MERGE WAVELETS (PMax core)
//     // ---------------------------------------------------------
//     std::shared_ptr<FStack::Task_Merge> prevMerge = nullptr;

//     emit updateStatus(false, "Merging wavelet subbands...", src);

//     for (int i = 0; i < N; i++) {
//         // vector of complex wavelet images to merge: here one at a time
//         std::vector<std::shared_ptr<FStack::ImgTask>> imgs;
//         imgs.push_back(waveColor[i]);

//         auto merge = std::make_shared<FStack::Task_Merge>(
//             prevMerge,
//             imgs,
//             m_consistency);

//         prevMerge = merge;
//         worker.add(merge);
//     }

//     // ---------------------------------------------------------
//     // 4) REASSIGN (color from gray→RGB)
//     // ---------------------------------------------------------
//     // Build incremental reassignment map
//     std::shared_ptr<FStack::Task_Reassign_Map> reassignMap = nullptr;

//     emit updateStatus(false, "Building reassignment map...", src);

//     {
//         std::vector<std::shared_ptr<FStack::ImgTask>> g;
//         std::vector<std::shared_ptr<FStack::ImgTask>> c;

//         g.reserve(N);
//         c.reserve(N);
//         for (int i = 0; i < N; i++) {
//             g.push_back(waveGray[i]);
//             c.push_back(colorLoad[i]);   // original 8UC3
//         }

//         auto map = std::make_shared<FStack::Task_Reassign_Map>(g, c, reassignMap);
//         reassignMap = map;
//         worker.add(map);
//     }

//     // Apply reassignment
//     auto finalReassign =
//         std::make_shared<FStack::Task_Reassign>(reassignMap, prevMerge);
//     worker.add(finalReassign);

//     // ---------------------------------------------------------
//     // 5) RUN
//     // ---------------------------------------------------------
//     worker.progress_callback = [&](int p, int t){
//         emit progress(p, t);
//     };

//     if (!worker.wait_all(-1)) {
//         emit updateStatus(true,
//                           QString("Fusion failed: %1")
//                               .arg(QString::fromStdString(worker.error())),
//                           src);
//         return false;
//     }

//     // ---------------------------------------------------------
//     // 6) SAVE OUTPUTS
//     // ---------------------------------------------------------

//     emit updateStatus(false, "Writing PMax outputs...", src);

//     // 6a: final RGB output
//     QString outFinal = m_outputFolder + "/pmax_final.png";
//     cv::imwrite(outFinal.toStdString(), finalReassign->img());

//     // 6b: depthmap from last merge
//     QString outDepth = m_outputFolder + "/pmax_depthmap.png";
//     cv::imwrite(outDepth.toStdString(), prevMerge->depthmap());

//     // 6c (optional): complex wavelet result
//     QString outComplex = m_outputFolder + "/pmax_complex.png";
//     cv::Mat complex;
//     prevMerge->img().convertTo(complex, CV_32F);
//     cv::imwrite(outComplex.toStdString(), complex);

//     emit updateStatus(false, "PMax fusion complete", src);
//     emit finished(m_outputFolder);

//     return true;
// }
