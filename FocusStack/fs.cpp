#include "FS.h"
#include "Main/global.h"
#include "FocusStack/fsalign.h"
#include "FocusStack/fsloader.h"
#include "FocusStack/fsfusion.h"
#include "FocusStack/fsfusionmerge.h"
#include "FocusStack/fsfusionreassign.h"
#include "FocusStack/fsloader.h"

#include <opencv2/imgcodecs.hpp>

#include <QDir>
#include <QFileInfo>
#include <QDebug>

FS::FS(QObject *parent)
    : QObject(parent)
{
}

void FS::setInput(const QStringList &paths)
{
    m_inputPaths = paths;
}

void FS::setProjectRoot(const QString &rootPath)
{
    m_projectRoot = rootPath;

    m_alignFolder = m_projectRoot + "/align";
    m_focusFolder = m_projectRoot + "/focus";
    m_depthFolder = m_projectRoot + "/depth";
    m_fusionFolder = m_projectRoot + "/fusion";
}

void FS::setOptions(const Options &opt)
{
    m_options = opt;
}

void FS::abort()
{
    m_abortRequested = true;
}

void FS::emitStageStatus(const QString &stage, const QString &msg, bool isError)
{
    emit updateStatus(isError, msg, stage);
}

bool FS::prepareFolders()
{
    if (m_projectRoot.isEmpty())
    {
        emitStageStatus("FS", "Project root not set", true);
        return false;
    }

    QDir dir;
    if (!dir.mkpath(m_projectRoot)) {
        emitStageStatus("FS", "Cannot create project root", true);
        return false;
    }

    dir.mkpath(m_alignFolder);
    dir.mkpath(m_focusFolder);
    dir.mkpath(m_depthFolder);
    dir.mkpath(m_fusionFolder);

    return true;
}

//
// ─────────────────────────────────────────────
//   detectSkips() — checks for existing outputs
// ─────────────────────────────────────────────
//
bool FS::detectSkips()
{
    // Reset
    m_skipAlign     = false;
    m_skipFocusMaps = false;
    m_skipDepthMap  = false;
    m_skipFusion    = false;

    // Always rebuild path lists (required for runAlign output file names)
    m_alignedColor.clear();
    m_alignedGray.clear();

    bool allAlignedExist = true;

    for (const QString &src : m_inputPaths)
    {
        QFileInfo fi(src);
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix();

        QString col = m_alignFolder + "/aligned_" + base + "." + ext;
        QString gry = m_alignFolder + "/gray_" + base + "." + ext;

        m_alignedColor.push_back(col);
        m_alignedGray.push_back(gry);

        if (!QFileInfo::exists(col) || !QFileInfo::exists(gry))
            allAlignedExist = false;
    }

    // Determine skip condition
    if (!m_options.overwriteExisting && allAlignedExist)
        m_skipAlign = true;

    // Placeholder skip logic for other stages
    if (!m_options.overwriteExisting)
    {
        if (QFileInfo::exists(m_focusFolder + "/focus_done.txt"))
            m_skipFocusMaps = true;
        if (QFileInfo::exists(m_depthFolder + "/depth_done.txt"))
            m_skipDepthMap = true;
        if (QFileInfo::exists(m_fusionFolder + "/fusion_done.txt"))
            m_skipFusion = true;
    }

    return true;
}

void FS::incrementProgress()
{
    // int current = ++m_progress;
    emit progress(++m_progress, m_total);
    qDebug() << "" << m_progress;
}

bool FS::run()
{
    m_abortRequested = false;

    int n = m_inputPaths.count();
    m_progress = -1;
    int alignTotal = (n - 1) * 2 + 1;
    m_total = alignTotal;

    if (m_inputPaths.isEmpty())
    {
        emitStageStatus("FS", "No input images", true);
        return false;
    }

    // 1. Create stage folders
    if (!prepareFolders())
        return false;

    // 2. Decide which stages to skip
    if (!detectSkips())
        return false;

    // 3. Run ALIGN
    if (m_options.enableAlign && !m_skipAlign)
    {
        if (!runAlign())
            return false;
    }
    else {
        emitStageStatus("Align", "Skipped");
    }

    // 4. RUN FOCUS MAPS
    if (m_options.enableFocusMaps && !m_skipFocusMaps)
    {
        if (!runFocusMaps())
            return false;
    }
    else {
        emitStageStatus("FocusMaps", "Skipped");
    }

    // 5. RUN DEPTH MAP
    if (m_options.enableDepthMap && !m_skipDepthMap)
    {
        if (!runDepthMap())
            return false;
    }
    else {
        emitStageStatus("DepthMap", "Skipped");
    }

    // 6. RUN FUSION
    if (m_options.enableFusion && !m_skipFusion)
    {
        if (!runFusion())
            return false;
    }
    else {
        emitStageStatus("Fusion", "Skipped");
    }

    emitStageStatus("FS", "Pipeline complete.");
    return true;
}

bool FS::runAlign()
{
    QString srcFun = "FS::runAlign";
    QString msg;
    const QString stage = "Align";
    G::log(srcFun, "Starting alignment…");

    if (m_inputPaths.isEmpty())
    {
        emitStageStatus(stage, "No input images", true);
        return false;
    }

    const int N = m_inputPaths.size();
    if (N < 2)
    {
        emitStageStatus(stage, "Need at least 2 images to align", true);
        return false;
    }

    // -----------------------------------------------------------
    // Load images using Petteri-compatible padding + validArea
    // -----------------------------------------------------------
    std::vector<FSLoader::Image> imgs(N);

    for (int i = 0; i < N; ++i)
    {
        if (m_abortRequested)
            return false;

        try {
            imgs[i] = FSLoader::load(m_inputPaths[i].toStdString());
        }
        catch (const std::exception &e)
        {
            emitStageStatus(stage,
                            QString("Error loading %1: %2").arg(m_inputPaths[i]).arg(e.what()),
                            true);
            return false;
        }

        G::log(srcFun, "Load image " + QString::number(i));
    }

    // -----------------------------------------------------------
    // Prepare FSAlign global chain (Petteri-style)
    // -----------------------------------------------------------
    using namespace FSAlign;

    std::vector<Result> globals(N);
    globals[0] = makeIdentity(imgs[0].validArea);

    FSAlign::Options opt;
    opt.matchContrast     = true;
    opt.matchWhiteBalance = true;
    opt.lowRes            = 256;
    opt.maxRes            = 2048;
    opt.fullResolution    = false;

    // -----------------------------------------------------------
    // Reference frame: write unmodified aligned color + gray
    // -----------------------------------------------------------
    {
        cv::imwrite(m_alignedColor[0].toStdString(), imgs[0].color);
        cv::imwrite(m_alignedGray[0].toStdString(),  imgs[0].gray);
    }

    G::log(srcFun, "Write unmodified aligned color + gray");
    incrementProgress();

    // -----------------------------------------------------------
    // Alignment loop: image i aligned to image (i - 1)
    // -----------------------------------------------------------
    for (int i = 1; i < N; ++i)
    {
        if (m_abortRequested)
            return false;

        msg = QString("Aligning %1 → %2").arg(m_inputPaths[i]).arg(m_inputPaths[i - 1]);
        G::log(srcFun, msg);
        incrementProgress();

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
            emitStageStatus(stage,
                            QString("Alignment failed for %1: %2")
                                .arg(m_inputPaths[i]).arg(e.what()),
                            true);
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
        cv::Mat alignedColor, alignedGray;
        G::log(srcFun, "cv::Mat alignedColor, alignedGray");
        applyTransform(imgs[i].color, globals[i].transform, alignedColor);
        G::log(srcFun, "applyTransform alignedColor");
        applyTransform(imgs[i].gray,  globals[i].transform, alignedGray);
        G::log(srcFun, "applyTransform alignedGray");
        incrementProgress();

        // Write outputs
        cv::imwrite(m_alignedColor[i].toStdString(), alignedColor);
        cv::imwrite(m_alignedGray[i].toStdString(),  alignedGray);

        msg = "Write outputs";
        G::log(srcFun, msg);
    }

    incrementProgress();
    emitStageStatus(stage, "Alignment complete.");
    return true;
}

bool FS::runFocusMaps()
{
    emitStageStatus("FocusMaps", "Starting…");

    if (m_abortRequested)
        return false;

    // TODO: integrate FSFocusStage
    QFile f(m_focusFolder + "/focus_done.txt");
    f.open(QIODevice::WriteOnly);
    f.write("ok");
    f.close();

    emitStageStatus("FocusMaps", "Focus maps complete");
    return true;
}

bool FS::runDepthMap()
{
    emitStageStatus("DepthMap", "Starting…");

    if (m_abortRequested)
        return false;

    // TODO: integrate FSDepthStage
    QFile f(m_depthFolder + "/depth_done.txt");
    f.open(QIODevice::WriteOnly);
    f.write("ok");
    f.close();

    emitStageStatus("DepthMap", "Depth map complete");
    return true;
}

bool FS::runFusion()
{
    QString srcFun = "FS::runFusion";
    const QString stage = "Fusion";
    G::log(srcFun, "Starting fusion…");

    if (m_alignedGray.empty() || m_alignedColor.empty())
    {
        emitStageStatus(stage, "No aligned images available", true);
        return false;
    }

    const int N = static_cast<int>(m_alignedGray.size());
    if (N != static_cast<int>(m_alignedColor.size()))
    {
        emitStageStatus(stage, "Aligned gray/color count mismatch", true);
        return false;
    }

    G::log(srcFun, "Load aligned images from disk");
    // Load aligned images from disk
    std::vector<cv::Mat> grayImgs(N);
    std::vector<cv::Mat> colorImgs(N);

    for (int i = 0; i < N; ++i)
    {
        grayImgs[i]  = cv::imread(m_alignedGray[i].toStdString(),  cv::IMREAD_GRAYSCALE);
        colorImgs[i] = cv::imread(m_alignedColor[i].toStdString(), cv::IMREAD_UNCHANGED);

        if (grayImgs[i].empty() || colorImgs[i].empty())
        {
            emitStageStatus(stage,
                            QString("Failed to load aligned image %1").arg(i),
                            true);
            return false;
        }

        incrementProgress();
    }

    G::log(srcFun, "Call FSFusion");
    // Call FSFusion
    FSFusion::Options opt;
    opt.useOpenCL   = true;  // or true if you want GPU & have OCL configured
    opt.consistency = 2;      // Petteri full consistency

    cv::Mat fusedColor8;
    cv::Mat depthIndex16;

    G::log(srcFun, "Call FSFusion::fuseStack");
    if (!FSFusion::fuseStack(grayImgs,
                             colorImgs,
                             opt,
                             fusedColor8,
                             depthIndex16))
    {
        emitStageStatus(stage, "Fusion failed", true);
        qDebug() << srcFun << "FSFusion::fuseStack failed";
        return false;
    }

    G::log(srcFun, "Save depth_index.png");
    // Save depth index (optional – or move to runDepthMap)
    QString depthPath = m_depthFolder + "/depth_index.png";
    cv::imwrite(depthPath.toStdString(), depthIndex16);

    G::log(srcFun, "Save fused image");
    // Save fused color result (8-bit; you can later adapt to 16-bit output)
    QString fusedPath = m_fusionFolder + "/fused.tif";
    cv::imwrite(fusedPath.toStdString(), fusedColor8);

    emitStageStatus(stage, "Fusion complete.");
    return true;
}

/*
#include "fs.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>

#include "FocusStack/fsfusionmerge.h"
#include "FocusStack/fsfusionreassign.h"
#include "FocusStack/fsloader.h"

// using namespace FStack;

// -----------------------------------------------------------
// Constructor
// -----------------------------------------------------------
FS::FS(QObject *parent)
    : QObject(parent)
{
    m_abortRequested = false;
    m_progressCurrent = 0;
    m_progressTotal   = 0;
}

// -----------------------------------------------------------
// Public API
// -----------------------------------------------------------
void FS::setInput(const QStringList &paths)
{
    m_inputPaths = paths;
}

void FS::setProjectRoot(const QString &root)
{
    m_projectRoot = root;
}

void FS::setOptions(const Options &opt)
{
    m_options = opt;
}

// -----------------------------------------------------------
// Run entire pipeline
// -----------------------------------------------------------
bool FS::run()
{
    if (!prepareFolders())
        return false;

    detectSkips();
    m_abortRequested = false;
    m_progressCurrent = 0;
    m_progressTotal = 200;     // temporary fixed progress scale

    if (m_options.enableAlign && !m_skipAlign)
        if (!runAlign()) return false;

    if (m_options.enableFusion && !m_skipFusion)
        if (!runFusion()) return false;

    return true;
}

// -----------------------------------------------------------
// Increment progress
// -----------------------------------------------------------
void FS::incrementProgress()
{
    m_progressCurrent++;
    emit progress(m_progressCurrent, m_progressTotal);
}

// -----------------------------------------------------------
// Folder preparation
// -----------------------------------------------------------
bool FS::prepareFolders()
{
    if (m_projectRoot.isEmpty()) return false;

    m_alignFolder = m_projectRoot + "/align";
    m_focusFolder = m_projectRoot + "/focus";
    m_depthFolder = m_projectRoot + "/depth";
    m_fusionFolder= m_projectRoot + "/fusion";

    QDir().mkpath(m_alignFolder);
    QDir().mkpath(m_depthFolder);
    QDir().mkpath(m_fusionFolder);

    return true;
}

// -----------------------------------------------------------
// Detect skips (simplified)
// -----------------------------------------------------------
bool FS::detectSkips()
{
    m_skipAlign = false;

    m_alignedColor.clear();
    m_alignedGray.clear();

    for (const QString &p : m_inputPaths)
    {
        QFileInfo fi(p);
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix();

        QString col = m_alignFolder + "/aligned_" + base + "." + ext;
        QString gry = m_alignFolder + "/gray_" + base + "." + ext;

        m_alignedColor.push_back(col);
        m_alignedGray.push_back(gry);
    }

    if (!m_options.overwriteExisting)
    {
        bool allExist = true;
        for (int i = 0; i < m_inputPaths.size(); i++)
        {
            if (!QFileInfo::exists(m_alignedColor[i]) ||
                !QFileInfo::exists(m_alignedGray[i]))
            {
                allExist = false;
                break;
            }
        }
        m_skipAlign = allExist;
    }

    return true;
}

// ===========================================================================
//                              ALIGNMENT
// ===========================================================================
bool FS::runAlign()
{
    QString srcFun = "FS::runAlign";
    const QString stage = "Align";
    G::log(srcFun, "Starting alignment…");

    const int N = m_inputPaths.size();
    if (N < 1) return false;

    // -----------------------------
    // Load all images (Petteri loader)
    // -----------------------------
    std::vector<FSLoader::Image> imgs(N);

    for (int i = 0; i < N; ++i)
    {
        try {
            imgs[i] = FSLoader::load(m_inputPaths[i].toStdString());
        }
        catch (const std::exception &e)
        {
            emitStageStatus(stage,
                            QString("Error loading %1: %2")
                                .arg(m_inputPaths[i]).arg(e.what()),
                            true);
            return false;
        }
        incrementProgress();
    }

    // -----------------------------------------------------------
    // Prepare FSAlign chain
    // -----------------------------------------------------------
    using namespace FSAlign;

    std::vector<Result> globals(N);
    globals[0] = makeIdentity(imgs[0].validArea);

    FSAlign::Options opt;
    opt.matchContrast     = true;
    opt.matchWhiteBalance = true;
    opt.lowRes            = 256;
    opt.maxRes            = 2048;

    // -----------------------------------------------------------
    // Reference frame output
    // -----------------------------------------------------------
    cv::imwrite(m_alignedColor[0].toStdString(), imgs[0].color);
    cv::imwrite(m_alignedGray[0].toStdString(),  imgs[0].gray);
    incrementProgress();

    // -----------------------------------------------------------
    // Loop through images
    // -----------------------------------------------------------
    for (int i = 1; i < N; i++)
    {
        Result local;

        try {
            local = computeLocal(
                imgs[i - 1].gray, imgs[i - 1].color,
                imgs[i].gray,     imgs[i].color,
                imgs[i].validArea,
                opt
                );
        }
        catch (const std::exception &e)
        {
            emitStageStatus(stage,
                            QString("Alignment failed for %1: %2")
                                .arg(m_inputPaths[i]).arg(e.what()),
                            true);
            return false;
        }

        globals[i] = accumulate(globals[i-1], local, imgs[i].validArea);

        cv::Mat col, gry;
        applyTransform(imgs[i].color, globals[i].transform, col);
        applyTransform(imgs[i].gray,  globals[i].transform, gry);

        cv::imwrite(m_alignedColor[i].toStdString(), col);
        cv::imwrite(m_alignedGray[i].toStdString(),  gry);

        incrementProgress();
    }

    emitStageStatus(stage, "Alignment complete.");
    return true;
}

// ===========================================================================
//                                FUSION
// ===========================================================================
bool FS::runFusion()
{
    QString srcFun = "FS::runFusion";
    const QString stage = "Fusion";
    G::log(srcFun, "Starting Petteri PMax fusion…");

    if (m_alignedGray.empty() || m_alignedColor.empty())
    {
        emitStageStatus(stage, "No aligned images available", true);
        return false;
    }

    const int N = static_cast<int>(m_alignedGray.size());
    if (N != static_cast<int>(m_alignedColor.size()))
    {
        emitStageStatus(stage, "Aligned gray/color count mismatch", true);
        return false;
    }

    // Load aligned images from disk
    std::vector<cv::Mat> grayImgs(N);
    std::vector<cv::Mat> colorImgs(N);

    for (int i = 0; i < N; ++i)
    {
        grayImgs[i]  = cv::imread(m_alignedGray[i].toStdString(),  cv::IMREAD_GRAYSCALE);
        colorImgs[i] = cv::imread(m_alignedColor[i].toStdString(), cv::IMREAD_UNCHANGED);

        if (grayImgs[i].empty() || colorImgs[i].empty())
        {
            emitStageStatus(stage,
                            QString("Failed to load aligned image %1").arg(i),
                            true);
            return false;
        }

        incrementProgress();
    }

    // Call FSFusion
    FSFusion::Options opt;
    opt.useOpenCL   = false;  // or true if you want GPU & have OCL configured
    opt.consistency = 2;      // Petteri full consistency

    cv::Mat fusedColor8;
    cv::Mat depthIndex16;

    if (!FSFusion::fuseStack(grayImgs,
                             colorImgs,
                             opt,
                             fusedColor8,
                             depthIndex16))
    {
        emitStageStatus(stage, "Fusion failed", true);
        return false;
    }

    // Save depth index (optional – or move to runDepthMap)
    QString depthPath = m_depthFolder + "/depth_index.png";
    cv::imwrite(depthPath.toStdString(), depthIndex16);

    // Save fused color result (8-bit; you can later adapt to 16-bit output)
    QString fusedPath = m_fusionFolder + "/fused.tif";
    cv::imwrite(fusedPath.toStdString(), fusedColor8);

    emitStageStatus(stage, "Fusion complete.");
    return true;
}

//   Status helpers
void FS::emitStageStatus(const QString &stage, const QString &msg, bool isError)
{
    emit updateStatus(isError, msg, stage);
}
*/
