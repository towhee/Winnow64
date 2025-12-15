#include "FSArtifact.h"
#include "FSUtilities.h"
#include "Main/global.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

namespace FSArtifact
{

// ---------------------------------------------------------------------
// Utilities (internal)
// ---------------------------------------------------------------------

static inline int oddK(int k)
{
    return (k % 2 == 1) ? k : (k + 1);
}

static inline bool abortRequested(std::atomic_bool *abortFlag)
{
    return abortFlag && abortFlag->load(std::memory_order_relaxed);
}

static cv::Mat toGray32(const cv::Mat &img)
{
    QString srcFun = "FSArtifact::toGray32";
    if (G::FSLog) G::log(srcFun);

    if (img.empty())
        return {};

    cv::Mat g;
    if (img.channels() == 1)
        g = img;
    else
        cv::cvtColor(img, g, cv::COLOR_BGR2GRAY);

    cv::Mat f;
    if (g.type() == CV_32F)
        f = g;
    else
        g.convertTo(f, CV_32F, 1.0 / 255.0);

    return f;
}

static void normalize01InPlace(cv::Mat &m)
{
    QString srcFun = "FSArtifact::toGray32";
    QString msg = "";
    if (m.empty()) msg = "Empty Mat";
    if (G::FSLog) G::log(srcFun, msg);

    if (m.empty())
        return;

    cv::threshold(m, m, 0.0, 0.0, cv::THRESH_TOZERO);

    double mn = 0, mx = 0;
    cv::minMaxLoc(m, &mn, &mx);

    if (mx > 1e-12)
        m.convertTo(m, CV_32F, 1.0 / mx);
    else
        m.setTo(0);
}

// ---------------------------------------------------------------------
// Stage 1: unsupported detail via Laplacian support
// ---------------------------------------------------------------------

static cv::Mat detectUnsupportedLaplacian(
    const cv::Mat &fusedGray32,
    const std::vector<cv::Mat> &slicesGray32,
    const Options &opt,
    std::atomic_bool *abortFlag,
    ProgressCallback progressCb,
    StatusCallback statusCb)
{
    QString srcFun = "FSArtifact::detectUnsupportedLaplacian";
    if (G::FSLog) G::log(srcFun);

    if (statusCb)
        statusCb("Detecting unsupported detail (Laplacian)");

    const int k = oddK(opt.laplacianKSize);

    cv::Mat fusedLap;
    cv::Laplacian(fusedGray32, fusedLap, CV_32F, k);
    fusedLap = cv::abs(fusedLap);

    cv::Mat maxSliceLap =
        cv::Mat::zeros(fusedLap.size(), CV_32F);

    if (G::FSLog) G::log(srcFun, "Processing slicesGray32");
    const int total = static_cast<int>(slicesGray32.size());
    for (int i = 0; i < total; ++i)
    {
        if (abortRequested(abortFlag)) return {};
        if (G::FSLog) G::log(srcFun, "Slice " + QString::number(i));

        cv::Mat lap;
        cv::Laplacian(slicesGray32[i], lap, CV_32F, k);
        lap = cv::abs(lap);

        cv::max(maxSliceLap, lap, maxSliceLap);

        if (progressCb) progressCb();
    }

    cv::Mat unsupported =
        fusedLap - opt.supportRatio * maxSliceLap;

    cv::threshold(unsupported, unsupported, 0.0, 0.0,
                  cv::THRESH_TOZERO);

    unsupported.setTo(0.0f, fusedLap < opt.detailThreshold);

    normalize01InPlace(unsupported);
    return unsupported;
}

// ---------------------------------------------------------------------
// Stage 2: depth-edge boost
// ---------------------------------------------------------------------

static void boostDepthEdges(cv::Mat &confidence01,
                            const cv::Mat &depthMap32,
                            const Options &opt)
{
    QString srcFun = "FSArtifact::boostDepthEdges";
    if (G::FSLog) G::log(srcFun);

    cv::Mat d = depthMap32;
    if (d.channels() != 1)
        cv::cvtColor(d, d, cv::COLOR_BGR2GRAY);
    if (d.type() != CV_32F)
        d.convertTo(d, CV_32F);

    cv::Mat lap;
    cv::Laplacian(d, lap, CV_32F, 3);
    lap = cv::abs(lap);

    cv::Mat edgeMask = (lap > opt.depthEdgeThresh);
    confidence01.setTo(
        confidence01 + opt.depthBoost, edgeMask);
}

// ---------------------------------------------------------------------
// Mask + post
// ---------------------------------------------------------------------

static void applyIncludeMask(cv::Mat &confidence01,
                             const cv::Mat *includeMask8)
{
    QString srcFun = "FSArtifact::applyIncludeMask";
    if (G::FSLog) G::log(srcFun);

    if (!includeMask8 || includeMask8->empty())
        return;

    confidence01.setTo(0.0f, (*includeMask8) == 0);
}

static void postBlur(cv::Mat &confidence01, const Options &opt)
{
    if (opt.blurRadius <= 0)
        return;

    const int k = 2 * opt.blurRadius + 1;
    cv::GaussianBlur(confidence01, confidence01,
                     cv::Size(k, k), 0);
}

// ---------------------------------------------------------------------
// Public entry points (mirrors FSDepth)
// ---------------------------------------------------------------------

bool detect(const cv::Mat &fusedGray32_in,
         const std::vector<cv::Mat> &slicesGray32_in,
         const cv::Mat *depthMap32,
         const cv::Mat *includeMask8,
         cv::Mat &artifactConfidence01,
         const Options &opt,
         std::atomic_bool *abortFlag,
         ProgressCallback progressCb,
         StatusCallback statusCb)
{
    QString srcFun = "FSArtifact::run";
    if (G::FSLog) G::log(srcFun);

    const QString method = opt.method.trimmed();

    if (method.startsWith("Simple", Qt::CaseInsensitive))
        return runSimple(fusedGray32_in, slicesGray32_in,
                         depthMap32, includeMask8,
                         artifactConfidence01,
                         opt, abortFlag,
                         progressCb, statusCb);

    return runMultiScale(fusedGray32_in, slicesGray32_in,
                         depthMap32, includeMask8,
                         artifactConfidence01,
                         opt, abortFlag,
                         progressCb, statusCb);
}

bool runSimple(const cv::Mat &fusedGray32_in,
               const std::vector<cv::Mat> &slicesGray32_in,
               const cv::Mat *depthMap32,
               const cv::Mat *includeMask8,
               cv::Mat &artifactConfidence01,
               const Options &opt,
               std::atomic_bool *abortFlag,
               ProgressCallback progressCb,
               StatusCallback statusCb)
{
    QString srcFun = "FSArtifact::runSimple";
    if (G::FSLog) G::log(srcFun);

    cv::Mat fusedGray32 = toGray32(fusedGray32_in);
    std::vector<cv::Mat> slicesGray32;
    for (const auto &s : slicesGray32_in)
        slicesGray32.push_back(toGray32(s));

    artifactConfidence01 =
        detectUnsupportedLaplacian(fusedGray32,
                                   slicesGray32,
                                   opt,
                                   abortFlag,
                                   progressCb,
                                   statusCb);

    applyIncludeMask(artifactConfidence01, includeMask8);
    postBlur(artifactConfidence01, opt);

    return !abortRequested(abortFlag);
}

bool runMultiScale(const cv::Mat &fusedGray32_in,
                   const std::vector<cv::Mat> &slicesGray32_in,
                   const cv::Mat *depthMap32,
                   const cv::Mat *includeMask8,
                   cv::Mat &artifactConfidence01,
                   const Options &opt,
                   std::atomic_bool *abortFlag,
                   ProgressCallback progressCb,
                   StatusCallback statusCb)
{
    QString srcFun = "ArtifactsDetector::runMultiScale";
    if (G::FSLog) G::log(srcFun);

    cv::Mat fusedGray32 = toGray32(fusedGray32_in);
    std::vector<cv::Mat> slicesGray32;
    for (const auto &s : slicesGray32_in) {
        slicesGray32.push_back(toGray32(s));
    }

    artifactConfidence01 =
        detectUnsupportedLaplacian(fusedGray32,
                                   slicesGray32,
                                   opt,
                                   abortFlag,
                                   progressCb,
                                   statusCb);

    if (abortRequested(abortFlag)) return false;

    if (opt.enableDepthEdges && depthMap32)
        boostDepthEdges(artifactConfidence01, *depthMap32, opt);

    applyIncludeMask(artifactConfidence01, includeMask8);
    postBlur(artifactConfidence01, opt);

    return !abortRequested(abortFlag);
}

bool FSArtifact::repair(cv::Mat &fusedInOut,
                        const cv::Mat &artifact01,
                        const std::vector<cv::Mat> &alignedSlices,
                        int backgroundSourceIndex,
                        float threshold,
                        std::atomic_bool *abortFlag)
{
    CV_Assert(!fusedInOut.empty());
    CV_Assert(artifact01.type() == CV_32F);
    CV_Assert(fusedInOut.size() == artifact01.size());
    CV_Assert(backgroundSourceIndex >= 0 &&
              backgroundSourceIndex < (int)alignedSlices.size());

    const cv::Mat &bg = alignedSlices[backgroundSourceIndex];

    CV_Assert(bg.size() == fusedInOut.size());
    CV_Assert(bg.type() == fusedInOut.type());

    const int rows = fusedInOut.rows;
    const int cols = fusedInOut.cols;

    const size_t pixBytes = fusedInOut.elemSize(); // bytes per pixel (all channels)

    int changed = 0;

    for (int y = 0; y < rows; ++y)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed))
            return false;

        const float *aptr = artifact01.ptr<float>(y);

        uchar *fptr = fusedInOut.ptr<uchar>(y);
        const uchar *bptr = bg.ptr<uchar>(y);

        for (int x = 0; x < cols; ++x)
        {
            if (aptr[x] >= threshold)
            {
                std::memcpy(fptr + x * pixBytes,
                            bptr + x * pixBytes,
                            pixBytes);
                ++changed;
            }
        }
    }

    qDebug() << "FSArtifact::repair changed pixels =" << changed
             << "threshold =" << threshold;

    return true;
}

} // namespace FSArtifact
