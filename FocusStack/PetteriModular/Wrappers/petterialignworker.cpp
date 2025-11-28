#include "PetteriAlignWorker.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include "Main/global.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>

#include "PetteriModular/Core/logger.h"
#include "PetteriModular/Core/worker.h"
#include "PetteriModular/Align/align.h"
#include "PetteriModular/Core/grayscale.h"
#include "PetteriModular/IO/loadimg.h"

using namespace FStack;

PetteriAlignWorker::PetteriAlignWorker(QObject *parent)
    : QObject(parent)
{
}

void PetteriAlignWorker::setInput(const QStringList &sourcePaths,
                                  const QStringList &alignedPaths,
                                  const QStringList &alignedGrayPaths,
                                  const QString &alignDir,
                                  const QString &grayDir,
                                  const QString &srcExt)
{
    m_sourcePaths      = sourcePaths;
    m_alignedPaths     = alignedPaths;
    m_alignedGrayPaths = alignedGrayPaths;
    m_alignDir         = alignDir;
    m_grayDir          = grayDir;
    m_srcExt           = srcExt;
}

bool PetteriAlignWorker::run(const std::function<bool()> &abortFn)
{
    const QString src = "PetteriAlignWorker::run";

    if (m_sourcePaths.isEmpty()) {
        emit updateStatus(false,
                          "No images provided for alignment",
                          src);
        return false;
    }

    if (m_alignedPaths.size() != m_sourcePaths.size() ||
        m_alignedGrayPaths.size() != m_sourcePaths.size()) {
        qWarning() << src << "path lists not sized correctly";
        return false;
    }

    QDir().mkpath(m_alignDir);
    QDir().mkpath(m_grayDir);

    auto logger = std::make_shared<FStack::Logger>();
    logger->set_level(FStack::Logger::LOG_INFO);

    G::log(src, "logger->set_level");

    FStack::Worker worker(std::thread::hardware_concurrency(), logger);

    const int total = m_sourcePaths.size();

    std::vector<std::shared_ptr<Task_LoadImg>>   loadColor;
    std::vector<std::shared_ptr<Task_Grayscale>> loadGray;
    std::vector<std::shared_ptr<Task_Align>>     alignTasks;

    G::log(src, "loadColor, loadGray, alignTasks");

    loadColor.reserve(total);
    G::log(src, "loadColor");
    loadGray.reserve(total);
    G::log(src, "loadGray");
    alignTasks.reserve(total);
    G::log(src, "alignTasks");

    msg = "Creating gray-scale images...";
    emit updateStatus(false, msg, src);
    G::log(src, msg);

    std::shared_ptr<Task_Grayscale> prevGray = nullptr;

    // ---- Build load + grayscale tasks ------------------------------------
    for (int i = 0; i < total; ++i) {
        // Color load
        auto colorTask = std::make_shared<Task_LoadImg>(m_sourcePaths[i].toStdString());

        // per-task progress callback
        colorTask->setProgressCallback([this]() {
            G::log("PetteriAlignWorker::run 1", "colorTask");
            // step(); // PipelinePMax::incrementProgress via m_stepFn
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        });

        loadColor.push_back(colorTask);
        worker.add(colorTask);

        // Grayscale
        auto grayTask = std::make_shared<Task_Grayscale>(colorTask, prevGray);

        grayTask->setProgressCallback([this]() {
            G::log("PetteriAlignWorker::run 2", "grayTask");
            // step();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        });

        loadGray.push_back(grayTask);
        worker.add(grayTask);

        prevGray = grayTask;
    }

    msg = "Aligning images...";
    emit updateStatus(false, msg, src);
    G::log(src, msg);

    std::shared_ptr<Task_Align> prevAlign = nullptr;

    // ---- Build alignment tasks -------------------------------------------
    for (int i = 0; i < total; ++i) {
        std::shared_ptr<Task_Align> stacked = nullptr;
        if (i > 0)
            stacked = prevAlign;

        auto refGray  = (i == 0 ? loadGray[i]  : loadGray[i - 1]);
        auto refColor = (i == 0 ? loadColor[i] : loadColor[i - 1]);

        auto task = std::make_shared<Task_Align>(
            refGray,
            refColor,
            loadGray[i],
            loadColor[i],
            nullptr,    // initial guess
            stacked,    // stacked transform
            FocusStack::ALIGN_DEFAULT
            );

        // per-task progress callback for the *heavy* ECC alignment
        task->setProgressCallback([this, task]() {
            const QString msg = QString::fromStdString(task->name());
            G::log("PetteriAlignWorker::run 3", msg);
            step();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        });

        alignTasks.push_back(task);
        worker.add(task);

        prevAlign = task;
        msg = "Built alignment tasks for image #" + QString::number(i);
        G::log(src, msg);
    }

    // Optional: keep or simplify worker-level progress
    worker.progress_callback = [this](int /*p*/, int /*t*/) {
        // We *already* get finer-grain progress via Task::step(),
        // so this can be a no-op or used for coarse milestones.
        // step();
        // QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    };

    msg = "Completing alignment...";
    // emit updateStatus(false, msg, src);
    G::log(src, msg);

    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);

    if (!worker.wait_all(-1)) {
        emit updateStatus(false, QString("Alignment failed: %1")
                                     .arg(QString::fromStdString(worker.error())),
                          src);
        return false;
    }

    msg = "Saving aligned and grayscale images...";
    emit updateStatus(false, msg, src);
    G::log(src, msg);

    int current = 0;

    for (int i = 0; i < total; ++i) {
        // Save aligned color image
        cv::Mat alignedColor = alignTasks[i]->img();
        if (alignedColor.empty()) {
            qWarning() << "Empty aligned image at index" << i;
            return false;
        }
        cv::imwrite(m_alignedPaths[i].toStdString(), alignedColor);

        // Save grayscale image (convert to 8-bit)
        cv::Mat g = loadGray[i]->img();
        if (g.empty()) {
            qWarning() << "Empty grayscale image at index" << i;
            return false;
        }
        cv::Mat g8;
        if (g.type() != CV_8U)
            g.convertTo(g8, CV_8U);
        else
            g8 = g;

        cv::imwrite(m_alignedGrayPaths[i].toStdString(), g8);

        G::log(src, "Finished alignment");
        step();  // emit progress(++current);
        if (abortFn && abortFn()) {
            msg = "Abort requested during alignment saving";
            emit updateStatus(false, msg, src);
            return false;
        }
    }

    msg = "Alignment complete";
    emit updateStatus(false, msg, src);
    G::log(src, msg);
    return true;
}

// bool PetteriAlignWorker::run(const std::function<bool()> &abortFn)
// {
//     const QString src = "PetteriAlignWorker::run";

//     if (m_sourcePaths.isEmpty()) {
//         emit updateStatus(false,
//                           "No images provided for alignment",
//                           src);
//         return false;
//     }

//     if (m_alignedPaths.size() != m_sourcePaths.size() ||
//         m_alignedGrayPaths.size() != m_sourcePaths.size()) {
//         qWarning() << src << "path lists not sized correctly";
//         return false;
//     }

//     QDir().mkpath(m_alignDir);
//     QDir().mkpath(m_grayDir);

//     auto logger = std::make_shared<FStack::Logger>();
//     logger->set_level(FStack::Logger::LOG_INFO);

//     G::log(src, "logger->set_level");

//     FStack::Worker worker(std::thread::hardware_concurrency(), logger);

//     const int total = m_sourcePaths.size();

//     std::vector<std::shared_ptr<Task_LoadImg>>   loadColor;
//     std::vector<std::shared_ptr<Task_Grayscale>> loadGray;
//     std::vector<std::shared_ptr<Task_Align>>     alignTasks;

//     G::log(src, "loadColor, loadGray, alignTasks");

//     loadColor.reserve(total);
//     G::log(src, "loadColor");
//     loadGray.reserve(total);
//     G::log(src, "loadGray");
//     alignTasks.reserve(total);
//     G::log(src, "alignTasks");

//     msg = "Loading images...";
//     emit updateStatus(false, msg, src);
//     G::log(src, msg);

//     std::shared_ptr<Task_Grayscale> prevGray = nullptr;

//     for (int i = 0; i < total; ++i) {
//         auto colorTask = std::make_shared<Task_LoadImg>(m_sourcePaths[i].toStdString());
//         loadColor.push_back(colorTask);
//         worker.add(colorTask);

//         auto grayTask = std::make_shared<Task_Grayscale>(colorTask, prevGray);
//         loadGray.push_back(grayTask);
//         worker.add(grayTask);

//         prevGray = grayTask;
//     }

//     msg = "Building alignment tasks...";
//     emit updateStatus(false, msg, src);
//     G::log(src, msg);

//     std::shared_ptr<Task_Align> prevAlign = nullptr;

//     for (int i = 0; i < total; ++i) {
//         std::shared_ptr<Task_Align> stacked = nullptr;
//         if (i > 0)
//             stacked = prevAlign;

//         auto refGray  = (i == 0 ? loadGray[i]  : loadGray[i - 1]);
//         auto refColor = (i == 0 ? loadColor[i] : loadColor[i - 1]);

//         auto task = std::make_shared<Task_Align>(
//             refGray,
//             refColor,
//             loadGray[i],
//             loadColor[i],
//             nullptr,    // initial guess
//             stacked,    // stacked transform
//             FocusStack::ALIGN_DEFAULT
//             );

//         alignTasks.push_back(task);
//         worker.add(task);

//         prevAlign = task;
//         msg = "Built alignment tasks for image #" + QString::number(i);
//         G::log(src, msg);
//     }

//     worker.progress_callback = [this](int p, int t) {
//         step();  // emit progress(p);
//         QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
//     };

//     msg = "Running alignment...";
//     emit updateStatus(false, msg, src);
//     G::log(src, msg);

//     QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
//     if (!worker.wait_all(-1)) {
//         emit updateStatus(false, QString("Alignment failed: %1")
//                   .arg(QString::fromStdString(worker.error())),
//                   src);
//         return false;
//     }

//     msg = "Saving aligned and grayscale images...";
//     emit updateStatus(false, msg, src);
//     G::log(src, msg);

//     int current = 0;

//     for (int i = 0; i < total; ++i) {
//         // Save aligned color image
//         cv::Mat alignedColor = alignTasks[i]->img();
//         if (alignedColor.empty()) {
//             qWarning() << "Empty aligned image at index" << i;
//             return false;
//         }
//         cv::imwrite(m_alignedPaths[i].toStdString(), alignedColor);

//         // Save grayscale image (convert to 8-bit)
//         cv::Mat g = loadGray[i]->img();
//         if (g.empty()) {
//             qWarning() << "Empty grayscale image at index" << i;
//             return false;
//         }
//         cv::Mat g8;
//         if (g.type() != CV_8U)
//             g.convertTo(g8, CV_8U);
//         else
//             g8 = g;

//         cv::imwrite(m_alignedGrayPaths[i].toStdString(), g8);

//         step();  // emit progress(++current);
//         if (abortFn && abortFn()) {
//             msg = "Abort requested during alignment saving";
//             emit updateStatus(false, msg, src);
//             return false;
//         }
//     }

//     msg = "Alignment complete";
//     emit updateStatus(false, msg, src);
//     G::log(src, msg);
//     return true;
// }

