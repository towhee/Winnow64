#include "PetteriFocusMapsWorker.h"

#include <QDir>
#include <QDebug>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "PetteriModular/Core/logger.h"
#include "PetteriModular/Core/worker.h"
#include "PetteriModular/IO/loadimg.h"
#include "PetteriModular/Focus/focusmeasure.h"

using namespace FStack;

PetteriFocusMapsWorker::PetteriFocusMapsWorker(QObject *parent)
    : QObject(parent)
{
}

void PetteriFocusMapsWorker::setInput(const QStringList &alignedPaths,
                                      const QStringList &focusMapPaths,
                                      const QString &focusDir)
{
    m_alignedPaths   = alignedPaths;
    m_focusMapPaths  = focusMapPaths;
    m_focusDir       = focusDir;
}

bool PetteriFocusMapsWorker::run(const std::function<bool()> &abortFn)
{
    const QString src = "PetteriFocusMapsWorker::run";

    if (m_alignedPaths.isEmpty()) {
        emit updateStatus(false, "No aligned images for focus maps", src);
        return false;
    }

    if (m_focusMapPaths.size() != m_alignedPaths.size()) {
        qWarning() << src << "focus map paths list not sized correctly";
        return false;
    }

    QDir().mkpath(m_focusDir);

    auto logger = std::make_shared<Logger>();
    logger->set_level(Logger::LOG_INFO);

    Worker worker(std::thread::hardware_concurrency(), logger);

    const int total = m_alignedPaths.size();

    std::vector<std::shared_ptr<Task_LoadImg>>     loads;
    std::vector<std::shared_ptr<Task_FocusMeasure>> focusTasks;

    loads.reserve(total);
    focusTasks.reserve(total);

    emit updateStatus(false, "Loading aligned images for focus measure...", src);

    for (int i = 0; i < total; ++i) {
        auto load = std::make_shared<Task_LoadImg>(m_alignedPaths[i].toStdString());
        loads.push_back(load);
        worker.add(load);

        // Default radius/threshold from Petteri's Task_FocusMeasure
        auto fm = std::make_shared<Task_FocusMeasure>(load, 0.0f, 0.0f);
        focusTasks.push_back(fm);
        worker.add(fm);
    }

    worker.progress_callback = [this](int p, int t) {
        emit progress(p, t);
    };

    emit updateStatus(false, "Computing focus measures...", src);

    if (!worker.wait_all(-1)) {
        emit updateStatus(false, QString("Focus measure failed: %1")
                              .arg(QString::fromStdString(worker.error())),
                               src);
        return false;
    }

    emit updateStatus(false, "Saving focus maps...", src);

    int current = 0;
    for (int i = 0; i < total; ++i) {
        cv::Mat fm = focusTasks[i]->img();
        if (fm.empty()) {
            qWarning() << src << "empty focus map at index" << i;
            return false;
        }

        // Normalize for visualization & downstream use
        cv::Mat fm8;
        double minv, maxv;
        cv::minMaxLoc(fm, &minv, &maxv);
        if (maxv <= minv) {
            fm8 = cv::Mat::zeros(fm.size(), CV_8U);
        } else {
            fm.convertTo(fm8, CV_8U, 255.0 / (maxv - minv), -minv * 255.0 / (maxv - minv));
        }

        cv::imwrite(m_focusMapPaths[i].toStdString(), fm8);

        emit progress(++current, total);
        if (abortFn && abortFn()) {
            emit updateStatus(false,
                              "Abort requested during focus map saving",
                              src);
            return false;
        }
    }

    emit updateStatus(false, "Focus map stage complete", src);
    return true;
}



// #include "petterifocusmapsworker.h"

// PetteriFocusMapsWorker::PetteriFocusMapsWorker(QObject *parent)
//     : QObject(parent)
// {
// }

// void PetteriFocusMapsWorker::setInput(const QStringList &alignedPaths,
//                                       const QString &outputFolder)
// {
//     m_paths = alignedPaths;
//     m_outputFolder = outputFolder;
// }

// bool PetteriFocusMapsWorker::run(const std::function<bool()> &abortFn)
// {
//     if (!abortFn) {
//         qWarning() << "Abort function missing";
//         return false;
//     }

//     const QString src = "PetteriFocusMapsWorker::run";

//     if (m_paths.isEmpty()) {
//         emit updateStatus(true, "No aligned images provided", src);
//         return false;
//     }

//     QDir().mkpath(m_outputFolder);

//     // Logger
//     auto logger = std::make_shared<FStack::Logger>();
//     logger->set_level(FStack::Logger::LOG_INFO);

//     // Thread pool
//     FStack::Worker worker(std::thread::hardware_concurrency(), logger);

//     const int total = m_paths.size();
//     int current = 0;

//     std::vector<std::shared_ptr<FStack::Task_LoadImg>> loadTasks;
//     std::vector<std::shared_ptr<FStack::Task_FocusMeasure>> fmTasks;

//     loadTasks.reserve(total);
//     fmTasks.reserve(total);

//     emit updateStatus(false, "Loading aligned images...", src);

//     // Build task graph
//     for (int i = 0; i < total; ++i) {
//         std::string file = m_paths[i].toStdString();

//         auto load = std::make_shared<FStack::Task_LoadImg>(file);
//         loadTasks.push_back(load);
//         worker.add(load);

//         // radius = 0, threshold = 0 per default Petteri settings
//         auto fm = std::make_shared<FStack::Task_FocusMeasure>(load, 0.0f, 0.0f);
//         fmTasks.push_back(fm);
//         worker.add(fm);
//     }

//     // Progress callback
//     worker.progress_callback = [&](int p, int t){
//         emit progress(p, t);
//     };

//     emit updateStatus(false, "Computing focus maps...", src);

//     if (!worker.wait_all(-1)) {
//         emit updateStatus(true,
//                           QString("Focus measure failed: %1")
//                               .arg(QString::fromStdString(worker.error())),
//                           src);
//         return false;
//     }

//     // Save results
//     emit updateStatus(false, "Saving focus map outputs...", src);

//     for (int i = 0; i < total; ++i) {
//         QString base = QFileInfo(m_paths[i]).completeBaseName();
//         QString out = QString("%1/focusmeasure_%2.png")
//                           .arg(m_outputFolder)
//                           .arg(base);

//         cv::imwrite(out.toStdString(), fmTasks[i]->img());

//         emit progress(++current, total);
//     }

//     emit updateStatus(false, "Focus map computation complete", src);
//     emit finished(m_outputFolder);

//     return true;
// }
