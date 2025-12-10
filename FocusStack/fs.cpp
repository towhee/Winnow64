#include "FS.h"
#include "Main/global.h"
#include "FocusStack/fsalign.h"
#include "FocusStack/fsloader.h"
#include "FocusStack/fsfocus.h"
#include "FocusStack/fsdepth.h"
#include "FocusStack/fsfusion.h"
#include "FocusStack/fsfusionmerge.h"
#include "FocusStack/fsfusionreassign.h"
#include "FocusStack/fsfusionwavelet.h"
#include "FocusStack/fsloader.h"

#include <opencv2/imgcodecs.hpp>

#include <QDir>
#include <QFileInfo>
#include <QDebug>

FS::FS(QObject *parent)
    : QObject(parent)
{
}

void FS::abort()
{
    abortRequested = true;
}

void FS::setInput(const QStringList &paths)
{
    inputPaths = paths;

    // Clear any in-memory aligned images from previous run
    alignedColorMats.clear();
    alignedGrayMats.clear();
}

void FS::setProjectRoot(const QString &rootPath)
{
    projectRoot = rootPath;

    alignFolder = projectRoot + "/align";
    focusFolder = projectRoot + "/focus";
    depthFolder = projectRoot + "/depth";
    fusionFolder = projectRoot + "/fusion";
}

void FS::setOptions(const Options &opt)
{
    o = opt;
}

void FS::status(const QString &msg)
{
    emit updateStatus(false, msg, "");
}

bool FS::prepareFolders()
{
    if (projectRoot.isEmpty())
    {
        status("Project root not set");
        return false;
    }

    QDir dir;
    if (!dir.mkpath(projectRoot)) {
        status("Cannot create project root");
        return false;
    }

    dir.mkpath(alignFolder);
    dir.mkpath(focusFolder);
    dir.mkpath(depthFolder);
    dir.mkpath(fusionFolder);

    return true;
}

void FS::setExistance()
{
    for (const QString &src : inputPaths)
    {
        QFileInfo fi(src);
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix();

        QString col = alignFolder + "/aligned_" + base + "." + ext;
        QString gry = alignFolder + "/gray_" + base + "." + ext;

        alignedColorPaths.push_back(col);
        alignedGrayPaths.push_back(gry);

        // // set allAlignedExist
        // {
        //     if (!QFileInfo::exists(col) || !QFileInfo::exists(gry))
        //         allAlignedExist = false;
        // }
        // else
        // {
        //     QFile::remove(col);
        //     QFile::remove(gry);
        // }
    }
}

// setParameters() — checks for existing outputs
bool FS::setParameters()
/*
    - Determine which stages can be skipped based on existing outputs.
    - If keepIntermediates == false, do not skip based on files and remove
      stale intermediates.
*/
{
    skipAlign     = false;
    skipFocusMaps = false;
    skipDepthMap  = false;
    skipFusion    = false;

    alignedColorPaths.clear();
    alignedGrayPaths.clear();

    const bool keep = o.keepIntermediates;

    // Do all the aligned (color + grayscale) exist?
    for (const QString &src : inputPaths)
    {
        QFileInfo fi(src);
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix();

        QString col = alignFolder + "/aligned_" + base + "." + ext;
        QString gry = alignFolder + "/gray_" + base + "." + ext;

        alignedColorPaths.push_back(col);
        alignedGrayPaths.push_back(gry);

        if (!QFileInfo::exists(col) || !QFileInfo::exists(gry))
            alignExists = false;
    }

    if (!o.overwriteAlign && alignExists) skipAlign = true;
    if (!o.overwriteFocusMaps && focusMapsExist) skipFocusMaps = true;
    if (!o.overwriteDepthMap && depthMapExists) skipDepthMap = true;
    if (!o.overWriteFusion && fusionExists) skipFusion = true;

    return true;
}

bool FS::validAlignMatsAvailable(int count) const
/*
    - Validate that in-memory aligned color and gray Mats exist
    - Validate the vector sizes match 'count'
    - Validate that no Mat is empty
*/
{
    if (alignedGrayMats.size()  != static_cast<size_t>(count) ||
        alignedColorMats.size() != static_cast<size_t>(count))
    {
        return false;
    }

    for (int i = 0; i < count; ++i)
    {
        if (alignedGrayMats[i].empty() || alignedColorMats[i].empty())
            return false;
    }

    return true;
}

void FS::setTotalProgress()
{
    int n = inputPaths.count();
    progressTotal = 0;
    if (o.enableAlign && !skipAlign)
        progressTotal += n;
    if (o.enableFocusMaps && !skipFocusMaps)
        progressTotal += n;
    if (o.enableDepthMap && !skipDepthMap)
        progressTotal += n;
    if (o.enableFusion && !skipFusion) {
        progressTotal += (n + 4);
        if (validAlignMatsAvailable(n)) progressTotal --;
    }
}

void FS::incrementProgress()
{
    QString srcFun = "FS::incrementProgress";
    emit progress(++progressCount, progressTotal);
    // G::log(srcFun, QString::number(progressCount));
}

bool FS::run()
{
    QString srcFun = "FS::run";
    abortRequested = false;

    if (inputPaths.isEmpty())
    {
        status("No input images to focus stack");
        return false;
    }

    // Create stage folders
    if (!prepareFolders())
        return false;

    // Decide which stages to skip
    if (!setParameters())
        return false;

    setTotalProgress();

    // temp while debugging
    // skipFocusMaps = false;

    qDebug() << srcFun<< "\n"
             << "o.enableAlign       =" << o.enableAlign << "\n"
             << "m_skipAlign         =" << skipAlign << "\n"
             << "o.enableFocusMaps   =" << o.enableFocusMaps << "\n"
             << "m_skipFocusMaps     =" << skipFocusMaps << "\n"
             << "m_total             =" << progressTotal
                ;
    // return false;

    QElapsedTimer t;
    t.start();

    // RUN ALIGN
    if (o.enableAlign && !skipAlign)
    {
        if (!runAlign())
            return false;
    }

    // RUN FOCUS MAPS
    if (o.enableFocusMaps && !skipFocusMaps)
    {
        if (!runFocusMaps())
            return false;
    }

    // RUN DEPTH MAP
    if (o.enableDepthMap && !skipDepthMap)
    {
        if (!runDepthMap())
            return false;
    }

    // RUN FUSION
    if (o.enableFusion && !skipFusion)
    {
        if (!runFusion())
            return false;
    }


    QString timeToRun = QString::number(t.elapsed() / 1000, 'f', 1) + " sec";
    QString progressSteps = " Progress step count = " + QString::number(progressCount);
    QString progressTot = " Progress step total = " + QString::number(progressTotal);
    G::log(srcFun, "Focus Stack completed in " + timeToRun + progressSteps + progressTot);
    G::log(srcFun, "");
    qDebug() << srcFun << "Done" << timeToRun;

    status("Focus Stack completed in " + timeToRun);

    return true;
}

bool FS::runAlign()
/*
    - Load images using FSLoader
    - Compute Petteri-style global transforms
    - Apply transforms to color + gray
    - Save aligned images and cache them in memory
*/
{
    QString srcFun = "FS::runAlign";
    QString msg;
    const QString stage = "Align";
    G::log(srcFun, "Starting alignment…");
    status("Aligning input images");

    if (inputPaths.isEmpty())
    {
        status("No input images to align.");
        return false;
    }

    QElapsedTimer t;
    t.start();

    const int N = inputPaths.size();
    if (N < 2)
    {
        status("Need at least 2 images to align.");
        return false;
    }

    alignedColorMats.resize(N);
    alignedGrayMats.resize(N);

    // Load images using Petteri-compatible padding + validArea
    std::vector<FSLoader::Image> imgs(N);

    for (int i = 0; i < N; ++i)
    {
        if (abortRequested)
            return false;

        try {
            imgs[i] = FSLoader::load(inputPaths[i].toStdString());
        }
        catch (const std::exception &e)
        {
            QString msg = QString("Error loading %1: %2").arg(inputPaths[i]).arg(e.what());
            status(msg);
            return false;
        }

        G::log(srcFun, "Load image " + QString::number(i));
        // incrementProgress();
    }

    // Prepare FSAlign global chain (Petteri-style)
    using namespace FSAlign;

    std::vector<Result> globals(N);
    globals[0] = makeIdentity(imgs[0].validArea);

    FSAlign::Options opt;
    opt.matchContrast     = true;
    opt.matchWhiteBalance = true;
    opt.lowRes            = 256;
    opt.maxRes            = 2048;
    opt.fullResolution    = false;

    // Reference frame: write unmodified aligned color + gray
    if (o.keepIntermediates)
    {
        cv::imwrite(alignedColorPaths[0].toStdString(), imgs[0].color);
        cv::imwrite(alignedGrayPaths[0].toStdString(),  imgs[0].gray);
    }

    alignedColorMats[0] = imgs[0].color.clone();
    alignedGrayMats[0]  = imgs[0].gray.clone();
    G::log(srcFun, "Write unmodified aligned color + gray");
    incrementProgress();

    // Alignment loop: image i aligned to image (i - 1)
    for (int i = 1; i < N; ++i)
    {
        if (abortRequested)
            return false;

        msg = QString("Aligning slice " + QString::number(i));
        G::log(srcFun, msg);
        status(msg);
        // incrementProgress();

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
        catch (const std::exception &e)
        {
            QString msg = QString("Alignment failed for %1: %2")
                              .arg(inputPaths[i]).arg(e.what());
            status(msg);
            return false;
        }
        G::log(srcFun, "computeLocal");

        // Stack transforms (Petteri-style)
        globals[i] = accumulate(
            globals[i - 1],
            local,
            imgs[i].validArea
            );

        // Apply transform to color + gray
        cv::Mat alignedColorMat, alignedGrayMat;
        G::log(srcFun, "cv::Mat alignedColor, alignedGray");
        applyTransform(imgs[i].color, globals[i].transform, alignedColorMat);
        applyTransform(imgs[i].gray,  globals[i].transform, alignedGrayMat);
        G::log(srcFun, "applyTransform alignedGray");

        // Write outputs
        if (o.overwriteAlign)
        {
            cv::imwrite(alignedColorPaths[i].toStdString(), alignedColorMat);
            cv::imwrite(alignedGrayPaths[i].toStdString(),  alignedGrayMat);
        }

        // Cache in memory for fast fusion
        alignedColorMats[i] = alignedColorMat.clone();
        alignedGrayMats[i]  = alignedGrayMat.clone();

        msg = "Write outputs";
        G::log(srcFun, msg);
        incrementProgress();
    }

    // incrementProgress();
    status("Alignment complete.");

    QString timeToFuse = QString::number(t.elapsed() / 1000, 'f', 1) + " sec";
    G::log(srcFun, "Alignment completed in " + timeToFuse);
    G::log(srcFun, "");

    return true;
}

bool FS::runFocusMaps()
/*
    - Use aligned grayscale images in m_alignFolder (gray_*.tif)
    - Delegate focus-map computation to FSFocus (wavelet-based, weighted multilevel energy)
    - Progress is reported through incrementProgress
*/
{
    QString srcFun = "FS::runFocusMaps";
    G::log(srcFun, "Starting focus maps…");

    if (inputPaths.isEmpty())
    {
        status("No input images");
        return false;
    }

    FSFocus::Options fopt;
    fopt.downsampleFactor   = 1;
    fopt.preview            = o.overwriteFocusMaps;
    fopt.numThreads         = 0;
    fopt.useOpenCL          = o.enableOpenCL;
    fopt.keepIntermediates  = o.keepIntermediates;
    fopt.preview            = o.previewFocusMaps;

    auto progressCb = [this](int)
    {
        incrementProgress();
    };

    auto statusCb = [this](const QString &message, bool /*isError*/)
    {
        status(message);
    };

    if (!FSFocus::run(alignFolder,
                      focusFolder,
                      fopt,
                      &abortRequested,
                      progressCb,
                      statusCb,
                      &focusMaps))
    {
        status("Focus maps failed.");
        return false;
    }

    status("Focus maps complete.");
    return true;
}

bool FS::runDepthMap()
/*
    - Use focus maps in m_focusFolder
    - Build depth index and preview using FSDepth
    - Cache depth_index.png in m_depthIndex16
*/
{
    QString srcFun = "FS::runDepthMap";
    G::log(srcFun, "Starting depth map.");
    status("Building depth map.");

    if (abortRequested)
        return false;

    FSDepth::Options dopt;
    dopt.preview = o.overwriteDepthMap;
    dopt.numThreads  = 0;

    auto progressCb = [this](int)
    {
        incrementProgress();
    };

    auto statusCb = [this](const QString &message, bool /*isError*/)
    {
        status(message);
    };

    if (!FSDepth::run(focusFolder,
                      depthFolder,
                      dopt,
                      &abortRequested,
                      progressCb,
                      statusCb,
                      &depthIndex16Mat,                  // <- filled in memory
                      focusMaps.empty() ? nullptr: &focusMaps))
    {
        status("Depth map failed.");
        return false;
    }

    QString depthIdxPath = depthFolder + "/depth_index.png";
    depthIndex16Mat = cv::imread(depthIdxPath.toStdString(), cv::IMREAD_UNCHANGED);

    status("Depth map complete.");
    return true;
}

bool FS::runFusion()
/*
    - Load the aligned grayscale and color images
      (prefer in-memory Mats from runAlign, fall back to disk if needed)
    - Optionally reuse m_depthIndex16 (avoids recomputing depth in FSFusion)
    - Run FSFusion::fuseStack
    - Save fused image
*/
{
    QString srcFun = "FS::runFusion";
    const QString stage = "Fusion";
    G::log(srcFun, "Starting fusion…");
    status("Starting fusion...");

    QElapsedTimer t;
    t.start();

    if (alignedGrayPaths.empty() || alignedColorPaths.empty())
    {
        status("No aligned images available");
        return false;
    }

    const int N = alignedGrayPaths.size();

    std::vector<cv::Mat> grayImgs;
    std::vector<cv::Mat> colorImgs;
    grayImgs.reserve(N);
    colorImgs.reserve(N);

    // Use AlignMats in memory (faster)
    if (validAlignMatsAvailable(N))
    {
        G::log(srcFun, "Using in-memory aligned images");
        for (int i = 0; i < N; ++i)
        {
            grayImgs.push_back(alignedGrayMats[i]);
            colorImgs.push_back(alignedColorMats[i]);
        }
    }
    // Use aligned images (slower)
    else
    {
        G::log(srcFun, "Load aligned images from disk");

        for (int i = 0; i < N; ++i)
        {
            const QString &grayPath  = alignedGrayPaths[i];
            const QString &colorPath = alignedColorPaths[i];

            G::log(srcFun, "Load aligned images from disk " + QString::number(i));

            cv::Mat gray  = cv::imread(grayPath.toStdString(),  cv::IMREAD_GRAYSCALE);
            cv::Mat color = cv::imread(colorPath.toStdString(), cv::IMREAD_UNCHANGED);

            if (gray.empty() || color.empty())
            {
                status(QString("Failed to load aligned image %1").arg(i));
                return false;
            }

            grayImgs.push_back(gray);
            colorImgs.push_back(color);

            incrementProgress();
        }
    }

    FSFusion::Options opt;
    opt.useOpenCL   = o.enableOpenCL;
    opt.consistency = 2;

    cv::Mat fusedColor8Mat;
    cv::Mat depthIndex16_outMat;

    auto progressCb = [this]()
    {
        this->incrementProgress();
    };

    bool haveDepth = (!depthIndex16Mat.empty());

    if (haveDepth)
        G::log(srcFun, "Reusing existing m_depthIndex16 for fusion.");
    else
        G::log(srcFun, "No precomputed depth, fusion will compute depth internally.");

    if (!FSFusion::fuseStack(grayImgs,
                             colorImgs,
                             opt,
                             fusedColor8Mat,
                             depthIndex16_outMat,
                             progressCb,
                             haveDepth ? &depthIndex16Mat : nullptr))
    {
        status("Fusion failed");
        return false;
    }

    if (!haveDepth)
        depthIndex16Mat = depthIndex16_outMat;

    QFileInfo lastFi(inputPaths.last());
    QString base = lastFi.completeBaseName();
    QString ext  = "." + lastFi.suffix();

    QString prefix = base + "_fused_";

    int nextIndex = 1;

    while (true)
    {
        QString candidate = QString("%1%2%3")
        .arg(prefix)
            .arg(nextIndex, 2, 10, QChar('0'))
            .arg(ext);
        QString fullCandidatePath = fusionFolder + "/" + candidate;

        if (!QFileInfo::exists(fullCandidatePath))
        {
            QString fusedPath = fullCandidatePath;

            G::log(srcFun, "Saving fused image to " + fusedPath);
            incrementProgress();

            cv::imwrite(fusedPath.toStdString(), fusedColor8Mat);

            status("Fusion complete.");
            QString timeToFuse = QString::number(t.elapsed() / 1000.0, 'f', 1) + " sec";
            G::log(srcFun, "Fusion completed in " + timeToFuse);
            return true;
        }

        nextIndex++;
        if (nextIndex > 999)
        {
            status("Unable to find available fused filename");
            return false;
        }
    }

    return true;
}
void FS::diagnostics()
{

}
