#ifndef SEGREFINE_H
#define SEGREFINE_H

/*
    Shared edge refinement for the AI segmentation masks (Select Subject = U^2-Net, Select Sky =
    single-channel sky net). Both models emit a coarse low-res single-channel map; this turns it into
    a full-resolution 0..1 coverage that hugs the photo's real edges, so the two masks share identical
    matting behaviour. Header-only (all inline), no build-system entry.

    Pipeline: min-max normalize the raw output -> upsample to the guide image's native size (this un-
    squishes the model's square input) -> guided-filter (He et al.) against the guide luminance so the
    matte snaps to real boundaries -> a mild contrast around 0.5 to remove the residual halo. Output
    cov is row-major w*h, output-oriented (it matches the guide image, which the caller develops
    output-oriented), so it drops straight into buildMaskBuffer / the loupe overlay's onx/ony space.
*/

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include <QImage>
#include <vector>
#include <algorithm>
#include <cmath>

namespace SegRefine {

/* Gray guided filter: edge-preserving smoothing of p using guide I. All CV_32F; ~9 box filters, a
   few ms at ~1 MP. eps sets edge sensitivity (smaller = follows the guide harder). */
inline void guidedFilterGray(const cv::Mat &I, const cv::Mat &p, cv::Mat &q, int radius, double eps)
{
    const cv::Size k(2 * radius + 1, 2 * radius + 1);
    cv::Mat mean_I, mean_p, corr_I, corr_Ip;
    cv::boxFilter(I, mean_I, CV_32F, k);
    cv::boxFilter(p, mean_p, CV_32F, k);
    cv::boxFilter(I.mul(I), corr_I, CV_32F, k);
    cv::boxFilter(I.mul(p), corr_Ip, CV_32F, k);
    const cv::Mat var_I  = corr_I  - mean_I.mul(mean_I);
    const cv::Mat cov_Ip = corr_Ip - mean_I.mul(mean_p);
    const cv::Mat a = cov_Ip / (var_I + eps);
    const cv::Mat b = mean_p - a.mul(mean_I);
    cv::Mat mean_a, mean_b;
    cv::boxFilter(a, mean_a, CV_32F, k);
    cv::boxFilter(b, mean_b, CV_32F, k);
    q = mean_a.mul(I) + mean_b;
}

/* rawSal: the model's single-channel target as a 2D CV_32F (any resolution / range). image: the
   developed base (output-oriented) used as the edge guide. Fills cov (w*h) + dims. */
inline bool refine(const cv::Mat &rawSal, const QImage &image,
                   std::vector<float> &cov, int &w, int &h)
{
    if (rawSal.empty()) return false;

    /* Min-max stretch to 0..1 (handles both sigmoid probabilities and raw logits). */
    double lo = 0.0, hi = 0.0;
    cv::minMaxLoc(rawSal, &lo, &hi);
    cv::Mat sal;
    if (hi - lo > 1e-6) rawSal.convertTo(sal, CV_32F, 1.0 / (hi - lo), -lo / (hi - lo));
    else                sal = cv::Mat::zeros(rawSal.size(), CV_32F);

    const QImage guideRgb = image.convertToFormat(QImage::Format_RGB888);
    const int ow = guideRgb.width(), oh = guideRgb.height();
    if (ow <= 0 || oh <= 0) return false;
    cv::Mat guide(oh, ow, CV_32F);
    for (int y = 0; y < oh; ++y) {
        const uchar *line = guideRgb.constScanLine(y);
        float *g = guide.ptr<float>(y);
        for (int x = 0; x < ow; ++x)                 // Rec.601 luma, 0..1
            g[x] = (0.299f * line[x*3+0] + 0.587f * line[x*3+1] + 0.114f * line[x*3+2]) / 255.0f;
    }

    cv::Mat salUp, refined;
    cv::resize(sal, salUp, cv::Size(ow, oh), 0, 0, cv::INTER_CUBIC);
    /* Tight cutout: small radius + low eps make the alpha hug the photo's edges (a larger radius/eps
       smears the matte into a soft halo). */
    const int radius = std::max(2, int(std::lround(std::max(ow, oh) / 256.0)));
    guidedFilterGray(guide, salUp, refined, radius, 1e-6);

    /* Narrow the transition band (contrast around 0.5): pushes the faint low-alpha fringe to 0 and
       the solid interior to 1, removing the residual halo while leaving a 1-2 px anti-aliased edge. */
    cov.resize(size_t(ow) * oh);
    for (int y = 0; y < oh; ++y) {
        const float *rp = refined.ptr<float>(y);
        for (int x = 0; x < ow; ++x) {
            const float a = std::clamp(rp[x], 0.0f, 1.0f);
            cov[size_t(y) * ow + x] = std::clamp((a - 0.5f) * 1.6f + 0.5f, 0.0f, 1.0f);
        }
    }
    w = ow; h = oh;
    return true;
}

/* Guided UPSAMPLING for a continuous field (the Depth mask's MiDaS map). Unlike refine() this keeps
   the values continuous -- NO binary contrast -- because a depth-range mask selects a band of the
   field, not a foreground alpha. Min-max normalize (optionally invert so 0=near, 1=far), cubic-
   upsample to the guide's native size, then guided-filter against the guide luminance (the classic
   guided-filter joint-upsampling use). Output out is w*h, output-oriented. */
inline bool guidedUpsample(const cv::Mat &rawField, const QImage &image, bool invert,
                           std::vector<float> &out, int &w, int &h)
{
    if (rawField.empty()) return false;
    double lo = 0.0, hi = 0.0;
    cv::minMaxLoc(rawField, &lo, &hi);
    cv::Mat norm;
    if (hi - lo > 1e-6) {
        rawField.convertTo(norm, CV_32F, 1.0 / (hi - lo), -lo / (hi - lo));
        if (invert) norm = 1.0f - norm;
    } else {
        norm = cv::Mat::zeros(rawField.size(), CV_32F);
    }

    const QImage guideRgb = image.convertToFormat(QImage::Format_RGB888);
    const int ow = guideRgb.width(), oh = guideRgb.height();
    if (ow <= 0 || oh <= 0) return false;
    cv::Mat guide(oh, ow, CV_32F);
    for (int y = 0; y < oh; ++y) {
        const uchar *line = guideRgb.constScanLine(y);
        float *g = guide.ptr<float>(y);
        for (int x = 0; x < ow; ++x)
            g[x] = (0.299f * line[x*3+0] + 0.587f * line[x*3+1] + 0.114f * line[x*3+2]) / 255.0f;
    }

    cv::Mat up, refined;
    cv::resize(norm, up, cv::Size(ow, oh), 0, 0, cv::INTER_CUBIC);
    const int radius = std::max(2, int(std::lround(std::max(ow, oh) / 200.0)));
    guidedFilterGray(guide, up, refined, radius, 1e-3);   // gentle eps: smooth field, edge-aware

    out.resize(size_t(ow) * oh);
    for (int y = 0; y < oh; ++y) {
        const float *rp = refined.ptr<float>(y);
        for (int x = 0; x < ow; ++x)
            out[size_t(y) * ow + x] = std::clamp(rp[x], 0.0f, 1.0f);
    }
    w = ow; h = oh;
    return true;
}

} // namespace SegRefine

#endif // SEGREFINE_H
