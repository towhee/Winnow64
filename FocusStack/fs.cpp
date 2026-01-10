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
#include "FocusStack/fsbackground.h"
#include "FocusStack/fsartifact.h"
#include "FocusStack/fsloader.h"
#include "FocusStack/fsutilities.h"

#include "utilities.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "ImageFormats/Tiff/tiff.h"

#include <opencv2/imgproc.hpp>
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

void FS::initialize(QString dstFolderPath, QString fusedBase)
{
    this->dstFolderPath = dstFolderPath;
    this->fusedBase = fusedBase;
}

bool FS::initializeGroup(int group)
{
    QString srcFun = "FS::initializeGroup";

    inputPaths.clear();
    inputPaths = groups.at(group);
    slices = inputPaths.count();
    lastSlice = slices - 1;

    QFileInfo info(inputPaths.first());
    const QString srcFolder = info.absolutePath();
    grpFolderPath = srcFolder + "/" + info.completeBaseName() + "_" + o.method;

    if (o.useIntermediates) prepareFolders();

    statusGroupPrefix = "Focus Stacking:  "
                        "Stack: " + QString::number(group+1) + " of " +
                        QString::number(groups.count()) + "  ";

    // Clear any in-memory aligned images from previous run
    alignedColorPaths.clear();
    alignedGrayPaths.clear();
    alignedColorSlices.clear();
    alignedGraySlices.clear();
    focusSlices.clear();

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

void FS::setOptions(const Options &opt)
{
    o = opt;

    // Fuse using grayscale wavelet merge and color map without using focus
    // or depth modules.
    if (o.method == "PMax1") {
        o.enableAlign = true;
        o.enableFocusMaps = false;
        o.enableDepthMap = false;
        o.enableFusion = true;
        o.methodFuse = "FullWaveletMerge";
    }

    // Fuse by streaming using grayscale wavelet merge and color map.  Do not need
    // to hold all images and Mats in memory.
    if (o.method == "StreamPMax") {
        o.enableAlign = false;
        o.enableFocusMaps = false;
        o.enableDepthMap = false;
        o.enableFusion = true;
        o.methodFuse = "FullWaveletMerge";
        o.isStream = true;
    }

    // Fuse using multiscale depthmap, focus module not used.
    if (o.method == "PMax2") {
        o.enableAlign = true;
        o.enableFocusMaps = true;
        o.enableDepthMap = true;
        o.enableFusion = true;
        o.methodDepth = "MultiScale";
        o.methodFuse = "Simple";
    }

    // Test tenengrad focus depthmap
    if (o.method == "Ten") {
        o.enableAlign = true;
        o.enableFocusMaps = false;
        o.enableDepthMap = true;
        o.enableFusion = false;
        o.methodDepth = "Tenengrad";
        o.methodFuse = "Simple";
    }
}

void FS::status(const QString &msg)
{
    emit updateStatus(false, statusGroupPrefix + msg, "");
}

bool FS::prepareFolders()
{
/*
    Lightroom folder (only if remote)
        srcFolder (focus stack input tiffs)
            grpFolder ie 2025-11-07_0078_StreamPMax
                alignFolder
                focusFolder
                depthFolder
                fusionFolder
                backgroundFolder
                artifactsFolder

    srcFolder contains the input focus stack images
    dstFolder is where to save the fused result image
        - if local, use srcFolder
        - if remote, use srcFolder parent = Lightroom folder
*/
    if (grpFolderPath.isEmpty())
    {
        status("Project root not set");
        return false;
    }

    QDir dir;
    if (!dir.mkpath(grpFolderPath)) {
        status("Cannot create project root");
        return false;
    }

    alignFolderPath      = grpFolderPath + "/align";
    focusFolderPath      = grpFolderPath + "/focus";
    depthFolderPath      = grpFolderPath + "/depth";
    fusionFolderPath     = grpFolderPath + "/fusion";
    backgroundFolderPath = grpFolderPath + "/background";
    artifactsFolderPath  = grpFolderPath + "/artifacts";

    if (o.enableAlign)          dir.mkpath(alignFolderPath);
    if (o.enableFocusMaps)      dir.mkpath(focusFolderPath);
    if (o.enableDepthMap)       dir.mkpath(depthFolderPath);
    if (o.enableFusion)         dir.mkpath(fusionFolderPath);
    if (o.enableBackgroundMask) dir.mkpath(backgroundFolderPath);
    if (o.enableArtifactDetect) dir.mkpath(artifactsFolderPath);

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
    const QString srcFun = "FS::updateIntermediateStatus";
    if (G::FSLog) G::log(srcFun);

    // ALIGN
    QString path;
    /* The inputPaths are the source images. Build the list of aligned paths
       and check if they already exist */
    for (const QString &src : inputPaths) {
        QFileInfo fi(src);
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix();
        path = alignFolderPath + "/aligned_" + base + "." + ext;
        alignedColorPaths.push_back(path);
        if (!QFileInfo::exists(path)) alignExists = false;
        path = alignFolderPath + "/gray_" + base + "." + ext;
        alignedGrayPaths.push_back(path);
        if (!QFileInfo::exists(path)) alignExists = false;
    }
    // if aligned images already exist update cached align Mat (grayscale and color)
    if (alignExists && o.useCache) {
        alignedColorSlices.resize(slices);
        alignedGraySlices.resize(slices);
        int i = 0;
        for (const QString &f : alignedColorPaths) {
            cv::Mat img = cv::imread(f.toStdString(), cv::IMREAD_COLOR);
            if (img.empty()) continue;
            alignedColorSlices.push_back(img);
            incrementProgress();
            QString msg = "alignedColorSlices " + QString::number(i++);
            if (G::FSLog) G::log(srcFun, msg);
        }
        i = 0;
        for (const QString &f : alignedGrayPaths) {
            cv::Mat img = cv::imread(f.toStdString(), cv::IMREAD_GRAYSCALE);
            if (img.empty()) continue;
            alignedGraySlices.push_back(img);
            incrementProgress();
            QString msg = "alignedGraySlices " + QString::number(i++);
            if (G::FSLog) G::log(srcFun, msg);
        }
    }

    // Focus TO DO


    // Depth
    path = depthFolderPath + "/depth_index.png";
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
    QDir dir(fusionFolderPath);
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

void FS::setAlignedColorPaths()
{

}

void FS::setAlignedColorSlices()
{
    QDir alignDir(grpFolderPath + "/align");
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
    progressTotal = 0;
    for (const QStringList &g : groups) {
        int slices = g.count();
        if (o.isStream) {
            progressTotal += (slices * 3);
        }
        else {
            if (slices < 2) continue;
            if (o.enableAlign && !skipAlign)
                progressTotal += (slices - 1);
            if (o.enableFocusMaps && !skipFocusMaps)
                progressTotal += slices;
            if (o.enableDepthMap && !skipDepthMap)
                progressTotal += slices;
            if (o.enableFusion && !skipFusion)
                progressTotal += (slices + 4);
            if (o.enableBackgroundMask)
                progressTotal += slices;
        }
    }
}

void FS::incrementProgress()
{
    QString srcFun = "FS::incrementProgress";
    emit progress(++progressCount, progressTotal);
    QString msg = QString::number(progressCount) + "/" + QString::number(progressTotal);
    G::log(srcFun, msg);
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
        QString depthPrevPath = depthFolderPath + "/depth_preview.png";
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
        QString dbgPath = fusionFolderPath + "/debug_overview.png";

        FSUtilities::makeDebugOverview(
            depthPrev,         // depth_preview.png
            fusedColor8Mat,    // fused RGB result
            slice0,            // grayscale slice 0
            sliceMid,          // mid grayscale slice
            dbgPath,           // output path
            10000              // max width
            );

        if (G::FSLog) G::log(srcFun, "Wrote fusion debug overview -> " + dbgPath);
    }
}

QImage FS::thumbnail(const cv::Mat &mat)
{
    if (mat.empty()) return QImage();

    // --- Resize so the long side is 256 --------------------------------------
    const int target = 256;
    const int w = mat.cols;
    const int h = mat.rows;
    const int longSide = std::max(w, h);
    if (longSide <= 0) return QImage();

    const double scale = double(target) / double(longSide);
    const int newW = std::max(1, int(std::lround(w * scale)));
    const int newH = std::max(1, int(std::lround(h * scale)));

    cv::Mat resized;
    const int interp = (scale < 1.0) ? cv::INTER_AREA : cv::INTER_LINEAR;
    cv::resize(mat, resized, cv::Size(newW, newH), 0, 0, interp);

    // --- Convert to QImage (deep copy so it’s safe to return) ----------------
    cv::Mat converted;

    switch (resized.type())
    {
    case CV_8UC1:
    {
        QImage img(resized.data, resized.cols, resized.rows,
                   int(resized.step), QImage::Format_Grayscale8);
        return img.copy();
    }
    case CV_8UC3:
    {
        cv::cvtColor(resized, converted, cv::COLOR_BGR2RGB);
        QImage img(converted.data, converted.cols, converted.rows,
                   int(converted.step), QImage::Format_RGB888);
        return img.copy();
    }
    case CV_8UC4:
    {
        cv::cvtColor(resized, converted, cv::COLOR_BGRA2RGBA);
        QImage img(converted.data, converted.cols, converted.rows,
                   int(converted.step), QImage::Format_RGBA8888);
        return img.copy();
    }
    default:
    {
        // Fallback: convert to 8-bit then handle 1/3/4 channels
        cv::Mat tmp;
        if (resized.depth() != CV_8U)
            resized.convertTo(tmp, CV_8U, 255.0);
        else
            tmp = resized;

        if (tmp.channels() == 1)
        {
            QImage img(tmp.data, tmp.cols, tmp.rows, int(tmp.step), QImage::Format_Grayscale8);
            return img.copy();
        }
        if (tmp.channels() == 3)
        {
            cv::cvtColor(tmp, converted, cv::COLOR_BGR2RGB);
            QImage img(converted.data, converted.cols, converted.rows, int(converted.step), QImage::Format_RGB888);
            return img.copy();
        }
        if (tmp.channels() == 4)
        {
            cv::cvtColor(tmp, converted, cv::COLOR_BGRA2RGBA);
            QImage img(converted.data, converted.cols, converted.rows, int(converted.step), QImage::Format_RGBA8888);
            return img.copy();
        }

        return QImage();
    }
    }
}

bool FS::run()
{
    QString srcFun = "FS::run";

    setTotalProgress();
    QString msg = "progressTotal = " + QString::number(progressTotal);
    if (G::FSLog) G::log(srcFun, msg);


    int groupCounter = 0;
    for (const QStringList &g : groups) {

        slices = g.count();
        if (slices < 2) {
            groupCounter++;
            continue;
        }

        QString msg = "Preparing to focus stack group " + QString::number(groupCounter);
        status(msg);
        if (G::FSLog) G::log(srcFun, msg);

        QElapsedTimer t;
        t.start();

        if (!initializeGroup(groupCounter++)) return false;

        if (o.isStream) {
            runStreamWaveletPMax();
        }

        else {
            // RUN ALIGN
            if (o.enableAlign && !skipAlign)
            {
                if (!runAlign()) {
                    if (abort) status("Focus Stack was aborted.");
                    return false;
                }
                if (G::FSLog) G::log(""); // skip line
            }

            // RUN FOCUS MAPS
            if (o.enableFocusMaps && !skipFocusMaps)
            {
                if (!runFocusMaps()) {
                    if (abort) status("Focus Stack was aborted.");
                    return false;
                }
                if (G::FSLog) G::log(""); // skip line
            }

            // RUN DEPTH MAP
            if (o.enableDepthMap && !skipDepthMap)
            {
                if (!runDepthMap()) {
                    if (abort) status("Focus Stack was aborted.");
                    return false;
                }
                if (G::FSLog) G::log(""); // skip line
            }

            // RUN FUSION
            if (o.enableFusion && !skipFusion)
            {
                if (!runFusion()) {
                    if (abort) status("Focus Stack was aborted.");
                    return false;
                }
                if (G::FSLog) G::log(""); // skip line
            }

            // RUN BACKGROUND
            if (o.enableBackgroundMask)
            {
                if (!runBackground()) {
                    if (abort) status("Focus Stack was aborted.");
                    return false;
                }
                if (G::FSLog) G::log(""); // skip line
            }

            // RUN ARTIFACT DETECTION
            if (o.enableArtifactDetect)
            {
                if (!runArtifact()) {
                    if (abort) status("Focus Stack was aborted.");
                    return false;
                }
                if (G::FSLog) G::log(""); // skip line
            }
        }

        // SAVE
        if (o.enableFusion) saveFused(dstFolderPath);

        // STATS
        QString timeToRun = QString::number(t.elapsed() / 1000, 'f', 1) + " sec";
        QString progressSteps = " Progress step count = " + QString::number(progressCount);
        QString progressTot = " Progress step total = " + QString::number(progressTotal);
        G::log(srcFun, "Focus Stack completed in " + timeToRun + progressSteps + progressTot);
        if (G::FSLog) G::log("");
        status("Focus Stack completed in " + timeToRun);

        // qApp->processEvents();  // complete any waiting log msgs
    }

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

    // Already aligned and cached?
    if (!alignedGraySlices.empty() && !alignedColorSlices.empty()) return true;

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

    // Resize cache (done in updateIntermediateStatus)
    alignedColorSlices.resize(n);
    alignedGraySlices.resize(n);

    // Just cache if o.useCache = true and alignExists
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
                qWarning() << "WARNING:" << srcFun << msg;
                return false;
            }

            QString msg = "Load image " + QString::number(i);
            status(msg);
            if (G::FSLog) G::log(srcFun, msg);
            incrementProgress();
        }
    }

    // Resize cache
    alignedColorSlices.resize(n);
    alignedGraySlices.resize(n);

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
            // qApp->processEvents();
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

            msg = "Cached aligned slice " + QString::number(i);
            if (G::FSLog) G::log(srcFun, msg);
            incrementProgress();
        }
    }

    // incrementProgress();
    alignExists = true;
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
    fopt.keepIntermediates  = o.keepFocusMaps;
    fopt.preview            = o.previewFocusMaps;

    auto progressCb = [this](int)
    {
        incrementProgress();
    };

    auto statusCb = [this](const QString &message)
    {
        status(message);
    };

    if (!FSFocus::run(alignFolderPath,
                      focusFolderPath,
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
    dopt.method = o.methodDepth;
    dopt.keep = o.keepDepthMap;
    dopt.saveWaveletDebug = true;
    dopt.preview = o.previewDepthMap;
    if (o.method == "MultiScale") {
        dopt.alignFolder = alignFolderPath;
    }
    // dopt.method = "Simple"
    else {
        dopt.alignFolder.clear();
    }

    dopt.numThreads = 0;  // (reserved, not used inside yet)

    auto progressCb = [this]{ incrementProgress(); };
    // auto progressCb = [this](int){ incrementProgress(); };
    auto statusCb   = [this](const QString &msg){ status(msg); };

    if (abort) return false;

    bool ok = false;
    qDebug() << srcFun << "o.useCache" << o.useCache << "alignExists" <<alignExists;
    if (o.useCache && alignExists)
    {
        ok = FSDepth::runFromGraySlices(
            alignedGraySlices,
            depthFolderPath,
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
            focusFolderPath,
            depthFolderPath,
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
        QString msg = "Depth map not available. Run Depth stage first.";
        status(msg);
        if (G::FSLog) G::log(srcFun, msg);
        // return false;
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
    fopt.method = o.methodFuse;
    fopt.useOpenCL   = o.enableOpenCL;
    fopt.consistency = 2;

    auto progressCb = [this]{ incrementProgress(); };
    auto statusCb   = [this](const QString &msg){ status(msg); };


    if (!FSFusion::fuseStack(grayImgs,
                             colorImgs,
                             fopt,
                             depthIndex16Mat,
                             fusedColor8Mat,
                             &abort,
                             statusCb,
                             progressCb))
    {
        status("Fusion failed");
        return false;
    }

    saveFused(fusionFolderPath);

    return true;
}

bool FS::runBackground()
{
    const QString srcFun = "FS::runBackground";
    if (abort) return false;

    status("Building background mask...");
    if (G::FSLog) G::log(srcFun, "Building background mask...");

    FSBackground::Options bopt;
    bopt.method = o.backgroundMethod;
    bopt.writeDebug = true;
    bopt.backgroundFolder = backgroundFolderPath;

    // Ensure we have focusSlices and depthIndex16Mat (depending on method)
    const int N = slices;

    // alignedColor needed only if ColorVar enabled; pass empty otherwise
    std::vector<cv::Mat> colorForVar;
    if (bopt.enableColorVar)
    {
        // Prefer cache if available
        if (o.useCache && !alignedColorSlices.empty()) colorForVar = alignedColorSlices;
        else {
            // fallback: load from disk using alignedColorPaths
            colorForVar.reserve(N);
            for (int i = 0; i < N; ++i)
            {
                cv::Mat c = cv::imread(alignedColorPaths[i].toStdString(), cv::IMREAD_COLOR);
                if (c.empty()) return false;
                colorForVar.push_back(c);
            }
        }
    }

    auto progressCb = [this](int c, int t){ Q_UNUSED(t); Q_UNUSED(c); incrementProgress(); };
    auto statusCb   = [this, srcFun](const QString &m){ emit updateStatus(false, m, srcFun); };

    // cv::Mat subjectMask8;
    // cv::Mat bgConfidence01;
    // // store into members
    subjectMask8Mat.release();
    bgConfidence01Mat.release();

    if (!FSBackground::run(depthIndex16Mat,
                           focusSlices,
                           colorForVar,
                           N,
                           bopt,
                           &abort,
                           progressCb,
                           statusCb,
                           bgConfidence01Mat,
                           subjectMask8Mat))
    {
        status("Background mask failed.");
        return false;
    }

    // Debug overlay needs fused gray. If fusion hasn’t run yet, you can
    // overlay on any aligned gray slice (e.g., mid slice) for preview:
    if (!alignedGraySlices.empty() && o.previewBackgroundMask)
    {
        qDebug() << srcFun << "1";
        cv::Mat base = alignedGraySlices[N / 2];
        qDebug() << srcFun << "2";
        cv::Mat overlay = FSBackground::makeOverlayBGR(base, bgConfidence01Mat, bopt);
        qDebug() << srcFun << "3";
        cv::imwrite((bopt.backgroundFolder + "/bg_overlay.png").toStdString(), overlay);
    }

    // return true; // return before repair

    // Replace background to remove any halo
    if (o.enableBackgroundMask && !subjectMask8Mat.empty() && !alignedColorSlices.empty())
    {
        FSBackground::Options bopt;
        bopt.method = o.backgroundMethod;
        bopt.featherRadius = 6;
        bopt.featherGamma  = 1.2f;

        cv::Mat repaired = fusedColor8Mat.clone();              // canonical size+type target
        const cv::Size targetSize = repaired.size();
        const int targetType = repaired.type();

        auto ensure3ch = [](const cv::Mat& in) -> cv::Mat
        {
            if (in.empty()) return {};
            if (in.channels() == 3) return in;
            cv::Mat out;
            if (in.channels() == 4) cv::cvtColor(in, out, cv::COLOR_BGRA2BGR);
            else if (in.channels() == 1) cv::cvtColor(in, out, cv::COLOR_GRAY2BGR);
            else CV_Assert(false);
            return out;
        };

        auto canonicalizeMask8 = [&](const cv::Mat& inMask) -> cv::Mat
        {
            CV_Assert(!inMask.empty());
            cv::Mat m = inMask;

            // convert to 8U (0..255)
            if (m.type() != CV_8U)
            {
                if (m.depth() == CV_32F || m.depth() == CV_64F)
                {
                    // assume 0..1-ish mask
                    cv::Mat f; m.convertTo(f, CV_32F);
                    cv::threshold(f, f, 0.5, 1.0, cv::THRESH_BINARY);
                    f.convertTo(m, CV_8U, 255.0);
                }
                else
                {
                    // for 16U/other: treat >0 as on
                    cv::Mat tmp = (m > 0);
                    tmp.convertTo(m, CV_8U, 255.0);
                }
            }
            else
            {
                // if it's 8U but not binary, binarize defensively
                // (keeps your 0/255 masks as-is)
                // NOTE: if your mask is already 0/255 this does nothing harmful
                cv::threshold(m, m, 127, 255, cv::THRESH_BINARY);
            }

            // resize to target using NEAREST (categorical)
            if (m.size() != targetSize)
            {
                cv::Mat r;
                cv::resize(m, r, targetSize, 0, 0, cv::INTER_NEAREST);
                m = r;
            }

            return m;
        };

        auto canonicalizeBgToRepaired = [&](const cv::Mat& inBg) -> cv::Mat
        {
            CV_Assert(!inBg.empty());
            cv::Mat bg = ensure3ch(inBg);

            // resize to target using LINEAR (color image)
            if (bg.size() != targetSize)
            {
                cv::Mat r;
                cv::resize(bg, r, targetSize, 0, 0, cv::INTER_LINEAR);
                bg = r;
            }

            // convert type to match repaired
            if (bg.type() != targetType)
            {
                cv::Mat converted;

                const int targetDepth = CV_MAT_DEPTH(targetType);
                const int bgDepth     = CV_MAT_DEPTH(bg.type());

                if (targetDepth == CV_8U && (bgDepth == CV_32F || bgDepth == CV_64F))
                {
                    // assume float is 0..1-ish; clamp then scale
                    cv::Mat f; bg.convertTo(f, CV_32F);
                    cv::min(f, 1.0f, f);
                    cv::max(f, 0.0f, f);
                    f.convertTo(converted, targetType, 255.0);
                }
                else if ((targetDepth == CV_32F || targetDepth == CV_64F) && bgDepth == CV_8U)
                {
                    bg.convertTo(converted, targetType, 1.0 / 255.0);
                }
                else
                {
                    // generic conversion (no scaling guess)
                    bg.convertTo(converted, targetType);
                }

                bg = converted;
            }

            return bg;
        };

        // --- make mask canonical ---
        cv::Mat mask = canonicalizeMask8(subjectMask8Mat);

        // optional: erode subject edge to kill defocus spill/halo (still safe after resize/type fix)
        int edgeErodePx = 3;
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, {2*edgeErodePx+1, 2*edgeErodePx+1});
        cv::erode(mask, mask, se);

        // --- make bgSource canonical ---
        cv::Mat bgSource = canonicalizeBgToRepaired(alignedColorSlices.back());

        // Now guaranteed compatible with replaceBackground:
        // mask.size == repaired.size, mask.type==CV_8U
        // bgSource.size == repaired.size, bgSource.type == repaired.type
        FSBackground::replaceBackground(repaired, mask, bgSource, bopt, &abort);

        QFileInfo fi(lastFusedPath);
        QString repairedPath = fusionFolderPath + "/" + fi.baseName() + "_BBRepair.tif";
        cv::imwrite(repairedPath.toStdString(), repaired);
    }

    // // Replace background to remove any halo

    // // IMPORTANT: shrink foreground matte a bit to remove defocus spill
    // int edgeErodePx = 4; // start 3..6 at full res
    // cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, {2*edgeErodePx+1, 2*edgeErodePx+1});
    // cv::erode(subjectMask8Mat, subjectMask8Mat, se);

    // qDebug() << "FS::runBackground "
    //          << "depth"   << depthIndex16Mat.cols << depthIndex16Mat.rows
    //          << "mask"    << subjectMask8.cols << subjectMask8.rows
    //          << "fused"   << fusedColor8Mat.cols << fusedColor8Mat.rows
    //          << "bg"      << alignedColorSlices.back().cols << alignedColorSlices.back().rows;    // Composite fused over last slice

    // cv::Mat bgSource = alignedColorSlices.back();   // last aligned color image (same size as outputColor8)
    // cv::Mat fusedColor8InOut = fusedColor8Mat;
    // FSBackground::replaceBackground(fusedColor8InOut, subjectMask8, bgSource, bopt, &abort);

    // // Write fused with background repair
    // QFileInfo lastFused(lastFusedPath);
    // QString fusedbgRepairedPath = fusionFolder + "/" + lastFused.baseName() + "_BBRepair.tif";
    // if (G::FSLog) G::log(srcFun, "Saving fused image with background repair to "
    //                        + fusedbgRepairedPath);
    // cv::imwrite(fusedbgRepairedPath.toStdString(), fusedColor8InOut);

    // status("Background mask complete.");
    // return true;
}

bool FS::runArtifact()
{
    const QString srcFun = "FS::runArtifactDetection";

    if (abort) return false;

    emit updateStatus(false, "Detecting fusion artifacts...", srcFun);

    qDebug() << srcFun << "1";
    // Load fused image (color, aligned space)
    if (lastFusedPath.isEmpty())
    {
        QString msg = "No fused images found in " + fusionFolderPath;
        G::log(srcFun, msg);
        return false;
    }

    cv::Mat grayFused = cv::imread(lastFusedPath.toStdString(), cv::IMREAD_GRAYSCALE);
    qDebug() << srcFun << "2";

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

    QString depthPath = grpFolderPath + "/depth/depth_idx.exr"; // adjust
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

    QString maskPath = grpFolderPath + "/masks/include_mask.png";
    if (QFile::exists(maskPath))
    {
        includeMask8 = cv::imread(maskPath.toStdString(), cv::IMREAD_GRAYSCALE);
        if (!includeMask8.empty())
            maskPtr = &includeMask8;
    }

    // Artifact detection options
    FSArtifact::Options opt;
    opt.debugFolder = artifactsFolderPath;

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
    QString artifactFolder = grpFolderPath + "/artifact";
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

bool FS::runStreamWaveletPMax()
{
/*  Streaming Algorithm:
    for each slice
        - load input image
        - align (0 is special case) return graySlice, colorSlice
        - merge slice - update mergedWavelet
        - build color map
    next slice
    - finish merge
    - invert mergedWavelet
    - apply color map
    - crop back to original
*/
    QString srcFun = "FS::runStreamWaveletPMax";
    QString msg = "Start";
    if (G::FSLog) G::log(srcFun, msg);

    auto progressCb = [this]{ incrementProgress(); };
    auto statusCb   = [this](const QString &msg){ status(msg); };

    // Align options
    FSAlign::Options aopt;
    aopt.matchContrast     = true;
    aopt.matchWhiteBalance = true;
    aopt.lowRes            = 256;
    aopt.maxRes            = 2048;
    aopt.fullResolution    = false;

    // Fusion options
    FSFusion::Options fopt;
    fopt.method = o.methodFuse;
    fopt.useOpenCL   = o.enableOpenCL;
    fopt.consistency = 2;

    FSLoader::Image prevImage;
    FSLoader::Image currImage;
    FSAlign::Result prevGlobal;
    FSAlign::Result currGlobal;
    cv::Mat alignedGraySlice;
    cv::Mat alignedColorSlice;
    FSAlign::Align align;
    FSFusion fuse;

    for (int slice = 0; slice < slices; slice++) {
        QString s = " Slice: " + QString::number(slice) + " of " +
                    QString::number(slices) + " ";
        // Load input image slice
        status(s + "Loading source input image...");
        currImage = FSLoader::load(inputPaths.at(slice).toStdString());
        incrementProgress();

        // Align slice
        currGlobal = FSAlign::makeIdentity(currImage.validArea);
        if (slice == 0) {
            status(s + "Identifying for alignment");
            alignedGraySlice = currImage.gray.clone();
            alignedColorSlice = currImage.color.clone();
            fuse.orig = currImage.gray.size();
            incrementProgress();
        }
        else {
            status(s + "Aligning...");
            if (!align.alignSlice(slice, prevImage, currImage, prevGlobal, currGlobal,
                                  alignedGraySlice, alignedColorSlice,
                                  aopt, &abort, statusCb, progressCb)) {
                qWarning() << "WARNING:" << srcFun << "align.alignSlice failed.";
                return false;
            }
            incrementProgress();
        }

        // Fuse slice
        status(s + "Fusing...");
        if (!fuse.streamPMaxSlice(slice, alignedGraySlice, alignedColorSlice,
                                  fopt, &abort, statusCb, progressCb)) {
            qWarning() << "WARNING:" << srcFun << "fuse.streamPMaxSlice failed.";
            return false;
        }
        incrementProgress();

        prevImage = currImage;
        prevGlobal = currGlobal;

    }

    msg = "Slice processing done.  Finish merge, invert, recover color and crop padding";
    if (G::FSLog) G::log(srcFun, msg);

    // finish merge, invert, recover color and crop padding
    status("Finalizing fusion...");
    if (!fuse.streamPMaxFinish(fusedColor8Mat, fopt, &abort, statusCb, progressCb))
        return false;
    incrementProgress();

    if (o.useIntermediates) saveFused(fusionFolderPath);

    return true;
}

bool FS::saveFused(QString folderPath)
{
    QString srcFun = "FS::saveFused";

    // Make file name for fused image
    QFileInfo lastFi(inputPaths.last());
    QString base = lastFi.completeBaseName() + "_FocusStack_" + o.method;
    QString ext  = lastFi.suffix();
    QString fusedPath = folderPath + "/" + base + "." + ext;
    // if exists add incrementing suffix
    Utilities::uniqueFilePath(fusedPath, "_");
    QFileInfo fusedFi(fusedPath);
    base = fusedFi.completeBaseName();
    QString xmpPath   = folderPath + "/" + base + "." + "xmp";
    QString msg = "Folder: " + folderPath + "  Last input image: " + lastFi.completeBaseName();
    if (G::FSLog) G::log(srcFun, folderPath);


    // Write fused result
    msg = "Write to " + fusedPath;
    if (G::FSLog) G::log(srcFun, msg);
    cv::imwrite(fusedPath.toStdString(), fusedColor8Mat);

    // Copy metadata from first source using your existing logic
    msg = "Copy metadata using ExifTool from " + inputPaths.last();
    if (G::FSLog) G::log(srcFun, msg);
    ExifTool et;
    et.setOverWrite(true);
    et.copyAll(inputPaths.last(), fusedPath);
    et.close();

    // Embed thumbnail
    msg = "Embed thumbnail";
    if (G::FSLog) G::log(srcFun, msg);
    if (ext == "tif") {
        Tiff tiff;
        QImage thumb = thumbnail(fusedColor8Mat);
        qDebug() << srcFun << msg << thumb.width() << thumb.height();
        if (!thumb.isNull()) {
            if (!tiff.embedIRBThumbnail(fusedPath, thumb)) {
                QString msg = "Failed to embed thumbnail in tif file " + fusedPath;
                qWarning() << "WARNING" << srcFun << msg;
            }
        } else {
            QString msg = "Failed to create thumbnail for " + fusedPath;
            qWarning() << "WARNING" << srcFun << msg;
        }
    }
    if (ext == "jpg") {
        Jpeg jpeg;
        if (!jpeg.embedThumbnail(fusedPath)) {
            QString msg = "Failed to embed thumbnail in jpeg file " + fusedPath;
            qWarning() << "WARNING" << srcFun << msg;
        }
    }

    // Write XMP to highlight with color
    msg = "Write XMP color green";
    if (G::FSLog) G::log(srcFun, msg);
    QFile f(xmpPath);
    if (!f.open(QIODevice::ReadWrite)) return false;

    QString color = "Green";
    QString modifyDate = QDateTime::currentDateTime().toOffsetFromUtc
                         (QDateTime::currentDateTime().offsetFromUtc()).toString(Qt::ISODate);
    Xmp xmp(f, 0);
    xmp.setItem("Label", color.toLatin1());
    xmp.setItem("ModifyDate", modifyDate.toLatin1());
    xmp.writeSidecar(f);
    f.close();

    msg = "Save completed";
    if (G::FSLog) G::log(srcFun, msg);

    return true;
}

void FS::diagnostics()
{
    qDebug() << "Diagnostics:"
        << "\n"
        << "dstFolderPath           =" << dstFolderPath << "\n"
        << "fusedBase               =" << fusedBase << "\n"

        << "\n"
        << "o.method                =" << o.method << "\n"
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

    for (int gi = 0; gi < groups.size(); ++gi) {
        qDebug().noquote() << "FocusStack Group" << (gi + 1);
        for (const QString &p : groups[gi]) {
            qDebug().noquote() << " " << p;
        }
    }
    qDebug() << "\n";

}
