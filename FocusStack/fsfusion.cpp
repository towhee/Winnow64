#include "fsfusion.h"
#include "Main/global.h"

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

// end StreamPMax pipeline

