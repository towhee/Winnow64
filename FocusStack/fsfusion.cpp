#include "fsfusion.h"
#include "Main/global.h"
#include "fsloader.h"
#include "fsfusionwavelet.h"
#include "fsutilities.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cassert>

#include <QString>

//--------------------------------------------------------------
// Helpers
//--------------------------------------------------------------
namespace
{

using ProgressCallback = FSFusion::ProgressCallback;

struct DMapPass2
{
    bool initialized = false;
    int  sliceCount = 0;
    cv::Size origSize;

    // Final result (orig size)
    cv::Mat outColor;       // CV_8UC3 or CV_16UC3 depending on your pipeline choice

    // If you want: keep a float accumulation buffer for simple weighted blend
    // (useful as an intermediate before pyramid blending is implemented)
    cv::Mat accum32;        // CV_32FC3
    cv::Mat accumW32;       // CV_32F

    void begin(cv::Size orig, int outType)
    {
        initialized = true;
        sliceCount = 0;
        origSize = orig;
        outColor = cv::Mat(orig, outType, cv::Scalar::all(0));
        accum32.release();
        accumW32.release();
    }

    void reset()
    {
        initialized = false;
        sliceCount = 0;
        origSize = {};
        outColor.release();
        accum32.release();
        accumW32.release();
    }
};

static inline int clampi(int v, int lo, int hi)
{
    return std::max(lo, std::min(hi, v));
}

static inline int resolveLevelParam(int param, int levels, int defaultIfNegOne)
{
    // param == -1 means "auto default", otherwise clamp
    if (param < 0) return clampi(defaultIfNegOne, 0, std::max(0, levels - 1));
    return clampi(param, 0, std::max(0, levels - 1));
}

static void dmapDiagnosticsDerived(const FSFusion::DMapParams& p,
                                   const cv::Size& origSz,
                                   int N,
                                   int levels)
{
    qDebug().noquote() << "====== DMapParams (derived) ======";

    qDebug().noquote()
        << "origSz =" << origSz.width << "x" << origSz.height
        << "N slices =" << N
        << "levels =" << levels;

    // --- Resolve key level-dependent params exactly like your code intends ---
    // Your convention has been:
    //   - hardFromLevel = -1 => levels-2 (last 2 levels hard)
    //   - vetoFromLevel = -1 => levels-2 (or residual-only depending on how you set it)
    const int hardFromResolved = resolveLevelParam(
        p.hardFromLevel, levels,
        /*defaultIfNegOne=*/std::max(0, levels - 2)
        );

    const int vetoFromResolved = resolveLevelParam(
        p.vetoFromLevel, levels,
        /*defaultIfNegOne=*/std::max(0, levels - 2)
        );

    qDebug().noquote()
        << "hardFromLevel raw/resolved =" << p.hardFromLevel << "/" << hardFromResolved
        << "=> hard levels:" << hardFromResolved << ".." << (levels - 1);

    qDebug().noquote()
        << "vetoFromLevel raw/resolved =" << p.vetoFromLevel << "/" << vetoFromResolved
        << "=> veto levels:" << vetoFromResolved << ".." << (levels - 1);

    // --- Effective behavior toggles (the combinations that matter) ---
    const bool softWeightsEnabled = (p.topK >= 2);
    const bool anyBandSmoothing = (p.enableGaussianBandSmoothing || p.enableEdgeAwareBandSmoothing);

    qDebug().noquote()
        << "softWeightsEnabled (topK>=2) =" << softWeightsEnabled
        << "anyBandSmoothing =" << anyBandSmoothing;

    qDebug().noquote()
        << "EFFECTIVE: hardWeightsOnLowpass ="
        << (p.enableHardWeightsOnLowpass ? "ON" : "OFF")
        << "from level" << hardFromResolved;

    qDebug().noquote()
        << "EFFECTIVE: depthGradVeto ="
        << (p.enableDepthGradLowpassVeto ? "ON" : "OFF")
        << "from level" << vetoFromResolved
        << "strength =" << p.vetoStrength
        << "depthGradThresh =" << p.depthGradThresh
        << "vetoDilatePx =" << p.vetoDilatePx;

    // --- Sanity warnings that catch bad regimes early ---
    if (p.enableGaussianBandSmoothing && p.enableEdgeAwareBandSmoothing)
    {
        qDebug().noquote() << "WARNING: Both GaussianBandSmoothing and EdgeAwareBandSmoothing are ON.";
    }
    if (!softWeightsEnabled && (p.enableGaussianBandSmoothing || p.enableEdgeAwareBandSmoothing))
    {
        qDebug().noquote() << "WARNING: Band smoothing is ON but topK < 2 (no TopK weights to smooth).";
    }
    if (p.softTemp <= 0.0f)
    {
        qDebug().noquote() << "WARNING: softTemp <= 0. This will break softmax-like weighting.";
    }
    if (p.confLow > p.confHigh)
    {
        qDebug().noquote() << "WARNING: confLow > confHigh (thresholds inverted).";
    }
    if (p.bandDilatePx < 0 || p.uncertaintyDilatePx < 0)
    {
        qDebug().noquote() << "WARNING: bandDilatePx or uncertaintyDilatePx is negative.";
    }

    // --- Background override regime ---
    qDebug().noquote()
        << "bgConfThr =" << p.bgConfThr
        << "bgScoreThr =" << p.bgScoreThr
        << "(bg forcing requires BOTH low texture + low score in your current pipeline)";

    // --- Lowpass equalization regime ---
    qDebug().noquote()
        << "lowpassEq enabled =" << p.enableLowpassEqualization
        << "minPixels =" << p.lowpassEqMinPixels
        << "bandVetoPx =" << p.lowpassEqBandVetoPx
        << "sigma =" << p.lowpassEqSigma
        << "strength =" << p.lowpassEqStrength;

    // --- Edge-aware smoothing “shape” summary ---
    qDebug().noquote()
        << "edgeAware iters/radius =" << p.eaIters << "/" << p.eaRadius
        << "sigmaSpatial =" << p.eaSigmaSpatial
        << "sigmaRange01 =" << p.eaSigmaRange01;

    // --- Weight floors (these can quietly ruin blending if non-zero) ---
    qDebug().noquote()
        << "weight floors:"
        << "wMin =" << p.wMin
        << "wMinOutsideBand =" << p.wMinOutsideBand
        << "wMinInBand =" << p.wMinInBand;

    // --- Tiling: derived tile grid ---
    if (p.useTiling)
    {
        const int tilePx = std::max(1, p.tilePx);
        const int overlap = std::max(0, p.tileOverlapPx);
        const int step = std::max(1, tilePx - overlap);

        const int tilesX = (origSz.width  <= tilePx) ? 1 : (1 + (origSz.width  - tilePx + step - 1) / step);
        const int tilesY = (origSz.height <= tilePx) ? 1 : (1 + (origSz.height - tilePx + step - 1) / step);

        qDebug().noquote()
            << "tiling: tilePx =" << tilePx
            << "overlapPx =" << overlap
            << "step =" << step
            << "grid =" << tilesX << "x" << tilesY
            << "tiles total =" << (tilesX * tilesY);
    }
    else
    {
        qDebug().noquote() << "tiling: OFF";
    }

    qDebug().noquote() << "=================================";
}

static void dmapDiagnostics(const FSFusion::DMapParams& p)
{
    qDebug().noquote() << "========== DMapParams ==========";

    // --- Global switches ---
    qDebug().noquote()
        << "enableHardWeightsOnLowpass =" << p.enableHardWeightsOnLowpass
        << "enableGaussianBandSmoothing =" << p.enableGaussianBandSmoothing
        << "enableEdgeAwareBandSmoothing =" << p.enableEdgeAwareBandSmoothing
        << "enableLowpassEqualization =" << p.enableLowpassEqualization
        << "enableConfidenceGating =" << p.enableConfidenceGating
        << "enableDepthGradLowpassVeto =" << p.enableDepthGradLowpassVeto;

    // --- Pass-1: Top-K / focus scoring ---
    qDebug().noquote()
        << "topK =" << p.topK
        << "scoreSigma =" << p.scoreSigma
        << "scoreKSize =" << p.scoreKSize
        << "scoreEps =" << p.scoreEps;

    // --- Pass-2: soft weights ---
    qDebug().noquote()
        << "softTemp =" << p.softTemp
        << "wMin =" << p.wMin;

    // --- Background mask ---
    qDebug().noquote()
        << "bgConfThr =" << p.bgConfThr
        << "bgScoreThr =" << p.bgScoreThr;

    // --- Pyramid / hardening ---
    qDebug().noquote()
        << "hardFromLevel =" << p.hardFromLevel
        << "pyrLevels =" << p.pyrLevels;

    // --- Lowpass equalization ---
    qDebug().noquote()
        << "lowpassEqMinPixels =" << p.lowpassEqMinPixels
        << "lowpassEqBandVetoPx =" << p.lowpassEqBandVetoPx
        << "lowpassEqSigma =" << p.lowpassEqSigma
        << "lowpassEqStrength =" << p.lowpassEqStrength;

    // --- Depth-gradient veto ---
    qDebug().noquote()
        << "vetoFromLevel =" << p.vetoFromLevel
        << "vetoStrength =" << p.vetoStrength
        << "depthGradThresh =" << p.depthGradThresh
        << "vetoDilatePx =" << p.vetoDilatePx;

    // --- Confidence ---
    qDebug().noquote()
        << "confEps =" << p.confEps
        << "confLow =" << p.confLow
        << "confHigh =" << p.confHigh;

    // --- Weight floor strategy ---
    qDebug().noquote()
        << "wMinOutsideBand =" << p.wMinOutsideBand
        << "wMinInBand =" << p.wMinInBand;

    // --- Edge-aware smoothing ---
    qDebug().noquote()
        << "eaIters =" << p.eaIters
        << "eaRadius =" << p.eaRadius
        << "eaSigmaSpatial =" << p.eaSigmaSpatial
        << "eaSigmaRange01 =" << p.eaSigmaRange01;

    // --- Bands / uncertainty ---
    qDebug().noquote()
        << "bandDilatePx =" << p.bandDilatePx
        << "uncertaintyDilatePx =" << p.uncertaintyDilatePx;

    // --- Weight smoothing ---
    qDebug().noquote()
        << "weightBlurPx =" << p.weightBlurPx
        << "weightBlurSigma =" << p.weightBlurSigma;

    // --- Tiling ---
    qDebug().noquote()
        << "useTiling =" << p.useTiling
        << "tilePx =" << p.tilePx
        << "tileOverlapPx =" << p.tileOverlapPx;

    qDebug().noquote() << "================================";
}

static inline QString matInfo(const cv::Mat& m)
{
    return QString("size=%1x%2 type=%3 ch=%4 empty=%5")
    .arg(m.cols).arg(m.rows)
        .arg(m.type())
        .arg(m.channels())
        .arg(m.empty());
}

static inline void assertSameMat(const cv::Mat& a, const cv::Mat& b, const char* where)
{
    if (a.size() != b.size() || a.type() != b.type())
    {
        qWarning().noquote()
        << "MAT MISMATCH at" << where
        << "A:" << matInfo(a)
        << "B:" << matInfo(b);
        CV_Assert(a.size() == b.size());
        CV_Assert(a.type() == b.type());
    }
}

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

static cv::Mat modeFilterIdx16(
    const cv::Mat& idx16,
    const cv::Mat& mask8,
    int radius)
{
    CV_Assert(idx16.type() == CV_16U);
    CV_Assert(mask8.type() == CV_8U);
    CV_Assert(idx16.size() == mask8.size());

    cv::Mat out = idx16.clone();
    const int R = radius;

    for (int y = 0; y < idx16.rows; ++y)
    {
        for (int x = 0; x < idx16.cols; ++x)
        {
            if (!mask8.at<uint8_t>(y, x))
                continue;

            std::unordered_map<uint16_t, int> hist;

            for (int dy = -R; dy <= R; ++dy)
            {
                int yy = y + dy;
                if ((unsigned)yy >= (unsigned)idx16.rows) continue;

                for (int dx = -R; dx <= R; ++dx)
                {
                    int xx = x + dx;
                    if ((unsigned)xx >= (unsigned)idx16.cols) continue;

                    uint16_t v = idx16.at<uint16_t>(yy, xx);
                    hist[v]++;
                }
            }

            uint16_t best = idx16.at<uint16_t>(y, x);
            int bestCount = 0;

            for (auto& kv : hist)
            {
                if (kv.second > bestCount)
                {
                    best = kv.first;
                    bestCount = kv.second;
                }
            }

            out.at<uint16_t>(y, x) = best;
        }
    }

    return out;
}

static cv::Mat boundaryThinFromIdx16(const cv::Mat& idx16)
{
    CV_Assert(idx16.type() == CV_16U);
    cv::Mat boundary(idx16.size(), CV_8U, cv::Scalar(0));

    // horizontal diffs
    cv::Mat a = idx16.colRange(0, idx16.cols - 1);
    cv::Mat b = idx16.colRange(1, idx16.cols);
    cv::Mat h = (a != b);
    h.copyTo(boundary.colRange(1, idx16.cols));

    // vertical diffs (OR in)
    cv::Mat c = idx16.rowRange(0, idx16.rows - 1);
    cv::Mat d = idx16.rowRange(1, idx16.rows);
    cv::Mat v = (c != d);
    cv::Mat dst = boundary.rowRange(1, idx16.rows);
    cv::bitwise_or(dst, v, dst);

    return boundary; // CV_8U 0/255
}

// -----------------------------------------------------------------------------
// Foreground mask (authoritative)
//
// Goal: produce a "do not touch" mask for halo replacement.
//   255 = FOREGROUND (protect)
//   0   = BACKGROUND (eligible)
//
// Inputs:
//   gray8Orig    : CV_8U 1-channel, original (cropped) fused grayscale
//   edgeMask8Orig: CV_8U 0/255, same size; edges you always want to protect
//
// Output (ForegroundResult):
//   fgMask8      : final 0/255 mask
//   hfMask8      : high-frequency texture/bark/filaments
//   thinMask8    : thin smooth twigs (small-scale gradient)
//   darkMask8    : solid dark interiors (edge-gated) for main branch core
// -----------------------------------------------------------------------------

struct ForegroundParams
{
    // Pre-smoothing for gradient-based detectors (reduces noise-triggered edges)
    float preBlurSigma   = 1.2f;   // 0.8..1.6

    // (A) HF texture mask (Laplacian magnitude)
    float hfFrac         = 0.015f;   // 0.010..0.030  (fraction of max |lap|)
    float hfBlurSigma    = 1.0f;     // 0.6..1.4

    // (B) Thin twigs mask (small-scale gradient magnitude)
    float thinFrac       = 0.035f;   // fraction of max grad (0.02..0.06)
    float thinMxClipFrac = 0.35f;    // clip max grad to reduce “hot pixel dominates” (0.2..0.6)
    float thinBlurSigma  = 0.0f;     // extra blur for twigs only (0..1.5)

    int   thinOpenPx     = 0;        // removes specks (0..1)
    int   thinClosePx    = 1;        // connects along twig (0..1)  (NOTE: close fattens)
    int   thinErodePx    = 1;        // **main thinning control** (0..2)
    int   thinMinAreaPx  = 32;       // remove tiny islands (0..128)

    // (C) Dark solid interiors (main branch core)
    float darkFrac       = 0.55f;   // relative to Otsu threshold (0.45..0.70)
    float darkBlurSigma  = 2.0f;    // blur for Otsu/dark detector (1.5..3.0)
    int   darkClosePx    = 8;       // fill interior (6..12)
    int   darkMinAreaPx  = 256;     // remove dark blobs (128..1024)
    int   darkEdgeGatePx = 6;       // gate dark blobs near edges (4..10), 0 disables

    // Edge protection (always OR’d in)
    int   edgeDilatePx   = 0;       // 0..2 (usually 0)

    // Final expansion of FG (usually 0; keep 0 to avoid fattening)
    int   finalDilatePx  = 0;       // 0..1
};

struct ForegroundResult
{
    cv::Mat fgMask8;    // CV_8U 0/255 final
    cv::Mat hfMask8;    // CV_8U 0/255
    cv::Mat thinMask8;  // CV_8U 0/255
    cv::Mat darkMask8;  // CV_8U 0/255
    cv::Mat edgeMask8;  // CV_8U 0/255
};

static inline cv::Mat seEllipse(int px)
{
    int k = 2 * px + 1;
    return cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
}

// Gaussian blur with "px or sigma" convenience
static inline void gaussianBlurInPlace(cv::Mat& m32, float sigma)
{
    if (sigma <= 0.0f) return;
    cv::GaussianBlur(m32, m32, cv::Size(0,0), sigma, sigma, cv::BORDER_REFLECT);
}

static cv::Mat filterMinArea8U(const cv::Mat& mask8, int minAreaPx)
{
    CV_Assert(mask8.type() == CV_8U);
    if (minAreaPx <= 0) return mask8.clone();

    cv::Mat labels, stats, cents;
    int n = cv::connectedComponentsWithStats(mask8, labels, stats, cents, 8, CV_32S);

    cv::Mat out = cv::Mat::zeros(mask8.size(), CV_8U);
    for (int i = 1; i < n; ++i)
    {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area >= minAreaPx)
            out.setTo(255, labels == i);
    }
    return out;
}

// Fill holes in a 0/255 mask: flood fill background, invert.
static void fillHoles8U(cv::Mat& mask8)
{
    CV_Assert(mask8.type() == CV_8U);

    cv::Mat ff = mask8.clone();
    cv::copyMakeBorder(ff, ff, 1, 1, 1, 1, cv::BORDER_CONSTANT, cv::Scalar(0));
    cv::floodFill(ff, cv::Point(0, 0), cv::Scalar(255));

    ff = ff(cv::Rect(1, 1, mask8.cols, mask8.rows));
    cv::Mat holes = (ff == 0);         // holes become true
    holes.convertTo(holes, CV_8U, 255.0);

    cv::bitwise_or(mask8, holes, mask8);
}

// --------- Start DMAP functions ----------

// -----------------------------------------------------------------------------
// updateTopKPerSlice
//   Updates TopKMaps in-place given per-pixel score for the current slice.
//   - topk.idx16[k]   : CV_16U, orig size
//   - topk.score32[k] : CV_32F, orig size
//   - score32This     : CV_32F, orig size
//   - sliceIndex      : current slice id (0..N-1)
//
// TopK is maintained as descending score: k=0 is best.
// -----------------------------------------------------------------------------
static void updateTopKPerSlice(FSFusion::TopKMaps& topk,
                               const cv::Mat& score32This,
                               uint16_t sliceIndex)
{
    CV_Assert(topk.K > 0);
    CV_Assert(!score32This.empty());
    CV_Assert(score32This.type() == CV_32F);
    CV_Assert(score32This.size() == topk.sz);

    const int K = topk.K;
    const int rows = topk.sz.height;
    const int cols = topk.sz.width;

    // Per-row pointers to each K layer for speed
    std::vector<float*>   sPtr(K);
    std::vector<uint16_t*> iPtr(K);

    for (int y = 0; y < rows; ++y)
    {
        const float* sNew = score32This.ptr<float>(y);

        for (int k = 0; k < K; ++k)
        {
            sPtr[k] = topk.score32[k].ptr<float>(y);
            iPtr[k] = topk.idx16[k].ptr<uint16_t>(y);
        }

        for (int x = 0; x < cols; ++x)
        {
            const float s = sNew[x];

            // Find insertion position in descending score list
            int ins = -1;
            for (int k = 0; k < K; ++k)
            {
                if (s > sPtr[k][x])
                {
                    ins = k;
                    break;
                }
            }

            if (ins < 0) continue; // not in top-K

            // Shift down [K-1 .. ins+1]
            for (int k = K - 1; k > ins; --k)
            {
                sPtr[k][x] = sPtr[k-1][x];
                iPtr[k][x] = iPtr[k-1][x];
            }

            // Insert new
            sPtr[ins][x] = s;
            iPtr[ins][x] = sliceIndex;
        }
    }
}

// -----------------------------------------------------------------------------
// buildBoundaryAndBand
//   boundary8 = 255 where top-1 idx differs from neighbor (right/down).
//   band8     = dilated boundary8 (band around depth transitions).
// -----------------------------------------------------------------------------
static void buildBoundaryAndBand(const cv::Mat& top1Idx16,
                                 int bandDilatePx,
                                 cv::Mat& boundary8,
                                 cv::Mat& band8)
{
    CV_Assert(top1Idx16.type() == CV_16U);
    CV_Assert(!top1Idx16.empty());

    const int rows = top1Idx16.rows;
    const int cols = top1Idx16.cols;

    boundary8 = cv::Mat::zeros(top1Idx16.size(), CV_8U);

    for (int y = 0; y < rows; ++y)
    {
        const uint16_t* row = top1Idx16.ptr<uint16_t>(y);
        const uint16_t* rowUp = (y > 0) ? top1Idx16.ptr<uint16_t>(y - 1) : nullptr;
        const uint16_t* rowDn = (y + 1 < rows) ? top1Idx16.ptr<uint16_t>(y + 1) : nullptr;

        uint8_t* out = boundary8.ptr<uint8_t>(y);

        for (int x = 0; x < cols; ++x)
        {
            const uint16_t v = row[x];

            bool diff = false;
            if (x > 0)        diff |= (v != row[x - 1]);
            if (x + 1 < cols) diff |= (v != row[x + 1]);
            if (rowUp)        diff |= (v != rowUp[x]);
            if (rowDn)        diff |= (v != rowDn[x]);

            if (diff) out[x] = 255;
        }
    }

    band8 = boundary8;
    if (bandDilatePx > 0)
        cv::dilate(band8, band8, seEllipse(bandDilatePx));
}

static void buildBoundaryAndBandGated(const cv::Mat& top1Idx16,
                                      const cv::Mat& reliable8,   // CV_8U 0/255, may be empty
                                      int bandDilatePx,
                                      cv::Mat& boundary8,
                                      cv::Mat& band8)
{
    CV_Assert(top1Idx16.type() == CV_16U);
    CV_Assert(reliable8.empty() || (reliable8.type() == CV_8U && reliable8.size() == top1Idx16.size()));

    const int rows = top1Idx16.rows;
    const int cols = top1Idx16.cols;

    boundary8 = cv::Mat::zeros(top1Idx16.size(), CV_8U);

    for (int y = 0; y < rows; ++y)
    {
        const uint16_t* row   = top1Idx16.ptr<uint16_t>(y);
        const uint16_t* rowUp = (y > 0) ? top1Idx16.ptr<uint16_t>(y - 1) : nullptr;
        const uint16_t* rowDn = (y + 1 < rows) ? top1Idx16.ptr<uint16_t>(y + 1) : nullptr;

        const uint8_t* rel   = reliable8.empty() ? nullptr : reliable8.ptr<uint8_t>(y);
        const uint8_t* relUp = (reliable8.empty() || y == 0) ? nullptr : reliable8.ptr<uint8_t>(y - 1);
        const uint8_t* relDn = (reliable8.empty() || y + 1 >= rows) ? nullptr : reliable8.ptr<uint8_t>(y + 1);

        uint8_t* out = boundary8.ptr<uint8_t>(y);

        for (int x = 0; x < cols; ++x)
        {
            // If anything around here is “reliable”, we SKIP boundary marking.
            // We only want boundaries in UNCERTAIN areas.
            if (rel)
            {
                bool anyReliable =
                    (rel[x] != 0) ||
                    (x > 0        && rel[x - 1] != 0) ||
                    (x + 1 < cols && rel[x + 1] != 0) ||
                    (relUp && relUp[x] != 0) ||
                    (relDn && relDn[x] != 0);

                if (anyReliable) continue;   // <-- INVERTED (this is the key fix)
            }

            const uint16_t v = row[x];

            bool diff = false;
            if (x > 0)        diff |= (v != row[x - 1]);
            if (x + 1 < cols) diff |= (v != row[x + 1]);
            if (rowUp)        diff |= (v != rowUp[x]);
            if (rowDn)        diff |= (v != rowDn[x]);

            if (diff) out[x] = 255;
        }
    }

    band8 = boundary8.clone();
    if (bandDilatePx > 0)
        cv::dilate(band8, band8, seEllipse(bandDilatePx));
}

static bool maskedMeanStdBGR(const cv::Mat& bgr32, const cv::Mat& mask8,
                             cv::Vec3f& mean, cv::Vec3f& stdev)
{
    CV_Assert(bgr32.type() == CV_32FC3);
    CV_Assert(mask8.type() == CV_8U);
    CV_Assert(bgr32.size() == mask8.size());

    cv::Scalar m, s;
    cv::meanStdDev(bgr32, m, s, mask8);

    mean  = cv::Vec3f((float)m[0], (float)m[1], (float)m[2]);
    stdev = cv::Vec3f((float)s[0], (float)s[1], (float)s[2]);

    // Avoid degenerate fits
    const float eps = 1e-3f;
    return (stdev[0] > eps && stdev[1] > eps && stdev[2] > eps);
}

static void applyGainOffsetBGR(cv::Mat& bgr32, const cv::Vec3f& gain, const cv::Vec3f& off, float alpha)
{
    CV_Assert(bgr32.type() == CV_32FC3);
    alpha = std::max(0.0f, std::min(1.0f, alpha));

    for (int y = 0; y < bgr32.rows; ++y)
    {
        cv::Vec3f* p = bgr32.ptr<cv::Vec3f>(y);
        for (int x = 0; x < bgr32.cols; ++x)
        {
            cv::Vec3f v = p[x];
            cv::Vec3f v2;
            v2[0] = v[0] * gain[0] + off[0];
            v2[1] = v[1] * gain[1] + off[1];
            v2[2] = v[2] * gain[2] + off[2];
            p[x] = v * (1.0f - alpha) + v2 * alpha;
        }
    }
}

static cv::Mat stabilizeTop1IndexMedian3(const cv::Mat& idx16)
{
    CV_Assert(idx16.type() == CV_16U);
    cv::Mat out;
    cv::medianBlur(idx16, out, 3);   // 3x3 “mode-ish” for label maps
    return out;
}

struct LowpassRefStats
{
    bool   valid = false;
    cv::Vec3f mean, stdev;
};

static bool fitLowpassEq(const cv::Mat& bgr32, const cv::Mat& mask8,
                         LowpassRefStats& ref,
                         cv::Vec3f& gainOut, cv::Vec3f& offOut)
{
    cv::Vec3f m, sd;
    if (!maskedMeanStdBGR(bgr32, mask8, m, sd))
        return false;

    const float sdFloor = 1e-6f;
    sd[0] = std::max(sd[0], sdFloor);
    sd[1] = std::max(sd[1], sdFloor);
    sd[2] = std::max(sd[2], sdFloor);

    if (!ref.valid)
    {
        ref.valid = true;
        ref.mean = m;
        ref.stdev = sd;
        gainOut = cv::Vec3f(1,1,1);
        offOut  = cv::Vec3f(0,0,0);
        return true;
    }

    ref.stdev[0] = std::max(ref.stdev[0], sdFloor);
    ref.stdev[1] = std::max(ref.stdev[1], sdFloor);
    ref.stdev[2] = std::max(ref.stdev[2], sdFloor);

    gainOut[0] = ref.stdev[0] / sd[0];
    gainOut[1] = ref.stdev[1] / sd[1];
    gainOut[2] = ref.stdev[2] / sd[2];

    offOut[0] = ref.mean[0] - m[0] * gainOut[0];
    offOut[1] = ref.mean[1] - m[1] * gainOut[1];
    offOut[2] = ref.mean[2] - m[2] * gainOut[2];

    return true;
}

struct DMapWeights
{
    // For K candidates, we store K float weights (CV_32F) over orig size.
    // These correspond to the indices stored in TopKMaps.idx16[k] (per pixel).
    std::vector<cv::Mat> w32;       // K mats, CV_32F, size=orig

    // Debug visuals
    cv::Mat boundary8;              // 0/255 where top-1 index changes
    cv::Mat band8;                  // dilated boundary band (halo zone)
    cv::Mat overrideMask8;          // CV_8U 0/255
    cv::Mat overrideWinner16;       // CV_16U slice index

    void create(cv::Size sz, int K)
    {
        w32.resize(K);
        for (int i = 0; i < K; ++i)
            w32[i] = cv::Mat(sz, CV_32F, cv::Scalar(0));

        boundary8.release();
        band8.release();
        overrideMask8.release();
        overrideWinner16.release();
    }

    void reset()
    {
        w32.clear();
        boundary8.release();
        band8.release();
        overrideMask8.release();
        overrideWinner16.release();
    }
};

static cv::Mat computeConfidence01(const FSFusion::TopKMaps& topk, float eps)
{
    CV_Assert(topk.K >= 2);
    cv::Mat s0 = topk.score32[0];
    cv::Mat s1 = topk.score32[1];

    // conf = (s0 - s1) / (abs(s0) + eps)  -> clamp 0..1
    cv::Mat denom;
    cv::absdiff(s0, cv::Scalar::all(0), denom);
    denom = denom + eps;

    cv::Mat conf = (s0 - s1) / denom;
    cv::max(conf, 0.0f, conf);
    cv::min(conf, 1.0f, conf);
    return conf; // CV_32F 0..1
}

static cv::Mat computeConfidence01_Top2(const FSFusion::TopKMaps& topk, float eps)
{
    CV_Assert(topk.valid());
    if (topk.K < 2)
        return cv::Mat(topk.sz, CV_32F, cv::Scalar(1.0f));

    const cv::Mat& s0 = topk.score32[0];
    const cv::Mat& s1 = topk.score32[1];
    CV_Assert(s0.type() == CV_32F && s1.type() == CV_32F);
    CV_Assert(s0.size() == topk.sz && s1.size() == topk.sz);

    // If scores are >=0, this is a much better “margin confidence”:
    // conf = (s0 - s1) / (s0 + eps)   in [0..1]
    cv::Mat conf = (s0 - s1) / (s0 + std::max(1e-12f, eps));

    // clamp
    cv::max(conf, 0.0f, conf);
    cv::min(conf, 1.0f, conf);
    return conf;
}

// -----------------------------------------------------------------------------
// buildTopKWeightsSoftmax
//   Builds weights for topK candidates from topK scores.
//   weights.w32[k] corresponds to topk.score32[k]/idx16[k] *at each pixel*.
// -----------------------------------------------------------------------------
static void buildTopKWeightsSoftmax(const FSFusion::TopKMaps& topk,
                                    const FSFusion::DMapParams& p,
                                    DMapWeights& weightsOut)
{
    CV_Assert(topk.K > 0);
    CV_Assert(!topk.score32.empty());
    CV_Assert((int)topk.score32.size() == topk.K);
    CV_Assert((int)topk.idx16.size() == topk.K);

    const int K = topk.K;
    const cv::Size sz = topk.sz;

    weightsOut.create(sz, K);

    const float temp = std::max(1e-6f, p.softTemp);
    const float wMin = std::max(0.0f, p.wMin);

    std::vector<const float*> sPtr(K);
    std::vector<float*> wPtr(K);

    for (int y = 0; y < sz.height; ++y)
    {
        for (int k = 0; k < K; ++k)
        {
            sPtr[k] = topk.score32[k].ptr<float>(y);
            wPtr[k] = weightsOut.w32[k].ptr<float>(y);
        }

        for (int x = 0; x < sz.width; ++x)
        {
            // find max score for stability
            float sMax = sPtr[0][x];
            for (int k = 1; k < K; ++k)
                sMax = std::max(sMax, sPtr[k][x]);

            // exp scores
            float sum = 0.0f;
            float e[16]; // K is small; if you might exceed 16, change this.
            CV_Assert(K <= 16);

            for (int k = 0; k < K; ++k)
            {
                float z = (sPtr[k][x] - sMax) / temp;
                // clamp z to avoid inf; exp(-80) is ~1e-35 (effectively zero)
                z = std::max(-80.0f, std::min(80.0f, z));
                e[k] = std::exp(z);
                sum += e[k];
            }

            if (sum <= 0.0f)
            {
                // fallback: hard assign to top-1
                for (int k = 0; k < K; ++k) wPtr[k][x] = 0.0f;
                wPtr[0][x] = 1.0f;
                continue;
            }

            // normalize + floor + renormalize
            float sum2 = 0.0f;
            for (int k = 0; k < K; ++k)
            {
                float w = e[k] / sum;
                if (wMin > 0.0f) w = std::max(w, wMin);
                wPtr[k][x] = w;
                sum2 += w;
            }

            const float inv = (sum2 > 0.0f) ? (1.0f / sum2) : 1.0f;
            for (int k = 0; k < K; ++k)
                wPtr[k][x] *= inv;
        }
    }
}

// -----------------------------------------------------------------------------
// smoothWeightsInBandAndRenormalize
//   - Blurs each weight map (Gaussian).
//   - Replaces weights only where band8 != 0.
//   - Renormalizes across K per pixel (in band).
// -----------------------------------------------------------------------------
static void smoothWeightsInBandAndRenormalize(DMapWeights& w,
                                              const cv::Mat& band8,
                                              const FSFusion::DMapParams& p)
{
    CV_Assert(!band8.empty());
    CV_Assert(band8.type() == CV_8U);
    CV_Assert((int)w.w32.size() > 0);
    CV_Assert(w.w32[0].type() == CV_32F);
    CV_Assert(w.w32[0].size() == band8.size());

    const int K = (int)w.w32.size();
    const float wMin = std::max(0.0f, p.wMin);

    // Blur each weight map and write back only in the band.
    for (int k = 0; k < K; ++k)
    {
        cv::Mat blur = w.w32[k].clone();
        gaussianBlurInPlace(blur, p.weightBlurSigma);

        // replace inside band only
        blur.copyTo(w.w32[k], band8);
    }

    // Renormalize in the band region
    const int rows = band8.rows;
    const int cols = band8.cols;

    std::vector<float*> wPtr(K);

    for (int y = 0; y < rows; ++y)
    {
        const uint8_t* b = band8.ptr<uint8_t>(y);
        for (int k = 0; k < K; ++k)
            wPtr[k] = w.w32[k].ptr<float>(y);

        for (int x = 0; x < cols; ++x)
        {
            if (!b[x]) continue;

            float sum = 0.0f;
            for (int k = 0; k < K; ++k)
            {
                float v = wPtr[k][x];
                if (wMin > 0.0f) v = std::max(v, wMin);
                wPtr[k][x] = v;
                sum += v;
            }

            if (sum <= 0.0f)
            {
                // fallback: hard assign
                for (int k = 0; k < K; ++k) wPtr[k][x] = 0.0f;
                wPtr[0][x] = 1.0f;
                continue;
            }

            const float inv = 1.0f / sum;
            for (int k = 0; k < K; ++k)
                wPtr[k][x] *= inv;
        }
    }
}

// -----------------------------------------------------------------------------
// buildDMapWeightsFromTopK
//   Produces:
//     weights.w32[k]  : CV_32F weights for topK candidates
//     weights.boundary8, weights.band8
//   Steps:
//     (1) soft weights from scores
//     (2) boundary/band from top-1 index map
//     (3) smooth weights in band and renormalize
// -----------------------------------------------------------------------------
static void buildDMapWeightsFromTopK(const FSFusion::TopKMaps& topk,
                                     const FSFusion::DMapParams& p,
                                     DMapWeights& weightsOut)
{
    QString srcFun = "FSFusion::buildDMapWeightsFromTopK";
    if (G::FSLog) G::log(srcFun);

    CV_Assert(topk.valid());
    CV_Assert(topk.K == p.topK);
    CV_Assert(topk.K >= 1);

    // 1) Base weights (this also sizes weightsOut.w32 via weightsOut.create())
    buildTopKWeightsSoftmax(topk, p, weightsOut);

    // 2) Reliability mask: reliable if (strong focus) OR (confident)
    cv::Mat conf01 = computeConfidence01_Top2(topk, p.confEps); // CV_32F 0..1

    double smn=0, smx=0;
    cv::minMaxLoc(topk.score32[0], &smn, &smx);

    // IMPORTANT: loosen this. 2% of max was too strict in your case.
    const float scoreThr = float(smx) * 0.005f;   // try 0.003..0.01

    cv::Mat strong = (topk.score32[0] > scoreThr);
    strong.convertTo(strong, CV_8U, 255.0);

    cv::Mat confident = (conf01 > p.confHigh);
    confident.convertTo(confident, CV_8U, 255.0);

    cv::Mat reliable8;
    cv::bitwise_or(strong, confident, reliable8);

    // 3) Boundary & band (THIS is the crucial change)
    buildBoundaryAndBandGated(topk.idx16[0], reliable8,
                              std::max(0, p.bandDilatePx),
                              weightsOut.boundary8,
                              weightsOut.band8);

    qDebug() << "boundary gated px =" << cv::countNonZero(weightsOut.boundary8)
             << "band px =" << cv::countNonZero(weightsOut.band8);

    // 4) Optional old Gaussian smoothing inside the band
    if (p.enableGaussianBandSmoothing &&
        !weightsOut.band8.empty() &&
        cv::countNonZero(weightsOut.band8) > 0 &&
        p.weightBlurSigma > 0.0f)
    {
        smoothWeightsInBandAndRenormalize(weightsOut, weightsOut.band8, p);
    }
}

// ==============================
// Streamed DMap fusion (2-pass)
// ==============================

// -----------------------------
// Small helpers
// -----------------------------
static inline float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

static inline bool rectInside(const cv::Rect& r, const cv::Size& s)
{
    return r.x >= 0 && r.y >= 0 &&
           r.width > 0 && r.height > 0 &&
           r.x + r.width  <= s.width &&
           r.y + r.height <= s.height;
}

static cv::Mat toFloat01(const cv::Mat& gray8)
{
    CV_Assert(gray8.type() == CV_8U);
    cv::Mat f;
    gray8.convertTo(f, CV_32F, 1.0 / 255.0);
    return f;
}

// Focus metric for DMap:
// Laplacian magnitude of preblurred gray (stable, cheap).
static cv::Mat focusMetric32(const cv::Mat& gray8, float preBlurSigma)
{
    CV_Assert(gray8.type() == CV_8U);

    cv::Mat g32 = toFloat01(gray8);
    if (preBlurSigma > 0.0f)
        cv::GaussianBlur(g32, g32, cv::Size(0,0),
                         preBlurSigma, preBlurSigma,
                         cv::BORDER_REFLECT);

    cv::Mat lap32;
    cv::Laplacian(g32, lap32, CV_32F, 3);

    cv::Mat absLap32;
    cv::absdiff(lap32, cv::Scalar::all(0), absLap32); // |lap|

    // Light blur to reduce “hot pixel” domination
    cv::GaussianBlur(absLap32, absLap32, cv::Size(0,0), 4, 4, cv::BORDER_REFLECT);
    // cv::GaussianBlur(absLap32, absLap32, cv::Size(0,0), 0.8, 0.8, cv::BORDER_REFLECT);
    return absLap32; // CV_32F, 0..small
}

static void smoothWeightsEdgeAwareInBand(
    std::vector<cv::Mat>& wK,         // K weight maps, CV_32F
    const cv::Mat& band8,             // CV_8U 0/255 (boundary band)
    const cv::Mat& guide01,           // CV_32F 0..1 guide image (orig size)
    const cv::Mat& conf01,            // CV_32F 0..1 confidence (orig size)
    const FSFusion::DMapParams& p)
{
    CV_Assert(!band8.empty() && band8.type() == CV_8U);
    CV_Assert(guide01.type() == CV_32F);
    CV_Assert(conf01.type() == CV_32F);
    CV_Assert(guide01.size() == band8.size());
    CV_Assert(conf01.size() == band8.size());

    const int K = (int)wK.size();
    CV_Assert(K >= 1);
    for (int k = 0; k < K; ++k)
    {
        CV_Assert(wK[k].type() == CV_32F);
        CV_Assert(wK[k].size() == band8.size());
    }

    // Build mask = band && (conf < confLow) if confidence gating is enabled
    cv::Mat workMask8;
    if (p.enableConfidenceGating)
    {
        cv::Mat low = (conf01 < p.confLow);
        low.convertTo(low, CV_8U, 255.0);
        cv::bitwise_and(low, band8, workMask8);
    }
    else
    {
        workMask8 = band8.clone();
    }

    if (cv::countNonZero(workMask8) == 0)
        return;

    const int   R    = std::max(1, p.eaRadius);
    const float sigS = std::max(1e-6f, p.eaSigmaSpatial);
    const float sigR = std::max(1e-6f, p.eaSigmaRange01);

    const float inv2SigS2 = 1.0f / (2.0f * sigS * sigS);
    const float inv2SigR2 = 1.0f / (2.0f * sigR * sigR);

    // Precompute spatial weights
    const int W = 2 * R + 1;
    std::vector<float> wSpatial(W * W);
    for (int dy = -R; dy <= R; ++dy)
        for (int dx = -R; dx <= R; ++dx)
            wSpatial[(dy + R) * W + (dx + R)] =
                std::exp(-(dx*dx + dy*dy) * inv2SigS2);

    const int iters = std::max(1, p.eaIters);
    for (int it = 0; it < iters; ++it)
    {
        std::vector<cv::Mat> nextK(K);
        for (int k = 0; k < K; ++k)
            nextK[k] = wK[k].clone();

        for (int y = 0; y < band8.rows; ++y)
        {
            const uint8_t* mRow = workMask8.ptr<uint8_t>(y);
            for (int x = 0; x < band8.cols; ++x)
            {
                if (!mRow[x]) continue;

                const float g0 = guide01.at<float>(y, x);

                float acc[16] = {0};
                CV_Assert(K <= 16);

                float norm = 0.0f;

                for (int dy = -R; dy <= R; ++dy)
                {
                    int yy = y + dy;
                    if ((unsigned)yy >= (unsigned)band8.rows) continue;

                    for (int dx = -R; dx <= R; ++dx)
                    {
                        int xx = x + dx;
                        if ((unsigned)xx >= (unsigned)band8.cols) continue;

                        const float g1 = guide01.at<float>(yy, xx);
                        const float dg = g1 - g0;
                        const float wRange = std::exp(-(dg*dg) * inv2SigR2);

                        const float ws = wSpatial[(dy + R) * W + (dx + R)] * wRange;
                        norm += ws;

                        for (int k = 0; k < K; ++k)
                            acc[k] += ws * wK[k].at<float>(yy, xx);
                    }
                }

                if (norm <= 1e-12f)
                    continue;

                // write smoothed weights
                float sumW = 0.0f;
                for (int k = 0; k < K; ++k)
                {
                    float v = acc[k] / norm;
                    v = std::max(0.0f, v);
                    nextK[k].at<float>(y, x) = v;
                    sumW += v;
                }

                // renormalize at pixel
                if (sumW > 1e-12f)
                {
                    const float inv = 1.0f / sumW;
                    for (int k = 0; k < K; ++k)
                        nextK[k].at<float>(y, x) *= inv;
                }
                else
                {
                    nextK[0].at<float>(y, x) = 1.0f;
                    for (int k = 1; k < K; ++k)
                        nextK[k].at<float>(y, x) = 0.0f;
                }
            }
        }

        wK.swap(nextK);
    }
}

// Insert (score, idx) into topK at pixel (x,y) using small insertion sort.
// Highest score is best.
static inline void topKInsertAt(FSFusion::TopKMaps& t,
                                int x, int y,
                                float score,
                                uint16_t idx)
{
    // Fast path: if score <= worst, do nothing
    float worst = t.score32[t.K - 1].at<float>(y, x);
    if (score <= worst) return;

    // Find insert position
    int pos = t.K - 1;
    while (pos > 0)
    {
        float prev = t.score32[pos - 1].at<float>(y, x);
        if (score <= prev) break;
        pos--;
    }

    // Shift down [pos..K-2] -> [pos+1..K-1]
    for (int k = t.K - 1; k > pos; --k)
    {
        t.score32[k].at<float>(y, x) = t.score32[k - 1].at<float>(y, x);
        t.idx16[k].at<uint16_t>(y, x) = t.idx16[k - 1].at<uint16_t>(y, x);
    }

    // Insert
    t.score32[pos].at<float>(y, x) = score;
    t.idx16[pos].at<uint16_t>(y, x) = idx;
}

// Update Top-K for the whole image using the new focus metric map.
static void updateTopKForSlice(FSFusion::TopKMaps& t,
                               const cv::Mat& focus32,
                               int sliceIdx)
{
    CV_Assert(t.valid());
    CV_Assert(focus32.type() == CV_32F);
    CV_Assert(focus32.size() == t.sz);

    const int w = t.sz.width;
    const int h = t.sz.height;
    const uint16_t s = (uint16_t)std::max(0, sliceIdx);

    for (int y = 0; y < h; ++y)
    {
        const float* f = focus32.ptr<float>(y);
        for (int x = 0; x < w; ++x)
        {
            topKInsertAt(t, x, y, f[x], s);
        }
    }
}

// Build per-slice weight map from Top-K (only K contributors per pixel).
// For slice s, w(x)=sum_k (idx_k==s)*w_k.
static cv::Mat buildSliceWeightMap32(const FSFusion::TopKMaps& topk,
                                     const DMapWeights& W,
                                     int sliceIdx)
{
    CV_Assert(topk.valid());
    CV_Assert(topk.K >= 1);
    CV_Assert((int)W.w32.size() == topk.K);

    cv::Mat w32(topk.sz, CV_32F, cv::Scalar(0));

    // A) Soft weights from TopK ranks
    for (int k = 0; k < topk.K; ++k)
    {
        CV_Assert(W.w32[k].type() == CV_32F);
        CV_Assert(W.w32[k].size() == topk.sz);

        cv::Mat m = (topk.idx16[k] == (uint16_t)sliceIdx); // CV_8U 0/255

        cv::Mat mk, tmp;
        m.convertTo(mk, CV_32F, 1.0 / 255.0);
        cv::multiply(W.w32[k], mk, tmp);
        w32 += tmp;
    }

    // B) HARD overrides (if provided)
    if (!W.overrideMask8.empty() && !W.overrideWinner16.empty())
    {
        CV_Assert(W.overrideMask8.type() == CV_8U);
        CV_Assert(W.overrideWinner16.type() == CV_16U);
        CV_Assert(W.overrideMask8.size() == topk.sz);
        CV_Assert(W.overrideWinner16.size() == topk.sz);

        // Enforce binary mask (0/255)
        cv::Mat overrideBin;
        cv::compare(W.overrideMask8, 0, overrideBin, cv::CMP_GT);

        cv::Mat isMe = (W.overrideWinner16 == (uint16_t)sliceIdx); // 0/255

        cv::Mat m;
        cv::bitwise_and(isMe, overrideBin, m);
        w32.setTo(1.0f, m);

        cv::Mat notIsMe, notMe;
        cv::bitwise_not(isMe, notIsMe);
        cv::bitwise_and(overrideBin, notIsMe, notMe);
        w32.setTo(0.0f, notMe);
    }

    return w32;
}

// -----------------------------
// Pyramid blending (Pass-2)
// -----------------------------
static void buildGaussianPyr(const cv::Mat& img32, int levels, std::vector<cv::Mat>& gp)
{
    gp.clear();
    gp.reserve(levels);
    gp.push_back(img32);
    for (int i = 1; i < levels; ++i)
    {
        cv::Mat down;
        cv::pyrDown(gp.back(), down);
        gp.push_back(down);
    }
}

static void buildLaplacianPyr(const cv::Mat& img32, int levels, std::vector<cv::Mat>& lp)
{
    std::vector<cv::Mat> gp;
    buildGaussianPyr(img32, levels, gp);

    lp.resize(levels);
    for (int i = 0; i < levels - 1; ++i)
    {
        cv::Mat up;
        cv::pyrUp(gp[i + 1], up, gp[i].size());
        lp[i] = gp[i] - up;
    }
    lp[levels - 1] = gp[levels - 1]; // residual
}

static cv::Mat collapseLaplacianPyr(const std::vector<cv::Mat>& lp)
{
    CV_Assert(!lp.empty());
    cv::Mat cur = lp.back().clone();
    for (int i = (int)lp.size() - 2; i >= 0; --i)
    {
        cv::Mat up;
        cv::pyrUp(cur, up, lp[i].size());
        cur = up + lp[i];
    }
    return cur;
}

static int percentileU8All(const cv::Mat& u8, float pct01)
{
    CV_Assert(u8.type() == CV_8U);
    CV_Assert(pct01 >= 0.0f && pct01 <= 1.0f);

    // Fast enough for a single call; if you want faster, histogram it.
    std::vector<uint8_t> v;
    v.reserve(u8.total());

    for (int y = 0; y < u8.rows; ++y)
    {
        const uint8_t* row = u8.ptr<uint8_t>(y);
        v.insert(v.end(), row, row + u8.cols);
    }

    if (v.empty()) return 0;

    const size_t k = (size_t)std::clamp<double>(pct01 * (double)(v.size() - 1), 0.0, (double)(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + (ptrdiff_t)k, v.end());
    return (int)v[k];
}

static int percentileU8Masked(const cv::Mat& u8, const cv::Mat& mask8, float pct01)
{
    CV_Assert(u8.type() == CV_8U);
    CV_Assert(mask8.type() == CV_8U);
    CV_Assert(u8.size() == mask8.size());
    pct01 = std::max(0.0f, std::min(1.0f, pct01));

    int hist[256] = {0};
    int total = 0;

    for (int y = 0; y < u8.rows; ++y)
    {
        const uint8_t* r = u8.ptr<uint8_t>(y);
        const uint8_t* m = mask8.ptr<uint8_t>(y);
        for (int x = 0; x < u8.cols; ++x)
        {
            if (!m[x]) continue;
            hist[r[x]]++;
            total++;
        }
    }
    if (total <= 0) return 0;

    const int target = (int)std::floor(pct01 * (float)(total - 1));
    int acc = 0;
    for (int v = 0; v < 256; ++v)
    {
        acc += hist[v];
        if (acc > target) return v;
    }
    return 255;
}

// Accumulator pyramids (numerator + denom) for weighted blending
struct PyrAccum
{
    int levels = 0;
    std::vector<cv::Mat> num3; // CV_32FC3
    std::vector<cv::Mat> den1; // CV_32F

    void reset(const cv::Size& sz, int levels_)
    {
        levels = levels_;
        num3.resize(levels);
        den1.resize(levels);

        cv::Size s = sz;
        for (int i = 0; i < levels; ++i)
        {
            num3[i] = cv::Mat::zeros(s, CV_32FC3);
            den1[i] = cv::Mat::zeros(s, CV_32F);
            if (i < levels - 1) s = cv::Size((s.width + 1)/2, (s.height + 1)/2);
        }
    }
};

static void buildDepthEdgeVetoPyr(const std::vector<cv::Mat>& idxPyr16,
                                  int levels,
                                  const FSFusion::DMapParams& p,
                                  std::vector<cv::Mat>& vetoPyr8)
{
    CV_Assert((int)idxPyr16.size() == levels);
    vetoPyr8.assign(levels, cv::Mat());

    // Only build veto at/after vetoFromLevel.
    const int start = (p.vetoFromLevel < 0)
                          ? (levels - 1) // residual only
                          : std::max(0, std::min(levels - 1, p.vetoFromLevel));

    for (int i = 0; i < levels; ++i)
    {
        CV_Assert(idxPyr16[i].type() == CV_16U);

        if (i < start)
        {
            vetoPyr8[i] = cv::Mat::zeros(idxPyr16[i].size(), CV_8U);
            continue;
        }

        // 1) Median on labels to kill pixel jitter
        cv::Mat idxSm16;
        if (idxPyr16[i].cols >= 5 && idxPyr16[i].rows >= 5)
            cv::medianBlur(idxPyr16[i], idxSm16, 5);
        else
            idxSm16 = idxPyr16[i];

        // 2) Convert to float
        cv::Mat depthF;
        idxSm16.convertTo(depthF, CV_32F);

        // 3) Gradient (Sobel k=3)
        cv::Mat dx, dy;
        cv::Sobel(depthF, dx, CV_32F, 1, 0, 3);
        cv::Sobel(depthF, dy, CV_32F, 0, 1, 3);

        cv::Mat grad = cv::abs(dx) + cv::abs(dy);

        // 4) Normalize Sobel gain to “index units”
        grad = grad * 0.25f;

        // Optional: tiny blur on veto levels to reduce speckly veto
        // cv::GaussianBlur(grad, grad, cv::Size(0,0), 0.8, 0.8, cv::BORDER_REFLECT);

        // 5) Threshold
        cv::Mat veto = (grad > p.depthGradThresh); // CV_8U 0/255

        // 6) Dilate
        if (p.vetoDilatePx > 0)
            cv::dilate(veto, veto, seEllipse(p.vetoDilatePx));

        vetoPyr8[i] = veto;
    }
}

static cv::Mat downsampleIdx16_Mode2x2(const cv::Mat& src16)
{
    CV_Assert(src16.type() == CV_16U);

    const int outW = (src16.cols + 1) / 2;
    const int outH = (src16.rows + 1) / 2;

    cv::Mat dst16(outH, outW, CV_16U);

    for (int y = 0; y < outH; ++y)
    {
        const int y0 = 2 * y;
        const int y1 = std::min(y0 + 1, src16.rows - 1);

        const uint16_t* r0 = src16.ptr<uint16_t>(y0);
        const uint16_t* r1 = src16.ptr<uint16_t>(y1);
        uint16_t* out = dst16.ptr<uint16_t>(y);

        for (int x = 0; x < outW; ++x)
        {
            const int x0 = 2 * x;
            const int x1 = std::min(x0 + 1, src16.cols - 1);

            const uint16_t a = r0[x0];
            const uint16_t b = r0[x1];
            const uint16_t c = r1[x0];
            const uint16_t d = r1[x1];

            // Majority vote among a,b,c,d (2x2 mode)
            // Cases: if any value appears >=2, choose it; else choose a.
            uint16_t v = a;
            if (b == a || c == a || d == a) v = a;
            else if (c == b || d == b)      v = b;
            else if (d == c)                v = c;
            else                             v = a;

            out[x] = v;
        }
    }
    return dst16;
}

static void buildIndexPyrNearest(const cv::Mat& idx16Level0, int levels, std::vector<cv::Mat>& idxPyr)
{
    CV_Assert(idx16Level0.type() == CV_16U);
    idxPyr.clear();
    idxPyr.reserve(levels);

    // IMPORTANT: make sure level0 is stabilized before pyramid (see Option B).
    idxPyr.push_back(idx16Level0);

    for (int i = 1; i < levels; ++i)
    {
        cv::Mat down = downsampleIdx16_Mode2x2(idxPyr.back());
        idxPyr.push_back(down);
    }
}

// Accumulate one slice into pyramids: num += lap(color)*gw, den += gw
static void accumulateSlicePyr(PyrAccum& A,
                               const cv::Mat& color32FC3,
                               const cv::Mat& weight32F,
                               const std::vector<cv::Mat>& idxPyr16,
                               const std::vector<cv::Mat>* vetoPyr8,
                               int sliceIdx,
                               const FSFusion::DMapParams& p,
                               int levels,
                               float weightBlurSigma)
{
    CV_Assert(color32FC3.type() == CV_32FC3);
    CV_Assert(weight32F.type() == CV_32F);
    CV_Assert(color32FC3.size() == weight32F.size());

    cv::Mat w = weight32F;
    if (weightBlurSigma > 0.0f)
        cv::GaussianBlur(w, w, cv::Size(0,0), weightBlurSigma, weightBlurSigma, cv::BORDER_REFLECT);

    // Color Laplacian pyramid (unchanged)
    std::vector<cv::Mat> lpColor;
    buildLaplacianPyr(color32FC3, levels, lpColor);

    // Weight Gaussian pyramid (DEFAULT behavior)
    std::vector<cv::Mat> gpW;
    buildGaussianPyr(w, levels, gpW);

    // ---- Module E: depth-gradient lowpass veto (SAFE version)
    // Instead of zeroing weights (can collapse denominator), force HARD winner only in veto pixels.
    if (p.enableDepthGradLowpassVeto && vetoPyr8 && !vetoPyr8->empty())
    {
        CV_Assert((int)vetoPyr8->size() == levels);
        CV_Assert((int)idxPyr16.size() == levels);

        int start = p.vetoFromLevel;
        if (start < 0) start = levels - 1;                 // default residual only
        start = std::max(0, std::min(levels - 1, start));

        const float s = std::max(0.0f, std::min(1.0f, p.vetoStrength));
        const uint16_t slice = (uint16_t)std::max(0, sliceIdx);

        for (int i = start; i < levels; ++i)
        {
            const cv::Mat& veto = (*vetoPyr8)[i];
            CV_Assert(veto.type() == CV_8U);
            CV_Assert(veto.size() == gpW[i].size());
            CV_Assert(idxPyr16[i].type() == CV_16U);
            CV_Assert(idxPyr16[i].size() == gpW[i].size());

            // hard winner mask for THIS slice at THIS level
            cv::Mat hard8 = (idxPyr16[i] == slice);           // 0/255
            cv::Mat hardF;
            hard8.convertTo(hardF, CV_32F, 1.0 / 255.0);      // 0/1

            if (s >= 0.999f)
            {
                // Full strength: replace weights with hard winner inside veto pixels
                hardF.copyTo(gpW[i], veto);
            }
            else if (s > 0.0f)
            {
                // Partial: gpW = lerp(gpW, hardF, s) inside veto pixels
                cv::Mat out = gpW[i].clone();

                cv::Mat mix = (1.0f - s) * gpW[i] + s * hardF;
                mix.copyTo(out, veto);
                gpW[i] = out;
            }
        }
    }

    // Optionally replace coarse gpW with HARD winner mask to stop lowpass bleeding
    if (p.enableHardWeightsOnLowpass)
    {
        CV_Assert((int)idxPyr16.size() == levels);

        int start = p.hardFromLevel;
        if (start < 0) start = levels - 1;            // only residual by default
        start = std::max(0, std::min(levels - 1, start));

        const uint16_t s = (uint16_t)std::max(0, sliceIdx);

        for (int i = start; i < levels; ++i)
        {
            CV_Assert(idxPyr16[i].type() == CV_16U);
            CV_Assert(idxPyr16[i].size() == gpW[i].size());

            cv::Mat hard = (idxPyr16[i] == s);   // CV_8U 0/255
            hard.convertTo(gpW[i], CV_32F, 1.0 / 255.0); // 0 or 1
        }
    }

    // Accumulate
    for (int i = 0; i < levels; ++i)
    {
        cv::Mat w3;
        cv::Mat ch[] = { gpW[i], gpW[i], gpW[i] };
        cv::merge(ch, 3, w3);

        A.num3[i] += lpColor[i].mul(w3);
        A.den1[i] += gpW[i];
    }
}

static cv::Mat finalizeBlend(const PyrAccum& A, float eps)
{
    std::vector<cv::Mat> lpOut(A.levels);

    for (int i = 0; i < A.levels; ++i)
    {
        cv::Mat den = A.den1[i] + eps;

        cv::Mat den3;
        cv::Mat ch[] = { den, den, den };
        cv::merge(ch, 3, den3);

        lpOut[i] = A.num3[i] / den3;
    }

    return collapseLaplacianPyr(lpOut);
}

// --------- End DMAP functions ----------


static ForegroundResult buildForegroundMask(
    const cv::Mat& gray8Orig,        // CV_8U
    const cv::Mat& edgeMask8Orig,    // CV_8U 0/255
    const ForegroundParams& fp)
{
    CV_Assert(gray8Orig.type() == CV_8U);
    CV_Assert(edgeMask8Orig.type() == CV_8U);
    CV_Assert(gray8Orig.size() == edgeMask8Orig.size());

    ForegroundResult r;

    // Convert to float [0..1]
    cv::Mat g32;
    gray8Orig.convertTo(g32, CV_32F, 1.0 / 255.0);

    // Optional pre-blur for stable gradients/laplacian
    if (fp.preBlurSigma > 0.0f)
        cv::GaussianBlur(g32, g32, cv::Size(0, 0),
                         fp.preBlurSigma, fp.preBlurSigma, cv::BORDER_REFLECT);

    // Prepare edge mask (optionally dilated)
    r.edgeMask8 = edgeMask8Orig.clone();
    if (fp.edgeDilatePx > 0)
        cv::dilate(r.edgeMask8, r.edgeMask8, seEllipse(fp.edgeDilatePx));

    // -------------------------------------------------------------------------
    // (A) High-frequency texture mask (abs Laplacian)
    // -------------------------------------------------------------------------
    {
        cv::Mat lap32;
        cv::Laplacian(g32, lap32, CV_32F, 3);

        cv::Mat absLap32;
        cv::absdiff(lap32, cv::Scalar::all(0), absLap32);

        if (fp.hfBlurSigma > 0.0f)
            cv::GaussianBlur(absLap32, absLap32, cv::Size(0, 0),
                             fp.hfBlurSigma, fp.hfBlurSigma, cv::BORDER_REFLECT);

        double mn = 0.0, mx = 0.0;
        cv::minMaxLoc(absLap32, &mn, &mx);

        float thr = float(mx) * fp.hfFrac;
        cv::Mat hf = (absLap32 > thr);
        hf.convertTo(r.hfMask8, CV_8U, 255.0);
    }

    // -------------------------------------------------------------------------
    // (B) Thin smooth twigs mask (grad magnitude, with max clipping + thinning)
    //   Key knobs to make it LESS FAT:
    //     - thinClosePx = 0 or 1 (close fattens)
    //     - thinErodePx >= 1 (this actually thins)
    // -------------------------------------------------------------------------
    {
        cv::Mat gTw = g32;
        if (fp.thinBlurSigma > 0.0f)
            cv::GaussianBlur(gTw, gTw, cv::Size(0, 0),
                             fp.thinBlurSigma, fp.thinBlurSigma, cv::BORDER_REFLECT);

        cv::Mat gx, gy, mag32;
        cv::Sobel(gTw, gx, CV_32F, 1, 0, 3);
        cv::Sobel(gTw, gy, CV_32F, 0, 1, 3);
        cv::magnitude(gx, gy, mag32);

        // Clip extreme max so thinFrac actually responds
        double mn = 0.0, mx = 0.0;
        cv::minMaxLoc(mag32, &mn, &mx);
        const float clip = float(mx) * fp.thinMxClipFrac;
        if (clip > 0.0f)
            cv::min(mag32, clip, mag32);

        double mx2 = 0.0;
        cv::minMaxLoc(mag32, nullptr, &mx2);

        const float thr = float(mx2) * fp.thinFrac;
        cv::Mat thin = (mag32 > thr);
        thin.convertTo(r.thinMask8, CV_8U, 255.0);

        if (fp.thinOpenPx > 0)
            cv::morphologyEx(r.thinMask8, r.thinMask8, cv::MORPH_OPEN, seEllipse(fp.thinOpenPx));

        if (fp.thinClosePx > 0)
            cv::morphologyEx(r.thinMask8, r.thinMask8, cv::MORPH_CLOSE, seEllipse(fp.thinClosePx));

        // This is the “make it less fat” step
        if (fp.thinErodePx > 0)
            cv::erode(r.thinMask8, r.thinMask8, seEllipse(fp.thinErodePx));

        r.thinMask8 = filterMinArea8U(r.thinMask8, fp.thinMinAreaPx);
    }

    // -------------------------------------------------------------------------
    // (C) Dark solid interiors (main branch core)
    //   Otsu reference on blurred 8-bit, then "darker-than", close, hole-fill,
    //   min-area, optional edge gating.
    // -------------------------------------------------------------------------
    {
        cv::Mat blur8;
        cv::GaussianBlur(gray8Orig, blur8, cv::Size(0, 0),
                         fp.darkBlurSigma, fp.darkBlurSigma, cv::BORDER_REFLECT);

        cv::Mat tmp; // IMPORTANT: do not use cv::Mat() placeholder
        double t = cv::threshold(blur8, tmp, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        double darkT = t * fp.darkFrac;
        cv::Mat dark = (blur8 < darkT);
        dark.convertTo(dark, CV_8U, 255.0);

        if (fp.darkClosePx > 0)
            cv::morphologyEx(dark, dark, cv::MORPH_CLOSE, seEllipse(fp.darkClosePx));

        // Fill holes
        {
            cv::Mat ff = dark.clone();
            cv::copyMakeBorder(ff, ff, 1,1,1,1, cv::BORDER_CONSTANT, cv::Scalar(0));
            cv::floodFill(ff, cv::Point(0,0), cv::Scalar(255));
            ff = ff(cv::Rect(1,1,dark.cols,dark.rows));
            cv::Mat holes = (ff == 0);
            holes.convertTo(holes, CV_8U, 255.0);
            cv::bitwise_or(dark, holes, dark);
        }

        dark = filterMinArea8U(dark, fp.darkMinAreaPx);

        if (fp.darkEdgeGatePx > 0)
        {
            cv::Mat gate = r.edgeMask8.clone();
            cv::dilate(gate, gate, seEllipse(fp.darkEdgeGatePx));
            cv::bitwise_and(dark, gate, dark);
        }

        r.darkMask8 = dark;
    }

    // -------------------------------------------------------------------------
    // Combine (all CV_8U, same size)
    // -------------------------------------------------------------------------
    r.fgMask8 = cv::Mat::zeros(gray8Orig.size(), CV_8U);
    cv::bitwise_or(r.fgMask8, r.hfMask8,   r.fgMask8);
    cv::bitwise_or(r.fgMask8, r.thinMask8, r.fgMask8);
    cv::bitwise_or(r.fgMask8, r.darkMask8, r.fgMask8);
    cv::bitwise_or(r.fgMask8, r.edgeMask8, r.fgMask8);

    if (fp.finalDilatePx > 0)
        cv::dilate(r.fgMask8, r.fgMask8, seEllipse(fp.finalDilatePx));

    return r;
}

// -----------------------------------------------------------------------------
// Dump foreground debug outputs
//   Writes:
//     fg_hf.png      (0/255 mask)
//     fg_thin.png    (0/255 mask)
//     fg_dark.png    (0/255 mask)
//     fg_mask.png    (0/255 final)
//     fg_overlay.png (green overlay on grayscale)
// -----------------------------------------------------------------------------
static void dumpForegroundDebug(
    const QString& basePath,
    const cv::Mat& gray8Orig,
    const ForegroundParams& /*fp*/,
    const ForegroundResult& fr)
{
    cv::imwrite((basePath + "/fg_hf.png").toStdString(),   fr.hfMask8);
    cv::imwrite((basePath + "/fg_thin.png").toStdString(), fr.thinMask8);
    cv::imwrite((basePath + "/fg_dark.png").toStdString(), fr.darkMask8);
    cv::imwrite((basePath + "/fg_mask.png").toStdString(), fr.fgMask8);

    // overlay (green)
    cv::Mat bgr;
    cv::cvtColor(gray8Orig, bgr, cv::COLOR_GRAY2BGR);

    const float alpha = 0.55f;
    for (int y = 0; y < bgr.rows; ++y)
    {
        const uint8_t* m = fr.fgMask8.ptr<uint8_t>(y);
        cv::Vec3b* p = bgr.ptr<cv::Vec3b>(y);
        for (int x = 0; x < bgr.cols; ++x)
        {
            if (!m[x]) continue;
            cv::Vec3b c = p[x];
            c[0] = uint8_t((1 - alpha) * c[0] + alpha * 0);
            c[1] = uint8_t((1 - alpha) * c[1] + alpha * 255);
            c[2] = uint8_t((1 - alpha) * c[2] + alpha * 0);
            p[x] = c;
        }
    }

    cv::imwrite((basePath + "/fg_overlay.png").toStdString(), bgr);
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
    QString srcFun = "HaloDetectResult detectLowpassHalosOrig";
    CV_Assert(fusedGray8Orig.type() == CV_8U);

    HaloDetectResult r;
    qDebug() << srcFun << "0";

    // 1) Edge mask (orig size)
    r.edgeMask8 = makeEdgeMask8U(fusedGray8Orig, hp.edgeSigma, hp.edgeThreshFrac, hp.edgeDilatePx);

    CV_Assert(!r.edgeMask8.empty());
    CV_Assert(r.edgeMask8.type() == CV_8U);
    CV_Assert(r.edgeMask8.size() == fusedGray8Orig.size());

    // If no edges, no halo detection possible
    if (cv::countNonZero(r.edgeMask8) == 0)
    {
        r.dogMax32  = cv::Mat::zeros(fusedGray8Orig.size(), CV_32F);
        r.dist32    = cv::Mat::zeros(fusedGray8Orig.size(), CV_32F);
        r.haloMask8 = cv::Mat::zeros(fusedGray8Orig.size(), CV_8U);
        return r;
    }

    qDebug() << srcFun << "1";
    // 2) Distance-to-edge gating (optional)
    cv::Mat edgeInv;
    cv::bitwise_not(r.edgeMask8, edgeInv);               // edge pixels -> 0, else 255
    cv::Mat edgeInvBin = (edgeInv != 0);                 // 0/255 -> 0/1
    edgeInvBin.convertTo(edgeInvBin, CV_8U, 255.0);      // ensure 0/255
    cv::distanceTransform(edgeInvBin, r.dist32, cv::DIST_L2, 3);

    cv::Mat nearEdgeMask = (r.dist32 <= float(hp.maxEdgeDistPx));
    nearEdgeMask.convertTo(nearEdgeMask, CV_8U, 255.0);

    qDebug() << srcFun << "2";
    // 3) Normalize grayscale to 0..1 float
    cv::Mat img32;
    fusedGray8Orig.convertTo(img32, CV_32F, 1.0/255.0);

    qDebug() << srcFun << "3";
    // 4) Multi-scale max positive DoG
    r.dogMax32 = cv::Mat::zeros(fusedGray8Orig.size(), CV_32F);
    for (const auto &p : hp.dogScales)
    {
        cv::Mat dog = dogBright32(img32, p.first, p.second);
        cv::max(r.dogMax32, dog, r.dogMax32);
    }

    qDebug() << srcFun << "4";
    // 5) Threshold and (optionally) gate to near-edge
    cv::Mat halo = (r.dogMax32 > hp.dogThresh);
    halo.convertTo(halo, CV_8U, 255.0);

    // If you want gating ON, use this (recommended once FG is solid):
    // cv::bitwise_and(halo, nearEdgeMask, halo);

    qDebug() << srcFun << "5";
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
                cleaned.setTo(255, labels == i);
        }
        halo = cleaned;
    }

    r.haloMask8 = halo;

    qDebug() << srcFun << "6";
    // ------------------------------------------------------------
    // 7) Foreground + edge veto (do NOT bake edges into fgMask8)
    // ------------------------------------------------------------
    ForegroundParams fp;  // or pass in / use a shared tuned default
    fp.preBlurSigma   = 1.2f;
    fp.hfFrac         = 0.15f;
    fp.hfBlurSigma    = 1.0f;
    fp.thinFrac       = 0.050f;
    fp.thinClosePx    = 1;
    fp.thinOpenPx     = 0;
    fp.thinMinAreaPx  = 32;
    fp.darkFrac       = 0.55f;
    fp.darkClosePx    = 8;
    fp.darkMinAreaPx  = 256;
    fp.darkEdgeGatePx = 6;
    fp.edgeDilatePx   = 0;   // keep 0 here; hp.edgeDilatePx already handled r.edgeMask8
    fp.finalDilatePx  = 0;

    // IMPORTANT: use the edge mask we already computed for this halo run
    auto fr = buildForegroundMask(fusedGray8Orig, r.edgeMask8, fp);
    qDebug() << srcFun << "61";

    CV_Assert(!fr.fgMask8.empty());
    CV_Assert(fr.fgMask8.type() == CV_8U);
    CV_Assert(fr.fgMask8.size() == fusedGray8Orig.size());

    // veto = "true foreground" OR "edge safety band"
    assertSameMat(fr.fgMask8, r.edgeMask8, "detectLowpassHalosOrig veto OR");
    cv::Mat veto;
    cv::bitwise_or(fr.fgMask8, r.edgeMask8, veto);
    qDebug() << srcFun << "62";

    CV_Assert(veto.type() == CV_8U);
    CV_Assert(veto.size() == r.haloMask8.size());
    r.haloMask8.setTo(0, veto);

    qDebug() << srcFun << "7";
    // 8) Score: mean DoG inside halo mask
    r.nHalo = cv::countNonZero(r.haloMask8);
    if (r.nHalo > 0)
    {
        cv::Scalar m = cv::mean(r.dogMax32, r.haloMask8);
        r.scoreMean = m[0];
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

// ----- end temp debug section -----

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
    const std::vector<Result> &globals,
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
    hp.edgeThreshFrac = 0.05f;
    hp.edgeDilatePx   = 2;
    hp.maxEdgeDistPx  = 30;
    hp.dogScales      = { {4.0f,8.0f}, {8.0f,16.0f}, {16.0f,32.0f} };
    hp.dogThresh      = 0.015f;
    hp.minAreaPx      = 128;
    hp.closePx        = 3;

    cv::Mat fusedGrayAlign = fusedGray8(roiPadToAlign);
    cv::Mat fusedGray8Orig = fusedGrayAlign(validAreaAlign).clone();

    CV_Assert(!fusedGray8Orig.empty());
    CV_Assert(fusedGray8Orig.type() == CV_8U);
    CV_Assert(fusedGray8Orig.size() == outputColor.size());

    HaloDetectResult haloDetectResult;
    qDebug() << srcFun << "0";
    haloDetectResult = detectLowpassHalosOrig(fusedGray8Orig, hp);
    qDebug() << srcFun << "00";

    dumpHaloDetectorDebugOrig(opt.depthFolderPath,
                              fusedGray8Orig,
                              hp,
                              haloDetectResult
                              );

    // foreground
    ForegroundParams fp;
    // Pre-smoothing for gradient-based detectors (reduces noise-triggered edges)
    fp.preBlurSigma   = 1.2f;
    // High frequency texture mask
    fp.hfFrac         = 0.15f;      // 0.10f;
    fp.hfBlurSigma    = 1;
    // Thin twigs mask (small-scale gradient magnitude)
    fp.thinFrac       = 0.040f;   // 0.025..0.050  (fraction of max grad) 0.035
    fp.thinClosePx    = 0;        // 0..2  (connect along twigs) 1
    fp.thinOpenPx     = 1;        // 0..1  (optional speck cleanup) 0
    fp.thinMinAreaPx  = 32;       // 0..128 (remove tiny islands)
    fp.thinBlurSigma  = 0;        // 0..2   extra blur just for twigs (0 = none)
    fp.thinErodePx    = 1;        // 0..2  (1 usually enough)
    fp.thinMxClipFrac = 0.25f;    // 0.15..0.40 : ignore extreme mx so thinFrac works    // (C) Dark solid interiors (main branch core)
    // Dark solid interiors (main branch core)
    fp.darkFrac       = 0.55f;    // 0.45..0.70  (relative to Otsu threshold)
    fp.darkClosePx    = 8;        // 6..12       (fill interior)
    fp.darkMinAreaPx  = 256;      // 128..1024   (remove small dark blobs)
    // Gate dark solids to be near edges (kills background blobs)
    fp.darkEdgeGatePx = 6;        // 4..10
    // Always protect edges; optionally expand edge band slightly
    fp.edgeDilatePx   = 0;        // 0..2
    // Final expansion of FG (usually 0; keep 0 to avoid fattening)
    fp.finalDilatePx  = 0;        // 0..1

    qDebug() << srcFun << "1";
    auto fr = buildForegroundMask(fusedGray8Orig, haloDetectResult.edgeMask8, fp);
    qDebug() << srcFun << "2";
    dumpForegroundDebug(opt.depthFolderPath, fusedGray8Orig, fp, fr);

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

// ------------------------------------------------------------
// streamDMapSlice  (Pass-1 streamed)
// ------------------------------------------------------------
bool FSFusion::streamDMapSlice(int slice,
                               const cv::Mat& grayAlignPad8,   // CV_8U padded
                               const cv::Mat& colorAlignPad,   // used for outDepth + sanity
                               const Options& /*opt*/,
                               std::atomic_bool* abortFlag,
                               StatusCallback /*statusCallback*/,
                               ProgressCallback progressCallback)
{
    QString srcFun = "FSFusion::streamDMapSlice";
    QString s = QString::number(slice);
    QString msg = "Slice " + s;
    if (G::FSLog) G::log(srcFun, msg);

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    CV_Assert(!grayAlignPad8.empty());
    CV_Assert(grayAlignPad8.type() == CV_8U);

    if (!colorAlignPad.empty())
    {
        CV_Assert(colorAlignPad.size() == grayAlignPad8.size());
        CV_Assert(colorAlignPad.channels() == 1 || colorAlignPad.channels() == 3 || colorAlignPad.channels() == 4);
        CV_Assert(colorAlignPad.depth() == CV_8U || colorAlignPad.depth() == CV_16U);
    }

    // Slice-0 init
    if (slice == 0)
    {
        padSize  = grayAlignPad8.size();

        // If your pipeline already sets alignSize elsewhere, keep it.
        // If not, assume alignSize==padSize for DMap path.
        if (alignSize.width <= 0 || alignSize.height <= 0)
            alignSize = padSize;

        // Track output depth (used by finish conversion)
        if (!colorAlignPad.empty())
            outDepth = colorAlignPad.depth();
        else if (outDepth != CV_8U && outDepth != CV_16U)
            outDepth = CV_8U;

        // Clamp topK to sane range
        dmapParams.topK = std::max(1, std::min(8, dmapParams.topK));

        // Allocate TopK maps in PAD space
        dmapTopKPad.create(padSize, dmapParams.topK);

        dmapActive = true;
        dmapSliceCount = 0;
    }
    else
    {
        if (!dmapActive || !dmapTopKPad.valid())
        {
            qWarning().noquote() << "WARNING:" << srcFun << "called without active DMap state.";
            return false;
        }
        if (grayAlignPad8.size() != padSize)
        {
            qWarning().noquote() << "WARNING:" << srcFun << "padSize mismatch.";
            return false;
        }
    }

    // ---- Focus metric (CV_32F) ----
    auto focusMetric32 = [&](const cv::Mat& gray8) -> cv::Mat
    {
        // 0..1 float
        cv::Mat g32;
        gray8.convertTo(g32, CV_32F, 1.0 / 255.0);

        // pre-blur for stability
        if (dmapParams.scoreSigma > 0.0f)
            cv::GaussianBlur(g32, g32, cv::Size(0,0),
                             dmapParams.scoreSigma, dmapParams.scoreSigma,
                             cv::BORDER_REFLECT);

        // Laplacian magnitude
        int ksz = dmapParams.scoreKSize;
        if (ksz != 3 && ksz != 5) ksz = 3;

        cv::Mat lap32;
        cv::Laplacian(g32, lap32, CV_32F, ksz);

        cv::Mat absLap32;
        cv::absdiff(lap32, cv::Scalar::all(0), absLap32);

        // light blur to reduce hot pixels
        cv::GaussianBlur(absLap32, absLap32, cv::Size(0,0),
                         0.8, 0.8, cv::BORDER_REFLECT);

        return absLap32; // CV_32F
    };

    cv::Mat score32 = focusMetric32(grayAlignPad8);

    // Update top-K with current slice score
    updateTopKPerSlice(dmapTopKPad, score32, (uint16_t)std::max(0, slice));

    dmapSliceCount++;

    // if (progressCallback) progressCallback();
    return true;
}


// ------------------------------------------------------------
// streamDMapFinish (Pass-2 pyramid blend, halo suppression hooks)
//
// Key ideas:
//   1) Crop TopK maps PAD -> ALIGN -> ORIG
//   2) Build soft TopK weights W (orig space)
//   3) OPTIONAL: edge-aware smoothing of weights *only inside boundary band*,
//      gated by confidence (prevents bleeding across real edges)
//   4) Pass-2 pyramid accumulation:
//        - per-slice weight map w32 from TopK+W
//        - OPTIONAL: lowpass equalization per slice using a SAFE interior mask
//          (fit on *lowpass-blurred* color, apply to full color)
//        - accumulate pyramids (optionally with hard lowpass behavior if you have it)
//
// Notes:
//   - This version avoids using "guide slice 0" (single slice is unreliable).
//     The guide for edge-aware smoothing is derived from top-1 focus score.
//   - SAFE mask uses W.band8 (already a boundary band), not boundary8.
// ------------------------------------------------------------
bool FSFusion::streamDMapFinish(cv::Mat& outputColor,
                                const Options& /*opt*/,
                                cv::Mat& depthIndex16,
                                const QStringList& inputPaths,
                                const std::vector<Result>& globals,
                                std::atomic_bool* abortFlag,
                                StatusCallback /*statusCallback*/,
                                ProgressCallback progressCallback)
{
    QString srcFun = "FSFusion::streamDMapFinish";
    if (G::FSLog) G::log(srcFun);

    // ----------------------------
    // State checks
    // ----------------------------
    if (!dmapActive || !dmapTopKPad.valid())
    {
        qWarning().noquote() << "WARNING:" << srcFun << "DMap state not initialized.";
        return false;
    }

    const int N = inputPaths.size();
    if (N <= 0 || (int)globals.size() != N)
    {
        qWarning().noquote() << "WARNING:" << srcFun << "inputPaths/globals size mismatch.";
        return false;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    if (alignSize.width <= 0 || alignSize.height <= 0 ||
        padSize.width   <= 0 || padSize.height   <= 0 ||
        validAreaAlign.width <= 0 || validAreaAlign.height <= 0 ||
        origSize.width <= 0 || origSize.height <= 0)
    {
        qWarning().noquote() << "WARNING:" << srcFun
                             << "Missing geometry state (alignSize/padSize/validAreaAlign/origSize).";
        return false;
    }

    // ----------------------------
    // PAD -> ALIGN crop rect
    // ----------------------------
    cv::Rect roiPadToAlign(0, 0, alignSize.width, alignSize.height);
    if (padSize != alignSize)
    {
        const int padW = padSize.width  - alignSize.width;
        const int padH = padSize.height - alignSize.height;
        if (padW < 0 || padH < 0)
        {
            qWarning().noquote() << "WARNING:" << srcFun << "padSize smaller than alignSize.";
            return false;
        }
        roiPadToAlign.x = padW / 2;
        roiPadToAlign.y = padH / 2;
        roiPadToAlign.width  = alignSize.width;
        roiPadToAlign.height = alignSize.height;
    }

    if (!rectInside(roiPadToAlign, dmapTopKPad.sz))
    {
        qWarning().noquote() << "WARNING:" << srcFun << "roiPadToAlign out of bounds for TopK PAD maps.";
        return false;
    }

    cv::Size alignSz(roiPadToAlign.width, roiPadToAlign.height);
    if (!rectInside(validAreaAlign, alignSz))
    {
        qWarning().noquote() << "WARNING:" << srcFun << "validAreaAlign out of bounds for ALIGN crop.";
        return false;
    }

    const cv::Size origSz(validAreaAlign.width, validAreaAlign.height);
    if (origSz != origSize) origSize = origSz;

    auto countHorizontalDiffs16 = [](const cv::Mat& m16) -> long long
    {
        CV_Assert(m16.type() == CV_16U);
        long long diffCount = 0;
        for (int y = 0; y < m16.rows; ++y)
        {
            const uint16_t* r = m16.ptr<uint16_t>(y);
            for (int x = 0; x < m16.cols - 1; ++x)
                diffCount += (r[x] != r[x + 1]);
        }
        return diffCount;
    };

    // ----------------------------
    // Crop TopK maps: PAD -> ALIGN -> ORIG
    // Keep TopK consistent.
    // ----------------------------
    TopKMaps topkOrig;
    topkOrig.create(origSz, dmapTopKPad.K);

    for (int k = 0; k < dmapTopKPad.K; ++k)
    {
        cv::Mat idxAlign = dmapTopKPad.idx16[k](roiPadToAlign);
        cv::Mat scAlign  = dmapTopKPad.score32[k](roiPadToAlign);
        topkOrig.idx16[k]   = idxAlign(validAreaAlign).clone();
        topkOrig.score32[k] = scAlign(validAreaAlign).clone();
    }

    {
        const cv::Mat& d = topkOrig.idx16[0];
        const long long diffCount = countHorizontalDiffs16(d);
        const long long total = (long long)d.rows * (d.cols - 1);
        qDebug() << "top1 diffs (ORIG, pre-weight) =" << diffCount << "/" << total
                 << " frac=" << (double)diffCount / (double)total;
    }

    // Build a normalized top1 score 0..1 (robust enough)
    double sMin=0, sMax=0;
    cv::minMaxLoc(topkOrig.score32[0], &sMin, &sMax);
    const float invS = (float)(1.0 / std::max(1e-6, sMax - sMin));
    cv::Mat score01 = (topkOrig.score32[0] - (float)sMin) * invS;

    // ----------------------------
    // SANITY: Top-1 index range must match slice count
    // ----------------------------
    {
        double idxMin = 0, idxMax = 0;
        cv::minMaxLoc(topkOrig.idx16[0], &idxMin, &idxMax);

        qDebug() << "Top1 idx min/max =" << idxMin << idxMax << " N=" << N;

        CV_Assert(idxMin >= 0);
        CV_Assert(idxMax < N);
    }

    // ----------------------------
    // Build weights from TopK
    // ----------------------------
    DMapWeights W;
    buildDMapWeightsFromTopK(topkOrig, dmapParams, W);

    // ----------------------------
    // Choose a representative slice for "detail guide":
    // pick slice with maximum total soft weight over the image
    // ----------------------------
    int bgSlice = 0;
    double bestSum = -1.0;
    for (int s = 0; s < N; ++s)
    {
        cv::Mat ws = buildSliceWeightMap32(topkOrig, W, s); // CV_32F
        const double sum = cv::sum(ws)[0];
        if (sum > bestSum) { bestSum = sum; bgSlice = s; }
    }
    qDebug() << "bgSlice (early) =" << bgSlice << " bestSum =" << bestSum;

    // ----------------------------
    // Build dmapGrayOrig: a real grayscale guide in ORIG space for detail protection.
    // Use bgSlice (most "claimed" slice) as representative.
    // ----------------------------
    cv::Mat dmapGrayOrig; // CV_8U, ORIG size
    {
        cv::Mat grayTmp, colorTmp;
        if (!FSLoader::loadAlignedSliceOrig(bgSlice, inputPaths, globals,
                                            roiPadToAlign, validAreaAlign,
                                            grayTmp, colorTmp))
        {
            qWarning().noquote() << "WARNING:" << srcFun
                                 << "loadAlignedSliceOrig failed for bgSlice" << bgSlice;
            return false;
        }

        if (grayTmp.empty())
        {
            qWarning().noquote() << "WARNING:" << srcFun << "grayTmp empty for bgSlice" << bgSlice;
            return false;
        }

        // Ensure CV_8U
        if (grayTmp.type() != CV_8U)
            grayTmp.convertTo(dmapGrayOrig, CV_8U);
        else
            dmapGrayOrig = grayTmp;

        if (dmapGrayOrig.size() != origSz)
        {
            qWarning().noquote() << "WARNING:" << srcFun << "dmapGrayOrig size mismatch"
                                 << dmapGrayOrig.cols << dmapGrayOrig.rows
                                 << "expected" << origSz.width << origSz.height;
            return false;
        }

        dmapGrayOrig = dmapGrayOrig.clone(); // own the data
    }

    // ------------------------------------------------------------
    // Build edge veto mask from REAL IMAGE edges (not win16 labels)
    // This is the only "do-not-override" edge protection you want.
    // ------------------------------------------------------------
    cv::Mat edgeVeto8; // CV_8U 0/255
    {
        CV_Assert(!dmapGrayOrig.empty() && dmapGrayOrig.type() == CV_8U);

        cv::Mat gBlur;
        cv::GaussianBlur(dmapGrayOrig, gBlur, cv::Size(0,0), 1.0, 1.0, cv::BORDER_REFLECT);

        cv::Mat gx, gy;
        cv::Sobel(gBlur, gx, CV_16S, 1, 0, 3);
        cv::Sobel(gBlur, gy, CV_16S, 0, 1, 3);

        cv::Mat ax, ay, mag;
        cv::convertScaleAbs(gx, ax);
        cv::convertScaleAbs(gy, ay);
        cv::addWeighted(ax, 1.0, ay, 1.0, 0.0, mag); // CV_8U

        // Strong edges = top 5-10% of gradient magnitudes
        // Use percentile over the whole image (NOT masked by lowConf).
        const int edgeThr = std::max(8, percentileU8All(mag, 0.94f)); // try 0.88..0.95
        edgeVeto8 = (mag >= edgeThr);
        edgeVeto8.convertTo(edgeVeto8, CV_8U, 255.0);

        // Expand a bit to protect edges
        cv::dilate(edgeVeto8, edgeVeto8, seEllipse(2)); // try 2..4

        qDebug() << "edgeThr(p90)=" << edgeThr
                 << " edgeVeto px =" << cv::countNonZero(edgeVeto8);
    }

    // ----------------------------
    // Confidence map (Top-2)
    // ----------------------------
    cv::Mat conf01 = computeConfidence01_Top2(topkOrig, dmapParams.confEps); // CV_32F [0..1]
    {
        double cmin, cmax;
        cv::minMaxLoc(conf01, &cmin, &cmax);
        const cv::Scalar mean = cv::mean(conf01);
        qDebug() << "DMap conf01 min/max/mean =" << cmin << cmax << mean[0];

        cv::Mat low = (conf01 < dmapParams.confLow);
        low.convertTo(low, CV_8U, 255.0);
        qDebug() << "DMap lowConf px =" << cv::countNonZero(low)
                 << " / total =" << conf01.total();
    }

    // ----------------------------
    // Optional: edge-aware smoothing inside W.band8
    // ----------------------------
    if (dmapParams.enableEdgeAwareBandSmoothing &&
        topkOrig.K >= 2 &&
        !W.band8.empty() &&
        cv::countNonZero(W.band8) > 0)
    {
        qDebug() << "boundary px =" << (W.boundary8.empty() ? 0 : cv::countNonZero(W.boundary8))
                 << "band px =" << cv::countNonZero(W.band8);

        cv::Mat guide01 = topkOrig.score32[0].clone();
        double mn = 0, mx = 0;
        cv::minMaxLoc(guide01, &mn, &mx);
        const float inv = (float)(1.0 / std::max(1e-6, mx - mn));
        guide01 = (guide01 - (float)mn) * inv;

        smoothWeightsEdgeAwareInBand(W.w32, W.band8, guide01, conf01, dmapParams);
    }

    // ----------------------------
    // Winner index map + stabilize ONLY uncertain pixels
    // ----------------------------
    cv::Mat win16 = topkOrig.idx16[0].clone(); // CV_16U slice indices

    cv::Mat lowConf8 = (conf01 < dmapParams.confLow);
    lowConf8.convertTo(lowConf8, CV_8U, 255.0);

    if (!W.boundary8.empty())
        lowConf8.setTo(0, W.boundary8); // protect boundary pixels

    if (dmapParams.uncertaintyDilatePx > 0)
        cv::dilate(lowConf8, lowConf8, seEllipse(dmapParams.uncertaintyDilatePx));

    const long long before = countHorizontalDiffs16(win16);
    win16 = modeFilterIdx16(win16, lowConf8, /*radius=*/2);

    // ------------------------------------------------------------
    // Build a SMOOTHED winner map for boundary detection only.
    // This kills pixel-scale salt/pepper that makes boundary ~= whole image.
    // ------------------------------------------------------------
    cv::Mat winForEdges16 = win16;
    if (win16.cols >= 5 && win16.rows >= 5)
        cv::medianBlur(win16, winForEdges16, 5);   // labels: median works well

    // Light global cleanup for label speckle (doesn't destroy big edges much)
    if (win16.cols >= 5 && win16.rows >= 5)
    {
        cv::Mat tmp;
        cv::medianBlur(win16, tmp, 5);
        win16 = tmp;
    }
    const long long after  = countHorizontalDiffs16(win16);
    const long long total  = (long long)win16.rows * (win16.cols - 1);
    qDebug() << "win16 diffs AFTER mode (masked) =" << after << "/" << total
             << " (before=" << before << ")";

    // ------------------------------------------------------------
    // Rebuild edge/band from SMOOTHED winners so band doesn't explode.
    //  - bandStrong8: protect real strong edges (for overrides / bg masks)
    // ------------------------------------------------------------
    // boundaryThin from smoothed winners
    cv::Mat boundaryThin8 = boundaryThinFromIdx16(winForEdges16);

    // "strong" pixels (either high confidence OR strong focus score)
    cv::Mat strong8 = (conf01 > dmapParams.confHigh) | (score01 > 0.30f);
    strong8.convertTo(strong8, CV_8U, 255.0);

    // strong boundaries => protect overrides
    cv::Mat boundaryStrong8;
    cv::bitwise_and(boundaryThin8, strong8, boundaryStrong8);

    cv::Mat bandStrong8;
    cv::dilate(boundaryStrong8, bandStrong8, seEllipse(2)); // 2..3

    // uncertain boundaries => smooth weights here (NOT strong edges)
    cv::Mat notStrong8;
    cv::bitwise_not(strong8, notStrong8);

    cv::Mat boundaryUncertain8;
    cv::bitwise_and(boundaryThin8, notStrong8, boundaryUncertain8);

    // keep smoothing band tight
    cv::dilate(boundaryUncertain8, W.band8, seEllipse(2));  // 2..3

    // keep W.boundary8 as strong boundaries if you still want it
    W.boundary8 = boundaryStrong8;

    qDebug() << "boundaryThin px =" << cv::countNonZero(boundaryThin8)
             << "boundaryStrong px =" << cv::countNonZero(boundaryStrong8)
             << "bandStrong px =" << cv::countNonZero(bandStrong8)
             << "bandUncertain px =" << cv::countNonZero(W.band8);

    // ----------------------------
    // Smooth-background mask (texture-based):
    // Use real image texture from dmapGrayOrig, NOT focus score.
    // bg8 = low texture + low confidence, away from boundary band
    // ----------------------------
    cv::Mat bg8;
    {
        CV_Assert(!dmapGrayOrig.empty());
        CV_Assert(dmapGrayOrig.type() == CV_8U);
        CV_Assert(dmapGrayOrig.size() == origSz);

        cv::Mat g = dmapGrayOrig;

        // kill sensor noise / grain BEFORE measuring "texture"
        cv::Mat gBlur;
        cv::GaussianBlur(g, gBlur, cv::Size(0,0), 1.2, 1.2, cv::BORDER_REFLECT);

        // Texture magnitude from Sobel
        cv::Mat gx, gy, mag;
        cv::Sobel(gBlur, gx, CV_16S, 1, 0, 3);
        cv::Sobel(gBlur, gy, CV_16S, 0, 1, 3);
        cv::convertScaleAbs(gx, gx);
        cv::convertScaleAbs(gy, gy);
        cv::addWeighted(gx, 1.0, gy, 1.0, 0.0, mag);   // CV_8U

        // Low confidence threshold for "likely background / uncertain"
        cv::Mat lowConf = (conf01 < dmapParams.bgConfThr);
        lowConf.convertTo(lowConf, CV_8U, 255.0);

        // Choose texThr as the 25th percentile of mag within lowConf.
        // Raise pct -> captures more background; lower pct -> more conservative.
        // try 0.60..0.80
        int texThr = percentileU8Masked(mag, lowConf, 0.85f);
        texThr = std::max(1, std::min(254, texThr));
        qDebug() << "texThr(p85)=" << texThr;
        cv::Mat lowTex = (mag < texThr);
        lowTex.convertTo(lowTex, CV_8U, 255.0);

        // low focus-energy area (this is key)
        cv::Mat lowScore = (score01 < dmapParams.bgScoreThr);
        lowScore.convertTo(lowScore, CV_8U, 255.0);

        // bg candidate = lowTex AND lowScore AND lowConf   <-- ADD THIS
        cv::Mat tmp;
        cv::bitwise_and(lowTex, lowScore, tmp);
        cv::bitwise_and(tmp, lowConf, bg8);

        // Never force background near boundaries (halo/detail risk)
        if (!edgeVeto8.empty())
        {
            cv::Mat veto = edgeVeto8.clone();
            cv::dilate(veto, veto, seEllipse(2)); // small (2..5)
            bg8.setTo(0, veto);
        }

        // Cleanup: connect regions and remove speckle
        cv::morphologyEx(bg8, bg8, cv::MORPH_OPEN,  seEllipse(1));
        cv::morphologyEx(bg8, bg8, cv::MORPH_CLOSE, seEllipse(3));

        qDebug() << "bg px =" << cv::countNonZero(bg8)
                 << " / total =" << bg8.total();

        qDebug() << "lowConf px =" << cv::countNonZero(lowConf)
                 << "lowTex px =" << cv::countNonZero(lowTex)
                 << "bg px =" << cv::countNonZero(bg8);
    }

    // redo bgSlice (weight sum restricted to bg8 mask)
    bgSlice = 0;
    double best = -1.0;

    cv::Mat tmp(topkOrig.sz, CV_32F);  // reuse buffer
    for (int s = 0; s < N; ++s)
    {
        cv::Mat ws = buildSliceWeightMap32(topkOrig, W, s); // CV_32F
        tmp.setTo(0.0f);
        ws.copyTo(tmp, bg8);                                // keep ws only inside bg8
        const double sumBg = cv::sum(tmp)[0];
        if (sumBg > best) { best = sumBg; bgSlice = s; }
    }
    qDebug() << "bgSlice(bgmask) =" << bgSlice << " bestBgSum =" << best;

    // ----------------------------
    // Build additional hardening mask (NOT bg):
    // extremely uncertain + low score + LOW DETAIL, away from boundaries
    // ----------------------------
    cv::Mat lowHard8;
    {
        // score01 in 0..1
        double sMin=0, sMax=0;
        cv::minMaxLoc(topkOrig.score32[0], &sMin, &sMax);
        const float invS = (float)(1.0 / std::max(1e-6, sMax - sMin));
        cv::Mat score01 = (topkOrig.score32[0] - (float)sMin) * invS;

        // 1) Base condition: very uncertain + low focus score
        const float hardConfThr  = 0.06f;  // 0.05..0.08
        const float hardScoreThr = 0.05f;  // 0.04..0.08
        lowHard8 = (conf01 < hardConfThr) & (score01 < hardScoreThr);
        lowHard8.convertTo(lowHard8, CV_8U, 255.0);

        // 2) DO NOT harden near boundaries (halo/detail loss)
        if (!bandStrong8.empty())
        {
            cv::Mat veto = bandStrong8.clone();
            cv::dilate(veto, veto, seEllipse(6)); // 4..10
            lowHard8.setTo(0, veto);
        }

        // 3) If it's smooth background (bg8), do NOT lowHard it (bg forcing handles it)
        if (!bg8.empty())
            lowHard8.setTo(0, bg8);

        // 4) NEW: DO NOT harden where there is real local DETAIL
        //    Use a cheap “detail energy” from Laplacian magnitude on a grayscale guide.
        cv::Mat detail01;
        {
            CV_Assert(!dmapGrayOrig.empty());
            CV_Assert(dmapGrayOrig.type() == CV_8U);
            CV_Assert(dmapGrayOrig.size() == origSz);

            cv::Mat g32;
            dmapGrayOrig.convertTo(g32, CV_32F, 1.0f/255.0f);

            cv::Mat lap;
            cv::Laplacian(g32, lap, CV_32F, 3);

            cv::Mat absLap = cv::abs(lap);

            // Normalize to 0..1 robustly
            double lmn=0, lmx=0;
            cv::minMaxLoc(absLap, &lmn, &lmx);
            const float invL = (float)(1.0 / std::max(1e-6, lmx - lmn));
            detail01 = (absLap - (float)lmn) * invL;
        }

        // Threshold: anything above this is "detail" => protect from lowHard
        const float protectDetailThr = 0.12f; // try 0.08..0.20
        cv::Mat protectDetail8 = (detail01 > protectDetailThr);
        protectDetail8.convertTo(protectDetail8, CV_8U, 255.0);

        // Expand protection slightly
        cv::dilate(protectDetail8, protectDetail8, seEllipse(1));

        // Remove lowHard where detail exists
        lowHard8.setTo(0, protectDetail8);

        // 5) Optional cleanup: remove speckle islands
        cv::erode(lowHard8, lowHard8, seEllipse(1));
        cv::dilate(lowHard8, lowHard8, seEllipse(2));

        qDebug() << "lowHard px =" << cv::countNonZero(lowHard8);
    }

    // ----------------------------
    // Build FINAL overrides:
    // - bg8      -> force bgSlice
    // - lowHard8 -> force stabilized win16
    // ----------------------------
    {
        cv::Mat override8;
        cv::bitwise_or(bg8, lowHard8, override8);

        // Don't override on real image edges
        if (!edgeVeto8.empty())
            override8.setTo(0, edgeVeto8);

        // (Optional) also don't override in uncertainty band
        if (!bandStrong8.empty())
            override8.setTo(0, bandStrong8);

        cv::Mat winForced16 = win16.clone();

        // Force bgSlice only in bg8 (and only where override is allowed)
        cv::Mat bgAllowed;
        cv::bitwise_and(bg8, override8, bgAllowed);

        winForced16.setTo((uint16_t)bgSlice, bgAllowed);

        W.overrideMask8   = override8;
        W.overrideWinner16 = winForced16;

        double mn=0, mx=0;
        cv::minMaxLoc(W.overrideWinner16, &mn, &mx);
        qDebug() << "override px =" << cv::countNonZero(W.overrideMask8)
                 << " winner min/max =" << mn << mx;
    }
    depthIndex16 = W.overrideWinner16.clone();

    // ----------------------------
    // Pyramid setup
    // ----------------------------
    int levels = dmapParams.pyrLevels;
    if (levels <= 0) levels = levelsForSize(origSz);
    levels = std::max(3, std::min(10, levels));

    if (dmapParams.hardFromLevel < 0)
        dmapParams.hardFromLevel = std::max(0, levels - 2);
    if (dmapParams.vetoFromLevel < 0)
        dmapParams.vetoFromLevel = std::max(0, levels - 2);

    // Use final winners for index pyramid (important!)
    std::vector<cv::Mat> idxPyr16;
    buildIndexPyrNearest(W.overrideWinner16, levels, idxPyr16);

    std::vector<cv::Mat> vetoPyr8;
    dmapParams.vetoFromLevel = levels - 2;   // or levels - 1 for residual only
    if (dmapParams.enableDepthGradLowpassVeto)
        buildDepthEdgeVetoPyr(idxPyr16, levels, dmapParams, vetoPyr8);

    if (!vetoPyr8.empty())
    {
        const double frac0 = (double)cv::countNonZero(vetoPyr8[0]) / (double)vetoPyr8[0].total();
        qDebug() << "veto level0 frac =" << frac0;

        // If veto covers more than ~35% at full res, it's too aggressive -> disable it.
        if (frac0 > 0.35)
        {
            qDebug() << "veto too dense, disabling depthGrad veto";
            vetoPyr8.clear();
        }
        for (int i = 0; i < levels; ++i)
            qDebug() << "veto level" << i << "px =" << cv::countNonZero(vetoPyr8[i]);
    }

    PyrAccum A;
    A.reset(origSz, levels);

    LowpassRefStats lpRef;

    // ----------------------------
    // Pass-2: stream slices
    // ----------------------------
    cv::Mat sumW(topkOrig.sz, CV_32F, cv::Scalar(0));
    for (int s = 0; s < N; ++s)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

        cv::Mat w32 = buildSliceWeightMap32(topkOrig, W, s);
        sumW += w32;

        if (!W.overrideMask8.empty())
        {
            cv::Mat t;
            w32.convertTo(t, CV_8U, 255.0);
            cv::Mat inter;
            cv::bitwise_and(t, W.overrideMask8, inter);
            qDebug() << "slice" << s
                     << "override weight>0 px =" << cv::countNonZero(inter);
        }

        const double wsum = cv::sum(w32)[0];
        if (wsum <= 1e-6)
        {
            if (progressCallback) progressCallback();
            continue;
        }

        // SAFE mask for lowpass equalization fitting
        cv::Mat safe = (w32 > 0.95f);
        safe.convertTo(safe, CV_8U, 255.0);

        if (!W.band8.empty())
        {
            cv::Mat veto = W.band8.clone();
            if (dmapParams.lowpassEqBandVetoPx > 0)
                cv::dilate(veto, veto, seEllipse(dmapParams.lowpassEqBandVetoPx));
            safe.setTo(0, veto);
        }

        cv::erode(safe, safe, seEllipse(2));
        const int nSafe = cv::countNonZero(safe);

        cv::Mat gray8Orig, colorOrig;
        if (!FSLoader::loadAlignedSliceOrig(s, inputPaths, globals,
                                            roiPadToAlign, validAreaAlign,
                                            gray8Orig, colorOrig))
        {
            qWarning().noquote() << "WARNING:" << srcFun << "loadAlignedSliceOrig failed slice" << s;
            return false;
        }

        if (colorOrig.channels() == 1)
            cv::cvtColor(colorOrig, colorOrig, cv::COLOR_GRAY2BGR);
        else if (colorOrig.channels() == 4)
            cv::cvtColor(colorOrig, colorOrig, cv::COLOR_BGRA2BGR);

        if (colorOrig.channels() != 3 || colorOrig.size() != origSz)
        {
            qWarning().noquote() << "WARNING:" << srcFun << "colorOrig shape mismatch slice" << s;
            return false;
        }

        cv::Mat color32;
        if (colorOrig.depth() == CV_8U)
            colorOrig.convertTo(color32, CV_32FC3, 1.0);
        else if (colorOrig.depth() == CV_16U)
            colorOrig.convertTo(color32, CV_32FC3, 1.0);
        else
        {
            qWarning().noquote() << "WARNING:" << srcFun << "unsupported color depth" << colorOrig.depth();
            return false;
        }

        if (dmapParams.enableLowpassEqualization &&
            nSafe >= dmapParams.lowpassEqMinPixels)
        {
            cv::Mat lowpass32 = color32.clone();
            cv::GaussianBlur(lowpass32, lowpass32, cv::Size(0, 0),
                             dmapParams.lowpassEqSigma,
                             dmapParams.lowpassEqSigma,
                             cv::BORDER_REFLECT);

            cv::Vec3f gain, off;
            if (fitLowpassEq(lowpass32, safe, lpRef, gain, off))
                applyGainOffsetBGR(color32, gain, off, dmapParams.lowpassEqStrength);
        }

        accumulateSlicePyr(A, color32, w32, idxPyr16,
                           dmapParams.enableDepthGradLowpassVeto ? &vetoPyr8 : nullptr,
                           s, dmapParams, levels,
                           /*weightBlurSigma=*/0.0f);

        if (progressCallback) progressCallback();
    }

    {
        double mn=0, mx=0;
        cv::minMaxLoc(sumW, &mn, &mx);
        cv::Scalar mean = cv::mean(sumW);
        qDebug() << "sumW min/max/mean =" << mn << mx << mean[0];
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // ----------------------------
    // Finalize blend
    // ----------------------------
    cv::Mat out32 = finalizeBlend(A, 1e-10f);

    if (outDepth == CV_16U)
    {
        cv::max(out32, 0.0f, out32);
        cv::min(out32, 65535.0f, out32);
        cv::Mat out16;
        out32.convertTo(out16, CV_16UC3, 1.0);
        outputColor = out16;
    }
    else
    {
        cv::max(out32, 0.0f, out32);
        cv::min(out32, 255.0f, out32);
        cv::Mat out8;
        out32.convertTo(out8, CV_8UC3, 1.0);
        outputColor = out8;
    }

    dmapDiagnostics(dmapParams);
    dmapDiagnosticsDerived(dmapParams, origSz, N, levels);

    // ----------------------------
    // Housekeeping
    // ----------------------------
    dmapTopKPad.reset();
    dmapActive = false;
    dmapSliceCount = 0;

    return true;
}

// end StreamPMax pipeline

