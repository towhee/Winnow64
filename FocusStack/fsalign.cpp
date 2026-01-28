#include "FSAlign.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/video.hpp>
#include <cmath>

// SliceBySlice
#include "Main/global.h"
// #include "fsloader.h"

/*
FSAlign implements the image alignment stage of the focus stacking pipeline.
Its responsibility is to geometrically align each slice to a reference image
and to normalize appearance differences (contrast and white balance) so that
later focus fusion operates on consistent data.

The design closely follows Petteri Aimonen’s Task_Align logic, translated into
OpenCV-based C++ code with explicit separation of local alignment, global
accumulation, and application steps.

CORE RESPONSIBILITIES

1. Geometric alignment
   - Uses OpenCV’s ECC (Enhanced Correlation Coefficient) method.
   - Estimates an affine transform between source and reference grayscale
     images.
   - Supports multi-resolution alignment:
       * low-resolution “rough” pass
       * high-resolution refinement pass
   - Alignment is constrained to a valid image region (ROI) to avoid padded or
     invalid areas.

2. Photometric normalization
   - Matches contrast across the image using a low-order polynomial model
     (spatially varying contrast).
   - Optionally matches per-channel white balance for color images.
   - Ensures aligned images have consistent appearance before fusion.

3. Transform accumulation
   - Local transforms (between adjacent slices) are accumulated into a global
     transform relative to the reference slice.
   - Contrast and white balance parameters are also accumulated so each slice
     can be corrected in a single step.

4. Valid-area tracking
   - Computes the region of each image that remains valid after applying
     geometric transforms.
   - Ensures later stages operate only on pixels that are meaningful across
     the stack.

INTERNAL STRUCTURE

An anonymous namespace contains low-level helper functions:
   - Math helpers (square, rounding with dithering).
   - Contrast and white balance application routines.
   - ECC-based transform estimation with scaling and masking.
   - Solvers for contrast and white balance coefficients.
   - Geometry helpers for transforming points and computing valid areas.

These helpers are intentionally hidden to keep the public API focused and
stable.

PUBLIC API (FSAlign namespace)

makeIdentity()
   - Creates a trivial alignment result (identity transform) for the reference
     image.

computeLocal()
   - Computes alignment of a source image to the reference:
       * rough low-resolution alignment
       * optional contrast fitting
       * optional white balance fitting
       * refined high-resolution alignment
   - Produces a Result containing:
       * affine transform
       * contrast coefficients
       * white balance coefficients
       * valid image area

accumulate()
   - Combines a local Result with a previously accumulated global Result:
       * stacks affine transforms
       * stacks contrast coefficients
       * stacks white balance coefficients
       * updates the valid area
   - Mirrors how Petteri’s original code builds global alignment incrementally.

applyTransform()
   - Applies an affine transform to an image using cubic interpolation and
     reflected borders.

applyContrastWhiteBalance()
   - Applies the accumulated contrast and white balance correction to an image.

ROLE IN THE FOCUS STACK PIPELINE

FSAlign sits between image loading and focus analysis/fusion.
Its output ensures that:
   - all slices are geometrically aligned,
   - brightness and color differences are minimized,
   - each slice has a known valid region.

This makes downstream focus metrics, depth estimation, and fusion more robust
and predictable.
*/

namespace {

inline float sq(float x) { return x * x; }

// Round and dither as in Petteri
inline int round_and_dither(float value, float &delta)
{
    int intval = static_cast<int>(value + delta);
    delta += value - intval;
    return std::min(255, std::max(0, intval));
}

static inline void ensureIdentityContrast(cv::Mat& c)
{
    if (c.rows != 5 || c.cols != 1 || c.type() != CV_32F)
    {
        c.create(5, 1, CV_32F);
        c = 0.0f;
        c.at<float>(0,0) = 1.0f;
    }
}

static inline void ensureIdentityWB(cv::Mat& wb)
{
    if (wb.rows != 6 || wb.cols != 1 || wb.type() != CV_32F)
    {
        wb.create(6, 1, CV_32F);
        wb = 0.0f;
        wb.at<float>(1,0) = 1.0f;
        wb.at<float>(3,0) = 1.0f;
        wb.at<float>(5,0) = 1.0f;
    }
}

void apply_contrast_whitebalance_internal(cv::Mat &img,
                                          const cv::Mat &contrast,
                                          const cv::Mat &whitebalance)
{
    // Defensive: this implementation is 8-bit only.
    CV_Assert(img.depth() == CV_8U);

    if (img.channels() == 1)
    {
        // Grayscale: contrast only
        for (int y = 0; y < img.rows; ++y)
        {
            float delta = 0.0f;
            for (int x = 0; x < img.cols; ++x)
            {
                float yd = (y - img.rows / 2.0f) / static_cast<float>(img.rows);
                float xd = (x - img.cols / 2.0f) / static_cast<float>(img.cols);

                float c = contrast.at<float>(0)
                          + xd * (contrast.at<float>(1) + contrast.at<float>(2) * xd)
                          + yd * (contrast.at<float>(3) + contrast.at<float>(4) * yd);

                uint8_t v = img.at<uint8_t>(y, x);
                float f = v * c;
                v = static_cast<uint8_t>(round_and_dither(f, delta));
                img.at<uint8_t>(y, x) = v;
            }
        }
    }
    else
    {
        // Color: contrast + whitebalance
        CV_Assert(!whitebalance.empty());
        CV_Assert(whitebalance.type() == CV_32F);
        CV_Assert(whitebalance.total() == 6);

        for (int y = 0; y < img.rows; ++y)
        {
            float delta[3] = {0.0f, 0.0f, 0.0f};

            for (int x = 0; x < img.cols; ++x)
            {
                float yd = (y - img.rows / 2.0f) / static_cast<float>(img.rows);
                float xd = (x - img.cols / 2.0f) / static_cast<float>(img.cols);

                float c = contrast.at<float>(0)
                          + xd * (contrast.at<float>(1) + contrast.at<float>(2) * xd)
                          + yd * (contrast.at<float>(3) + contrast.at<float>(4) * yd);

                cv::Vec3b v = img.at<cv::Vec3b>(y, x);

                float b = v[0] * c * whitebalance.at<float>(1) + whitebalance.at<float>(0);
                float g = v[1] * c * whitebalance.at<float>(3) + whitebalance.at<float>(2);
                float r = v[2] * c * whitebalance.at<float>(5) + whitebalance.at<float>(4);

                v[0] = static_cast<uint8_t>(round_and_dither(b, delta[0]));
                v[1] = static_cast<uint8_t>(round_and_dither(g, delta[1]));
                v[2] = static_cast<uint8_t>(round_and_dither(r, delta[2]));

                img.at<cv::Vec3b>(y, x) = v;
            }
        }
    }
}

// ECC alignment at a given max_resolution, modeled after Task_Align::match_transform.
void match_transform(const cv::Mat &refGray,
                     const cv::Mat &srcGray,
                     const cv::Rect &roi,
                     cv::Mat &transform,
                     const cv::Mat &contrast,
                     const cv::Mat &whitebalance,
                     int max_resolution,
                     bool rough)
{
    cv::Mat ref, src, mask;

    int resolution = std::max(refGray.cols, refGray.rows);
    float scale_ratio = 1.0f;

    if (resolution <= max_resolution)
    {
        ref = refGray;
        srcGray.copyTo(src);
    }
    else
    {
        scale_ratio = max_resolution / static_cast<float>(resolution);
        cv::resize(refGray, ref, cv::Size(), scale_ratio, scale_ratio, cv::INTER_AREA);
        cv::resize(srcGray, src, cv::Size(), scale_ratio, scale_ratio, cv::INTER_AREA);
    }

    mask.create(ref.rows, ref.cols, CV_8U);
    mask = 0;

    cv::Rect scaledRoi(
        static_cast<int>(roi.x * scale_ratio),
        static_cast<int>(roi.y * scale_ratio),
        static_cast<int>(roi.width * scale_ratio),
        static_cast<int>(roi.height * scale_ratio));
    scaledRoi &= cv::Rect(0, 0, mask.cols, mask.rows);
    if (scaledRoi.width > 0 && scaledRoi.height > 0)
        mask(scaledRoi) = 255;

    // Apply contrast only for grayscale when align
    cv::Mat srcAdj = src.clone();
    apply_contrast_whitebalance_internal(srcAdj, contrast, cv::Mat()); // only contrast is used for 1ch

    // Adjust translation terms to scaled space
    transform.at<float>(0, 2) *= scale_ratio;
    transform.at<float>(1, 2) *= scale_ratio;

    int maxCount = rough ? 25 : 50;
    double eps = rough ? 0.01 : 0.001;

    cv::TermCriteria criteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS,
                              maxCount, eps);

    cv::findTransformECC(
        srcAdj, ref,
        transform,
        cv::MOTION_AFFINE,
        criteria,
        mask,
        rough ? 1 : 3
        );

    // Back to original scale
    transform.at<float>(0, 2) /= scale_ratio;
    transform.at<float>(1, 2) /= scale_ratio;
}

static void toGray8(const cv::Mat& in, cv::Mat& outGray8)
{
    outGray8.release();
    if (in.empty()) return;

    cv::Mat gray;

    // Ensure 1ch grayscale
    if (in.channels() == 1)
    {
        gray = in;
    }
    else if (in.channels() == 3)
    {
        cv::cvtColor(in, gray, cv::COLOR_BGR2GRAY);
    }
    else if (in.channels() == 4)
    {
        cv::cvtColor(in, gray, cv::COLOR_BGRA2GRAY);
    }
    else
    {
        // Unusual channel count; best effort: reshape to 1 channel and convert
        gray = in.reshape(1);
    }

    // Convert depth -> 8U with sane scaling
    if (gray.depth() == CV_8U)
    {
        outGray8 = gray;
        return;
    }
    else if (gray.depth() == CV_16U)
    {
        // Assume full-range 16U (0..65535) -> 0..255
        gray.convertTo(outGray8, CV_8U, 255.0 / 65535.0);
        return;
    }
    else if (gray.depth() == CV_32F)
    {
        // Could be 0..1, 0..255, or 0..65535. Infer from max.
        double mn=0, mx=0;
        cv::minMaxLoc(gray, &mn, &mx);

        double scale = 1.0;
        if (mx <= 1.5)          scale = 255.0;           // 0..1
        else if (mx <= 300.0)   scale = 1.0;             // ~0..255
        else                    scale = 255.0 / 65535.0; // ~0..65535

        gray.convertTo(outGray8, CV_8U, scale);
        return;
    }

    // Fallback: best effort
    gray.convertTo(outGray8, CV_8U);
}

static void match_contrast(const cv::Mat &refGray,
                           const cv::Mat &srcGray,
                           const cv::Rect &roi,
                           cv::Mat &contrast,
                           const cv::Mat &transform)
{
    // Convert both to 8-bit grayscale for fitting
    cv::Mat ref8, src8;
    toGray8(refGray, ref8);
    toGray8(srcGray, src8);

    if (ref8.empty() || src8.empty() || ref8.type() != CV_8UC1 || src8.type() != CV_8UC1)
        throw std::runtime_error("match_contrast: failed to convert inputs to CV_8UC1");

    // Clamp ROI to bounds (avoid crashes on odd validArea)
    cv::Rect roiClamped = roi & cv::Rect(0, 0, ref8.cols, ref8.rows) & cv::Rect(0, 0, src8.cols, src8.rows);
    if (roiClamped.width <= 1 || roiClamped.height <= 1)
        throw std::runtime_error("match_contrast: ROI empty");

    // Warp src into ref space using current transform
    cv::Mat tmp;
    cv::warpAffine(src8, tmp, transform,
                   cv::Size(src8.cols, src8.rows),
                   cv::INTER_CUBIC,
                   cv::BORDER_REFLECT);

    // Downsample ROI for stable solve
    const int xsamples = 64;
    const int ysamples = 64;
    const int total = xsamples * ysamples;

    cv::Mat refS, srcS;
    cv::resize(ref8(roiClamped), refS, cv::Size(xsamples, ysamples), 0, 0, cv::INTER_AREA);
    cv::resize(tmp(roiClamped),  srcS, cv::Size(xsamples, ysamples), 0, 0, cv::INTER_AREA);

    // Solve: c(x,y) = C0 + Cx*xd + Cx2*xd^2 + Cy*yd + Cy2*yd^2
    // where xd,yd are normalized like your existing code.
    cv::Mat contrastVals(total, 1, CV_32F);
    cv::Mat positions(total, 5, CV_32F);

    for (int y = 0; y < ysamples; ++y)
    {
        for (int x = 0; x < xsamples; ++x)
        {
            const int idx = y * xsamples + x;

            const float yd = (y - refS.rows / 2.0f) / float(refS.rows);
            const float xd = (x - refS.cols / 2.0f) / float(refS.cols);

            const float refpix = float(refS.at<uint8_t>(y, x));
            const float srcpix = float(srcS.at<uint8_t>(y, x));

            float c = 1.0f;
            // Keep your original “avoid dividing dark noise” logic,
            // but tighten slightly to reduce pathological values.
            if (refpix > 4.0f && srcpix > 4.0f)
                c = refpix / srcpix;

            // If srcpix is tiny but refpix isn’t, c can explode.
            // Clamp the sample contribution (still allow solve to vary spatially).
            c = std::min(4.0f, std::max(0.25f, c));

            contrastVals.at<float>(idx, 0) = c;

            positions.at<float>(idx, 0) = 1.0f;
            positions.at<float>(idx, 1) = xd;
            positions.at<float>(idx, 2) = sq(xd);
            positions.at<float>(idx, 3) = yd;
            positions.at<float>(idx, 4) = sq(yd);
        }
    }

    cv::solve(positions, contrastVals, contrast, cv::DECOMP_SVD);

    // Sanity checks / guardrails
    // (Your earlier checkRange -2..2 is very strict for the higher-order terms
    // but can be OK. Keep it, and also check finiteness.)
    if (!cv::checkRange(contrast, true, nullptr, -2.0, 2.0))
        throw std::runtime_error("Contrast match result out of range, try disabling contrast.");

    for (int i = 0; i < contrast.rows; ++i)
    {
        float v = contrast.at<float>(i,0);
        if (!std::isfinite(v))
            throw std::runtime_error("Contrast match produced NaN/Inf.");
    }

    // Optional: prevent blackout if C0 goes near-zero.
    // (FSPhotometric will clamp gain anyway, but keep coefficients sane upstream.)
    const float minC0 = 0.05f;
    if (std::abs(contrast.at<float>(0,0)) < minC0)
        throw std::runtime_error("Contrast match produced near-zero C0 (unstable).");
}

// Contrast fitting as in Task_Align::match_contrast
// void match_contrast(const cv::Mat &refGray,
//                     const cv::Mat &srcGray,
//                     const cv::Rect &roi,
//                     cv::Mat &contrast,
//                     const cv::Mat &transform)
// {
//     cv::Mat ref, src;

//     int xsamples = 64;
//     int ysamples = 64;
//     int total = xsamples * ysamples;

//     cv::Mat tmp;
//     // Apply current transform to src
//     int invflag = 0;
//     cv::warpAffine(srcGray, tmp, transform,
//                    cv::Size(srcGray.cols, srcGray.rows),
//                    cv::INTER_CUBIC | invflag,
//                    cv::BORDER_REFLECT);

//     cv::resize(refGray(roi), ref,
//                cv::Size(xsamples, ysamples), 0, 0, cv::INTER_AREA);
//     cv::resize(tmp(roi), src,
//                cv::Size(xsamples, ysamples), 0, 0, cv::INTER_AREA);

//     cv::Mat contrastVals(total, 1, CV_32F);
//     cv::Mat positions(total, 5, CV_32F);

//     for (int y = 0; y < ysamples; ++y)
//     {
//         for (int x = 0; x < xsamples; ++x)
//         {
//             int idx = y * xsamples + x;

//             float yd = (y - ref.rows / 2.0f) / static_cast<float>(ref.rows);
//             float xd = (x - ref.cols / 2.0f) / static_cast<float>(ref.cols);

//             float refpix = static_cast<float>(ref.at<uint8_t>(y, x));
//             float srcpix = static_cast<float>(src.at<uint8_t>(y, x));

//             float c = 1.0f;
//             if (refpix > 4.0f && srcpix > 4.0f)
//             {
//                 c = refpix / srcpix;
//             }

//             contrastVals.at<float>(idx, 0) = c;
//             positions.at<float>(idx, 0) = 1.0f;
//             positions.at<float>(idx, 1) = xd;
//             positions.at<float>(idx, 2) = sq(xd);
//             positions.at<float>(idx, 3) = yd;
//             positions.at<float>(idx, 4) = sq(yd);
//         }
//     }

//     cv::solve(positions, contrastVals, contrast, cv::DECOMP_SVD);

//     if (!cv::checkRange(contrast, true, nullptr, -2.0f, 2.0f))
//     {
//         throw std::runtime_error("Contrast match result out of range, try disabling contrast.");
//     }
// }

static void toBGR8(const cv::Mat& in, cv::Mat& outBgr8)
{
    outBgr8.release();
    if (in.empty()) return;

    cv::Mat bgr;

    // Ensure BGR 3ch
    if (in.channels() == 1)
        cv::cvtColor(in, bgr, cv::COLOR_GRAY2BGR);
    else if (in.channels() == 4)
        cv::cvtColor(in, bgr, cv::COLOR_BGRA2BGR);
    else
        bgr = in;

    // Convert depth -> 8U with sane scaling
    if (bgr.depth() == CV_8U)
    {
        outBgr8 = bgr;
        return;
    }
    else if (bgr.depth() == CV_16U)
    {
        // Assume full-range 16U (0..65535) -> 0..255
        bgr.convertTo(outBgr8, CV_8U, 255.0 / 65535.0);
        return;
    }
    else if (bgr.depth() == CV_32F)
    {
        // Could be 0..1, 0..255, or 0..65535. Try to infer from max.
        double mn=0, mx=0;
        cv::minMaxLoc(bgr.reshape(1), &mn, &mx);

        double scale = 1.0;
        if (mx <= 1.5)          scale = 255.0;           // 0..1
        else if (mx <= 300.0)   scale = 1.0;             // ~0..255
        else                    scale = 255.0 / 65535.0; // ~0..65535

        bgr.convertTo(outBgr8, CV_8U, scale);
        return;
    }

    // Fallback: best effort
    bgr.convertTo(outBgr8, CV_8U);
}

static void match_whitebalance(const cv::Mat &refColor,
                               const cv::Mat &srcColor,
                               const cv::Rect &roi,
                               const cv::Mat &contrast,
                               cv::Mat &whitebalance,
                               const cv::Mat &transform)
{
    // --- Convert inputs to 8-bit BGR for robust fitting ---
    cv::Mat ref8, src8;
    toBGR8(refColor, ref8);
    toBGR8(srcColor, src8);

    if (ref8.empty() || src8.empty() || ref8.type() != CV_8UC3 || src8.type() != CV_8UC3)
        throw std::runtime_error("match_whitebalance: failed to convert inputs to CV_8UC3");

    // Clamp ROI to bounds (avoid crashes on odd validArea)
    cv::Rect roiClamped = roi & cv::Rect(0, 0, ref8.cols, ref8.rows) & cv::Rect(0, 0, src8.cols, src8.rows);
    if (roiClamped.width <= 1 || roiClamped.height <= 1)
        throw std::runtime_error("match_whitebalance: ROI empty");

    // --- Warp src into ref space using current transform ---
    cv::Mat tmp;
    cv::warpAffine(src8, tmp, transform,
                   cv::Size(src8.cols, src8.rows),
                   cv::INTER_CUBIC,
                   cv::BORDER_REFLECT);

    // Identity WB: only contrast gets applied before fitting WB
    cv::Mat identityWB(6, 1, CV_32F);
    identityWB.at<float>(0, 0) = 0.0f;  identityWB.at<float>(1, 0) = 1.0f; // B
    identityWB.at<float>(2, 0) = 0.0f;  identityWB.at<float>(3, 0) = 1.0f; // G
    identityWB.at<float>(4, 0) = 0.0f;  identityWB.at<float>(5, 0) = 1.0f; // R

    // Apply contrast (and identity WB) to warped src before sampling
    // (apply_contrast_whitebalance_internal expects 8-bit, which we guarantee here)
    apply_contrast_whitebalance_internal(tmp, contrast, identityWB);

    // --- Downsample ROI for stable linear solve ---
    const int xsamples = 64;
    const int ysamples = 64;
    const int total = xsamples * ysamples;

    cv::Mat refS, srcS;
    cv::resize(ref8(roiClamped), refS, cv::Size(xsamples, ysamples), 0, 0, cv::INTER_AREA);
    cv::resize(tmp(roiClamped), srcS, cv::Size(xsamples, ysamples), 0, 0, cv::INTER_AREA);

    // Build linear system:
    // For each channel independently: out = gain * in + offset
    // B: targets = refB, factors columns [B_off, B_gain] use [1, srcB]
    // G: columns [G_off, G_gain] use [1, srcG]
    // R: columns [R_off, R_gain] use [1, srcR]
    cv::Mat targets(total * 3, 1, CV_32F);
    cv::Mat factors(total * 3, 6, CV_32F, cv::Scalar(0));

    for (int y = 0; y < ysamples; ++y)
    {
        for (int x = 0; x < xsamples; ++x)
        {
            const int idx = y * xsamples + x;

            const cv::Vec3b s = srcS.at<cv::Vec3b>(y, x);
            const cv::Vec3b r = refS.at<cv::Vec3b>(y, x);

            // Targets
            targets.at<float>(idx * 3 + 0, 0) = float(r[0]); // B
            targets.at<float>(idx * 3 + 1, 0) = float(r[1]); // G
            targets.at<float>(idx * 3 + 2, 0) = float(r[2]); // R

            // Factors (B)
            factors.at<float>(idx * 3 + 0, 0) = 1.0f;
            factors.at<float>(idx * 3 + 0, 1) = float(s[0]);

            // Factors (G)
            factors.at<float>(idx * 3 + 1, 2) = 1.0f;
            factors.at<float>(idx * 3 + 1, 3) = float(s[1]);

            // Factors (R)
            factors.at<float>(idx * 3 + 2, 4) = 1.0f;
            factors.at<float>(idx * 3 + 2, 5) = float(s[2]);
        }
    }

    cv::solve(factors, targets, whitebalance, cv::DECOMP_SVD);

    // --- Sanity checks / guardrails ---
    // Offsets in 8-bit domain, gains around ~1 typically.
    // Keep your original range, but also reject NaN/Inf.
    if (!cv::checkRange(whitebalance, true, nullptr, -128.0, 128.0))
        throw std::runtime_error("Whitebalance match result out of range, try disabling WB.");

    for (int i = 0; i < whitebalance.rows; ++i)
    {
        float v = whitebalance.at<float>(i,0);
        if (!std::isfinite(v))
            throw std::runtime_error("Whitebalance match produced NaN/Inf.");
    }

    // Optional: prevent “blackout” if gains go near-zero.
    // (FSPhotometric also defends, but better to keep coefficients sane at the source.)
    const float minGain = 0.05f;
    if (std::abs(whitebalance.at<float>(1,0)) < minGain ||
        std::abs(whitebalance.at<float>(3,0)) < minGain ||
        std::abs(whitebalance.at<float>(5,0)) < minGain)
    {
        throw std::runtime_error("Whitebalance match produced near-zero gain (unstable).");
    }
}

cv::Point2f transform_point(const cv::Mat &transform, cv::Point2f pt)
{
    float x = transform.at<float>(0, 0) * pt.x
              + transform.at<float>(0, 1) * pt.y
              + transform.at<float>(0, 2);
    float y = transform.at<float>(1, 0) * pt.x
              + transform.at<float>(1, 1) * pt.y
              + transform.at<float>(1, 2);
    return cv::Point2f(x, y);
}

// Compute valid area of srcValid after applying global transform
cv::Rect compute_valid_area(const cv::Rect &srcValid,
                            const cv::Mat &transform,
                            bool keepSize)
{
    cv::Rect a = srcValid;

    cv::Point2f tl = transform_point(transform, cv::Point2f(a.x, a.y));
    cv::Point2f tr = transform_point(transform, cv::Point2f(a.x + a.width, a.y));
    cv::Point2f bl = transform_point(transform, cv::Point2f(a.x, a.y + a.height));
    cv::Point2f br = transform_point(transform, cv::Point2f(a.x + a.width, a.y + a.height));

    int top    = static_cast<int>(std::ceil(std::max(tl.y, tr.y)));
    int left   = static_cast<int>(std::ceil(std::max(tl.x, bl.x)));
    int bottom = static_cast<int>(std::floor(std::min(bl.y, br.y)));
    int right  = static_cast<int>(std::floor(std::min(br.x, tr.x)));

    if (keepSize)
        return a; // ALIGN_KEEP_SIZE behaviour

    if (right <= left || bottom <= top)
        return cv::Rect(); // empty

    return cv::Rect(left, top, right - left, bottom - top);
}

} // anon namespace

// ---------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------

namespace FSAlign {

Result makeIdentity(const cv::Rect &validArea)
{
    Result r;

    r.transform = cv::Mat::eye(2, 3, CV_32F);

    r.contrast = cv::Mat(5, 1, CV_32F);
    r.contrast.at<float>(0) = 1.0f;
    r.contrast.at<float>(1) = 0.0f;
    r.contrast.at<float>(2) = 0.0f;
    r.contrast.at<float>(3) = 0.0f;
    r.contrast.at<float>(4) = 0.0f;

    r.whitebalance = cv::Mat(6, 1, CV_32F);
    r.whitebalance.at<float>(0) = 0.0f;  r.whitebalance.at<float>(1) = 1.0f; // B
    r.whitebalance.at<float>(2) = 0.0f;  r.whitebalance.at<float>(3) = 1.0f; // G
    r.whitebalance.at<float>(4) = 0.0f;  r.whitebalance.at<float>(5) = 1.0f; // R

    r.validArea = validArea;

    // Redundant but bullet-proof
    ensureIdentityContrast(r.contrast);
    ensureIdentityWB(r.whitebalance);
    if (r.transform.rows != 2 || r.transform.cols != 3 || r.transform.type() != CV_32F)
        r = Result();         // reset everything if transform is wrong

    return r;
}

Result computeLocal(const cv::Mat &refGray,
                    const cv::Mat &refColor,
                    const cv::Mat &srcGray,
                    const cv::Mat &srcColor,
                    const cv::Rect &srcValidArea,
                    const Options &opt)
{
    Result r = makeIdentity(srcValidArea);

    // Start with identity transform and default coeffs
    // r.validArea = srcValidArea;

    // Low-resolution rough geometric alignment
    match_transform(refGray,
                    srcGray,
                    srcValidArea,
                    r.transform,
                    r.contrast,
                    r.whitebalance,
                    opt.lowRes,
                    true);

    // Contrast matching (grayscale)
    if (opt.matchContrast)
    {
        match_contrast(refGray,
                       srcGray,
                       srcValidArea,
                       r.contrast,
                       r.transform);
    }

    // Whitebalance matching (color)
    if (opt.matchWhiteBalance && srcColor.channels() == 3)
    {
        match_whitebalance(refColor,
                           srcColor,
                           srcValidArea,
                           r.contrast,
                           r.whitebalance,
                           r.transform);
    }

    // High-resolution refinement
    int res = opt.fullResolution
                  ? std::max(srcGray.cols, srcGray.rows)
                  : opt.maxRes;

    match_transform(refGray,
                    srcGray,
                    srcValidArea,
                    r.transform,
                    r.contrast,
                    r.whitebalance,
                    res,
                    false);

    return r;
}

Result accumulate(const Result &prevGlobal,
                  const Result &local,
                  const cv::Rect &srcValid)
{
    Result g = local; // will become global

    // Prevent empty/invalid mats from crashing or corrupting results
    ensureIdentityContrast(g.contrast);
    ensureIdentityWB(g.whitebalance);
    cv::Mat sC = prevGlobal.contrast;
    cv::Mat sW = prevGlobal.whitebalance;
    ensureIdentityContrast(sC);
    ensureIdentityWB(sW);

    // --- Transform stacking (like Task_Align with m_stacked_transform) ----
    cv::Mat prev3 = cv::Mat::eye(3, 3, CV_32F);
    prevGlobal.transform.copyTo(prev3(cv::Rect(0, 0, 3, 2)));

    cv::Mat local3 = cv::Mat::eye(3, 3, CV_32F);
    local.transform.copyTo(local3(cv::Rect(0, 0, 3, 2)));

    cv::Mat combined3 = local3 * prev3;
    g.transform = combined3(cv::Rect(0, 0, 3, 2)).clone();

    // --- Contrast stacking ---------------------------------
    {
        CV_Assert(prevGlobal.contrast.rows == 5 && prevGlobal.contrast.cols == 1);
        CV_Assert(g.contrast.rows == 5 && g.contrast.cols == 1);

        cv::Mat c = g.contrast.clone();
        const cv::Mat &sC = prevGlobal.contrast;

        g.contrast *= sC.at<float>(0);
        g.contrast.at<float>(1) += sC.at<float>(1) * c.at<float>(0);
        g.contrast.at<float>(2) += sC.at<float>(2) * c.at<float>(0);
        g.contrast.at<float>(2) += sC.at<float>(1) * c.at<float>(1);
        g.contrast.at<float>(3) += sC.at<float>(3) * c.at<float>(0);
        g.contrast.at<float>(4) += sC.at<float>(4) * c.at<float>(0);
        g.contrast.at<float>(4) += sC.at<float>(3) * c.at<float>(3);
    }

    // --- Whitebalance stacking ------------------------------
    {
        CV_Assert(prevGlobal.whitebalance.rows == 6 && prevGlobal.whitebalance.cols == 1);
        CV_Assert(g.whitebalance.rows == 6 && g.whitebalance.cols == 1);

        const cv::Mat &sWB = prevGlobal.whitebalance;

        // Compose: (gain2*(gain1*x + off1) + off2) -> (gain2*gain1)*x + (gain2*off1 + off2)
        // In your parameterization: out = gain*in + offset
        g.whitebalance.at<float>(0) = sWB.at<float>(1) * g.whitebalance.at<float>(0) + sWB.at<float>(0);
        g.whitebalance.at<float>(1) = sWB.at<float>(1) * g.whitebalance.at<float>(1);

        g.whitebalance.at<float>(2) = sWB.at<float>(3) * g.whitebalance.at<float>(2) + sWB.at<float>(2);
        g.whitebalance.at<float>(3) = sWB.at<float>(3) * g.whitebalance.at<float>(3);

        g.whitebalance.at<float>(4) = sWB.at<float>(5) * g.whitebalance.at<float>(4) + sWB.at<float>(4);
        g.whitebalance.at<float>(5) = sWB.at<float>(5) * g.whitebalance.at<float>(5);
    }
    // --- Valid area stacking ----------------------------------------------
    // Compute valid area for this image in global space
    g.validArea = compute_valid_area(srcValid, g.transform, false);

    return g;
}

void applyTransform(const cv::Mat &src,
                    const cv::Mat &transform,
                    cv::Mat &dst,
                    bool inverse)
{
    int invflag = (!inverse) ? 0 : cv::WARP_INVERSE_MAP;
    dst.create(src.rows, src.cols, src.type());
    cv::warpAffine(src, dst, transform,
                   cv::Size(src.cols, src.rows),
                   cv::INTER_CUBIC | invflag,
                   cv::BORDER_REFLECT);
}

void applyContrastWhiteBalance(cv::Mat &img,
                               const Result &r)
{
    apply_contrast_whitebalance_internal(img, r.contrast, r.whitebalance);
}

//-----------------------------------------------------------------------------------
// StreamPMax pipeline
//-----------------------------------------------------------------------------------

bool Align::alignSlice(int              slice,
                       FSLoader::Image  &prevImage,
                       FSLoader::Image  &currImage,
                       Result           &prevGlobal,
                       Result           &currGlobal,
                       cv::Mat          &alignedGraySlice,
                       cv::Mat          &alignedColorSlice,
                       const Options    &opt,
                       std::atomic_bool *abortFlag,
                       StatusCallback   status,
                       ProgressCallback progressCallback
                       )
{
    QString srcFun = "FSAlign::alignSlice";
    QString s = QString::number(slice);
    QString msg = "Slice: " + s + " Aligning";
    if (G::FSLog) G::log(srcFun, msg);
    // if (status) status(msg);

    // first slice is already "aligned"
    if (prevImage.gray.empty())
    {
        alignedGraySlice  = currImage.gray.clone();
        alignedColorSlice = currImage.color.clone();
        return true;
    }

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;


    Result local;

    try {
        local = computeLocal(
            prevImage.gray,
            prevImage.color,
            currImage.gray,
            currImage.color,
            currImage.validArea,
            opt
            );
    }
    catch (const std::exception &e)
    {
        QString msg = "Alignment failed for ...";
        if (status) status(msg);
        return false;
    }
    if (G::FSLog) G::log(srcFun, "computeLocal");

    // Stack transforms
    currGlobal = accumulate(
        prevGlobal,
        local,
        currImage.validArea
        );

    // Apply transform to color + gray
    cv::Mat alignedColorMat, alignedGrayMat;
    if (G::FSLog) G::log(srcFun, "cv::Mat alignedColor, alignedGray");
    applyTransform(currImage.color, currGlobal.transform, alignedColorMat);
    applyTransform(currImage.gray,  currGlobal.transform, alignedGrayMat);
    if (G::FSLog) G::log(srcFun, "applyTransform alignedGray");

    // Write outputs
    // if (o.keepAlign)
    // {
    //     cv::imwrite(alignedColorPaths[i].toStdString(), alignedColorMat);
    //     cv::imwrite(alignedGrayPaths[i].toStdString(),  alignedGrayMat);
    // }

    // Cache in memory for fast fusion
    alignedColorSlice = alignedColorMat.clone();
    alignedGraySlice  = alignedGrayMat.clone();

    // if (progressCallback) progressCallback();

    return true;
}

} // namespace FSAlign
