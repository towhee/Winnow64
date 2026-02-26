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
    grpFolderPaths.clear();
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
    grpFolderPaths << grpFolderPath;
    qDebug() << "group" << group << "grpFolderPaths =" << grpFolderPaths;

    prepareFolders();

    statusGroupPrefix = "Focus Stacking:  "
                        "Stack: " + QString::number(group+1) + " of " +
                        QString::number(groups.count()) + "  ";

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

    for (const QString &s : alignedColorPaths)  qDebug().noquote() << s;
    for (const QString &s : alignedGrayPaths)  qDebug().noquote() << s;

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
        int slices = g.count();
            progressTotal += (slices * 2 + 2);
    }
    QString msg = "progressTotal = " + QString::number(progressTotal);
    if (G::FSLog) G::log(srcFun, msg);

}

void FS::incrementProgress()
{
    QString srcFun = "FS::incrementProgress";
    emit progress(++progressCount, progressTotal);
    QString msg = QString::number(progressCount) + "/" + QString::number(progressTotal);
    if (G::FSLog) G::log(srcFun, msg);
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
    This is the entry point for the focus stacking pipeline.
*/
    QString srcFun = "FS::run";
    if (G::FSLog) G::log(srcFun, o.method);

    initializeProgress();
    incrementProgress();

    // iterate groups
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


        if (o.method == "DMap") {
            if (!runDMap()) {
                emit finished(false);
                return false;
            }
        }

        if (o.method == "PMax") {
            if (!runPMax()) {
                emit finished(false);
                return false;
            }
        }

       // SAVE
        if (o.writeFusedBackToSource) save(dstFolderPath);

        // STATS
        QString timeToRun = QString::number(t.elapsed() / 1000, 'f', 1) + " sec";
        QString progressSteps = " Progress step count = " + QString::number(progressCount);
        QString progressTot = " Progress step total = " + QString::number(progressTotal);
        if (G::FSLog) G::log(srcFun, "Focus Stack completed in " + timeToRun + progressSteps + progressTot);
        if (G::FSLog) G::log("");
        status("Focus Stack completed in " + timeToRun);

        // qApp->processEvents();  // complete any waiting log msgs
    }

    if (o.removeTempFiles) cleanup();

    return true;
}

bool FS::runDMap()
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
    fopt.useOpenCL       = o.enableOpenCL;
    fopt.consistency     = 2;
    fopt.depthFolderPath = depthFolderPath;

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
        QString s = " Slice: " + QString::number(slice+1) + " of " + QString::number(slices) + " ";
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

        // write aligned for reuse in fuse.streamFinish blending
        cv::Mat alignedImg = FSUtilities::alignToOrigSize(alignedColorSlice, currImage.origSize);
        cv::imwrite(alignedColorPaths[slice].toStdString(), alignedImg);
        // cv::Mat alignedGrayImg = FSUtilities::alignToOrigSize(alignedGraySlice, currImage.origSize);
        // cv::imwrite(alignedGrayPaths[slice].toStdString(), alignedGrayImg);

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

    msg = "Slice processing done. DMap finish: build maps, stream slices, blend, crop.";
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
        qWarning() << "WARNING:" << srcFun << "FSFusionDMap::streamFinish failed.";
        return false;
    }

    if (G::abortFocusStack) return false;
    incrementProgress();

    if (o.isLocal && o.saveDiagnostics) save(fusionFolderPath);

    return true;
}

bool FS::runPMax()
{
    QString srcFun = "FS::runPMax";
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
    // Fusion options
    // -----------------------------
    FSFusion::Options fopt;
    fopt.method          = o.method;     // "DMap" or "PMax"
    fopt.useOpenCL       = o.enableOpenCL;
    fopt.consistency     = 2;
    fopt.depthFolderPath = depthFolderPath;

    // -----------------------------
    // PMax Fusion options
    // -----------------------------
    FSFusionPMax::Options popt;
    popt.energyMode             = "Max";    // "Max" or "Weighted"
    popt.weightedPower          = 7.0f;     // 4.0f default
    popt.weightedSigma0         = 2.0f;     // 1.0f default
    popt.weightedIncludeLowpass = true;     // false looks bad
    popt.weightedEpsEnergy      = 1e-8f;
    popt.weightedEpsWeight      = 1e-8f;

    // optional: tag special runs
    o.methodInfo = "LL";  // LL = Low-frequency / Low-pass band

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
    if (false)
    {
        qDebug().noquote()
            << "\n weightedPower            =" << popt.weightedPower
            << "\n weightedSigma0           =" << popt.weightedSigma0
            << "\n weightedIncludeLowpass   =" << popt.weightedIncludeLowpass
            << "\n weightedEpsEnergy        =" << popt.weightedEpsEnergy
            << "\n weightedEpsWeight        =" << popt.weightedEpsWeight
            << "\n";
    }

    if (o.saveDiagnostics) save(fusionFolderPath);
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
    QString base = lastFi.completeBaseName() + "_FocusStack";
    if (o.isLocal) base = base + "_" + o.method; // + o.methodInfo;
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

bool FS::cleanup()
{
    QString srcFun = "FS::cleanup";
    if (G::FSLog) G::log(srcFun);

    QString msg;

    // remove the temp working folders (align, depth, fusion)
    for (QString subFolder : grpFolderPaths) {
        qDebug() << "Remove work folder: " << subFolder;
        msg = "Remove work folder: " + subFolder;
        if (G::isFileLogger) Utilities::log(srcFun, msg);

        QDir(subFolder).removeRecursively();
    }

    if (o.isLocal) return true;

    // remove the temp focus stack src tiffs
    QStringList srcFolders;
    for (QStringList gList : groups) {
        // files in group
        for (QString s : gList) {
            QString srcFolder = QFileInfo(s).absolutePath();
            if (!srcFolders.contains(srcFolder)) srcFolders << srcFolder;
            // qDebug() << "SrcFolder =" << srcFolder << "SrcFolders =" << srcFolders;
            qDebug() << "Remove src file: " << s;
            msg = "Remove src file: " + s;
            if (G::isFileLogger) Utilities::log(srcFun, msg);

            if (!o.isLocal) QFile::remove(s);
        }
    }

    // remove source folder(s) if empty
    for (QString d : srcFolders) {
        qDebug() << "src folder isEmpty: " << QDir(d).isEmpty();
        msg = "src folder: " + d + "  isEmpty: " + QVariant(QDir(d).isEmpty()).toString();
        if (G::isFileLogger) Utilities::log(srcFun, msg);
        if (QDir(d).isEmpty()) {
            qDebug() << "Remove src folder: " << d;
            msg = "Remove src folder: " + d;
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
        << "dstFolderPath           =" << dstFolderPath << "\n"
        << "fusedBase               =" << fusedBase << "\n"

        << "\n"
        << "o.method                =" << o.method << "\n"
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
