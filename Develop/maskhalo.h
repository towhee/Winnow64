#ifndef MASKHALO_H
#define MASKHALO_H

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

/*
    REMOVE THE HALO a misregistered Develop mask leaves along a subject's edge.

    THE PROBLEM. A masked tone or colour adjustment is composited as a per-pixel lerp,
    acc = acc*(1-m) + scope*m (WorkingImageCache::renderStack). Where the mask's
    transition band does not sit exactly on the subject's real boundary, every pixel in
    that band gets a share of the adjustment whether it belongs to the subject or to the
    background -- so a +1 EV subject mask lifts a rim of background and reads as a glow
    around the subject.

    NEITHER EXISTING CONTROL FIXES IT. Feather sets the WIDTH of the transition band, not
    its position. Edge (maskedge.h) slides the whole boundary by a uniform pixel distance,
    so wherever the mask's error is not uniform it trades a halo on one side for a bitten
    edge on the other.

    WHAT THIS DOES. It fades the adjustment out ACROSS the picture's edge, leaving the
    mask boundary where the user put it. Where the guide says there is a committed edge
    near the mask's own boundary, the alpha is pulled down, so the adjustment tapers
    through the very band the halo lives in and the rim stops reading as an outline.

    IT IS AN ALPHA OPERATION, which is why it lives in buildMaskBuffer and not in the
    compositor. Because the composite is a lerp, "weaken the adjustment near the edge" is
    arithmetically identical to "multiply the alpha near the edge", and doing it on the
    alpha keeps a mask a pure function of its components -- which is what the per-scope
    render cache assumes.

    THE GUIDE is the perceptual luminance of the WORKING IMAGE, on exactly this buffer's
    grid (the caller builds it from the same WorkingImage it sizes the mask from). It is
    the UNDEVELOPED image on purpose: a guide derived from the developed result would move
    when a slider moved, which is a feedback loop between the mask and the adjustment it
    is masking.

    REFINE WAS BUILT, TESTED ON REAL PICTURES AND REJECTED (2026-08-24). The other obvious
    answer is to MOVE the alpha rather than fade it: guided-filter the mask against the
    picture (He et al., as SegRefine still does for the AI cutouts) so the boundary snaps
    onto the real edge. It is the more principled fix on paper and it passed its
    displacement tests -- a boundary 6 px out was pulled to within 2 px, and preview
    matched render across a 0.4 proxy -- but in the loupe it BLURRED the halo instead of
    removing it: the rim softened and spread rather than going away. That fits the
    mechanism. A guided filter reconstructs the alpha as a local linear function of the
    guide, and a wide window over a soft, low-contrast boundary (fur, hair, foliage --
    exactly where masks miss) has no sharp answer to converge on, so it returns a smooth
    ramp. Damping never has to decide where the edge IS; it only has to know one is near,
    which is a far easier question of the same data. The synthetic step edge the tests use
    is precisely the case Refine was good at, which is why the tests did not catch this
    and a picture did. Do not re-add the guided-filter path without a real photograph it
    beats damping on.

    RESOLUTION INDEPENDENCE is the same contract Edge signs: the caller passes pixelScale
    (this render's long edge over the full-res long edge) and every distance and gradient
    below is converted through it, so the proxy preview, the loupe veil and the settle
    render agree. Get this wrong and the halo visibly changes when the settle lands.
*/
namespace MaskHalo {

/* Slider units (0..100). Below this the work is pure cost for no visible change. */
inline constexpr float kMinAmount = 0.5f;

/* The slider's REACH -- how far away an edge may be and still damp a pixel -- in
   FULL-RESOLUTION pixels, at amount 0 and amount 100. It is how far out a halo can be and
   still be found, which is the thing that varies between pictures. */
inline constexpr double kReachRadiusMinPx = 2.0;
inline constexpr double kReachRadiusMaxPx = 24.0;

/* Perceptual-luma gradient, per FULL-RESOLUTION pixel, that counts as a fully committed
   edge. Anything at or above this damps by the full slider amount. */
inline constexpr float kGradRefPerPx = 0.25f;

/* How far either side of the MASK's own boundary damping is allowed to act, in
   full-resolution pixels. Deliberately fixed and fairly tight: widening it with the
   slider would eat further and further into the subject's interior, which is the failure
   this approach is prone to. Amount widens the search for an edge, not the damage. */
inline constexpr double kDampBandPx = 8.0;

namespace detail {

/* Wrap a caller's buffer as a cv::Mat without copying it -- the same zero-copy view
   WorkingImageCache::downscaled takes over a WorkingImage. */
inline cv::Mat view(std::vector<float> &v, int w, int h)
{
    return cv::Mat(h, w, CV_32F, v.data());
}
inline cv::Mat view(const std::vector<float> &v, int w, int h)
{
    return cv::Mat(h, w, CV_32F, const_cast<float *>(v.data()));
}

} // namespace detail

/*
    Fade `alpha` (w*h, 0..1, modified in place) out across the edges in `guide` (w*h,
    perceptual luma on the SAME grid). amount is the slider, 0..100.

    WHICH PIXELS. Not "wherever 0 < alpha < 1": on a crisp mask that band is one pixel
    wide and this would do nothing at all. Instead the mask's own boundary NEIGHBOURHOOD
    is found by blurring the alpha and taking 4*b*(1-b), which is 1 where the blurred mask
    is halfway (the boundary) and falls to 0 into the solid interior and the clean
    exterior. One box filter, and it behaves the same on a hard mask and a feathered one.

    THE EDGE FIELD IS SPREAD BY A LOCAL MAX, not by a blur. Scharr responds over three
    pixels, so the raw magnitude is a hairline: used directly it cuts a two-pixel notch at
    the boundary and leaves the whole rim beyond it fully adjusted -- which is not a halo
    fix. Dilating it over the reach radius answers the question that actually matters,
    "is there a committed edge within reach of this pixel", and does it without a blur's
    attenuation of the peak.

    THE GRADIENT is measured per pixel OF THIS BUFFER, so it grows as the buffer shrinks
    (the same ramp spans fewer pixels): multiplying by pixelScale converts it back to a
    per-full-resolution-pixel figure and makes the proxy and the settle agree.

    `par(count, fn)` must run fn(i0, i1) over disjoint half-open sub-ranges covering
    [0, count) -- supplied by the caller, as in MaskEdge, so this header stays free of Qt
    and of the project's thread pool. Here count is always the row count.
*/
template <class Par>
inline void apply(std::vector<float> &alpha, const std::vector<float> &guide,
                  int w, int h, float amount, double pixelScale, Par &&par)
{
    if (w <= 0 || h <= 0) return;
    if (amount < kMinAmount) return;
    const size_t n = size_t(w) * size_t(h);
    if (alpha.size() != n || guide.size() != n) return;
    if (pixelScale <= 0.0) return;

    const float a = std::clamp(amount / 100.0f, 0.0f, 1.0f);
    const double span = kReachRadiusMaxPx - kReachRadiusMinPx;
    const double rPx = (kReachRadiusMinPx + span * double(a)) * pixelScale;
    const int radius = std::max(1, int(std::lround(rPx)));

    const cv::Mat g = detail::view(guide, w, h);
    cv::Mat gx, gy;
    /* 1/16 is the Scharr kernel's weight sum: it turns the response back into an
       intensity difference per pixel rather than a kernel-scaled one. */
    cv::Scharr(g, gx, CV_32F, 1, 0, 1.0 / 16.0);
    cv::Scharr(g, gy, CV_32F, 0, 1, 1.0 / 16.0);

    cv::Mat mag;
    cv::magnitude(gx, gy, mag);
    cv::dilate(mag, mag, cv::getStructuringElement(
                   cv::MORPH_RECT, cv::Size(2 * radius + 1, 2 * radius + 1)));

    const int bandR = std::max(1, int(std::lround(kDampBandPx * pixelScale)));
    cv::Mat blurred;
    cv::boxFilter(detail::view(alpha, w, h), blurred, CV_32F,
                  cv::Size(2 * bandR + 1, 2 * bandR + 1));

    const float inv = float(pixelScale) / kGradRefPerPx;
    par(h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float *mp = mag.ptr<float>(y);
            const float *bp = blurred.ptr<float>(y);
            float *ap = alpha.data() + size_t(y) * size_t(w);
            for (int x = 0; x < w; ++x) {
                const float m = ap[x];
                if (m <= 0.0f) continue;                     // nothing to take away
                const float b = std::clamp(bp[x], 0.0f, 1.0f);
                const float band = 4.0f * b * (1.0f - b);    // 1 at the boundary, 0 away
                if (band <= 0.0f) continue;
                const float edge = std::clamp(mp[x] * inv, 0.0f, 1.0f);
                ap[x] = std::clamp(m * (1.0f - a * edge * band), 0.0f, 1.0f);
            }
        }
    });
}

} // namespace MaskHalo

#endif // MASKHALO_H
