#include "fsfusion.h"
#include "Main/global.h"

#include "fsfusionwavelet.h"
#include "fsutilities.h"

#include <opencv2/imgproc.hpp>
#include <cassert>

#include <QString>

/*
Purpose
FSFusion implements the fusion stage of the focus stacking pipeline. It takes
a set of aligned grayscale slices and aligned color slices and produces a
single fused color image (CV_8UC3) using either a simple per-pixel slice pick
(“Simple”) or the full Petteri-style wavelet PMax path (default).

Key Inputs / Outputs
Inputs
- grayImgs      : vector of aligned grayscale images (1 channel)
- colorImgs     : vector of aligned color images (3 channel)
- depthIndex16  : CV_16U map with per-pixel slice indices (0..N-1)
- opt           : FSFusion options (method, OpenCL, consistency, etc.)
- abortFlag     : optional cancellation flag
- callbacks     : optional status/progress callbacks

Output
- outputColor8  : fused result as CV_8UC3 at original (non-padded) size

Two Fusion Modes
    1.	Simple fusion (method == “Simple”)
    •	Uses depthIndex16 directly: for each pixel, select the color from the
“winning” slice index stored in depthIndex16.
    •	Does NOT use wavelets.
    •	Normalizes input color slices to CV_8UC3 (converts from 16UC3 if needed).
    •	Produces outputColor8 by direct lookup per pixel.
    2.	PMax fusion (default path)
    •	Reproduces the current successful wavelet-based fusion workflow.
    •	IMPORTANT: depthIndex16 is validated (type/size) but is not used to drive
the wavelet merge decisions. The merge computes its own internal decisions.
    •	Uses the following high-level stages:
(a) Pad inputs to wavelet-friendly size (reflect border padding)
(b) Forward wavelet transform per grayscale slice
(c) Merge wavelet stacks with FSMerge::merge (consistency parameter)
(d) Inverse wavelet to get fused grayscale (still padded)
(e) Build a color reassignment map from padded gray+color slices
(f) Apply color map to fused grayscale to produce padded color result
(g) Crop padded result back to original size to produce outputColor8

Padding Behavior
Wavelet code requires dimensions compatible with its level structure. The
helper padForWavelet() expands images via cv::BORDER_REFLECT using the same
expanded-size logic as FSFusionWavelet::computeLevelsAndExpandedSize(). The
result is cropped back to original size at the end of PMax fusion.

Abort / Progress
    •	abortFlag is checked at multiple points in the PMax path (between stages and
per-slice wavelet forward).
    •	progressCallback is invoked as a simple “tick” at key milestones and per
slice during wavelet forward.
    •	statusCallback is used to provide coarse stage messages in the PMax path.

Role in the Pipeline:
FSFusion is the stage that converts aligned slice stacks into the final fused
image. In “Simple” mode it relies directly on FSDepth’s depthIndex16. In PMax
mode it uses wavelet merge + color reassignment to produce the fused color
image, while treating the FSDepth depthIndex16 primarily as a validated
artifact rather than a control signal.
*/

//--------------------------------------------------------------
// Helpers
//--------------------------------------------------------------
namespace
{

using ProgressCallback = FSFusion::ProgressCallback;

static cv::Mat cropPadToOrig(const cv::Mat &pad,
                             const cv::Rect &roiPadToAlign,
                             const cv::Rect &validAreaAlign)
{
    CV_Assert(!pad.empty());
    CV_Assert(roiPadToAlign.width  > 0);
    CV_Assert(validAreaAlign.width > 0);

    cv::Mat align = pad(roiPadToAlign);
    cv::Mat orig  = align(validAreaAlign);
    return orig.clone();
}

// ----- temp debug section -----

static cv::Mat makeEdgeMask8U(const cv::Mat &fusedGray8,
                              float sigma,
                              float threshFrac,
                              int dilatePx)
{
/*
    Used by DepthBiasedErosion and (LL) testing
*/
    QString srcFun = "FSFusion::makeEdgeMask8U";
    if (G::FSLog) G::log(srcFun);

    CV_Assert(fusedGray8.type() == CV_8U);

    cv::Mat gx, gy;
    cv::Sobel(fusedGray8, gx, CV_32F, 1, 0, 3);
    cv::Sobel(fusedGray8, gy, CV_32F, 0, 1, 3);

    cv::Mat mag;
    cv::magnitude(gx, gy, mag);

    if (sigma > 0.0f)
    {
        int k = int(sigma * 4.0f) + 1;
        if ((k & 1) == 0) ++k;
        if (k < 3) k = 3;
        cv::GaussianBlur(mag, mag, cv::Size(k, k), sigma, sigma,
                         cv::BORDER_REFLECT);
    }

    double minV = 0.0, maxV = 0.0;
    cv::minMaxLoc(mag, &minV, &maxV);
    float thr = float(maxV) * threshFrac;

    cv::Mat mask;
    cv::threshold(mag, mask, thr, 255.0, cv::THRESH_BINARY);
    mask.convertTo(mask, CV_8U);

    if (dilatePx > 0)
    {
        int k = 2 * dilatePx + 1;
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                               cv::Size(k, k));
        cv::dilate(mask, mask, se);
    }

    return mask;
}

static cv::Mat makeObjectMask8U_Otsu(const cv::Mat &gray8)
{
    CV_Assert(gray8.type() == CV_8U);

    cv::Mat blur;
    cv::GaussianBlur(gray8, blur, cv::Size(0,0), 2.0, 2.0, cv::BORDER_REFLECT);

    cv::Mat th;
    cv::threshold(blur, th, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Pick the smaller connected “class” as object (branches usually occupy less area).
    // If branches are darker, invert.
    int white = cv::countNonZero(th);
    int total = th.rows * th.cols;
    int black = total - white;

    cv::Mat obj = (white < black) ? th : (255 - th);

    // Clean specks
    cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5,5));
    cv::morphologyEx(obj, obj, cv::MORPH_OPEN, se);
    cv::morphologyEx(obj, obj, cv::MORPH_CLOSE, se);

    return obj; // 0/255
}

static cv::Mat dilateMaskPx(const cv::Mat &m8, int px)
{
    CV_Assert(m8.type() == CV_8U);
    if (px <= 0) return m8.clone();
    int k = 2 * px + 1;
    cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k,k));
    cv::Mat out;
    cv::dilate(m8, out, se);
    return out;
}

// halo detection section
struct HaloDetectParams
{
    // Edge detection (to define where "near edges" is)
    float edgeSigma      = 1.0f;   // blur mag before threshold
    float edgeThreshFrac = 0.06f;  // fraction of max grad
    int   edgeDilatePx   = 2;      // widen the edge band

    // Distance gating (how far from edges we still allow halos)
    int   maxEdgeDistPx  = 24;     // 12..40 typical (orig pixels)

    // Multi-scale DoG (bright LL halos)
    // Each pair is (sigmaSmall, sigmaLarge) in orig pixels
    std::vector<std::pair<float,float>> dogScales = {
        {3.0f,  6.0f},
        {6.0f, 12.0f},
        {12.0f, 24.0f}
    };

    // Thresholding & cleanup
    float dogThresh      = 0.020f; // in 0..1 intensity units (after normalization)
    int   minAreaPx      = 64;     // remove tiny speckles
    int   closePx        = 2;      // close small gaps along edges (0 disables)

    // Optional "one-sided" gating: require halo to be on background side of edge
    // Keep this OFF for now; it depends on reliable FG/BG.
    bool  useOneSidedGate = false;
};

struct HaloDetectResult
{
    cv::Mat edgeMask8;     // CV_8U 0/255 (orig)
    cv::Mat dogMax32;      // CV_32F (orig) max positive DoG
    cv::Mat dist32;        // CV_32F (orig) distance to edge (pixels)
    cv::Mat haloMask8;     // CV_8U 0/255 (orig)
    double  scoreMean = 0; // mean(dogMax | haloMask)
    int     nHalo = 0;
};

static cv::Mat gaussianBlur32(const cv::Mat &src32, float sigma)
{
    CV_Assert(src32.type() == CV_32F);
    if (sigma <= 0.0f) return src32.clone();

    int k = int(sigma * 4.0f) + 1;
    if ((k & 1) == 0) ++k;
    if (k < 3) k = 3;

    cv::Mat out;
    cv::GaussianBlur(src32, out, cv::Size(k,k), sigma, sigma, cv::BORDER_REFLECT);
    return out;
}

// Keep only "bright halo" signal: blurLarge - blurSmall, clamp at 0
static cv::Mat dogBright32(const cv::Mat &img32_0to1, float sigmaSmall, float sigmaLarge)
{
    cv::Mat a = gaussianBlur32(img32_0to1, sigmaSmall);
    cv::Mat b = gaussianBlur32(img32_0to1, sigmaLarge);
    cv::Mat dog = b - a;                 // signed: positive = brighter at larger scale
    cv::max(dog, 0.0f, dog);             // keep bright halos only
    return dog;                          // CV_32F
}

static HaloDetectResult detectLowpassHalosOrig(const cv::Mat &fusedGray8Orig,
                                               const HaloDetectParams &hp)
{
    CV_Assert(fusedGray8Orig.type() == CV_8U);

    HaloDetectResult r;

    // 1) Edge mask (orig size)
    r.edgeMask8 = makeEdgeMask8U(fusedGray8Orig, hp.edgeSigma, hp.edgeThreshFrac, hp.edgeDilatePx);

    // If no edges, no halo detection possible
    if (cv::countNonZero(r.edgeMask8) == 0)
    {
        r.dogMax32 = cv::Mat::zeros(fusedGray8Orig.size(), CV_32F);
        r.dist32   = cv::Mat::zeros(fusedGray8Orig.size(), CV_32F);
        r.haloMask8= cv::Mat::zeros(fusedGray8Orig.size(), CV_8U);
        return r;
    }

    // 2) Distance-to-edge gating
    // distanceTransform expects non-zero as "free space", zero as "obstacle".
    // We want distance to edge pixels, so treat edges as obstacles (0).
    cv::Mat edgeInv;
    cv::bitwise_not(r.edgeMask8, edgeInv);              // edge pixels -> 0, else 255
    cv::Mat edgeInvBin = (edgeInv != 0);                // 0/255 -> 0/1
    edgeInvBin.convertTo(edgeInvBin, CV_8U, 255.0);     // ensure 0/255
    cv::distanceTransform(edgeInvBin, r.dist32, cv::DIST_L2, 3); // CV_32F in pixels

    cv::Mat nearEdgeMask = (r.dist32 <= float(hp.maxEdgeDistPx));
    nearEdgeMask.convertTo(nearEdgeMask, CV_8U, 255.0);

    // 3) Normalize grayscale to 0..1 float
    cv::Mat img32;
    fusedGray8Orig.convertTo(img32, CV_32F, 1.0/255.0);

    // 4) Multi-scale max positive DoG
    r.dogMax32 = cv::Mat::zeros(fusedGray8Orig.size(), CV_32F);
    for (auto &p : hp.dogScales)
    {
        cv::Mat dog = dogBright32(img32, p.first, p.second);
        cv::max(r.dogMax32, dog, r.dogMax32);
    }

    // 5) Threshold and gate to near-edge
    cv::Mat halo = (r.dogMax32 > hp.dogThresh);
    halo.convertTo(halo, CV_8U, 255.0);

    cv::bitwise_and(halo, nearEdgeMask, halo);

    // 6) Optional cleanup: close + remove tiny components
    if (hp.closePx > 0)
    {
        int k = 2 * hp.closePx + 1;
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k,k));
        cv::morphologyEx(halo, halo, cv::MORPH_CLOSE, se);
    }

    if (hp.minAreaPx > 0)
    {
        cv::Mat labels, stats, cents;
        int n = cv::connectedComponentsWithStats(halo, labels, stats, cents, 8, CV_32S);
        cv::Mat cleaned = cv::Mat::zeros(halo.size(), CV_8U);
        for (int i = 1; i < n; ++i)
        {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            if (area >= hp.minAreaPx)
            {
                cleaned.setTo(255, labels == i);
            }
        }
        halo = cleaned;
    }

    r.haloMask8 = halo;

    // 7) Score: mean DoG inside halo mask
    r.nHalo = cv::countNonZero(r.haloMask8);
    if (r.nHalo > 0)
    {
        cv::Scalar m = cv::mean(r.dogMax32, r.haloMask8);
        r.scoreMean = m[0]; // 0..~0.1 typical
    }

    return r;
}

static void dumpHaloDetectorDebugOrig(const QString &basePath,
                                      const cv::Mat &fusedGray8Orig,
                                      const HaloDetectParams &hp,
                                      const HaloDetectResult &r)
{
    // 1) edge mask
    cv::imwrite((basePath + "/halo_edgeMask.png").toStdString(), r.edgeMask8);

    // 2) dist (visualize 0..maxEdgeDist)
    {
        cv::Mat distVis;
        cv::Mat d = r.dist32.clone();
        cv::min(d, float(hp.maxEdgeDistPx), d);
        d.convertTo(distVis, CV_8U, 255.0 / double(hp.maxEdgeDistPx));
        cv::imwrite((basePath + "/halo_edgeDist.png").toStdString(), distVis);
    }

    // 3) dogMax (visualize)
    {
        cv::Mat dogVis;
        cv::Mat d = r.dogMax32.clone();
        // map [0..dogThresh*4] to [0..255] for visibility
        double scaleMax = std::max(1e-6, double(hp.dogThresh) * 4.0);
        cv::min(d, float(scaleMax), d);
        d.convertTo(dogVis, CV_8U, 255.0 / scaleMax);
        cv::imwrite((basePath + "/halo_dogMax.png").toStdString(), dogVis);
    }

    // 4) halo mask
    cv::imwrite((basePath + "/halo_mask.png").toStdString(), r.haloMask8);

    // 5) Red overlay on grayscale
    {
        cv::Mat gray3;
        cv::cvtColor(fusedGray8Orig, gray3, cv::COLOR_GRAY2BGR);

        // paint mask pixels red (BGR: 0,0,255) with alpha blend
        const float alpha = 0.55f;
        for (int y = 0; y < gray3.rows; ++y)
        {
            const uint8_t *m = r.haloMask8.ptr<uint8_t>(y);
            cv::Vec3b *p = gray3.ptr<cv::Vec3b>(y);
            for (int x = 0; x < gray3.cols; ++x)
            {
                if (!m[x]) continue;
                cv::Vec3b c = p[x];
                // blend toward red
                c[0] = uint8_t((1-alpha)*c[0] + alpha*0);
                c[1] = uint8_t((1-alpha)*c[1] + alpha*0);
                c[2] = uint8_t((1-alpha)*c[2] + alpha*255);
                p[x] = c;
            }
        }
        cv::imwrite((basePath + "/halo_overlay.png").toStdString(), gray3);
    }
}

// (LL) testing
struct HaloMetrics
{
    double meanNear = 0.0;
    double meanFar  = 0.0;
    double score    = 0.0;
    int    nNear    = 0;
    int    nFar     = 0;
};

static HaloMetrics computeLowpassHaloScoreOrig(const cv::Mat &fusedGray8Orig,   // CV_8U
                                               float edgeSigma,
                                               float edgeThreshFrac,
                                               int   edgeDilatePx,  // your edge band width
                                               float lowpassSigma,  // e.g. 8..20 (orig-size pixels)
                                               int   nearGrowPx,    // e.g. 0..6
                                               int   farGrowPx)     // e.g. 20..60
{
    CV_Assert(fusedGray8Orig.type() == CV_8U);

    // Edge band (orig size)
    cv::Mat edge8 = makeEdgeMask8U(fusedGray8Orig, edgeSigma, edgeThreshFrac, edgeDilatePx); // returns 0/255
    cv::Mat edge = (edge8 != 0);
    edge.convertTo(edge, CV_8U, 255.0);

    HaloMetrics hm;

    if (cv::countNonZero(edge) == 0)
        return hm;

    auto dilateMask = [&](const cv::Mat &m, int px)
    {
        if (px <= 0) return m.clone();
        int k = 2 * px + 1;
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
        cv::Mat out;
        cv::dilate(m, out, se);
        return out;
    };

    // near ring: edge band grown a bit (optional)
    cv::Mat nearMask = dilateMask(edge, nearGrowPx);

    // far ring: [dilate(edge, farGrowPx) - dilate(edge, nearGrowPx)]
    cv::Mat farOuter = dilateMask(edge, farGrowPx);
    cv::Mat farInner = nearMask.clone();
    cv::Mat farMask;
    cv::subtract(farOuter, farInner, farMask);          // 0/255, but subtract can underflow; use logic:
    farMask = (farOuter != 0) & (farInner == 0);
    farMask.convertTo(farMask, CV_8U, 255.0);

    hm.nNear = cv::countNonZero(nearMask);
    hm.nFar  = cv::countNonZero(farMask);
    if (hm.nNear == 0 || hm.nFar == 0)
        return hm;

    // Lowpass image (Gaussian blur)
    cv::Mat lp32;
    fusedGray8Orig.convertTo(lp32, CV_32F, 1.0 / 255.0);
    if (lowpassSigma > 0.0f)
    {
        int k = int(lowpassSigma * 4.0f) + 1;
        if ((k & 1) == 0) ++k;
        if (k < 3) k = 3;
        cv::GaussianBlur(lp32, lp32, cv::Size(k, k), lowpassSigma, lowpassSigma, cv::BORDER_REFLECT);
    }

    cv::Scalar meanNear = cv::mean(lp32, nearMask);
    cv::Scalar meanFar  = cv::mean(lp32, farMask);

    hm.meanNear = meanNear[0];
    hm.meanFar  = meanFar[0];
    hm.score    = hm.meanNear - hm.meanFar; // >0 means a bright lowpass “halo” near edges

    return hm;
}

static void dumpHaloDiagnosticPNGs(const QString &basePath,
                                   const cv::Mat &fusedGray8Orig,
                                   float edgeSigma, float edgeThreshFrac, int edgeDilatePx,
                                   float lowpassSigma, int nearGrowPx, int farGrowPx)
{
    cv::Mat edge8 = makeEdgeMask8U(fusedGray8Orig, edgeSigma, edgeThreshFrac, edgeDilatePx);

    cv::Mat lp32;
    fusedGray8Orig.convertTo(lp32, CV_32F, 1.0 / 255.0);
    int k = int(lowpassSigma * 4.0f) + 1;
    if ((k & 1) == 0) ++k;
    if (k < 3) k = 3;
    cv::GaussianBlur(lp32, lp32, cv::Size(k, k), lowpassSigma, lowpassSigma, cv::BORDER_REFLECT);

    cv::Mat lp8;
    lp32.convertTo(lp8, CV_8U, 255.0);

    // Rebuild near/far masks (same logic as computeLowpassHaloScoreOrig)
    cv::Mat edge = (edge8 != 0); edge.convertTo(edge, CV_8U, 255.0);

    auto dilateMask = [&](const cv::Mat &m, int px)
    {
        if (px <= 0) return m.clone();
        int k2 = 2 * px + 1;
        cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k2, k2));
        cv::Mat out;
        cv::dilate(m, out, se);
        return out;
    };

    cv::Mat nearMask = dilateMask(edge, nearGrowPx);
    cv::Mat farOuter = dilateMask(edge, farGrowPx);
    cv::Mat farMask  = (farOuter != 0) & (nearMask == 0);
    farMask.convertTo(farMask, CV_8U, 255.0);

    cv::imwrite((basePath + "/halo_edgeMask.png").toStdString(), edge8);
    cv::imwrite((basePath + "/halo_lowpass.png").toStdString(), lp8);
    cv::imwrite((basePath + "/halo_nearMask.png").toStdString(), nearMask);
    cv::imwrite((basePath + "/halo_farMask.png").toStdString(), farMask);
}

// end (LL) testing

int levelsForSize(cv::Size size)
{
    const int minLevels = 5;
    const int maxLevels = 10;

    int dimension = std::max(size.width, size.height);

    int levels = minLevels;
    while ((dimension >> levels) > 8 && levels < maxLevels)
    {
        levels++;
    }

    int divider = (1 << levels);
    cv::Size expanded = size;

    if (expanded.width % divider != 0)
    {
        expanded.width += divider - expanded.width % divider;
    }
    if (expanded.height % divider != 0)
    {
        expanded.height += divider - expanded.height % divider;
    }

    if (levels < maxLevels && expanded != size)
    {
        levels = levelsForSize(expanded);
    }

    return levels;
}

static void zeroHighpassSubbandsInPlace(cv::Mat &wavelet32FC2)
{
    CV_Assert(wavelet32FC2.type() == CV_32FC2);

    const int levels = levelsForSize(wavelet32FC2.size()); // if not accessible, copy levelsForSize here

    for (int level = 0; level < levels; ++level)
    {
        const int w  = wavelet32FC2.cols >> level;
        const int h  = wavelet32FC2.rows >> level;
        const int w2 = w / 2;
        const int h2 = h / 2;

        // Subband layout you already use:
        // TL = lowpass, TR/BR/BL = highpass bands
        cv::Rect rTR(w2, 0,  w2, h2);
        cv::Rect rBR(w2, h2, w2, h2);
        cv::Rect rBL(0,  h2, w2, h2);

        wavelet32FC2(rTR).setTo(cv::Scalar(0,0));
        wavelet32FC2(rBR).setTo(cv::Scalar(0,0));
        wavelet32FC2(rBL).setTo(cv::Scalar(0,0));
    }
}

static bool inverseAndWriteOrig(const QString &path,
                                const cv::Mat &wavelet32FC2,
                                const cv::Rect &roiPadToAlign,
                                const cv::Rect &validAreaAlign,
                                bool useOpenCL)
{
    cv::Mat gray8;
    if (!FSFusionWavelet::inverse(wavelet32FC2, useOpenCL, gray8))
        return false;

    if (gray8.type() != CV_8U)
        return false;

    cv::Mat align = gray8(roiPadToAlign);
    cv::Mat orig  = align(validAreaAlign);
    return cv::imwrite(path.toStdString(), orig);
}

static void keepLevel0LowpassOnly(cv::Mat &wavelet32FC2)
{
    CV_Assert(wavelet32FC2.type() == CV_32FC2);

    const int w2 = wavelet32FC2.cols / 2;
    const int h2 = wavelet32FC2.rows / 2;

    // zero TR/BR/BL of the full image (level 0 only)
    wavelet32FC2(cv::Rect(w2, 0,  w2, h2)).setTo(cv::Scalar(0,0)); // TR
    wavelet32FC2(cv::Rect(w2, h2, w2, h2)).setTo(cv::Scalar(0,0)); // BR
    wavelet32FC2(cv::Rect(0,  h2, w2, h2)).setTo(cv::Scalar(0,0)); // BL
}

static void keepLevel0HighpassOnly(cv::Mat &wavelet32FC2)
{
    CV_Assert(wavelet32FC2.type() == CV_32FC2);

    const int w2 = wavelet32FC2.cols / 2;
    const int h2 = wavelet32FC2.rows / 2;

    // zero TL (level 0 lowpass), keep TR/BR/BL
    wavelet32FC2(cv::Rect(0, 0, w2, h2)).setTo(cv::Scalar(0,0));
}

static void dumpWaveletBandDebugOrigSize(const QString &basePath,
                                         const cv::Mat &mergedWavelet32FC2,
                                         const cv::Rect &roiPadToAlign,
                                         const cv::Rect &validAreaAlign,
                                         bool useOpenCL)
{
    CV_Assert(mergedWavelet32FC2.type() == CV_32FC2);

    // (1) Deepest-only approximation (your current behavior)
    {
        cv::Mat w = mergedWavelet32FC2.clone();
        zeroHighpassSubbandsInPlace(w); // your recursive version
        inverseAndWriteOrig(basePath + "/fused_deepestApproxOnly_orig.png",
                            w, roiPadToAlign, validAreaAlign, useOpenCL);
    }

    // (2) Level-0 lowpass only (NON-recursive)
    {
        cv::Mat w = mergedWavelet32FC2.clone();
        keepLevel0LowpassOnly(w);
        inverseAndWriteOrig(basePath + "/fused_level0LowpassOnly_orig.png",
                            w, roiPadToAlign, validAreaAlign, useOpenCL);
    }

    // (3) Level-0 highpass only
    {
        cv::Mat w = mergedWavelet32FC2.clone();
        keepLevel0HighpassOnly(w);
        inverseAndWriteOrig(basePath + "/fused_level0HighpassOnly_orig.png",
                            w, roiPadToAlign, validAreaAlign, useOpenCL);
    }
}

static void dumpLowpassOnlyDebugOrigSize(const QString &basePath,
                                         const cv::Mat &mergedWavelet32FC2, // padSize
                                         const cv::Rect &roiPadToAlign,     // pad -> align
                                         const cv::Rect &validAreaAlign,    // align -> orig
                                         bool useOpenCL)
{
    CV_Assert(mergedWavelet32FC2.type() == CV_32FC2);

    // 1) Copy wavelet and kill all highpass bands
    cv::Mat wLP = mergedWavelet32FC2.clone();
    zeroHighpassSubbandsInPlace(wLP);

    // 2) Inverse -> gray (padSize)
    cv::Mat lpGray8;
    if (!FSFusionWavelet::inverse(wLP, useOpenCL, lpGray8))
        return;

    CV_Assert(lpGray8.type() == CV_8U);

    // 3) Crop to origSize
    cv::Mat lpAlign = lpGray8(roiPadToAlign);
    cv::Mat lpOrig  = lpAlign(validAreaAlign);

    // 4) Write
    cv::imwrite((basePath + "/fused_lowpassOnly_orig.png").toStdString(), lpOrig);
}
// ----- end temp debug section -----

static void dumpDepthErosionDebugCropped(
    const QString &basePath,
    const cv::Mat &fusedGray8_padded,
    const cv::Mat &edgeMask8U_padded,
    const cv::Mat &winnerBefore16_padded,
    const cv::Mat &winnerAfter16_padded,
    const cv::Rect &roiPadToAlign,
    const cv::Rect &validAreaAlign)
{
    QString srcFun = "FSFusion::dumpDepthErosionDebugCropped";
    if (G::FSLog) G::log(srcFun, basePath);

    CV_Assert(fusedGray8_padded.type() == CV_8U);
    CV_Assert(edgeMask8U_padded.type() == CV_8U);
    CV_Assert(winnerBefore16_padded.type() == CV_16U);
    CV_Assert(winnerAfter16_padded.type() == CV_16U);

    // ------------------------------------------------------------
    // Crop EVERYTHING to origSize
    // ------------------------------------------------------------
    cv::Mat fusedGray8   = cropPadToOrig(fusedGray8_padded,   roiPadToAlign, validAreaAlign);
    cv::Mat edgeMask8U   = cropPadToOrig(edgeMask8U_padded,   roiPadToAlign, validAreaAlign);
    cv::Mat before16     = cropPadToOrig(winnerBefore16_padded, roiPadToAlign, validAreaAlign);
    cv::Mat after16      = cropPadToOrig(winnerAfter16_padded,  roiPadToAlign, validAreaAlign);

    // ------------------------------------------------------------
    // Edge mask
    // ------------------------------------------------------------
    cv::imwrite((basePath + "/edgeMask.png").toStdString(), edgeMask8U);

    // ------------------------------------------------------------
    // Depth maps (normalized)
    // ------------------------------------------------------------
    double minV = 0.0, maxV = 0.0;
    cv::minMaxLoc(before16, &minV, &maxV);
    if (maxV <= minV) maxV = minV + 1.0;

    cv::Mat before8, after8;
    before16.convertTo(before8, CV_8U,
                       255.0 / (maxV - minV),
                       -minV * 255.0 / (maxV - minV));
    after16.convertTo(after8, CV_8U,
                      255.0 / (maxV - minV),
                      -minV * 255.0 / (maxV - minV));

    cv::imwrite((basePath + "/depthBefore.png").toStdString(), before8);
    cv::imwrite((basePath + "/depthAfter.png").toStdString(),  after8);

    // ------------------------------------------------------------
    // Delta map
    // ------------------------------------------------------------
    cv::Mat delta32;
    after16.convertTo(delta32, CV_32F);
    cv::Mat before32;
    before16.convertTo(before32, CV_32F);
    delta32 -= before32;

    cv::min(delta32,  10.0f, delta32);
    cv::max(delta32, -10.0f, delta32);

    cv::Mat delta8;
    delta32.convertTo(delta8, CV_8U, 255.0 / 20.0, 128.0);
    cv::imwrite((basePath + "/depthDelta.png").toStdString(), delta8);

    // ------------------------------------------------------------
    // Changed mask
    // ------------------------------------------------------------
    cv::Mat changedMask = (before16 != after16);
    changedMask.convertTo(changedMask, CV_8U, 255.0);
    cv::imwrite((basePath + "/changedMask.png").toStdString(), changedMask);

    // ------------------------------------------------------------
    // Overlays on fused grayscale
    // ------------------------------------------------------------
    cv::Mat fusedBGR;
    cv::cvtColor(fusedGray8, fusedBGR, cv::COLOR_GRAY2BGR);

    // Red = changed
    {
        cv::Mat overlay = fusedBGR.clone();
        overlay.setTo(cv::Scalar(0,0,255), changedMask);
        cv::Mat out;
        cv::addWeighted(fusedBGR, 0.65, overlay, 0.35, 0.0, out);
        cv::imwrite((basePath + "/fusedGray_overlayChanged.png").toStdString(), out);
    }

    // Yellow = edge band
    {
        cv::Mat overlay = fusedBGR.clone();
        overlay.setTo(cv::Scalar(0,255,255), edgeMask8U);
        cv::Mat out;
        cv::addWeighted(fusedBGR, 0.70, overlay, 0.30, 0.0, out);
        cv::imwrite((basePath + "/fusedGray_overlayEdge.png").toStdString(), out);
    }

    // Red = changed inside edge
    {
        cv::Mat changedInEdge;
        cv::bitwise_and(changedMask, edgeMask8U, changedInEdge);

        cv::Mat overlay = fusedBGR.clone();
        overlay.setTo(cv::Scalar(0,0,255), changedInEdge);
        cv::Mat out;
        cv::addWeighted(fusedBGR, 0.65, overlay, 0.35, 0.0, out);
        cv::imwrite((basePath + "/fusedGray_overlayChangedInsideEdge.png").toStdString(), out);
    }
}

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

// Save: grayscale + red overlay where DBE changed pixels, at origSize.
static void dumpDepthErosionOverlayDebugOrigSize(
    const QString &basePath,
    const cv::Mat &fusedGray8_padded,        // CV_8U padSize (same one used for color map)
    const cv::Mat &edgeMask8U_padded,        // CV_8U padSize
    const cv::Mat &winnerBefore16_padded,    // CV_16U padSize
    const cv::Mat &winnerAfter16_padded,     // CV_16U padSize
    const cv::Size &alignSize,               // alignSize
    const cv::Size &padSize,                 // padSize
    const cv::Rect &validAreaAlign,          // origSize inside alignSize
    bool restrictChangedToEdge = false       // if true: changedMask &= edgeMask
    )
{
    QString srcFun = "FSFusion::dumpDepthErosionOverlayDebugOrigSize";
    if (G::FSLog) G::log(srcFun, basePath);

    CV_Assert(fusedGray8_padded.type() == CV_8U);
    CV_Assert(edgeMask8U_padded.type() == CV_8U);
    CV_Assert(winnerBefore16_padded.type() == CV_16U);
    CV_Assert(winnerAfter16_padded.type() == CV_16U);

    CV_Assert(fusedGray8_padded.size() == padSize);
    CV_Assert(edgeMask8U_padded.size() == padSize);
    CV_Assert(winnerBefore16_padded.size() == padSize);
    CV_Assert(winnerAfter16_padded.size() == padSize);

    // ----------------------------
    // Step 1: padSize -> alignSize crop
    // ----------------------------
    cv::Rect roiPadToAlign(0, 0, alignSize.width, alignSize.height);

    if (padSize != alignSize)
    {
        const int padW = padSize.width  - alignSize.width;
        const int padH = padSize.height - alignSize.height;

        CV_Assert(padW >= 0 && padH >= 0);

        const int left = padW / 2;
        const int top  = padH / 2;

        roiPadToAlign = cv::Rect(left, top, alignSize.width, alignSize.height);
    }

    // Bounds
    CV_Assert(roiPadToAlign.x >= 0 && roiPadToAlign.y >= 0);
    CV_Assert(roiPadToAlign.x + roiPadToAlign.width  <= fusedGray8_padded.cols);
    CV_Assert(roiPadToAlign.y + roiPadToAlign.height <= fusedGray8_padded.rows);

    cv::Mat fusedGray_align   = fusedGray8_padded(roiPadToAlign);
    cv::Mat edgeMask_align    = edgeMask8U_padded(roiPadToAlign);
    cv::Mat before_align      = winnerBefore16_padded(roiPadToAlign);
    cv::Mat after_align       = winnerAfter16_padded(roiPadToAlign);

    // ----------------------------
    // Step 2: alignSize -> origSize crop
    // ----------------------------
    CV_Assert(validAreaAlign.x >= 0 && validAreaAlign.y >= 0);
    CV_Assert(validAreaAlign.x + validAreaAlign.width  <= fusedGray_align.cols);
    CV_Assert(validAreaAlign.y + validAreaAlign.height <= fusedGray_align.rows);

    cv::Mat fusedGray_orig = fusedGray_align(validAreaAlign).clone();
    cv::Mat edgeMask_orig  = edgeMask_align(validAreaAlign).clone();
    cv::Mat before_orig    = before_align(validAreaAlign).clone();
    cv::Mat after_orig     = after_align(validAreaAlign).clone();

    // ----------------------------
    // Build masks (origSize)
    // ----------------------------
    cv::Mat edgeMaskBin;
    cv::compare(edgeMask_orig, 0, edgeMaskBin, cv::CMP_NE); // CV_8U 0/255

    cv::Mat changedMask;
    cv::compare(after_orig, before_orig, changedMask, cv::CMP_NE); // CV_8U 0/255

    if (restrictChangedToEdge)
        cv::bitwise_and(changedMask, edgeMaskBin, changedMask);

    const int changedCount = cv::countNonZero(changedMask);
    const int edgeCount    = cv::countNonZero(edgeMaskBin);

    if (G::FSLog)
    {
        G::log(srcFun, "origSize edgeCount=" + QString::number(edgeCount) +
                           " changedCount=" + QString::number(changedCount) +
                           " restrictChangedToEdge=" + QString(restrictChangedToEdge ? "true" : "false"));
    }

    // ----------------------------
    // Create overlays (origSize)
    // ----------------------------
    cv::Mat grayBgr;
    cv::cvtColor(fusedGray_orig, grayBgr, cv::COLOR_GRAY2BGR);

    // overlay_changed_red: set R=255 on changed pixels
    cv::Mat overlayChanged = grayBgr.clone();
    {
        std::vector<cv::Mat> ch(3);
        cv::split(overlayChanged, ch); // B,G,R
        ch[2].setTo(255, changedMask); // red channel
        cv::merge(ch, overlayChanged);
    }

    // overlay_edge_green: set G=255 on edge pixels
    cv::Mat overlayEdge = grayBgr.clone();
    {
        std::vector<cv::Mat> ch(3);
        cv::split(overlayEdge, ch); // B,G,R
        ch[1].setTo(255, edgeMaskBin); // green channel
        cv::merge(ch, overlayEdge);
    }

    // overlay_both: apply both in one image (yellow where both)
    cv::Mat overlayBoth = grayBgr.clone();
    {
        std::vector<cv::Mat> ch(3);
        cv::split(overlayBoth, ch); // B,G,R
        ch[1].setTo(255, edgeMaskBin);  // green = edge
        ch[2].setTo(255, changedMask);  // red   = changed
        cv::merge(ch, overlayBoth);
    }

    // ----------------------------
    // Write PNGs
    // ----------------------------
    const std::string base = basePath.toStdString();

    cv::imwrite(base + "/fusedGray_orig.png", fusedGray_orig);
    cv::imwrite(base + "/edgeMask_orig.png", edgeMaskBin);
    cv::imwrite(base + "/changedMask_orig.png", changedMask);
    cv::imwrite(base + "/overlay_changed_red_orig.png", overlayChanged);
    cv::imwrite(base + "/overlay_edge_green_orig.png", overlayEdge);
    cv::imwrite(base + "/overlay_both_orig.png", overlayBoth);
}

static cv::Mat toLuma32F(const cv::Mat& img)
{
    CV_Assert(!img.empty());
    cv::Mat l32;

    if (img.channels() == 1) {
        img.convertTo(l32, CV_32F);
        return l32;
    }

    CV_Assert(img.channels() == 3);
    cv::Mat f; img.convertTo(f, CV_32F);
    std::vector<cv::Mat> ch(3);
    cv::split(f, ch); // B,G,R
    l32 = 0.114f*ch[0] + 0.587f*ch[1] + 0.299f*ch[2];
    return l32;
}

static float bilerp32F(const cv::Mat& m32, float x, float y)
{
    x = std::max(0.0f, std::min(x, float(m32.cols - 1)));
    y = std::max(0.0f, std::min(y, float(m32.rows - 1)));

    int x0 = int(std::floor(x));
    int y0 = int(std::floor(y));
    int x1 = std::min(x0 + 1, m32.cols - 1);
    int y1 = std::min(y0 + 1, m32.rows - 1);

    float fx = x - x0;
    float fy = y - y0;

    float v00 = m32.at<float>(y0, x0);
    float v10 = m32.at<float>(y0, x1);
    float v01 = m32.at<float>(y1, x0);
    float v11 = m32.at<float>(y1, x1);

    float v0 = v00 + fx*(v10 - v00);
    float v1 = v01 + fx*(v11 - v01);
    return v0 + fy*(v1 - v0);
}

static void dumpHaloOverlayOrigSize(const QString &basePath,
                                    const cv::Mat &fusedGray8Orig, // CV_8U orig size
                                    float lowpassSigma,
                                    int nearPx,
                                    int farPx,
                                    float overlayDelta)
{
    QString srcFun = "FSFusion::dumpHaloOverlayOrigSize";
    if (G::FSLog) G::log(srcFun, basePath);

    CV_Assert(fusedGray8Orig.type() == CV_8U);

    // 1) Object mask (branches)
    cv::Mat obj = makeObjectMask8U_Otsu(fusedGray8Orig);
    cv::imwrite((basePath + "/halo_objMask.png").toStdString(), obj);

    // 2) Outside rings
    cv::Mat dNear = dilateMaskPx(obj, nearPx);
    cv::Mat dFar  = dilateMaskPx(obj, farPx);

    cv::Mat nearOutside = (dNear != 0) & (obj == 0);
    nearOutside.convertTo(nearOutside, CV_8U, 255.0);

    cv::Mat farOutside  = (dFar != 0) & (dNear == 0);
    farOutside.convertTo(farOutside, CV_8U, 255.0);

    cv::imwrite((basePath + "/halo_nearOutside.png").toStdString(), nearOutside);
    cv::imwrite((basePath + "/halo_farOutside.png").toStdString(),  farOutside);

    const int nNear = cv::countNonZero(nearOutside);
    const int nFar  = cv::countNonZero(farOutside);
    if (G::FSLog) {
        G::log(srcFun, "nNearOutside=" + QString::number(nNear) +
                           " nFarOutside="  + QString::number(nFar));
    }
    if (nNear == 0 || nFar == 0) return;

    // 3) Lowpass image
    cv::Mat lp32;
    fusedGray8Orig.convertTo(lp32, CV_32F, 1.0/255.0);

    if (lowpassSigma > 0.0f) {
        int k = int(lowpassSigma * 4.0f) + 1;
        if ((k & 1) == 0) ++k;
        if (k < 3) k = 3;
        cv::GaussianBlur(lp32, lp32, cv::Size(k,k), lowpassSigma, lowpassSigma, cv::BORDER_REFLECT);
    }

    // Debug visualize lowpass
    cv::Mat lp8;
    lp32.convertTo(lp8, CV_8U, 255.0);
    cv::imwrite((basePath + "/halo_lowpass.png").toStdString(), lp8);

    // 4) Halo score (outside-only)
    const double meanNear = cv::mean(lp32, nearOutside)[0];
    const double meanFar  = cv::mean(lp32, farOutside)[0];
    const double score    = meanNear - meanFar;

    if (G::FSLog) {
        G::log(srcFun, "HaloScore OUTSIDE: near=" + QString::number(meanNear, 'f', 6) +
                           " far=" + QString::number(meanFar, 'f', 6) +
                           " score=" + QString::number(score, 'f', 6));
    }

    // 5) Overlay: mark pixels in nearOutside where lowpass is "too bright"
    // threshold = meanFar + overlayDelta
    cv::Mat hot = (lp32 > float(meanFar + overlayDelta));
    hot.convertTo(hot, CV_8U, 255.0);
    cv::bitwise_and(hot, nearOutside, hot);

    // base grayscale -> BGR
    cv::Mat bgr;
    cv::cvtColor(fusedGray8Orig, bgr, cv::COLOR_GRAY2BGR);

    // red overlay
    // set R=255 where hot
    for (int y = 0; y < bgr.rows; ++y) {
        cv::Vec3b *p = bgr.ptr<cv::Vec3b>(y);
        const uint8_t *m = hot.ptr<uint8_t>(y);
        for (int x = 0; x < bgr.cols; ++x) {
            if (m[x]) p[x][2] = 255; // R
        }
    }

    cv::imwrite((basePath + "/haloOverlay_outside.png").toStdString(), bgr);
}

static void dumpHaloDoGOverlayOrigSize(const QString &basePath,
                                       const cv::Mat &fusedGray8Orig, // CV_8U
                                       float sigmaNear,               // e.g. 4..8
                                       float sigmaFar,                // e.g. 16..30
                                       int outsidePx,                 // e.g. 12..30
                                       float dogThresh)               // e.g. 0.01..0.05 (normalized)
{
    QString srcFun = "FSFusion::dumpHaloDoGOverlayOrigSize";
    if (G::FSLog) G::log(srcFun, basePath);

    CV_Assert(fusedGray8Orig.type() == CV_8U);
    CV_Assert(sigmaFar > sigmaNear);

    // 1) Build a "structure" mask for the branch using gradients (more robust than Otsu for your case)
    cv::Mat gx, gy, mag;
    cv::Sobel(fusedGray8Orig, gx, CV_32F, 1, 0, 3);
    cv::Sobel(fusedGray8Orig, gy, CV_32F, 0, 1, 3);
    cv::magnitude(gx, gy, mag);

    double minV=0, maxV=0;
    cv::minMaxLoc(mag, &minV, &maxV);

    // Keep strongest gradients as "structure"
    const float t = float(maxV) * 0.20f;  // try 0.10..0.30
    cv::Mat structure = (mag > t);
    structure.convertTo(structure, CV_8U, 255.0);

    // Thicken & clean structure so we can define an outside band
    cv::Mat se5 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5,5));
    cv::morphologyEx(structure, structure, cv::MORPH_CLOSE, se5);
    cv::morphologyEx(structure, structure, cv::MORPH_OPEN,  se5);

    // 2) Outside band around structure
    int k = 2 * outsidePx + 1;
    cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k,k));
    cv::Mat dil;
    cv::dilate(structure, dil, se);

    cv::Mat outside = (dil != 0) & (structure == 0);
    outside.convertTo(outside, CV_8U, 255.0);

    // 3) DoG halo map (normalized float)
    cv::Mat f32;
    fusedGray8Orig.convertTo(f32, CV_32F, 1.0/255.0);

    cv::Mat lpNear = f32.clone();
    cv::Mat lpFar  = f32.clone();

    cv::GaussianBlur(lpNear, lpNear, cv::Size(0,0), sigmaNear, sigmaNear, cv::BORDER_REFLECT);
    cv::GaussianBlur(lpFar,  lpFar,  cv::Size(0,0), sigmaFar,  sigmaFar,  cv::BORDER_REFLECT);

    cv::Mat dog = lpNear - lpFar;   // bright mid-scale "glow" pops positive

    // Debug visualize dog as 8U (scaled)
    cv::Mat dogVis;
    {
        cv::Mat clipped = cv::max(cv::min(dog, 0.10f), -0.10f);  // +/-0.1
        clipped.convertTo(dogVis, CV_8U, 255.0 / 0.20, 128.0);
        cv::imwrite((basePath + "/halo_DoG.png").toStdString(), dogVis);
    }

    // 4) Halo candidates: dog > threshold AND in outside band
    cv::Mat hot = (dog > dogThresh);
    hot.convertTo(hot, CV_8U, 255.0);
    cv::bitwise_and(hot, outside, hot);

    // 5) Overlay red on grayscale
    cv::Mat bgr;
    cv::cvtColor(fusedGray8Orig, bgr, cv::COLOR_GRAY2BGR);

    for (int y = 0; y < bgr.rows; ++y) {
        cv::Vec3b *p = bgr.ptr<cv::Vec3b>(y);
        const uint8_t *m = hot.ptr<uint8_t>(y);
        for (int x = 0; x < bgr.cols; ++x) {
            if (m[x]) {
                p[x][2] = 255; // R
            }
        }
    }

    cv::imwrite((basePath + "/haloOverlay_DoG.png").toStdString(), bgr);
}

static cv::Mat depthBiasedErode16(const cv::Mat &winner16,
                                  const cv::Mat &edgeMask8U,
                                  int radius,
                                  int iters,
                                  int maxDelta,
                                  float minEdgeDelta)
{
/*
    Used by DepthBiasedErosion
*/
    QString srcFun = "FSFusion::depthBiasedErode16";
    if (G::FSLog) G::log(srcFun);

    CV_Assert(winner16.type() == CV_16U);
    CV_Assert(edgeMask8U.type() == CV_8U);
    CV_Assert(winner16.size() == edgeMask8U.size());

    cv::Mat cur = winner16.clone();

    int k = 2 * radius + 1;
    cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                           cv::Size(k, k));

    for (int i = 0; i < iters; ++i)
    {
        cv::Mat localMax, localMin;
        cv::dilate(cur, localMax, se);
        cv::erode(cur,  localMin, se);

        // Only act where:
        //  - in edge band
        //  - there is meaningful local depth contrast
        cv::Mat contrast32;
        {
            cv::Mat max32, min32;
            localMax.convertTo(max32, CV_32F);
            localMin.convertTo(min32, CV_32F);
            contrast32 = max32 - min32;
        }

        cv::Mat actMask = (edgeMask8U != 0) & (contrast32 >= minEdgeDelta);
        actMask.convertTo(actMask, CV_8U); // ensure 0/255
        cv::bitwise_and(actMask, edgeMask8U, actMask);

        // target = localMax (deeper)
        // cur = min(cur + maxDelta, target) in actMask
        cv::Mat cur32, tgt32;
        cur.convertTo(cur32, CV_32F);
        localMax.convertTo(tgt32, CV_32F);

        cv::Mat raised = cur32 + float(maxDelta);
        cv::min(raised, tgt32, raised);

        // write back only in mask
        cv::Mat raised16;
        raised.convertTo(raised16, CV_16U);
        raised16.copyTo(cur, actMask);
    }

    return cur;
}

/*
 * Padding helper
 * Ensures image is padded to a wavelet-friendly size using the same logic
 * as FSFusionWavelet::computeLevelsAndExpandedSize().
 */
cv::Mat padForWavelet(const cv::Mat &img, cv::Size &paddedSizeOut)
{
    if (img.empty()) return img;

    CV_Assert(img.channels() == 1 || img.channels() == 3);

    cv::Size expanded;
    FSFusionWavelet::computeLevelsAndExpandedSize(img.size(), expanded);
    paddedSizeOut = expanded;

    if (expanded == img.size())
        return img;   // No padding required

    int padW = expanded.width  - img.cols;
    int padH = expanded.height - img.rows;

    int left   = padW / 2;
    int right  = padW - left;
    int top    = padH / 2;
    int bottom = padH - top;

    cv::Mat padded;
    cv::copyMakeBorder(img,
                       padded,
                       top, bottom,
                       left, right,
                       cv::BORDER_REFLECT);

    return padded;
}

/*
 * Simple fusion: use depthIndex16 to pick color from the winning slice.
 * This does not use wavelets; it pairs naturally with FSDepth::method="Simple".
 */
bool fuseSimple(const std::vector<cv::Mat> &grayImgs,
                const std::vector<cv::Mat> &colorImgs,
                const cv::Mat              &depthIndex16,
                cv::Mat                    &outputColor8,
                std::atomic_bool           *abortFlag,
                ProgressCallback            cb)
{
    QString srcFun = "FSFusion::fuseSimple";

    const int N = static_cast<int>(grayImgs.size());
    if (N == 0 || N != static_cast<int>(colorImgs.size()))
        return false;

    if (depthIndex16.empty() || depthIndex16.type() != CV_16U)
        return false;

    const cv::Size size = grayImgs[0].size();

    for (int i = 0; i < N; ++i)
    {
        if (grayImgs[i].empty() || colorImgs[i].empty())
            return false;
        if (grayImgs[i].size() != size || colorImgs[i].size() != size)
            return false;
    }

    if (depthIndex16.size() != size)
    {
        if (G::FSLog) G::log(srcFun, "Depth index size mismatch vs input images");
        return false;
    }

    // Normalize all colors to 8UC3
    std::vector<cv::Mat> color8(N);
    for (int i = 0; i < N; ++i)
    {
        if (colorImgs[i].type() == CV_8UC3)
        {
            color8[i] = colorImgs[i];
        }
        else if (colorImgs[i].type() == CV_16UC3)
        {
            colorImgs[i].convertTo(color8[i], CV_8UC3, 255.0 / 65535.0);
        }
        else
        {
            if (G::FSLog) G::log(srcFun, "Unsupported color type in slice " + QString::number(i));
            return false;
        }
    }

    outputColor8.create(size.height, size.width, CV_8UC3);

    for (int y = 0; y < size.height; ++y)
    {
        const uint16_t *dRow = depthIndex16.ptr<uint16_t>(y);
        cv::Vec3b *outRow    = outputColor8.ptr<cv::Vec3b>(y);

        for (int x = 0; x < size.width; ++x)
        {
            uint16_t s = dRow[x];
            if (s >= static_cast<uint16_t>(N))
                s = static_cast<uint16_t>(N - 1);

            outRow[x] = color8[s].at<cv::Vec3b>(y, x);
        }
    }

    if (cb) cb();
    return true;
}

/*
 * Full PMax fusion: this reproduces the current successful pipeline.
 *
 * IMPORTANT:
 *  - DepthIndex16 from FSDepth is validated (size only) but not used to drive
 *    the PMax decisions; the wavelet merge is self-contained.
 *  - The canonical depth map in the system is the one from FSDepth. Any
 *    implicit "depth" inside the PMax merge is internal and not exposed.
 */
bool fusePMax(const std::vector<cv::Mat> &grayImgs,
              const std::vector<cv::Mat> &colorImgs,
              const FSFusion::Options    &opt,
              cv::Mat                    &depthIndex16,
              cv::Mat                    &outputColor8,
              std::atomic_bool           *abortFlag,
              FSFusion::StatusCallback    statusCallback,
              ProgressCallback            progressCallback)
{
    QString srcFun = "FSFusion::fusePMax";
    if (G::FSLog) G::log(srcFun, "Start PMax fusion");

    int outDepth = colorImgs.at(0).depth();

    const int N = static_cast<int>(grayImgs.size());
    if (N == 0 || N != static_cast<int>(colorImgs.size()))
        return false;

    // Validate sizes and types
    const cv::Size orig = grayImgs[0].size();
    for (int i = 0; i < N; ++i)
    {
        if (grayImgs[i].empty() || colorImgs[i].empty())
            return false;
        if (grayImgs[i].size() != orig || colorImgs[i].size() != orig)
            return false;
    }

    // --------------------------------------------------------------------
    // 0. Pad grayscale + color images BEFORE processing (wavelet-friendly)
    // --------------------------------------------------------------------
    std::vector<cv::Mat> grayP(N);
    std::vector<cv::Mat> colorP(N);
    cv::Size paddedSize;

    for (int i = 0; i < N; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;
        grayP[i]  = padForWavelet(grayImgs[i], paddedSize);
        colorP[i] = padForWavelet(colorImgs[i], paddedSize);
    }

    const cv::Size ps = paddedSize;
    // --------------------------------------------------------------------
    // 1. Forward wavelet per slice (grayscale only)
    // --------------------------------------------------------------------
    std::vector<cv::Mat> wavelets(N);

    if (G::FSLog) G::log(srcFun, "Forward wavelet per slice");
    for (int i = 0; i < N; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;
        QString msg = "Forward wavelet slice" + QString::number(i);
        if (G::FSLog) G::log(srcFun, msg);
        if (statusCallback) statusCallback(msg);
        if (progressCallback) progressCallback();

        if (!FSFusionWavelet::forward(grayP[i], opt.useOpenCL, wavelets[i]))
        {
            if (G::FSLog) G::log(srcFun, "Wavelet forward failed");
            return false;
        }
    }

    // --------------------------------------------------------------------
    // 2. Merge wavelet stacks → mergedWavelet (we ignore depthIndex here)
    // --------------------------------------------------------------------
    QString msg = "Merge wavelet stacks";
    if (G::FSLog) G::log(srcFun, msg);
    if (statusCallback) statusCallback(msg);
    if (progressCallback) progressCallback();

    cv::Mat mergedWavelet;

    if (!FSMerge::merge(wavelets,
                        opt.consistency,
                        abortFlag,
                        mergedWavelet,
                        depthIndex16))
    {
        if (G::FSLog) G::log(srcFun, "FSMerge::merge failed");
        return false;
    }

    // --------------------------------------------------------------------
    // 3. Inverse wavelet → fusedGray8 (still padded size)
    // --------------------------------------------------------------------
    msg = "Inverse wavelet";
    if (G::FSLog) G::log(srcFun, msg);
    if (statusCallback) statusCallback(msg);
    if (progressCallback) progressCallback();

    cv::Mat fusedGray8;
    if (!FSFusionWavelet::inverse(mergedWavelet, opt.useOpenCL, fusedGray8))
    {
        if (G::FSLog) G::log(srcFun, "FSFusionWavelet::inverse failed");
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // 4. Build color map using padded grayscale + padded RGB images
    // --------------------------------------------------------------------
    msg = "Build color map";
    if (G::FSLog) G::log(srcFun, msg);
    if (statusCallback) statusCallback(msg);
    if (progressCallback) progressCallback();

    std::vector<FSFusionReassign::ColorEntry> colorEntries;
    std::vector<uint8_t> counts;

    if (!FSFusionReassign::buildColorMap(grayP,
                                         colorP,
                                         colorEntries,
                                         counts))
    {
        QString msg ="FSFusionReassign::buildColorMap failed";
        if (G::FSLog) G::log(srcFun, msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // 5. Apply color reassignment to padded fused grayscale
    // --------------------------------------------------------------------
    msg = "Apply color reassignment";
    if (G::FSLog) G::log(srcFun, msg);
    if (statusCallback) statusCallback(msg);
    if (progressCallback) progressCallback();

    cv::Mat paddedColorOut;
    FSFusionReassign::ColorDepth cd = (outDepth == CV_16U)
           ? FSFusionReassign::ColorDepth::U16
           : FSFusionReassign::ColorDepth::U8;
    if (!FSFusionReassign::applyColorMap(fusedGray8,
                                         colorEntries,
                                         counts,
                                         paddedColorOut,
                                         cd))
    {
        QString msg = "FSFusionReassign::applyColorMap failed";
        if (G::FSLog) G::log(srcFun, msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // 6. Crop back to original (non-padded) size
    // --------------------------------------------------------------------
    if (G::FSLog) G::log(srcFun, "Crop back to original size");

    if (ps == orig)
    {
        outputColor8 = paddedColorOut;
    }
    else
    {
        int padW = ps.width  - orig.width;
        int padH = ps.height - orig.height;

        int left = padW / 2;
        int top  = padH / 2;

        cv::Rect roi(left, top, orig.width, orig.height);
        outputColor8 = paddedColorOut(roi).clone();
    }

    return true;
}

} // end anonymous namespace

static inline FSFusionReassign::ColorDepth reassignDepth(int depth)
{
    return (depth == CV_16U)
        ? FSFusionReassign::ColorDepth::U16
        : FSFusionReassign::ColorDepth::U8;
}

// Depth-Biased Erosion
// Purpose:
//   Second-pass color override after Depth-Biased Erosion changed the winner map.
//   Only override *edge-band* pixels whose winner index changed, and only when the
//   replacement slice actually contains non-trivial content (to avoid “black branches”).
//
// Key safety guards added:
//   1) Only operate on pixels where (edgeMask != 0) AND (winnerAfter != winnerBefore)
//   2) Per-slice mask is restricted to those changed pixels.
//   3) “Content check”: don’t override with a slice that is essentially background
//      at those pixels (counts luminance > threshold within mask). If too few “valid”
//      pixels, skip that slice’s override.
//   4) Apply contrast/WB in 8-bit space (FSAlign implementation is 8U).
//   5) Optional clamp of winner indices to [0, N-1] before comparing.
//   6) Optional debug: log per-slice mask size and accepted pixels.
//
// Call signature stays compatible with your current call site, BUT you should pass
// the winnerBefore and winnerAfter maps (recommended). If you can’t, see the overload
// note at the bottom.

bool FSFusion::applyDepthBiasedColorOverrideSecondPass(
    cv::Mat &paddedColorOut,
    const cv::Mat &winnerBeforePadded16,
    const cv::Mat &winnerAfterPadded16,
    const cv::Mat &edgeMask8U,
    const QStringList &inputPaths,
    const std::vector<FSAlign::Result> &globals,
    const Options &opt,
    std::atomic_bool *abortFlag
    )
{
    QString srcFun = "FSFusion::applyDepthBiasedColorOverrideSecondPass";
    if (G::FSLog) G::log(srcFun);

    // -----------------------------
    // Validate
    // -----------------------------
    CV_Assert(paddedColorOut.type() == CV_8UC3 || paddedColorOut.type() == CV_16UC3);
    CV_Assert(winnerAfterPadded16.type() == CV_16U);
    CV_Assert(edgeMask8U.type() == CV_8U);
    CV_Assert(winnerBeforePadded16.empty() || winnerBeforePadded16.type() == CV_16U);

    CV_Assert(paddedColorOut.size() == winnerAfterPadded16.size());
    CV_Assert(paddedColorOut.size() == edgeMask8U.size());
    if (!winnerBeforePadded16.empty())
        CV_Assert(winnerBeforePadded16.size() == winnerAfterPadded16.size());

    const int N = int(inputPaths.size());
    if (N <= 0 || int(globals.size()) != N)
        return false;

    // -----------------------------
    // Build act mask:
    //   - Must be in edge band
    //   - Must be a changed pixel (winnerAfter != winnerBefore)
    // -----------------------------
    cv::Mat edgeMask = (edgeMask8U != 0);
    edgeMask.convertTo(edgeMask, CV_8U, 255.0);

    if (cv::countNonZero(edgeMask) == 0) {
        if (G::FSLog) G::log(srcFun, "No edge pixels; skipping override");
        return true;
    }

    cv::Mat changedMask;
    if (!winnerBeforePadded16.empty())
    {
        changedMask = (winnerAfterPadded16 != winnerBeforePadded16);
        changedMask.convertTo(changedMask, CV_8U, 255.0);
    }
    else
    {
        // If you cannot supply winnerBefore, we can’t know what changed.
        // Fallback: operate on all edge pixels (less safe; can cause black branches).
        // Strongly recommended to pass winnerBeforePadded16.
        changedMask = edgeMask.clone();
        if (G::FSLog) G::log(srcFun, "WARNING: winnerBefore not provided; using edgeMask as changedMask");
    }

    cv::Mat actMask;
    cv::bitwise_and(edgeMask, changedMask, actMask);

    const int actCount = cv::countNonZero(actMask);
    if (actCount == 0) {
        if (G::FSLog) G::log(srcFun, "No changed edge pixels; skipping override");
        return true;
    }

    if (G::FSLog) {
        G::log(srcFun, "Act pixels (changed in edge band) = " + QString::number(actCount));
    }

    // -----------------------------
    // Optional: clamp winners to [0..N-1] for safety
    // (Rare, but protects against any out-of-range erosion artifacts.)
    // -----------------------------
    cv::Mat winnerClamped = winnerAfterPadded16;
    {
        // Only if we detect any out-of-range; keep fast path otherwise.
        double mn=0, mx=0;
        cv::minMaxLoc(winnerAfterPadded16, &mn, &mx);
        if (mn < 0.0 || mx > double(N - 1))
        {
            winnerClamped = winnerAfterPadded16.clone();
            for (int y = 0; y < winnerClamped.rows; ++y)
            {
                uint16_t *p = winnerClamped.ptr<uint16_t>(y);
                for (int x = 0; x < winnerClamped.cols; ++x)
                {
                    int v = int(p[x]);
                    if (v < 0) v = 0;
                    if (v > N - 1) v = N - 1;
                    p[x] = uint16_t(v);
                }
            }
            if (G::FSLog) G::log(srcFun, "Clamped winner indices to [0..N-1]");
        }
    }

    // -----------------------------
    // Content check thresholds
    // -----------------------------
    // We will reject “background/black” overrides by checking luminance on the candidate slice.
    //
    // For 16U: threshold ~ 1% (655) is conservative.
    // For 8U: threshold ~ 3 is conservative.
    //
    // You can expose these in Options later; for now keep fixed.
    // const int    minValidPixels = 256; // minimum pixels that pass luminance check for a slice override to be applied
    // const double lumThresh8  = 3.0;
    // const double lumThresh16 = 655.0;
    const int    minValidPixels = 1;   // instead of 256
    const double lumThresh8  = 0.0;    // instead of 3.0
    const double lumThresh16 = 0.0;    // instead of 655.0
    // -----------------------------
    // Helper: compute luminance (8U) for a BGR image
    // -----------------------------
    auto computeLuma8U = [&](const cv::Mat &bgr8, cv::Mat &luma8)
    {
        CV_Assert(bgr8.type() == CV_8UC3);
        std::vector<cv::Mat> ch(3);
        cv::split(bgr8, ch); // B,G,R

        // Use integer-ish weights in float then convert back:
        // Y ≈ 0.114B + 0.587G + 0.299R
        cv::Mat y32 = 0.114f * ch[0] + 0.587f * ch[1] + 0.299f * ch[2];
        y32.convertTo(luma8, CV_8U);
    };

    // -----------------------------
    // For each slice, override only pixels that want that slice AND are “safe”
    // -----------------------------
    for (int i = 0; i < N; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

        // maskI = (winner == i) & actMask
        cv::Mat maskI = (winnerClamped == uint16_t(i));
        maskI.convertTo(maskI, CV_8U, 255.0);
        cv::bitwise_and(maskI, actMask, maskI);

        const int wanted = cv::countNonZero(maskI);
        if (wanted == 0)
            continue;

        if (G::FSLog) {
            G::log(srcFun, "Slice " + QString::number(i) +
                               " wanted pixels (pre-check) = " + QString::number(wanted));
        }

        // Load original slice
        FSLoader::Image img = FSLoader::load(inputPaths[i].toStdString());

        // Warp to aligned space using global transform
        cv::Mat alignedColor;
        FSAlign::applyTransform(img.color, globals[i].transform, alignedColor);

        // Apply contrast/WB in 8-bit space
        cv::Mat aligned8;
        if (alignedColor.depth() == CV_16U)
            alignedColor.convertTo(aligned8, CV_8UC3, 255.0 / 65535.0);
        else
            aligned8 = alignedColor; // view

        FSAlign::applyContrastWhiteBalance(aligned8, globals[i]); // 8U safe

        // Convert back to output depth (to match paddedColorOut)
        cv::Mat alignedCorrected;
        if (paddedColorOut.depth() == CV_16U)
            aligned8.convertTo(alignedCorrected, CV_16UC3, 65535.0 / 255.0);
        else
            alignedCorrected = aligned8; // 8U

        // Pad to wavelet padSize (must match pad used in fusion)
        cv::Size paddedSizeDummy;
        cv::Mat colorP = padForWavelet(alignedCorrected, paddedSizeDummy);

        if (colorP.size() != paddedColorOut.size()) {
            qWarning() << "WARNING:" << srcFun << "colorP.size != padSize for slice" << i
                       << " got=" ;//<< colorP.size() << " expected=" << paddedColorOut.size();
            return false;
        }

        // Ensure type matches output
        if (colorP.type() != paddedColorOut.type())
        {
            if (paddedColorOut.type() == CV_8UC3 && colorP.type() == CV_16UC3)
                colorP.convertTo(colorP, CV_8UC3, 255.0 / 65535.0);
            else if (paddedColorOut.type() == CV_16UC3 && colorP.type() == CV_8UC3)
                colorP.convertTo(colorP, CV_16UC3, 65535.0 / 255.0);
            else {
                qWarning() << "WARNING:" << srcFun << "unsupported color type conversion";
                return false;
            }
        }

        // -----------------------------
        // Content check:
        //   Keep only pixels where candidate slice is “not black”
        // -----------------------------
        cv::Mat validMask = maskI.clone(); // will be reduced

        if (paddedColorOut.depth() == CV_16U)
        {
            // Compute simple luminance proxy: max(B,G,R) is cheap & robust in 16U
            // valid if maxChan > lumThresh16
            std::vector<cv::Mat> ch(3);
            cv::split(colorP, ch);

            cv::Mat max01, maxAll;
            cv::max(ch[0], ch[1], max01);
            cv::max(max01, ch[2], maxAll);           // CV_16U

            cv::Mat bright = (maxAll > uint16_t(lumThresh16));
            bright.convertTo(bright, CV_8U, 255.0);

            cv::bitwise_and(validMask, bright, validMask);
        }
        else
        {
            // 8U: compute luma and require luma > lumThresh8
            cv::Mat luma8;
            computeLuma8U(colorP, luma8);
            cv::Mat bright = (luma8 > uint8_t(lumThresh8));
            bright.convertTo(bright, CV_8U, 255.0);

            cv::bitwise_and(validMask, bright, validMask);
        }

        const int accepted = cv::countNonZero(validMask);
        if (accepted < minValidPixels)
        {
            // Too few pixels look non-black in this slice; skip override.
            if (G::FSLog) {
                G::log(srcFun, "Slice " + QString::number(i) +
                                   " skipped (accepted=" + QString::number(accepted) +
                                   " < " + QString::number(minValidPixels) + ")");
            }
            continue;
        }

        // -----------------------------
        // Apply override (only validMask pixels)
        // -----------------------------
        colorP.copyTo(paddedColorOut, validMask);

        if (G::FSLog) {
            G::log(srcFun, "Slice " + QString::number(i) +
                               " override applied accepted=" + QString::number(accepted));
        }
    }

    return true;
}

//--------------------------------------------------------------
// Public entry point
//--------------------------------------------------------------
bool FSFusion::fuseStack(const std::vector<cv::Mat> &grayImgs,
                         const std::vector<cv::Mat> &colorImgs,
                         const Options              &opt,
                         cv::Mat                    &depthIndex16,
                         cv::Mat                    &outputColor8,
                         std::atomic_bool           *abortFlag,
                         StatusCallback              statusCallback,
                         ProgressCallback            progressCallback)
{
    QString srcFun = "FSFusion::fuseStack";

    const QString method = opt.method.trimmed();
    if (G::FSLog) G::log(srcFun, "Method = " + method);

    if (method == "Simple")     // uses depthIndex16
    {
        return fuseSimple(grayImgs,
                          colorImgs,
                          depthIndex16,
                          outputColor8,
                          abortFlag,
                          progressCallback);
    }

    // Default: full PMax fusion
    if (method == "FullWaveletMerge")       // full wavelet merge
    return fusePMax(grayImgs,
                    colorImgs,
                    opt,
                    depthIndex16,
                    outputColor8,
                    abortFlag,
                    statusCallback,
                    progressCallback);

    qWarning() << "WARNING:" << srcFun << "Invalid method =" << method;
    return false;
}

// StreamPMax pipeline
bool FSFusion::streamPMaxSlice(int slice,
                               const cv::Mat      &grayImg,
                               const cv::Mat      &colorImg,
                               const Options      &opt,
                               std::atomic_bool   *abortFlag,
                               StatusCallback     statusCallback,
                               ProgressCallback   progressCallback
                               )
{
    QString srcFun = "FSFusion::fusePMaxSlice";
    QString s = QString::number(slice);
    QString msg = "Fusing slice " + s;
    msg += "  method: " + opt.method;
    msg += "  mergeMode: " + opt.mergeMode;
    msg += "  winnerMap: " + opt.winnerMap;
    if (G::FSLog) G::log(srcFun, msg);

    msg = "DepthBiasedErosion: enableDepthBiasedErosion = " +
          QVariant(opt.enableDepthBiasedErosion).toString();
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);

    // Validate sizes and types
    if (grayImg.empty() || colorImg.empty()) {
        QString msg = "Slice " + s + " grayImg.empty() || colorImg.empty()";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (slice == 0) {
        alignSize = grayImg.size();
        outDepth = colorImg.depth();
    }
    else if (grayImg.size() != alignSize) {
        QString msg = "Slice " + s + " grayImg.size() != orig";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }
    else if (colorImg.size() != alignSize) {
        QString msg = "Slice " + s + " colorImg.size() != orig";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }
    else if (colorImg.depth() != outDepth) {
        qWarning() << "WARNING:" << srcFun << "Color depth changed across slices";
        return false;
    }

    // --------------------------------------------------------------------
    // Pad grayscale + color images BEFORE processing (wavelet-friendly)
    // --------------------------------------------------------------------
    if (G::FSLog) G::log(srcFun, "Pad for wavelet");

    cv::Mat grayP;
    cv::Mat colorP;
    cv::Size paddedSize;

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    grayP  = padForWavelet(grayImg, paddedSize);
    colorP = padForWavelet(colorImg, paddedSize);

    // Lock padded size on slice 0; enforce identical thereafter
    if (slice == 0)
        padSize = paddedSize;
    else if (paddedSize != padSize)
    {
        QString msg = "Slice " + s + " paddedSize != ps";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    // Sanity Check
    // /*
    if (slice > 0 && paddedSize != padSize)
    {
        QString m = QString("Sanity: slice %1 paddedSize %2x%3 != ps %4x%5")
            .arg(slice)
            .arg(paddedSize.width).arg(paddedSize.height)
            .arg(padSize.width).arg(padSize.height);
        if (G::FSLog) G::log(srcFun, m);
        qWarning().noquote() << "WARNING:" << srcFun << m;
        return false;
    }
    //*/

    // Init builders/state on slice 0 (once)
    if (slice == 0)
    {
        colorBuilder.begin(grayP.size(), /*fixedCapPerPixel=*/4);
        mergeState.reset();
        mergedWavelet.release();
        wavelet.release();
    }

    // --------------------------------------------------------------------
    // Forward wavelet per slice (grayscale only)
    // --------------------------------------------------------------------

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    msg = "Forward wavelet slice " + s;
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    if (!FSFusionWavelet::forward(grayP, opt.useOpenCL, wavelet))
    {
        QString msg = "Slice " + s + " Wavelet forward failed";
        if (G::FSLog) G::log(srcFun, msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    // Lock wavelet size on slice 0; enforce identical thereafter
    if (slice == 0)
        waveletSize = wavelet.size();
    // Sanity Check
    // /*
    if (slice > 0 && wavelet.size() != waveletSize)
    {
        QString m = QString("Sanity: slice %1 wavelet.size %2x%3 != waveletSize %4x%5")
        .arg(slice)
            .arg(wavelet.cols).arg(wavelet.rows)
            .arg(waveletSize.width).arg(waveletSize.height);
        if (G::FSLog) G::log(srcFun, m);
        qWarning().noquote() << "WARNING:" << srcFun << m;
        return false;
    }

    auto matInfo = [](const cv::Mat& m) -> QString {
        return QString("size=%1x%2 type=%3 channels=%4 step=%5")
        .arg(m.cols).arg(m.rows)
            .arg(m.type())
            .arg(m.channels())
            .arg(static_cast<qulonglong>(m.step));
    };

    if (slice == 0)
    {
        QString msg =
            "Sanity(slice0): "
            "orig=" + QString("%1x%2").arg(alignSize.width).arg(alignSize.height) + " "
            "ps="   + QString("%1x%2").arg(padSize.width).arg(padSize.height) + " "
            "waveletSize=" +
            QString("%1x%2").arg(waveletSize.width).arg(waveletSize.height) +
            " | "
            "colorImg(" + matInfo(colorImg) + ") "
            "grayP("    + matInfo(grayP)    + ") "
            "colorP("   + matInfo(colorP)   + ") "
            "wavelet("  + matInfo(wavelet) + ")";

        if (G::FSLog) G::log(srcFun, msg);
        // qDebug().noquote() << srcFun << log;
    }
    //*/

    if (wavelet.size() != waveletSize)
    {
        QString msg = "Slice " + s + " wavelet.size() != waveletSize";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    // --------------------------------------------------------------------
    // Merge wavelet stacks → mergedWavelet (we ignore depthIndex here)
    // --------------------------------------------------------------------
    msg = "Merge wavelet stacks.  mergeMode: " + opt.mergeMode;
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    if (opt.mergeMode == "PMax") {
        if (!FSMerge::mergeSlice(mergeState,
                                 wavelet,
                                 waveletSize,
                                 opt.consistency,
                                 abortFlag,
                                 mergedWavelet))
        {
            QString msg = "Slice " + s + " FSMerge::mergeSlice PMax failed.";
            qWarning() << "WARNING:" << srcFun << msg;
            if (G::FSLog) G::log(srcFun, msg);
            return false;
        }
    }
    else if (opt.mergeMode == "Weighted") {
        FSMerge::WeightedParams wp;
        wp.power = opt.weightedPower;
        wp.sigma0 = opt.weightedSigma0;
        wp.includeLowpass = opt.weightedIncludeLowpass;
        wp.epsEnergy = opt.weightedEpsEnergy;
        wp.epsWeight = opt.weightedEpsWeight;
        if (!FSMerge::mergeSliceWeighted(mergeState,
                                         wavelet,
                                         waveletSize,
                                         wp,
                                         opt.consistency,
                                         abortFlag,
                                         mergedWavelet))
        {
            QString msg = "Slice " + s + " FSMerge::mergeSliceWeighted Weighted failed.";
            qWarning() << "WARNING:" << srcFun << msg;
            if (G::FSLog) G::log(srcFun, msg);
            return false;
        }
    }

    // --------------------------------------------------------------------
    // Build color map using padded grayscale + padded RGB images
    // --------------------------------------------------------------------
    msg = "Build color map";
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);

    FSFusionReassign::ColorEntry colorEntry;
    if (!colorBuilder.addSlice(grayP, colorP))
    {
        QString msg = "Slice " + s + "FSFusionReassign::addSlice failed.";
        if (G::FSLog) G::log(srcFun, msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // if (progressCallback) progressCallback();
    return true;
}

// StreamPMax pipeline
bool FSFusion::streamPMaxFinish(
    cv::Mat &outputColor,
    const Options &opt,
    cv::Mat &depthIndex16,
    const QStringList &inputPaths,
    const std::vector<FSAlign::Result> &globals,
    std::atomic_bool *abortFlag,
    StatusCallback statusCallback,
    ProgressCallback progressCallback)
{
    QString srcFun = "FSFusion::streamPMaxFinish";
    QString msg = "Finishing Fusion";
    msg = "DepthBiasedErosion: enableDepthBiasedErosion = " +
          QVariant(opt.enableDepthBiasedErosion).toString();
    if (statusCallback) statusCallback(msg);

    // --------------------------------------------------------------------
    // Finish merge after all slices processed and before invert
    // --------------------------------------------------------------------
    msg = "Finish merge after last slice";
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    if (opt.mergeMode == "PMax") {
        if (!FSMerge::mergeSliceFinish(mergeState,
                                       opt.consistency,
                                       abortFlag,
                                       mergedWavelet,
                                       depthIndexPadded16))
        {
            return false;
        }
    }
    else if (opt.mergeMode == "Weighted") {
        FSMerge::WeightedParams wp;
        wp.power = opt.weightedPower;
        wp.sigma0 = opt.weightedSigma0;
        wp.includeLowpass = opt.weightedIncludeLowpass;
        wp.epsEnergy = opt.weightedEpsEnergy;
        wp.epsWeight = opt.weightedEpsWeight;

        cv::Mat weightedWinnerPadded16;
        cv::Mat energyWinnerPadded16;

        if (!FSMerge::mergeSliceFinishWeighted(mergeState,
                                               wp,
                                               opt.consistency,
                                               abortFlag,
                                               mergedWavelet,
                                               weightedWinnerPadded16,
                                               energyWinnerPadded16))
        {
            return false;
        }

        // Choose which map becomes "the" depthIndexPadded16 used downstream
        if (G::FSLog) G::log(srcFun, "winnerMap = " + opt.winnerMap);
        if (opt.winnerMap == "Energy")
            depthIndexPadded16 = energyWinnerPadded16;
        else
            depthIndexPadded16 = weightedWinnerPadded16;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // Inverse wavelet → fusedGray (still padded size)
    // --------------------------------------------------------------------
    msg = "Inverse wavelet";
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    // cv::Mat fusedGray; // local, then ensure CV_8U
    if (!FSFusionWavelet::inverse(mergedWavelet, opt.useOpenCL, fusedGray8))
    {
        if (G::FSLog) G::log(srcFun, "FSFusionWavelet::inverse failed");
        return false;
    }

    if (fusedGray8.type() != CV_8U) {
        msg = "Inverse wavelet: fusedGray8.type() != CV_8U";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }
    if (fusedGray8.size() != padSize) {
        msg = "Inverse wavelet: fusedGray8.size() != ps";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // Depth-Biased Erosion: refines the winner map
    // --------------------------------------------------------------------
    msg = "DepthBiasedErosion: enableDepthBiasedErosion = " +
          QVariant(opt.enableDepthBiasedErosion).toString();

    if (G::FSLog) G::log(srcFun, msg);
    cv::Mat erodedWinnerPadded16;
    cv::Mat winnerBefore;
    cv::Mat winnerAfter;
    cv::Mat edgeMask8U;

    if (opt.enableDepthBiasedErosion)
    {
        msg = "DepthBiasedErosion:"
            " erosionEdgeSigma = " + QVariant(opt.erosionEdgeSigma).toString() +
            " erosionEdgeThresh = " + QVariant(opt.erosionEdgeThresh).toString() +
            " erosionEdgeThresh = " + QVariant(opt.erosionEdgeDilate).toString()
            ;
        if (G::FSLog) G::log(srcFun, msg);
        winnerBefore = depthIndexPadded16.clone();

        edgeMask8U = makeEdgeMask8U(fusedGray8,
                                            opt.erosionEdgeSigma,
                                            opt.erosionEdgeThresh,
                                            opt.erosionEdgeDilate);

        winnerAfter = depthBiasedErode16(winnerBefore,
                                                 edgeMask8U,
                                                 opt.erosionRadius,
                                                 opt.erosionIters,
                                                 opt.erosionMaxDelta,
                                                 opt.erosionMinEdgeDelta);

        int changed = cv::countNonZero(winnerAfter != winnerBefore);
        msg = "DepthBiasedErosion changed pixels = " + QString::number(changed);
        if (G::FSLog) G::log(srcFun, msg);

        depthIndexPadded16 = winnerAfter;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // Finish color builder after all slices processed
    // --------------------------------------------------------------------
    msg = "Finish color builder";
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    colorBuilder.finish(colorEntries, counts);

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // --------------------------------------------------------------------
    // Apply color reassignment to padded fused grayscale → paddedColorOut
    // --------------------------------------------------------------------
    msg = "Apply color reassignment";
    if (G::FSLog) G::log(srcFun, msg);
    // if (statusCallback) statusCallback(msg);
    // if (progressCallback) progressCallback();

    cv::Mat paddedColorOut;
    if (!FSFusionReassign::applyColorMap(fusedGray8, colorEntries,
                                         counts, paddedColorOut,
                                         reassignDepth(outDepth)))
    {
        msg = "FSFusionReassign::applyColorMap failed";
        if (G::FSLog) G::log(srcFun, msg);
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (paddedColorOut.size() != padSize) {
        msg = "Apply color map: paddedColorOut.size() != ps";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    const int expectedType = (outDepth == CV_16U) ? CV_16UC3 : CV_8UC3;
    if (paddedColorOut.type() != expectedType) {
        msg = "Apply color map: paddedColorOut.type() mismatch";
        qWarning() << "WARNING:" << srcFun << msg
                   << " got =" << paddedColorOut.type()
                   << " expected=" << expectedType;
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // ------------------------------------------------------------
    // Apply Depth-Biased Erosion
    // ------------------------------------------------------------
    if (opt.enableDepthBiasedErosion)
    {
        // Recompute edge mask (or reuse the one you already computed earlier)
        cv::Mat edgeMask8U = makeEdgeMask8U(fusedGray8,
                                            opt.erosionEdgeSigma,
                                            opt.erosionEdgeThresh,
                                            opt.erosionEdgeDilate);

        // depthIndexPadded16 already replaced by winnerAfter earlier in your code
        if (!applyDepthBiasedColorOverrideSecondPass(
                paddedColorOut,
                winnerBefore,          // <-- before DBE
                depthIndexPadded16,    // <-- after DBE (you set it earlier)
                edgeMask8U,
                inputPaths,
                globals,
                opt,
                abortFlag))
        {
            qWarning() << "WARNING:" << srcFun << "applyDepthBiasedColorOverrideSecondPass failed";
            return false;
        }
    }

    // --------------------------------------------------------------------
    // Crop back to original (non-padded) size
    //   Step 1: padSize   -> alignSize
    //   Step 2: alignSize -> origSize (validAreaAlign)
    // --------------------------------------------------------------------
    msg = "Crop back to original size";
    msg += " origSize=" + FSUtilities::cvSizeToText(origSize) +
           " alignSize=" + FSUtilities::cvSizeToText(alignSize) +
           " padSize=" + FSUtilities::cvSizeToText(padSize) +
           " validAreaAlign=(" +
           QString::number(validAreaAlign.x) + "," +
           QString::number(validAreaAlign.y) + "," +
           QString::number(validAreaAlign.width) + "," +
           QString::number(validAreaAlign.height) + ")";
    if (G::FSLog) G::log(srcFun, msg);

    // Sanity: validAreaAlign must describe origSize inside alignSize
    if (validAreaAlign.width  != origSize.width ||
        validAreaAlign.height != origSize.height)
    {
        qWarning() << "WARNING:" << srcFun
                   << "validAreaAlign does not match origSize";
        return false;
    }

    // ------------------------------------------------------------
    // 1) Undo FSFusion padding: padSize -> alignSize
    // ------------------------------------------------------------
    cv::Rect roiPadToAlign(0, 0, alignSize.width, alignSize.height);

    if (padSize != alignSize)
    {
        const int padW = padSize.width  - alignSize.width;
        const int padH = padSize.height - alignSize.height;

        if (padW < 0 || padH < 0)
        {
            qWarning() << "WARNING:" << srcFun
                       << "padSize smaller than alignSize";
            return false;
        }

        const int left = padW / 2;
        const int top  = padH / 2;

        roiPadToAlign = cv::Rect(left, top,
                                 alignSize.width,
                                 alignSize.height);
    }

    // Bounds check
    if (roiPadToAlign.x < 0 || roiPadToAlign.y < 0 ||
        roiPadToAlign.x + roiPadToAlign.width  > paddedColorOut.cols ||
        roiPadToAlign.y + roiPadToAlign.height > paddedColorOut.rows ||
        roiPadToAlign.x + roiPadToAlign.width  > depthIndexPadded16.cols ||
        roiPadToAlign.y + roiPadToAlign.height > depthIndexPadded16.rows)
    {
        qWarning() << "WARNING:" << srcFun
                   << "roiPadToAlign out of bounds";
        return false;
    }

    // Views at alignSize
    cv::Mat colorAlign = paddedColorOut(roiPadToAlign);
    cv::Mat depthAlign = depthIndexPadded16(roiPadToAlign);

    // ------------------------------------------------------------
    // 2) Undo FSLoader padding: alignSize -> origSize
    // ------------------------------------------------------------
    if (validAreaAlign.x < 0 || validAreaAlign.y < 0 ||
        validAreaAlign.x + validAreaAlign.width  > colorAlign.cols ||
        validAreaAlign.y + validAreaAlign.height > colorAlign.rows ||
        validAreaAlign.x + validAreaAlign.width  > depthAlign.cols ||
        validAreaAlign.y + validAreaAlign.height > depthAlign.rows)
    {
        qWarning() << "WARNING:" << srcFun
                   << "validAreaAlign out of bounds";
        return false;
    }

    // Final outputs (origSize)
    outputColor  = colorAlign(validAreaAlign).clone();
    depthIndex16 = depthAlign(validAreaAlign).clone();

    // debug log the min/max of the merged wavelet magnitude to chk not empty
    std::vector<cv::Mat> ch(2);
    cv::split(mergedWavelet, ch);
    cv::Mat mag;
    cv::magnitude(ch[0], ch[1], mag);
    double mn=0, mx=0;
    cv::minMaxLoc(mag, &mn, &mx);
    qDebug() << "mergedWavelet mag min/max =" << mn << mx;

    //halo detection tuning
    HaloDetectParams hp;
    hp.edgeSigma      = 1.0f;
    hp.edgeThreshFrac = 0.05f;   // slightly more sensitive than 0.06
    hp.edgeDilatePx   = 2;

    hp.maxEdgeDistPx  = 30;      // allow wider haze halos

    hp.dogScales = {
        {4.0f,  8.0f},
        {8.0f, 16.0f},
        {16.0f, 32.0f}
    };

    hp.dogThresh  = 0.015f;      // start slightly lower
    hp.minAreaPx  = 128;
    hp.closePx    = 3;

    cv::Mat fusedGray8Orig = fusedGray8(roiPadToAlign)(validAreaAlign).clone();

    HaloDetectResult haloDetectResult;
    haloDetectResult = detectLowpassHalosOrig(fusedGray8Orig, hp);

    dumpHaloDetectorDebugOrig(opt.depthFolderPath,
                              fusedGray8Orig,
                              hp,
                              haloDetectResult
                              );

    // dumpHaloDoGOverlayOrigSize(opt.depthFolderPath,
    //                            fusedGray8Orig,
    //                            /*sigmaNear*/ 6.0f,
    //                            /*sigmaFar */ 24.0f,
    //                            /*outsidePx*/ 20,
    //                            /*dogThresh*/ 0.02f);

    // ------------------------------------------------------------
    // Housekeeping
    // ------------------------------------------------------------
    colorEntries.clear();
    counts.clear();
    mergedWavelet.release();
    wavelet.release();
    mergeState.reset();         // also clears cached wavelets
    colorBuilder.reset();

    // if (progressCallback) progressCallback();

    return true;
}

// end StreamPMax pipeline

