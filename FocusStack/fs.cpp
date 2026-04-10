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
#include <QtConcurrent>
#include <QDebug>

FS::FS(QObject *parent)
    : QObject(parent)
{
}

bool FS::initializeGroup(int group)
{
    QString srcFun = "FS::initializeGroup";
    QString msg = "Group " + QString::number(group);
    if (G::FSLog) G::log(srcFun);

    inputPaths.clear();
    inputPaths = groups.at(group);
    grpSlices = inputPaths.count();
    grpLastSlice = grpSlices - 1;

    QFileInfo info(inputPaths.first());
    const QString srcFolder = info.absolutePath();
    grpFolderPath = srcFolder + "/" + info.completeBaseName() + "_" + o.method;
    grpFolderPaths << grpFolderPath;
    // qDebug() << "group" << group << "grpFolderPaths =" << grpFolderPaths;

    prepareFolders();

    statusGroupPrefix = "Stack: " + QString::number(group+1) + " of " +
                        QString::number(groups.count());

    // Clear any in-memory aligned images from previous run
    alignedColorPaths.clear();
    alignedGrayPaths.clear();
    alignedColorSlices.clear();
    alignedGraySlices.clear();

    // populate alignedColorPaths, alignedGrayPaths
    for (const QString &src : inputPaths) {
        QFileInfo fi(src);
        QString base = fi.completeBaseName();
        QString ext  = "png";
        QString path = alignFolderPath + "/aligned_" + base + "." + ext;
        alignedColorPaths.push_back(path);
        path = alignFolderPath + "/gray_" + base + "." + ext;
        alignedGrayPaths.push_back(path);
    }

    // for (const QString &s : alignedColorPaths)  qDebug().noquote() << s;
    // for (const QString &s : alignedGrayPaths)  qDebug().noquote() << s;

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
        return false;
    }
    return true;
}

void FS::status(const QString &msg)
{
    QString remaining = "Time remaining: " +
                        QTime(0, 0).addMSecs(msToGo).toString("hh:mm:ss");
    QString statusMsg = statusRunPrefix +
                        remaining.leftJustified(28) +
                        statusGroupPrefix.leftJustified(20) +
                        msg;
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
    depthFolderPath      = grpFolderPath + "/depth";
    fusionFolderPath     = grpFolderPath + "/fusion";

    dir.mkpath(alignFolderPath);
    dir.mkpath(depthFolderPath);
    dir.mkpath(fusionFolderPath);

    return true;
}

void FS::initializeProgress()
{
    QString srcFun = "FS::setTotalProgress";
    progressTotal = 0;
    for (const QStringList &g : groups) {
        int gSlices = g.count();
        if (o.method == "DMap") progressTotal += (gSlices * 3 + 2);
        if (o.method == "PMax") progressTotal += (gSlices * 2 + 1);
        totSlices += gSlices;
    }
    QString msg = "progressTotal = " + QString::number(progressTotal);
    if (G::FSLog) G::log(srcFun, msg);

}

void FS::incrementProgress()
{
    static int msPrevStep = 0;
    qint64 msElapsed = t.elapsed();
    int msStep = msElapsed - msPrevStep;
    QString srcFun = "FS::incrementProgress";
    qint64 msPerStep = msElapsed / progressCount;
    msToGo = msPerStep * (progressTotal - progressCount);
    emit progress(++progressCount, progressTotal);

    if (G::FSLog){
        QString msg = QString::number(progressCount) + "/" +
                      QString::number(progressTotal) +
                      "  msStep = " + QString::number(msStep) +
                      "  msPerStep = " + QString::number(msPerStep) +
                      "  msToGo = " + QString::number(msToGo)
            ;
        G::log(srcFun, msg);
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

void FS::run()
{
/*
    This is the entry point for the focus stacking pipeline.
*/
    QString srcFun = "FS::run";
    if (G::FSLog) G::log(srcFun, o.method);

    G::fsFusedPaths.clear();

    t.start();
    initializeProgress();
    incrementProgress();

    QElapsedTimer t;
    t.start();

    QString msg;

    // iterate groups
    int groupCounter = 0;
    for (const QStringList &g : groups) {

        grpSlices = g.count();
        if (grpSlices < 2) {
            groupCounter++;
            continue;
        }

        msg = "Preparing to focus stack group " + QString::number(groupCounter);
        status(msg);
        if (G::FSLog) G::log(srcFun, msg);

        if (!initializeGroup(groupCounter++)) return;


        if (o.method == "DMap" && !abortRequested()) runDMap();

        if (o.method == "PMax" && !abortRequested()) runPMax();

        if (abortRequested()) break;

        // SAVE
        if (o.writeFusedBackToSource) {
            QString fusedPath = save(srcFolderPath);
            // Save path in global for MW::generateFocusStack when finished
            if (!fusedPath.isEmpty()) {
                G::fsFusedPaths << fusedPath;
            }
        }
    }

    // cleanup even if aborted
    if (o.removeTemp) cleanup();

    incrementProgress();

    // STATS
    QString timeToRun = QString::number(t.elapsed() / 1000, 'f', 1) + " sec";
    QString progressSteps = " Progress step count = " + QString::number(progressCount);
    QString progressTot = " Progress step total = " + QString::number(progressTotal);
    msg = "Focus Stack completed in " + timeToRun;
    if (G::FSLog) G::log(srcFun, msg + progressSteps + progressTot);
    if (G::FSLog) G::log("");
    status(msg);

    bool success = !abortRequested();
    emit finished(success);
}

bool FS::runDMap()
{
    QString srcFun = "FS::runDMap_Parallel";
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
    aopt.fullResolution    = true;

    // -----------------------------
    // fusion options (dmap basic)
    // -----------------------------
    FSFusion::Options fopt;
    fopt.method          = "DMap";
    fopt.useOpenCL       = o.enableOpenCL;
    fopt.consistency     = 2;
    fopt.depthFolderPath = depthFolderPath;

    FSLoader::Image prevImage, currImage;
    Result prevGlobal;
    std::vector<Result> globals;
    globals.reserve(grpSlices);

    FSAlign::Align align;
    FSFusionDMap fuse;

    // Create a struct to hold the warped result
    struct WarpResult {
        int slice;
        cv::Mat gray;
        cv::Mat color;
    };

    QList<QFuture<WarpResult>> futures;

    // Identify the Master ROI on the main thread to anchor the stack
    cv::Rect masterROI;

    for (int slice = 0; slice < grpSlices; ++slice)
    {
        if (abortRequested()) return false;

        status(QString("Aligning Slice %1 of %2").arg(slice + 1).arg(grpSlices));

        QString path = inputPaths.at(slice);

        auto decoder = [this](const QString& p) -> cv::Mat {
            cv::Mat result;
            // This triggers MW::matFromQImage on the GUI thread and WAITS
            emit requestImage(p, result);
            return result.clone(); // Ensure we own the data
        };

        // 1. Sequential Part: Load and ECC Align
        currImage = FSLoader::load(path.toStdString(), decoder);
        // currImage = FSLoader::load(inputPaths.at(slice).toStdString());

        // Deep copy matrices to ensure the main loop doesn't overwrite them
        // while the background thread is still reading them
        cv::Mat colorForThread = currImage.color.clone();
        cv::Mat grayForThread = currImage.gray.clone();

        Result currGlobal;

        if (slice == 0) {
            // Anchor everything to Slice 0
            masterROI = currImage.validArea;
            currGlobal = FSAlign::makeIdentity(masterROI);

            // Initialize engine: tell it images are already cropped to ROI and zero-indexed
            fuse.alignSize      = masterROI.size();
            fuse.validAreaAlign = cv::Rect(0, 0, masterROI.width, masterROI.height);
            fuse.origSize       = masterROI.size();
            fuse.outDepth       = currImage.color.depth();

            // incrementProgress();
        }
        else {
            // Align subsequent slices to the reference
            if (!align.alignSlice(slice, prevImage, currImage, prevGlobal,
                                  currGlobal, nullptr, nullptr,
                                  aopt, &abort, statusCb, progressCb))
                return false;
            incrementProgress();
        }
        globals.push_back(currGlobal);

        // Parallel Part: Warping and I/O
        futures.append(QtConcurrent::run(
            QThreadPool::globalInstance(),
            [slice, colorForThread, grayForThread, currGlobal, masterROI,
            paths = alignedColorPaths[slice]]()
            {
                WarpResult res;
                res.slice = slice;

                // Perform the Warp
                cv::Mat warpedColor, warpedGray;
                FSAlign::applyTransform(colorForThread, currGlobal.transform, warpedColor);
                FSAlign::applyTransform(grayForThread,  currGlobal.transform, warpedGray);

                // Sync the focus score and the disk image
                // to the same 0,0-based coordinate system
                res.color = warpedColor(masterROI).clone();
                res.gray  = warpedGray(masterROI).clone();

                if (res.gray.type() != CV_8U) res.gray.convertTo(res.gray, CV_8U);

                // Save the exact same sharp ROI to disk for the final fusion read
                cv::imwrite(paths.toStdString(), res.color);

                return res;
            }));

        // Update tracking for the next slice
        prevImage = currImage;
        prevGlobal = currGlobal;
    }

    // // Diagnostic check for focus map range
    // double minV, maxV;
    // cv::minMaxLoc(depthIndex16Mat, &minV, &maxV);

    // Sequential Part: Feed results back into the fusion engine
    for (auto &f : futures) {
        if (G::FSLog) G::log(srcFun, "Build the depth map");
        WarpResult res = f.result();
        fuse.streamSlice(res.slice, res.gray, res.color, fopt,
                         &abort, statusCb, progressCb);
        incrementProgress();
        status("Building depth map");
    }

    if (abortRequested()) return false;

    // Finalization
    status("Finalizing DMap fusion...");

    // Link the temp file paths so streamFinish can read the warped slices
    fuse.alignedColorPaths = alignedColorPaths;
    fuse.alignedGrayPaths = alignedGrayPaths;

    bool success = fuse.streamFinish(
        fusedColorMat,
        fopt,
        depthIndex16Mat,
        inputPaths,
        globals,
        &abort,
        statusCb,
        progressCb
        );

    // Diagnostic check for focus map range
    if (success && !depthIndex16Mat.empty()) {
        double minV, maxV;
        cv::minMaxLoc(depthIndex16Mat, &minV, &maxV);
        // qDebug() << "DMap Depth Range:" << minV << "to" << maxV;
    }

    incrementProgress();

    return success;
}

bool FS::runPMax()
{
    QString srcFun = "FS::runPMax_Parallel";
    auto progressCb = [this]{ incrementProgress(); };
    auto statusCb   = [this](const QString &m){ status(m); };

    // -----------------------------
    // Align options
    // -----------------------------
    FSAlign::Options aopt;
    aopt.matchContrast     = true;
    aopt.matchWhiteBalance = true;
    aopt.lowRes            = 256;
    aopt.maxRes            = 2048;
    aopt.fullResolution    = true;

    // -----------------------------
    // Fusion options
    // -----------------------------
    FSFusion::Options fopt;
    fopt.method          = "PMax";
    fopt.useOpenCL       = o.enableOpenCL;
    fopt.consistency     = 2;
    fopt.depthFolderPath = depthFolderPath;

    FSFusionPMax::Options popt;
    popt.energyMode             = "Max";
    popt.weightedPower          = 7.0f;
    popt.weightedSigma0         = 2.0f;
    popt.weightedIncludeLowpass = true;
    popt.weightedEpsEnergy      = 1e-8f;
    popt.weightedEpsWeight      = 1e-8f;

    FSLoader::Image prevImage, currImage;
    Result prevGlobal;
    std::vector<Result> globals;
    globals.reserve(grpSlices);

    FSAlign::Align align;
    FSFusionPMax   fuse;

    struct WarpResult {
        int slice;
        cv::Mat gray;
        cv::Mat color;
    };

    QList<QFuture<WarpResult>> futures;
    cv::Rect masterROI;

    for (int slice = 0; slice < grpSlices; ++slice)
    {
        if (abortRequested()) return false;

        status(QString("Aligning Slice %1 of %2").arg(slice + 1).arg(grpSlices));

        currImage = FSLoader::load(inputPaths.at(slice).toStdString());

        cv::Mat colorForThread = currImage.color.clone();
        cv::Mat grayForThread = currImage.gray.clone();

        Result currGlobal;

        if (slice == 0) {
            masterROI = currImage.validArea;
            currGlobal = FSAlign::makeIdentity(masterROI);

            fuse.alignSize      = masterROI.size();
            fuse.validAreaAlign = cv::Rect(0, 0, masterROI.width, masterROI.height);
            fuse.origSize       = masterROI.size();
            fuse.outDepth       = currImage.color.depth();
        }
        else {
            if (!align.alignSlice(slice, prevImage, currImage, prevGlobal,
                                  currGlobal, nullptr, nullptr,
                                  aopt, &abort, statusCb, progressCb))
                return false;
            incrementProgress();
        }
        globals.push_back(currGlobal);

        futures.append(QtConcurrent::run(QThreadPool::globalInstance(),
            [slice, colorForThread, grayForThread, currGlobal, masterROI,
            paths = alignedColorPaths[slice]]()
            {
             WarpResult res;
             res.slice = slice;

             cv::Mat warpedColor, warpedGray;
             FSAlign::applyTransform(colorForThread, currGlobal.transform, warpedColor);
             FSAlign::applyTransform(grayForThread,  currGlobal.transform, warpedGray);

             res.color = warpedColor(masterROI).clone();
             res.gray  = warpedGray(masterROI).clone();

             if (res.gray.type() != CV_8U) res.gray.convertTo(res.gray, CV_8U);

             cv::imwrite(paths.toStdString(), res.color);

             return res;
            }));

        prevImage = currImage;
        prevGlobal = currGlobal;
    }

    // Process results as they finish
    for (auto &f : futures) {
        WarpResult res = f.result();
        fuse.streamSlice(res.slice, res.gray, res.color, fopt,
                         &abort, statusCb, progressCb);
        incrementProgress();
    }

    if (abortRequested()) return false;

    status("Finalizing PMax fusion...");

    bool success = fuse.streamFinish(
        fusedColorMat,
        fopt,
        depthIndex16Mat,
        inputPaths,
        globals,
        &abort,
        statusCb,
        progressCb
        );

    incrementProgress();

    return success;
}

QString FS::save(QString fuseFolderPath)
{
    QString srcFun = "FS::save";

    // Make file name for fused image based on last input image in stack
    QFileInfo lastFi(inputPaths.last());
    QString base = lastFi.completeBaseName() + "_FocusStack";
    if (o.isLocal) base += "_" + o.method; // + o.methodInfo;
    QString ext  = lastFi.suffix();
    // OpenCV cannot write RAW files. Force it to TIF if it's not a standard format.
    QStringList cvWritable = {"tif", "tiff", "png", "jpg", "jpeg", "bmp"};
    if (!cvWritable.contains(ext)) {
        ext = "tif"; // Default to TIF to preserve quality/depth
    }
    QString fusedPath = fuseFolderPath + "/" + base + "." + ext;
    // if exists add incrementing suffix
    Utilities::uniqueFilePath(fusedPath, "_");
    QFileInfo fusedFi(fusedPath);
    base = fusedFi.completeBaseName();
    QString xmpPath   = fuseFolderPath + "/" + base + "." + "xmp";
    QString msg = "Folder: " + fuseFolderPath + "  Last input image: " + lastFi.completeBaseName();

    if (G::FSLog) G::log(srcFun, fuseFolderPath);

    // // Save path in global for MW::generateFocusStack when finished
    // G::fsFusedPaths << fusedPath;

    // Write fused result
    try {
        if (G::FSLog) G::log(srcFun, "Write to " + fusedPath);
        cv::imwrite(fusedPath.toStdString(), fusedColorMat);
    }
    catch (const cv::Exception& e) {
        qWarning() << "OpenCV failed to save image:" << e.what();
        return ""; // Abort save
    }

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
    if (!f.open(QIODevice::ReadWrite)) return "";

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

    return fusedPath;
}

bool FS::cleanup()
{
    /* Folder structure:
    inputFolder         "FocusStack" if remote, srcFolder if local
        grpFolder       Working folder for each group
            align       Aligned color pngs
            depth       Depth map and diagnostics
            fusion      Fused results when testing

    */
    QString srcFun = "FS::cleanup";
    if (G::FSLog) G::log(srcFun);

    QString msg;

    // remove the group working folders (align, depth, fusion)
    for (QString subFolder : grpFolderPaths) {
        // qDebug() << "Remove work folder: " << subFolder;
        msg = "Remove work folder: " + subFolder;
        if (G::isFileLogger) Utilities::log(srcFun, msg);

        QDir(subFolder).removeRecursively();
    }

    if (o.isLocal) return true;

    /*
    When remote (ie Lightroom), the inputFolder(s) ("FocusStack") may contain files
    other than the input images, so we need to remove the input images, and then if
    the inputFolder is empty it can be removed too.
    */

    // aborted();

    // remove the temp focus stack source tiffs
    QStringList inputFolderPaths;
    // all input files
    for (QString s : inputPaths) {
        QString srcFolderPath = QFileInfo(s).absolutePath();
        if (!inputFolderPaths.contains(srcFolderPath)) inputFolderPaths << srcFolderPath;
        // qDebug() << "Remove input file: " << s;
        msg = "Remove input file: " + s;
        if (G::isFileLogger) Utilities::log(srcFun, msg);

        QFile::remove(s);
    }

    // remove input folder if empty
    for (QString d : inputFolderPaths) {
        // qDebug() << "input folder isEmpty: " << QDir(d).isEmpty();
        msg = "input folder: " + d + "  isEmpty: " + QVariant(QDir(d).isEmpty()).toString();
        if (G::isFileLogger) Utilities::log(srcFun, msg);
        if (QDir(d).isEmpty()) {
            // qDebug() << "Remove input folder: " << d;
            msg = "Remove input folder: " + d;
            if (G::isFileLogger) Utilities::log(srcFun, msg);

            QDir(d).removeRecursively();
        }
    }

    return true;
}

void FS::diagnostics()
{
    qDebug() << "Diagnostics:"
        << "\n"
        << "dstFolderPath           =" << srcFolderPath << "\n"

        << "\n"
        << "o.method                =" << o.method << "\n"
        << "o.methodInfo            =" << o.methodInfo << "\n"
        << "\n"
        << "o.saveDiagnostics       =" << o.saveDiagnostics << "\n"
        << "o.enableOpenCL          =" << o.enableOpenCL << "\n"

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
