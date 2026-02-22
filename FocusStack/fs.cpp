#include "fs.h"
#include "Main/global.h"
#include "FocusStack/fsalign.h"
#include "FocusStack/fsloader.h"
#include "FocusStack/fsfocus.h"
#include "FocusStack/fsdepth.h"
#include "FocusStack/fsfusion.h"
#include "FocusStack/fsfusiondmap.h"
#include "FocusStack/fsfusionpmax.h"
#include "FocusStack/fsmerge.h"
#include "FocusStack/fsfusionreassign.h"
#include "FocusStack/fsfusionwavelet.h"
// #include "FocusStack/fsbackground.h"
// #include "FocusStack/fsartifact.h"
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

static QStringList methods {
    "StreamPMax",         // PMax1, but streamed
    "PMax",               // align, fuse using multiscale wavelets
    "TennengradVersions"  // create multiple depth maps for diff radius/thresholds
};


void FS::initialize(QString dstFolderPath, QString fusedBase)
{
    this->dstFolderPath = dstFolderPath;
    this->fusedBase = fusedBase;
}

bool FS::initializeGroup(int group)
{
    QString srcFun = "FS::initializeGroup";
    QString msg = "Group " + QString::number(group);
    if (G::FSLog) G::log(srcFun);

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

bool FS::setOptions(const Options &opt)
{
    o = opt;
    QString srcFun = "FS::setOptions";
    QString msg = "method: " + o.method;
    if (G::FSLog) G::log(srcFun, msg);
    if (o.method.isEmpty()) {
        qWarning() << "WARNING:" << srcFun << "No method.";
    }

    msg = "Setting options: ";
    // Fuse using grayscale wavelet merge and color map without using focus
    // or depth modules.
    if (o.method == "PMax") {
        if (G::FSLog) G::log(srcFun, msg + "PMax");
        o.enableAlign = true;
        o.enableFocusMaps = false;
        o.enableDepthMap = false;
        o.enableFusion = true;
        o.methodFuse = "FullWaveletMerge";
        return true;
    }

    // if (o.method == "StmDMap") {
    //     if (G::FSLog) G::log(srcFun, msg + "StmDMap");
    //     o.useIntermediates = true;
    //     o.enableAlign = true;
    //     o.enableFocusMaps = false;
    //     o.enableDepthMap = true;
    //     o.enableFusion = true;
    //     o.methodFuse = "FullWaveletMerge";
    //     o.methodMerge = "PMax";
    //     o.isStream = true;
    //     return true;
    // }

    if (o.method == "StmDMap") {
        if (G::FSLog) G::log(srcFun, msg + "StmDMap");
        o.useIntermediates = true;
        o.enableAlign = true;
        o.enableFocusMaps = false;
        o.enableDepthMap = true;
        o.enableFusion = true;
        // o.methodFuse = "FullWaveletMerge";
        // o.methodMerge = "PMax";
        o.isStream = true;
        return true;
    }


    // Fuse by streaming using grayscale wavelet merge and color map.  Do not need
    // to hold all images and Mats in memory.
    if (o.method == "StmPMax") {
        if (G::FSLog) G::log(srcFun, msg + "StmPMax");
        o.useIntermediates = true;
        o.enableAlign = true;
        o.enableFocusMaps = false;
        o.enableDepthMap = true;
        o.enableFusion = true;
        o.methodFuse = "FullWaveletMerge";
        o.methodMerge = "PMax";
        o.isStream = true;
        return true;
    }

    if (o.method == "StmPMaxWt") {
        if (G::FSLog) G::log(srcFun, msg + "StrPMaxWt");
        o.useIntermediates = true;
        o.enableAlign = false;
        o.enableFocusMaps = false;
        o.enableDepthMap = true;
        o.enableFusion = true;
        o.methodFuse = "FullWaveletMerge";
        o.methodMerge = "Weighted";
        o.enableDepthBiasedErosion = false;
        o.enableEdgeAdaptiveSigma = false;
        o.isStream = true;
        return true;
    }

    if (o.method == "StmPMaxWtDbe") {
        if (G::FSLog) G::log(srcFun, msg + "StmPMaxWtDbe");
        o.useIntermediates = true;
        o.enableAlign = false;
        o.enableFocusMaps = false;
        o.enableDepthMap = true;
        o.enableFusion = true;
        o.methodFuse = "FullWaveletMerge";
        o.methodMerge = "Weighted";
        o.enableDepthBiasedErosion = true;
        o.enableEdgeAdaptiveSigma = false;
        o.isStream = true;
        return true;
    }

    // Fuse by streaming using grayscale wavelet merge and color map.  Do not need
    // to hold all images and Mats in memory.
    if (o.method == "TennengradVersions") {
        if (G::FSLog) G::log(srcFun, msg + "TennengradVersions");
        o.useIntermediates = true;
        o.enableAlign = false;
        o.enableFocusMaps = false;
        o.enableDepthMap = true;
        o.enableFusion = true;
        o.methodFuse = "FullWaveletMerge";
        o.isStream = true;
        return true;
    }

    // Fuse using multiscale depthmap, focus module not used.
    if (o.method == "PMax2") {
        if (G::FSLog) G::log(srcFun, msg + "PMax2");
        o.enableAlign = true;
        o.enableFocusMaps = true;
        o.enableDepthMap = true;
        o.enableFusion = true;
        o.methodDepth = "MultiScale";
        o.methodFuse = "Simple";
        return true;
    }

    // Test tenengrad focus depthmap
    if (o.method == "Ten") {
        if (G::FSLog) G::log(srcFun, msg + "Ten");
        o.enableAlign = true;
        o.enableFocusMaps = false;
        o.enableDepthMap = true;
        o.enableFusion = false;
        o.methodDepth = "TennengradVersions";
        o.methodFuse = "Simple";
        return true;
    }

    if (G::FSLog) G::log(srcFun, "WARNING: METHOD NOT RECOGNIZED");
    return false;
}

void FS::status(const QString &msg)
{
    QString statusMsg = statusRunPrefix + statusGroupPrefix + msg;
    emit updateStatus(false, statusMsg, "");
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

void FS::setTotalProgress(int runs)
{
    QString srcFun = "FS::setTotalProgress";
    progressTotal = 0;
    for (const QStringList &g : groups) {
        int slices = g.count();
        if (o.isStream) {
            progressTotal += (slices * 2 + 1) * runs;
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
    QString msg = "progressTotal = " + QString::number(progressTotal);
    if (G::FSLog) G::log(srcFun, msg);

}

void FS::incrementProgress()
{
    QString srcFun = "FS::incrementProgress";
    emit progress(++progressCount, progressTotal);
    QString msg = QString::number(progressCount) + "/" + QString::number(progressTotal);
    // if (G::FSLog) G::log(srcFun, msg);
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
/*
    This is the entry point for the focus stacking pipeline.  Normally, this function
    just calls run(), but it can also call run() multiple times for different settings
    or parameters to test which ones work best.

    When testing, the values in A and B must be set in the appropriate run method
    ie if A is fusion weightedPower method  uses runStreamWaveletPMax then you must
*/
    QString srcFun = "FS::run";

// Test Parameters in two vectors
    // A: weightedPower     aItem in header
    QVector<QVariant> A{};
    // B: weightedSigma0    bItem in header
    QVector<QVariant> B{};

    runIdx = 0;
    runTotal = A.count() * B.count();
    if (runTotal == 0) runTotal = 1;
    setTotalProgress(runTotal);

    // Just run existing settings
    if (A.isEmpty() && B.isEmpty()) {
        if (!runGroups()) return false;
    }

    // Iterate testing parameters
    else {
        for (const QVariant& a : A) {
            for (const QVariant& b : B) {
                runIdx++;
                statusRunPrefix = "Run " + QString::number(runIdx) +
                                  " of " + QString::number(runTotal) + "  ";
                if (G::FSLog) G::log(srcFun, statusRunPrefix);
                aItem = a;
                bItem = b;
                if (!runGroups()) return false;
            }
        }
    }

    return true;
}

bool FS::runGroups(QVariant aItem, QVariant bItem)
{
/*
    A group is a focus stack of images in a folder. The groups are converted into a
    list of paths in MW::groupFocusStacks.  This function iterates through all the
    groups that may have been selected.

    This function is called from FS::run.  If testing multiple options they will be
    passed as parameters, otherwise if null {} then they are ignored.
*/
    QString srcFun = "FS::runGroups";

    QString msg = "method = " + o.method +
                  "  stream = " + QVariant(o.isStream).toString();
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

            if (o.method == "StmDMap") {
                if (!runStreamDMap()) {
                    emit finished(false);
                    return false;
                }
            }

            if (o.method == "StmPMax") {
                if (!runStreamPMax()) {
                    emit finished(false);
                    return false;
                }
            }

            if (o.method == "StmPMaxWt") {
                if (!runStreamPMax()) {
                    emit finished(false);
                    return false;
                }
            }

            if (o.method == "TennengradVersions") {
                if (!runStreamTennengradVersions()) {
                    emit finished(false);
                    return false;
                }
            }

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

        }

        // SAVE
        // if (o.enableFusion) save(dstFolderPath);

        // STATS
        QString timeToRun = QString::number(t.elapsed() / 1000, 'f', 1) + " sec";
        QString progressSteps = " Progress step count = " + QString::number(progressCount);
        QString progressTot = " Progress step total = " + QString::number(progressTotal);
        if (G::FSLog) G::log(srcFun, "Focus Stack completed in " + timeToRun + progressSteps + progressTot);
        if (G::FSLog) G::log("");
        status("Focus Stack completed in " + timeToRun);

        // qApp->processEvents();  // complete any waiting log msgs
    }

    // diagnostics();

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


    // if (!FSFusion::fuseStack(grayImgs,
    //                          colorImgs,
    //                          fopt,
    //                          depthIndex16Mat,
    //                          fusedColorMat,
    //                          &abort,
    //                          statusCb,
    //                          progressCb))
    // {
    //     status("Fusion failed");
    //     return false;
    // }

    save(fusionFolderPath);

    return true;
}

bool FS::runStreamDMap()
{
    QString srcFun = "FS::runStreamDMap";
    QString msg = "Start using method: " + o.method;
    if (G::FSLog) G::log(srcFun, msg);

    auto progressCb = [this]{ incrementProgress(); };
    auto statusCb   = [this](const QString &m){ status(m); };

    // -----------------------------
    // align options (same as pmax)
    // -----------------------------
    FSAlign::Options aopt;
    aopt.matchContrast     = true;
    aopt.matchWhiteBalance = true;
    aopt.lowRes            = 256;
    aopt.maxRes            = 2048;
    aopt.fullResolution    = false;

    // -----------------------------
    // fusion options (dmap basic)
    // -----------------------------
    FSFusion::Options fopt;
    fopt.method          = "DMap";
    fopt.mergeMode       = o.methodMerge;
    fopt.useOpenCL       = o.enableOpenCL;
    fopt.consistency     = 2;
    fopt.depthFolderPath = depthFolderPath;
    fopt.enableDepthBiasedErosion = false;
    fopt.enableEdgeAdaptiveSigma  = false;

    // -----------------------------
    // working state
    // -----------------------------
    FSLoader::Image prevImage;
    FSLoader::Image currImage;

    Result prevGlobal;
    Result currGlobal;

    cv::Mat alignedGraySlice;   // ALIGN space
    cv::Mat alignedColorSlice;  // ALIGN space

    std::vector<Result> globals;
    globals.reserve(slices);

    FSAlign::Align align;

    // New basic engine
    FSFusionDMap fuse;

    // -----------------------------
    // pass-1: slice loopr
    // -----------------------------
    for (int slice = 0; slice < slices; ++slice)
    {
        QString s = " Slice: " + QString::number(slice) + " of " + QString::number(slices) + " ";
        if (G::FSLog) G::log("");

        status("Aligning and depth mapping" + s);

        currImage  = FSLoader::load(inputPaths.at(slice).toStdString());
        currGlobal = FSAlign::makeIdentity(currImage.validArea);

        if (G::abortFocusStack) return false;

        if (slice == 0)
        {
            alignedGraySlice  = currImage.gray.clone();
            alignedColorSlice = currImage.color.clone();

            // base geometry that dmap requires (same pattern as DMapAdvanced)
            fuse.validAreaAlign = currImage.validArea; // ALIGN -> ORIG crop
            fuse.alignSize      = alignedGraySlice.size();
            fuse.origSize       = cv::Size(currImage.validArea.width, currImage.validArea.height); // optional but ok
            fuse.outDepth       = alignedColorSlice.depth(); // 8u/16u output depth

            globals.push_back(currGlobal);
        }
        else
        {
            if (!align.alignSlice(slice,
                                  prevImage, currImage,
                                  prevGlobal, currGlobal,
                                  alignedGraySlice, alignedColorSlice,
                                  aopt, &abort, statusCb, progressCb))
            {
                qWarning() << "WARNING:" << srcFun << "align.alignSlice failed.";
                return false;
            }

            globals.push_back(currGlobal);
        }

        // enforce gray is 8-bit for dmap basic (focus metric expects CV_8U)
        if (alignedGraySlice.type() != CV_8U)
        {
            cv::Mat tmp;
            alignedGraySlice.convertTo(tmp, CV_8U);
            alignedGraySlice = tmp;
        }

        // debug write aligned slices (optional, matches your current workflow)
        cv::Mat alignedImg = FSUtilities::alignToOrigSize(alignedColorSlice, currImage.origSize);
        cv::imwrite(alignedColorPaths[slice].toStdString(), alignedImg);
        cv::Mat alignedGrayImg = FSUtilities::alignToOrigSize(alignedGraySlice, currImage.origSize);
        cv::imwrite(alignedGrayPaths[slice].toStdString(), alignedGrayImg);

        if (G::abortFocusStack) return false;

        // pass-1 update (pads internally)
        if (!fuse.streamSlice(slice,
                              alignedGraySlice,
                              alignedColorSlice,
                              fopt,
                              &abort,
                              statusCb,
                              progressCb))
        {
            qWarning() << "WARNING:" << srcFun << "fuse.streamSlice failed.";
            return false;
        }

        if (G::abortFocusStack) return false;
        incrementProgress();

        prevImage  = currImage;
        prevGlobal = currGlobal;
    }

    // tmp debugging
    fuse.alignedColorPaths = alignedColorPaths;
    fuse.alignedGrayPaths = alignedGrayPaths;

    msg = "Slice processing done. DMapBasic finish: build maps, stream slices, blend, crop.";
    if (G::FSLog) G::log(srcFun, msg);

    // -----------------------------
    // pass-2: finish
    // -----------------------------
    status("Finalizing DMapBasic fusion...");
    if (!fuse.streamFinish(fusedColorMat,
                           fopt,
                           depthIndex16Mat,
                           inputPaths,
                           globals,
                           &abort,
                           statusCb,
                           progressCb))
    {
        qWarning() << "WARNING:" << srcFun << "FSFusionDMapBasic::streamFinish failed.";
        return false;
    }

    if (G::abortFocusStack) return false;
    incrementProgress();

    if (o.useIntermediates) save(fusionFolderPath);

    return true;
}

bool FS::runStreamPMax()
{
    QString srcFun = "FS::runStreamPMax";
    QString msg = "Start using method: " + o.method;
    if (G::FSLog) G::log(srcFun, msg);

    FSFusion::ProgressCallback progressCb = [this]{ incrementProgress(); };
    FSFusion::StatusCallback   statusCb   = [this](const QString &m){ status(m); };

    // -----------------------------
    // Align options (same as DMap)
    // -----------------------------
    FSAlign::Options aopt;
    aopt.matchContrast     = true;
    aopt.matchWhiteBalance = true;
    aopt.lowRes            = 256;
    aopt.maxRes            = 2048;
    aopt.fullResolution    = false;

    // -----------------------------
    // Fusion options (PMax)
    // -----------------------------
    FSFusion::Options fopt;
    fopt.method          = o.methodFuse;     // e.g. "PMax"
    fopt.mergeMode       = o.methodMerge;    // "PMax" or "Weighted"
    fopt.useOpenCL       = o.enableOpenCL;
    fopt.consistency     = 2;
    fopt.depthFolderPath = depthFolderPath;

    // Weighted / staged pipeline toggles
    const bool wantsWeighted =
        (o.method == "StmPMaxWt" || o.method == "StmPMaxWtDbe" || o.method == "StmPMaxWtDbeEas");

    if (wantsWeighted)
    {
        // winner map selection for stage-2/3/4
        fopt.winnerMap = (o.enableDepthBiasedErosion ? "Energy" : "Weighted");

        // weighted params (your tuned defaults)
        fopt.weightedPower          = 7.0f;     // 4.0f default
        fopt.weightedSigma0         = 2.0f;     // 1.0f default
        fopt.weightedIncludeLowpass = true;     // false looks bad
        fopt.weightedEpsEnergy      = 1e-8f;
        fopt.weightedEpsWeight      = 1e-8f;

        // stage 3/4 toggles
        fopt.enableDepthBiasedErosion = o.enableDepthBiasedErosion;
        fopt.enableEdgeAdaptiveSigma  = o.enableEdgeAdaptiveSigma;

        // optional: tag special runs
        o.methodInfo = "LL";
    }
    else
    {
        // keep deterministic defaults
        fopt.winnerMap = "Weighted";
        fopt.enableDepthBiasedErosion = false;
        fopt.enableEdgeAdaptiveSigma  = false;
    }

    qDebug() << "o.enableDepthBiasedErosion =" << o.enableDepthBiasedErosion
             << "fopt.enableDepthBiasedErosion =" << fopt.enableDepthBiasedErosion;

    // -----------------------------
    // Working state
    // -----------------------------
    FSLoader::Image prevImage;
    FSLoader::Image currImage;

    Result prevGlobal;
    Result currGlobal;

    cv::Mat alignedGraySlice;   // ALIGN space
    cv::Mat alignedColorSlice;  // ALIGN space

    // Used by streamFinish for reloads / erosion stage
    std::vector<Result> globals;
    globals.reserve(slices);

    FSAlign::Align align;
    FSFusionPMax   fuse;    // engine (inherits FSFusion base)

    // -----------------------------
    // Pass-1: slice loop
    // -----------------------------
    for (int slice = 0; slice < slices; ++slice)
    {
        QString s = " Slice: " + QString::number(slice) + " of " + QString::number(slices) + " ";
        if (G::FSLog) G::log("");

        status(s + "Aligning...");

        currImage  = FSLoader::load(inputPaths.at(slice).toStdString());
        currGlobal = FSAlign::makeIdentity(currImage.validArea);

        if (G::abortFocusStack) return false;

        if (slice == 0)
        {
            alignedGraySlice  = currImage.gray.clone();
            alignedColorSlice = currImage.color.clone();

            // Geometry required by streaming engines (base-class fields)
            fuse.validAreaAlign = currImage.validArea;
            fuse.origSize       = currImage.origSize;         // informational; ORIG is validAreaAlign dims
            fuse.alignSize      = alignedGraySlice.size();
            fuse.outDepth       = alignedColorSlice.depth();

            globals.push_back(currGlobal);
        }
        else
        {
            if (!align.alignSlice(slice,
                                  prevImage, currImage,
                                  prevGlobal, currGlobal,
                                  alignedGraySlice, alignedColorSlice,
                                  aopt, &abort, statusCb, progressCb))
            {
                qWarning() << "WARNING:" << srcFun << "align.alignSlice failed.";
                return false;
            }

            globals.push_back(currGlobal);
        }

        // Debug/test write aligned slices
        {
            cv::Mat alignedImg = FSUtilities::alignToOrigSize(alignedColorSlice, currImage.origSize);
            cv::imwrite(alignedColorPaths[slice].toStdString(), alignedImg);
        }

        if (G::abortFocusStack) return false;
        incrementProgress();

        // -----------------------------
        // Fuse slice (PMax streaming)
        // -----------------------------
        status(s + "Fusing...");

        if (!fuse.streamSlice(slice,
                              alignedGraySlice,
                              alignedColorSlice,
                              fopt,
                              &abort,
                              statusCb,
                              progressCb))
        {
            qWarning() << "WARNING:" << srcFun << "fuse.streamSlice failed.";
            return false;
        }

        if (G::abortFocusStack) return false;
        incrementProgress();

        prevImage  = currImage;
        prevGlobal = currGlobal;
    }

    msg = "Slice processing done. Finish merge, invert, recover color and crop padding.";
    if (G::FSLog) G::log(srcFun, msg);

    // -----------------------------
    // Pass-2: finish
    // -----------------------------
    status("Finalizing fusion...");

    if (!fuse.streamFinish(fusedColorMat,
                           fopt,
                           depthIndex16Mat,
                           inputPaths,
                           globals,
                           &abort,
                           statusCb,
                           progressCb))
    {
        qWarning() << "WARNING:" << srcFun << "FSFusionPMax::streamFinish failed.";
        return false;
    }

    if (G::abortFocusStack) return false;
    incrementProgress();

    // Optional parameter dump (keep if useful)
    if (wantsWeighted)
    {
        qDebug().noquote()
            << "\n weightedPower            =" << fopt.weightedPower
            << "\n weightedSigma0           =" << fopt.weightedSigma0
            << "\n weightedIncludeLowpass   =" << fopt.weightedIncludeLowpass
            << "\n weightedEpsEnergy        =" << fopt.weightedEpsEnergy
            << "\n weightedEpsWeight        =" << fopt.weightedEpsWeight
            << "\n enableDepthBiasedErosion =" << fopt.enableDepthBiasedErosion
            << "\n enableEdgeAdaptiveSigma  =" << fopt.enableEdgeAdaptiveSigma
            << "\n";
    }

    if (o.useIntermediates) save(fusionFolderPath);
    return true;
}

bool FS::runStreamTennengradVersions()
/*
    Iterate radius and threshold values to produce different depth maps.  No fused
    image is produced.
*/
{
    QString srcFun = "FS::runStreamTennengradVersions";
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

    // Depth map options
    FSDepth::Options dopt;
    dopt.method = "Tenengrad";
    FSDepth::StreamState depthState;
    QVector<float> radius{1};
    QVector<float> threshold{4000,5000,6000};

    FSLoader::Image prevImage;
    FSLoader::Image currImage;
    Result prevGlobal;
    Result currGlobal;
    cv::Mat alignedGraySlice;
    cv::Mat alignedColorSlice;
    FSAlign::Align align;

    auto runVersion = [&]() {
        for (int slice = 0; slice < slices; slice++) {
            QString s = " Slice: " + QString::number(slice) + " of " +
                        QString::number(slices) + " ";
            // Load input image slice
            status(s + "Loading source input image...");
            currImage = FSLoader::load(inputPaths.at(slice).toStdString());
            if (G::abortFocusStack) return false;
            incrementProgress();

            // Align slice
            currGlobal = FSAlign::makeIdentity(currImage.validArea);
            if (slice == 0) {
                status(s + "Identifying for alignment");
                alignedGraySlice = currImage.gray.clone();
                alignedColorSlice = currImage.color.clone();
                if (G::abortFocusStack) return false;
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
                if (G::abortFocusStack) return false;
                incrementProgress();
            }

            // Depth map for slice
            if (!FSDepth::streamGraySlice(alignedGraySlice, slice, depthFolderPath, dopt,
                                          depthState, &abort, progressCb, statusCb)) {
                qWarning() << "WARNING:" << srcFun << "FSDepth::streamGraySlice failed.";
                return false;
            }

            if (G::abortFocusStack) return false;
            incrementProgress();

            prevImage = currImage;
            prevGlobal = currGlobal;

        }

        msg = "Slice processing done.  Finish merge, invert, recover color and crop padding";
        if (G::FSLog) G::log(srcFun, msg);

        // finish depth map
        if (!FSDepth::finishStreaming(depthFolderPath, dopt, slices, depthState,
                                      statusCb, &depthIndex16Mat)) {
            qWarning() << "WARNING:" << srcFun << "FSDepth::finishStreaming failed.";
        }

        return true;
    };


    for (float r : radius) {
        for (float t : threshold) {
            depthState.reset();
            depthState.ten.radius = r;
            depthState.ten.threshold = t;
            QString msg = "radius = " + QString::number(r) +
                          " threshold = " + QString::number(t);
            if (G::FSLog) G::log(srcFun, msg);
            if (!runVersion()){
                qWarning() << "WARNING:" << srcFun << "Failed.";
            }
        }
    }

    return true;
}

bool FS::save(QString fuseFolderPath)
{
    QString srcFun = "FS::save";

    // // Save wavelet depth map
    // qDebug() << srcFun << "depthFolderPath =" << depthFolderPath;
    // QString depthPath = depthFolderPath + "/depth_index.png";
    // FSUtilities::writePngWithTitle(depthPath, depthIndex16Mat);
    // QString depthPreviewPath = depthFolderPath + "/depth_preview.png";
    // cv::Mat preview = FSUtilities::makeDepthPreviewEnhanced(depthIndex16Mat, slices);
    // FSUtilities::writePngWithTitle(depthPreviewPath, preview);

    // Make file name for fused image
    QFileInfo lastFi(inputPaths.last());
    QString base = lastFi.completeBaseName() + "_FocusStack_" + o.method + o.methodInfo;
    // QString base = lastFi.completeBaseName() + "_FocusStack_" + o.method + o.methodInfo;
    QString ext  = lastFi.suffix();
    QString fusedPath = fuseFolderPath + "/" + base + "." + ext;
    // if exists add incrementing suffix
    Utilities::uniqueFilePath(fusedPath, "_");
    QFileInfo fusedFi(fusedPath);
    base = fusedFi.completeBaseName();
    QString xmpPath   = fuseFolderPath + "/" + base + "." + "xmp";
    QString msg = "Folder: " + fuseFolderPath + "  Last input image: " + lastFi.completeBaseName();
    if (G::FSLog) G::log(srcFun, fuseFolderPath);

    // Write fused result
    msg = "Write to " + fusedPath;
    if (G::FSLog) G::log(srcFun, msg);
    cv::imwrite(fusedPath.toStdString(), fusedColorMat);

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
        cv::Mat thumbSrc = fusedColorMat;
        if (thumbSrc.depth() == CV_16U) {
            cv::Mat tmp8;
            thumbSrc.convertTo(tmp8, CV_8U, 1.0 / 257.0);
            thumbSrc = tmp8;
        }
        QImage thumb = thumbnail(thumbSrc);
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
        << "o.methodAlign           =" << o.methodAlign << "\n"
        << "o.methodFocus           =" << o.methodFocus << "\n"
        << "o.methodDepth           =" << o.methodDepth << "\n"
        << "o.methodFuse            =" << o.methodFuse << "\n"
        << "o.methodMerge           =" << o.methodMerge << "\n"
        << "\n"
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
        << "\n"
        << "Fusion parameters:"
        ;

    for (int gi = 0; gi < groups.size(); ++gi) {
        qDebug().noquote() << "FocusStack Group" << (gi + 1);
        for (const QString &p : groups[gi]) {
            qDebug().noquote() << " " << p;
        }
    }
    qDebug() << "\n";

}
