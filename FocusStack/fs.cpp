#include "FS.h"
#include "Main/global.h"
#include "FocusStack/fsalign.h"
#include "FocusStack/fsloader.h"
#include "FocusStack/fsfocus.h"
#include "FocusStack/fsdepth.h"
#include "FocusStack/fsfusion.h"
#include "FocusStack/fsmerge.h"
#include "FocusStack/fsfusionreassign.h"
#include "FocusStack/fsfusionwavelet.h"
#include "FocusStack/fsartifact.h"
#include "FocusStack/fsloader.h"
#include "FocusStack/fsutilities.h"

#include <opencv2/imgcodecs.hpp>

#include <QDir>
#include <QFileInfo>
#include <QDebug>

FS::FS(QObject *parent)
    : QObject(parent)
{
}

// void FS::setAbort()
// {
//     abortRequested = true;
//     qDebug() << "FS::abort";
// }

void FS::setInput(const QStringList &paths)
{
    inputPaths = paths;
    slices = paths.count();
    lastSlice = slices - 1;

    // Clear any in-memory aligned images from previous run
    alignedColorSlices.clear();
    alignedGraySlices.clear();
}

void FS::setProjectRoot(const QString &rootPath)
{
    projectRoot = rootPath;

    alignFolder     = projectRoot + "/align";
    focusFolder     = projectRoot + "/focus";
    depthFolder     = projectRoot + "/depth";
    fusionFolder    = projectRoot + "/fusion";
    artifactsFolder = projectRoot + "/artifacts";
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

void FS::updateIntermediateStatus()
{
/*
    If o.keepIntermediates == true then intermediate files are saved
    and can be used in successive runs without rerunning the stage.

    The intermediate files are:

    STAGE       DISK                    MEMORY                  EXISTS
    Align       alignedColorPaths       alignedColorSlices      alignExists
    Align       alignedGrayPaths        alignedGraySlices       alignExists
    Depth       depthIdxPath            depthIndex16Mat         depthExists
    Fusion      lastFusedPath           fusedColor8Mat          fusionExists

    - Build a list of alignment files based on the source files.  Chk if exists.
    - Chk if depth and fusion intermediates exist.
*/
    // ALIGN
    alignedColorPaths.clear();
    alignedGrayPaths.clear();
    QString path;
    for (const QString &src : inputPaths) {
        QFileInfo fi(src);
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix();
        path = alignFolder + "/aligned_" + base + "." + ext;
        alignedColorPaths.push_back(path);
        if (!QFileInfo::exists(path)) alignExists = false;
        path = alignFolder + "/gray_" + base + "." + ext;
        alignedGrayPaths.push_back(path);
        if (!QFileInfo::exists(path)) alignExists = false;
    }
    // update memory
    if (alignExists && o.useCache) {
        alignedColorSlices.clear();
        alignedGraySlices.clear();
        for (const QString &f : alignedColorPaths) {
            cv::Mat img = cv::imread(f.toStdString(), cv::IMREAD_COLOR);
            if (img.empty()) continue;
            // cv::Mat g32;
            // img.convertTo(g32, CV_16UC3, 1.0 / 255.0);
            // alignedColorSlices.push_back(g32);
            alignedColorSlices.push_back(img);
        }
        for (const QString &f : alignedGrayPaths) {
            cv::Mat img = cv::imread(f.toStdString(), cv::IMREAD_GRAYSCALE);
            if (img.empty()) continue;
            // cv::Mat g32;
            // img.convertTo(g32, CV_8U, 1.0 / 255.0);
            // alignedGraySlices.push_back(g32);
            alignedGraySlices.push_back(img);
        }
    }

    // Focus TO DO


    // Depth
    path = depthFolder + "/depth_index.png";
    if (!QFileInfo::exists(path)) depthExists = false;
    else {
        depthExists = true;
        cv::Mat d = cv::imread(path.toStdString(), cv::IMREAD_UNCHANGED);
        if (d.empty()) {
            qWarning() << "FS::updateIntermediateStatus: depth_index.png exists but failed to load";
            depthExists = false;
            depthIndex16Mat.release();
        }
        else if (d.type() != CV_16U) {
            qWarning() << "FS::updateIntermediateStatus: depth_index.png has wrong type"
                       << d.type() << "(expected CV_16U)";
            depthExists = false;
            depthIndex16Mat.release();
        }
        else depthIndex16Mat = d;
    }

    // Fusion
    QFileInfo lastFi(inputPaths.last());
    QString base = lastFi.completeBaseName();
    QString ext  = "." + lastFi.suffix();
    QString prefix = base + "_fused_";
    QDir dir(fusionFolder);
    QStringList files = dir.entryList(
        QStringList() << (prefix + "*" + ext),
        QDir::Files,
        QDir::Name    // lexicographic works with zero-padded index
        );

    if (files.isEmpty()) {
        lastFusedPath = "";
        fusionExists = false;
    }
    else {
        lastFusedPath = dir.absoluteFilePath(files.last());
        fusionExists = true;
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
    skipArtifacts = false;

    updateIntermediateStatus();

    if (!o.enableAlign || (!o.useIntermediates && alignExists)) skipAlign = true;
    if (!o.enableFocusMaps || (!o.useIntermediates && focusExists)) skipFocusMaps = true;
    if (!o.enableDepthMap || (!o.useIntermediates && depthExists)) skipDepthMap = true;
    if (!o.enableFusion || (!o.useIntermediates && fusionExists)) skipFusion = true;

    return true;
}

void FS::setAlignedColorPaths()
{

}

void FS::setAlignedColorSlices()
{
    QDir alignDir(projectRoot + "/align");
    QStringList files = alignDir.entryList
    (
        QStringList() << "aligned*.png" << "aligned*.tif" << "aligned*.jpg",
        QDir::Files, QDir::Name
    );

    for (const QString &f : files)
    {
        cv::Mat img = cv::imread
        (
            (alignDir.absoluteFilePath(f)).toStdString(),
            cv::IMREAD_COLOR)
        ;

        if (img.empty()) continue;

        cv::Mat g32;
        img.convertTo(g32, CV_32F, 1.0 / 255.0);
        alignedColorSlices.push_back(g32);
    }
}

bool FS::validAlignMatsAvailable(int count) const
/*
    - Validate that in-memory aligned color and gray Mats exist
    - Validate the vector sizes match 'count'
    - Validate that no Mat is empty
*/
{
    if (alignedGraySlices.size()  != static_cast<size_t>(count) ||
        alignedColorSlices.size() != static_cast<size_t>(count))
    {
        return false;
    }

    for (int i = 0; i < count; ++i)
    {
        if (alignedGraySlices[i].empty() || alignedColorSlices[i].empty())
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
    G::log(srcFun, QString::number(progressCount));
}

void FS::previewOverview(cv::Mat &fusedColor8Mat)
{
/*
    depth + fused + two sample slices
    requires depth_preview.png, alignedGrayMats
*/
    QString srcFun = "FS::previewOverview";
    if (o.previewFusion)        // or add a new option `o.debugFusionOverview`
    {
        // Load depth preview image written by FSDepth
        QString depthPrevPath = depthFolder + "/depth_preview.png";
        cv::Mat depthPrev = cv::imread(depthPrevPath.toStdString(), cv::IMREAD_COLOR);

        // Select representative grayscale slices
        cv::Mat slice0, sliceMid;

        if (!alignedGraySlices.empty()) {
            slice0 = alignedGraySlices.front();
            sliceMid = alignedGraySlices[alignedGraySlices.size() / 2];
        }
        else {
            // fallback: load from disk
            slice0 = cv::imread(alignedGrayPaths.front().toStdString(), cv::IMREAD_GRAYSCALE);
            sliceMid = cv::imread(
                alignedGrayPaths[alignedGrayPaths.size() / 2].toStdString(),
                cv::IMREAD_GRAYSCALE
                );
        }

        // Output path
        QString dbgPath = fusionFolder + "/debug_overview.png";

        FSUtilities::makeDebugOverview(
            depthPrev,         // depth_preview.png
            fusedColor8Mat,    // fused RGB result
            slice0,            // grayscale slice 0
            sliceMid,          // mid grayscale slice
            dbgPath            // output path
            );

        if (G::FSLog) G::log(srcFun, "Wrote fusion debug overview -> " + dbgPath);
    }
}

bool FS::run()
{
    QString srcFun = "FS::run";

    if (inputPaths.isEmpty())
    {
        status("No input images to focus stack");
        return false;
    }

    QElapsedTimer t;
    t.start();

    status("Preparing to focus stack...");
    progressTotal = inputPaths.count() * 2;     // temp until setParameters
    incrementProgress();

    // Create stage folders
    if (!prepareFolders())
        return false;

    // Decide which stages to skip
    if (!setParameters())
        return false;

    setTotalProgress();

    // RUN ALIGN
    if (o.enableAlign && !skipAlign)
    {
        if (!runAlign()) {
            if (abort) status("Focus Stack was aborted.");
            return false;
        }
    }

    // RUN FOCUS MAPS
    if (o.enableFocusMaps && !skipFocusMaps)
    {
        if (!runFocusMaps()) {
            if (abort) status("Focus Stack was aborted.");
            return false;
        }
    }

    // RUN DEPTH MAP
    if (o.enableDepthMap && !skipDepthMap)
    {
        if (!runDepthMap()) {
            if (abort) status("Focus Stack was aborted.");
            return false;
        }
    }

    // RUN FUSION
    if (o.enableFusion && !skipFusion)
    {
        if (!runFusion()) {
            if (abort) status("Focus Stack was aborted.");
            return false;
        }
    }

    // RUN ARTIFACT DETECTION
    if (o.enableArtifactDetect)
    {
        if (!runArtifact()) {
            if (abort) status("Focus Stack was aborted.");
            return false;
        }
    }

    QString timeToRun = QString::number(t.elapsed() / 1000, 'f', 1) + " sec";
    QString progressSteps = " Progress step count = " + QString::number(progressCount);
    QString progressTot = " Progress step total = " + QString::number(progressTotal);
    if (G::FSLog) G::log(srcFun, "Focus Stack completed in " + timeToRun + progressSteps + progressTot);
    if (G::FSLog) G::log(srcFun, "");
    qDebug() << srcFun << "Done" << timeToRun;

    status("Focus Stack completed in " + timeToRun);

    qApp->processEvents();  // complete any waiting log msgs

    diagnostics();

    return true;
}

bool FS::runAlign()
/*
    if (alignExists && useCache)
        for each slice
            read saved image
            cache
        return
    if (!alignExists)
        for each slice
            Load images using FSLoader
    Initalize FSAlign
    if (keepAlign)
        Save first (reference) image
        if useCache
            cache first
    if (!alignExists && !useIntermediates)
        for each slice (2 to n)
            Compute global transforms
            Apply transforms to color + gray
    if (keepIntermediates)
        Save aligned images
    if (useCache)
        cache
*/
{
    QString srcFun = "FS::runAlign";
    QString msg;
    const QString stage = "Align";
    if (G::FSLog) G::log(srcFun, "Starting alignment…");
    status("Aligning input images");

    if (inputPaths.isEmpty())
    {
        status("No input images to align.");
        return false;
    }

    // Number of source stacked images = slices
    const int n = inputPaths.size();
    if (n < 2)
    {
        status("Need at least 2 images to align.");
        return false;
    }

    // We're good to go
    if (alignExists && o.useCache) return true;

    // Resize cache (done in updateIntermediateStatus)
    alignedColorSlices.resize(n);
    alignedGraySlices.resize(n);

    // Just cache if useCache = true and alignExists
    if (alignExists && o.useCache) {
        for (quint32 i = 0; i < n; ++i) {
            alignedGraySlices[i] = cv::imread(
                alignedGrayPaths.at(i).toStdString(), cv::IMREAD_GRAYSCALE);
            alignedColorSlices[i] = cv::imread(
                alignedColorPaths.at(i).toStdString(), cv::IMREAD_UNCHANGED);
        }
        return true;
    }

    std::vector<FSLoader::Image> imgs(n);

    if (!alignExists) {
        // Load images using Petteri-compatible padding + validArea

        for (int i = 0; i < n; ++i)
        {
            if (abort) return false;

            try {
                imgs[i] = FSLoader::load(inputPaths[i].toStdString());
            }
            catch (const std::exception &e)
            {
                QString msg = QString("Error loading %1: %2").arg(inputPaths[i]).arg(e.what());
                status(msg);
                return false;
            }

            if (G::FSLog) G::log(srcFun, "Load image " + QString::number(i));
        }
    }

    // Initalize FSAlign
    using namespace FSAlign;

    std::vector<Result> globals(n);
    globals[0] = makeIdentity(imgs[0].validArea);

    FSAlign::Options opt;
    opt.matchContrast     = true;
    opt.matchWhiteBalance = true;
    opt.lowRes            = 256;
    opt.maxRes            = 2048;
    opt.fullResolution    = false;

    // Reference slice: write unmodified aligned color + gray
    if (o.keepAlign)
    {
        cv::imwrite(alignedColorPaths[0].toStdString(), imgs[0].color);
        cv::imwrite(alignedGrayPaths[0].toStdString(),  imgs[0].gray);
    }
    // Cache reference slice
    alignedColorSlices[0] = imgs[0].color.clone();
    alignedGraySlices[0]  = imgs[0].gray.clone();
    if (G::FSLog) G::log(srcFun, "Write unmodified aligned color + gray");
    incrementProgress();

    // Alignment loop: image i aligned to image (i - 1)
    if (!alignExists) {
        for (int i = 1; i < n; ++i)
        {
            qApp->processEvents();
            if (abort) return false;

            msg = QString("Aligning slice " + QString::number(i));
            if (G::FSLog) G::log(srcFun, msg);
            status(msg);

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
            if (G::FSLog) G::log(srcFun, "computeLocal");

            // Stack transforms
            globals[i] = accumulate(
                globals[i - 1],
                local,
                imgs[i].validArea
                );

            // Apply transform to color + gray
            cv::Mat alignedColorMat, alignedGrayMat;
            if (G::FSLog) G::log(srcFun, "cv::Mat alignedColor, alignedGray");
            applyTransform(imgs[i].color, globals[i].transform, alignedColorMat);
            applyTransform(imgs[i].gray,  globals[i].transform, alignedGrayMat);
            if (G::FSLog) G::log(srcFun, "applyTransform alignedGray");

            // Write outputs
            if (o.keepAlign)
            {
                cv::imwrite(alignedColorPaths[i].toStdString(), alignedColorMat);
                cv::imwrite(alignedGrayPaths[i].toStdString(),  alignedGrayMat);
            }

            // Cache in memory for fast fusion
            alignedColorSlices[i] = alignedColorMat.clone();
            alignedGraySlices[i]  = alignedGrayMat.clone();

            msg = "Write outputs";
            if (G::FSLog) G::log(srcFun, msg);
            incrementProgress();
        }
    }

    // incrementProgress();
    status("Alignment complete.");

    // QString timeToFuse = QString::number(t.elapsed() / 1000, 'f', 1) + " sec";
    // if (G::FSLog) G::log(srcFun, "Alignment completed in " + timeToFuse);
    // if (G::FSLog) G::log(srcFun, "");

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
    if (G::FSLog) G::log(srcFun, "Starting focus maps…");

    if (inputPaths.isEmpty())
    {
        status("No input images");
        return false;
    }

    FSFocus::Options fopt;
    fopt.downsampleFactor   = 1;
    fopt.preview            = o.keepFocusMaps;
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
                      &abort,
                      progressCb,
                      statusCb,
                      &focusSlices))
    {
        status("Focus maps failed.");
        return false;
    }

    status("Focus maps complete.");
    return true;
}

bool FS::runDepthMap()
/*
    - Use focus maps in focusFolder (Simple)
    - OR aligned grayscale in alignFolder (MultiScale)
    - Build depth index and preview using FSDepth
    - Cache depth_index.png into depthIndex16Mat
*/
{
    QString srcFun = "FS::runDepthMap";
    if (G::FSLog) G::log(srcFun, "Starting depth map.");
    status("Building depth map.");

    if (abort)
        return false;

    FSDepth::Options dopt;

    // Map FS::Options.method to FSDepth method
    if (o.method == "PMax") {
        dopt.method = "MultiScale";
        dopt.alignFolder = alignFolder;
        dopt.preview = o.previewDepthMap;
        dopt.keep = o.keepDepthMap;
        dopt.saveWaveletDebug = true;       // per your "Save wavelet debug = yes"
    }
    else {
        dopt.method = "Simple";
        dopt.alignFolder.clear();
        dopt.preview = o.previewDepthMap;
        dopt.saveWaveletDebug = false;
    }

    dopt.numThreads = 0;  // (reserved, not used inside yet)

    auto progressCb = [this](int){ incrementProgress(); };
    auto statusCb   = [this](const QString &msg){ status(msg); };

    if (abort) return false;

    bool ok = false;

    if (o.useCache && alignExists)
    {
        ok = FSDepth::runFromGraySlices(
            alignedGraySlices,
            depthFolder,
            dopt,
            &abort,
            progressCb,
            statusCb,
            &depthIndex16Mat
            );
    }
    else
    {
        ok = FSDepth::run(
            focusFolder,
            depthFolder,
            dopt,
            &abort,
            progressCb,
            statusCb,
            &depthIndex16Mat
            );
    }

    if (!ok || depthIndex16Mat.empty())

    {
        status("Depth map failed: no depthIndex16Mat produced.");
        return false;
    }

    status("Depth map complete.");
    return true;
}

bool FS::runFusion()
/*
    - Load the aligned grayscale and color images
      (prefer in-memory Mats from runAlign, fall back to disk if needed)
    - REQUIRE a depth map from FSDepth (depthIndex16Mat)
    - Run FSFusion::fuseStack using the selected method
    - Save fused image
*/
{
    QString srcFun = "FS::runFusion";
    if (G::FSLog) G::log(srcFun, "Starting fusion…");
    status("Starting fusion...");

    QElapsedTimer t;
    t.start();

    if (o.useCache) {
        if (!validAlignMatsAvailable(slices)) return false;
    }

    const int N = alignedGrayPaths.size();

    if (depthIndex16Mat.empty()) {
        status("Depth map not available. Run Depth stage first.");
        return false;
    }

    // Build grayscale + color stacks
    std::vector<cv::Mat> grayImgs;
    std::vector<cv::Mat> colorImgs;
    grayImgs.reserve(N);
    colorImgs.reserve(N);

    // Prefer in-memory Mats
    if (validAlignMatsAvailable(N))
    {
        if (G::FSLog) G::log(srcFun, "Using in-memory aligned images");
        for (int i = 0; i < N; ++i)
        {
            if (abort.load(std::memory_order_relaxed)) return false;
            grayImgs.push_back(alignedGraySlices[i]);
            colorImgs.push_back(alignedColorSlices[i]);
        }
    }
    else
    {
        if (G::FSLog) G::log(srcFun, "Loading aligned images from disk");

        for (int i = 0; i < N; ++i)
        {
            if (abort) return false;
            const QString &grayPath  = alignedGrayPaths[i];
            const QString &colorPath = alignedColorPaths[i];

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

    // Fusion options
    FSFusion::Options fopt;
    fopt.useOpenCL   = o.enableOpenCL;
    fopt.consistency = 2;

    // Match depth method to fusion method:
    //  - If you used FSDepth::method == "Simple", you can set fopt.method = "Simple"
    //  - If you used FSDepth::method == "MultiScale", set "PMax" to reproduce
    //    the full PMax pipeline.
    if (o.method.compare("Simple", Qt::CaseInsensitive) == 0)
        fopt.method = "Simple";
    else
        fopt.method = "PMax";   // default: current successful PMax pipeline

    // cv::Mat fusedColor8Mat;

    auto progressCb = [this]()
    {
        this->incrementProgress();
    };

    if (!FSFusion::fuseStack(grayImgs,
                             colorImgs,
                             fopt,
                             depthIndex16Mat,
                             fusedColor8Mat,
                             &abort,
                             progressCb))
    {
        status("Fusion failed");
        return false;
    }

    // -------------------------------------------------------
    // Save fused image with incrementing suffix
    // -------------------------------------------------------
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
            lastFusedPath = fullCandidatePath;

            if (G::FSLog) G::log(srcFun, "Saving fused image to " + lastFusedPath);
            incrementProgress();

            cv::imwrite(lastFusedPath.toStdString(), fusedColor8Mat);

            // visually debug
            if (o.previewFusion) previewOverview(fusedColor8Mat);

            status("Fusion complete.");
            QString timeToFuse = QString::number(t.elapsed() / 1000.0, 'f', 1) + " sec";
            if (G::FSLog) G::log(srcFun, "Fusion completed in " + timeToFuse);
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

bool FS::runArtifact()
{
    const QString srcFun = "FS::runArtifactDetection";

    if (abort) return false;

    emit updateStatus(false, "Detecting fusion artifacts...", srcFun);

    // Load fused image (color, aligned space)
    if (lastFusedPath.isEmpty())
    {
        QString msg = "No fused images found in " + fusionFolder;
        G::log(srcFun, msg);
        return false;
    }

    cv::Mat grayFused = cv::imread(lastFusedPath.toStdString(), cv::IMREAD_GRAYSCALE);

    if (grayFused.empty())
    {
        G::log(srcFun, "Failed to load fused image");
        return false;
    }

    cv::Mat fusedGray32;
    grayFused.convertTo(fusedGray32, CV_32F, 1.0 / 255.0);

    /*
    // Load aligned slices (grayscale, aligned space)
    std::vector<cv::Mat> slicesGray32;

    QDir alignDir(projectRoot + "/align");
    QStringList files = alignDir.entryList
    (
        QStringList() << "aligned*.png" << "aligned*.tif" << "aligned*.jpg",
        QDir::Files, QDir::Name
    );

    for (const QString &f : files)
    {
        if (abort) return false;

        cv::Mat img = cv::imread(
            (alignDir.absoluteFilePath(f)).toStdString(),
            cv::IMREAD_GRAYSCALE);

        if (img.empty())
            continue;

        cv::Mat g32;
        img.convertTo(g32, CV_32F, 1.0 / 255.0);
        slicesGray32.push_back(g32);
    }

    if (slicesGray32.empty())
    {
        G::log(srcFun, "No aligned slices for artifact detection");
        return false;
    }
    */

    if (alignedGraySlices.empty())
    {
        G::log(srcFun, "No aligned slices for artifact detection");
        return false;
    }

    // Optional depth map
    cv::Mat depth32;
    cv::Mat *depthPtr = nullptr;

    QString depthPath = projectRoot + "/depth/depth_idx.exr"; // adjust
    if (QFile::exists(depthPath))
    {
        cv::Mat d = cv::imread(depthPath.toStdString(), cv::IMREAD_UNCHANGED);
        if (!d.empty())
        {
            d.convertTo(depth32, CV_32F);
            depthPtr = &depth32;
        }
    }

    // Optional include mask
    cv::Mat includeMask8;
    cv::Mat *maskPtr = nullptr;

    QString maskPath = projectRoot + "/masks/include_mask.png";
    if (QFile::exists(maskPath))
    {
        includeMask8 = cv::imread(maskPath.toStdString(), cv::IMREAD_GRAYSCALE);
        if (!includeMask8.empty())
            maskPtr = &includeMask8;
    }

    // Artifact detection options
    FSArtifact::Options opt;
    opt.debugFolder = artifactsFolder;

    cv::Mat artifact01;

    // Progress and Status
    auto progressCb = [this]()
    {
        this->incrementProgress();
    };

    auto statusCb = [this, srcFun](const QString &msg)
    {
        emit updateStatus(false, msg, srcFun);
    };

    // Run artifact detector
    bool ok = FSArtifact::detect
    (
        fusedGray32,
        alignedGraySlices, //slicesGray32,
        depthPtr,
        maskPtr,
        artifact01,
        opt,
        &abort,
        progressCb,
        statusCb
    );

    if (!ok || abort) return false;

    // Build heat map

    CV_Assert(artifact01.type() == CV_32F);

    cv::Mat conf;
    artifact01.copyTo(conf);
    cv::min(conf, 1.0f, conf);
    cv::max(conf, 0.0f, conf);

    // Heat map
    cv::Mat heat(conf.size(), CV_8UC3, cv::Scalar(0,0,0));

    // Set thresholds
    for (int y = 0; y < conf.rows; ++y)
    {
        const float *cptr = conf.ptr<float>(y);
        cv::Vec3b *hptr   = heat.ptr<cv::Vec3b>(y);

        for (int x = 0; x < conf.cols; ++x)
        {
            float v = cptr[x];

            if (v < 0.02f)
            {
                // Black (background)
                hptr[x] = {0, 0, 0};
            }
            else if (v < 0.05f)
            {
                // Yellow
                hptr[x] = {0, 255, 255};
            }
            else if (v < 0.10f)
            {
                // Orange
                hptr[x] = {0, 165, 255};
            }
            else
            {
                // Bright red
                hptr[x] = {0, 0, 255};
            }
        }
    }
    // Ensure artifact folder exists
    QString artifactFolder = projectRoot + "/artifact";
    QDir().mkpath(artifactFolder);

    // Overwrite artifact_confidence.png with COLOR heatmap
    QString confidencePath =
        artifactFolder + "/artifact_confidence.png";

    cv::imwrite(confidencePath.toStdString(), heat);

    // Overlay heatmap on fused grayscale
    QString overlayPath =
        artifactFolder + "/artifact_overlay.png";

    cv::Mat overlay =
        FSUtilities::showWithMask(fusedGray32, heat, 0.6f);

    // Optional labeling (recommended for debug clarity)
    FSUtilities::addLabel(
        overlay,
        "Artifact confidence overlay"
        );

    // Repair
    if (o.enableArtifactRepair)
    {
        emit updateStatus(false, "Repairing artifacts...", srcFun);

        // setAlignedColorSlices();

        cv::Mat colorFused8 =
            cv::imread(lastFusedPath.toStdString(), cv::IMREAD_COLOR);
        cv::Mat colorFused32;
        colorFused8.convertTo(colorFused32, CV_32F, 1.0 / 255.0);

        FSArtifact::repair(
            colorFused32,
            artifact01,
            alignedColorSlices,
            lastSlice,
            0.02f,
            &abort
            );

        // Save repair
        QString repairedPath = artifactFolder + "/artifact_repaired.png";

        cv::Mat repaired8;
        colorFused32.convertTo(repaired8, CV_8U, 255.0);
        cv::imwrite(repairedPath.toStdString(), repaired8);
    }

    emit updateStatus(false, "Artifact detection complete", srcFun);
    return true;
}

void FS::diagnostics()
{
    qDebug() << "Diagnostics:"
        << "\n"
        << "o.method                =" << o.method << "\n"
        << "o.keepIntermediates     =" << o.keepIntermediates << "\n"
        << "o.useIntermediates      =" << o.useIntermediates << "\n"
        << "o.useCache              =" << o.useCache << "\n"
        << "o.enableOpenCL          =" << o.enableOpenCL << "\n"
        << "\n"
        << "o.enableAlign           =" << o.enableAlign << "\n"
        << "o.keepAlign             =" << o.keepAlign << "\n"
        << "\n"
        << "o.enableFocusMaps       =" << o.enableFocusMaps << "\n"
        << "o.previewFocusMaps      =" << o.previewFocusMaps << "\n"
        << "o.keepFocusMaps         =" << o.keepFocusMaps << "\n"
        << "\n"
        << "o.enableDepthMap        =" << o.enableDepthMap << "\n"
        << "o.previewDepthMap       =" << o.previewDepthMap << "\n"
        << "o.keepDepthMap          =" << o.keepDepthMap << "\n"
        << "\n"
        << "o.enableFusion          =" << o.enableFusion << "\n"
        << "o.previewFusion         =" << o.previewFusion << "\n"
        << "\n"
        << "o.enableArtifactDetect  =" << o.enableArtifactDetect << "\n"
        << "o.enableArtifactRepair  =" << o.enableArtifactRepair << "\n"
        << "\n"
        << "alignExists             =" << alignExists << "\n"
        << "focusExists             =" << focusExists << "\n"
        << "depthExists             =" << depthExists << "\n"
        << "fusionExists            =" << fusionExists << "\n"
        << "\n"
        << "skipAlign               =" << skipAlign << "\n"
        << "skipFocusMaps           =" << skipFocusMaps << "\n"
        << "skipDepthMap            =" << skipDepthMap << "\n"
        << "skipFusion              =" << skipFusion << "\n"
        << "skipArtifacts           =" << skipArtifacts << "\n"
        ;

}
