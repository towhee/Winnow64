#ifndef MASKFALLOFF_H
#define MASKFALLOFF_H

#include <algorithm>
#include <cmath>
#include <vector>

/*
    ONE feather profile for every Develop mask.

    WHY THIS EXISTS. Each mask tool used to taper its own edge with a smootherstep, which
    is flat-ended by construction: coverage sits at exactly 1 across a core, then ramps,
    then sits at exactly 0. Measured against Lightroom on the same frame (white image,
    radial mask, feather 82, exposure -3.0) that reads wrong in two ways -- the flat core
    shows as a hard-edged disc of full-strength effect, and the effect stops dead at the
    mask boundary. Lightroom's radial coverage is a GAUSSIAN: it starts falling at the
    very centre and still carries ~4% at the boundary, dying out around 1.3x the radius.
    Recovering coverage from both screenshots through Winnow's own blend
    (WorkingImageCache's perceptual lerp):

        Winnow  smootherstep from 0.180R (== feather 82)   fit rms 0.002
        LR      Gaussian, sigma 0.450R                      fit rms 0.024
                (best smootherstep fit to LR needs k~0.02 and still misses, rms 0.086)

    Feeding a Gaussian coverage through Winnow's blend reproduced LR's whole radial
    profile to within ~3/255 levels, so the blend space was already right and only the
    coverage profile was wrong.

    THE FAMILY. A super-Gaussian in t, the normalised distance from full coverage (t = 0)
    to the mask boundary (t = 1):

        m(t) = 2^-((t/h)^p)      h = half-coverage radius, p = roundness

    h and p both come from the feather (shapeFor). p = 2 is a true Gaussian, a large p is
    a near-hard edge at h, and p below 2 peaks harder at the centre with a longer tail.
    Anchors:

        feather 0    h = 1,     p huge   -- hard edge at the boundary (callers short-
                                            circuit this case entirely)
        feather 82   h = 0.525, p = 2    -- the Lightroom measurement above
        feather 100  h = 0.421, p = 1.28 -- softest

    Only the feather-82 point is measured. The rest is the curve that makes the 10%-90%
    transition width come out PROPORTIONAL to the feather (width ~ 0.92*f) while passing
    through that anchor, solved numerically and fitted -- so the slider still behaves the
    way it reads, and softness increases monotonically all the way to 100. Re-anchor
    freely if more Lightroom captures at other feather values turn up.

    The profile has no hard end: coverage reaches the boundary (t=1) at 1% for feather 10,
    8% at feather 82, 12% at feather 100, dying out by t = tMax. A mask therefore reaches
    a little past its drawn outline, exactly as Lightroom's does. Overlay guides use the
    HALF-COVERAGE radius h, the only radius the profile makes a promise about.

    THREE ENTRY POINTS, because the tools are not all the same shape:

      Lut     distance-to-boundary falloff -- Radial ellipse, Brush dab. The feather sets
              both the width and the roundness. Table-driven: the pow lives in build(),
              never in the per-pixel loop.
      cdf     the Linear Gradient's two-sided S. A Gaussian CDF, sigma matched to the
              smootherstep's old 10-90 width, so the ramp keeps its apparent width but
              gains tails past p1/p2 instead of stopping dead.
      taper   the fixed-band tools (Luminance/Colour Range, Depth, Subject, Sky, Object).
              There the feather is a TOLERANCE band the caller has already scaled, so
              only the curve's character is shared: same half-coverage at mid-band as the
              smootherstep it replaces, but no flat start and a softer landing. Not
              calibrated against Lightroom -- there is no reference capture for these.
*/
namespace MaskFalloff {

/* h: half-coverage radius in t units. p: roundness (2 = Gaussian). */
struct Shape {
    double h = 1.0;
    double p = 2.0;
};

inline Shape shapeFor(double featherPct)
{
    const double f = std::clamp(featherPct / 100.0, 0.0, 1.0);
    Shape s;
    if (f <= 0.0) { s.h = 1.0; s.p = 1e6; return s; }    // hard edge (callers skip this)
    s.h = 1.0 - 0.579 * f;                  // 1 -> 0.421; 0.525 at the LR-measured f=0.82
    s.p = (3.3 - 2.024 * f) / f;            // huge -> 2 at f=0.82 -> 1.28 at f=1
    return s;
}

/* Within this of 0 or 1 the coverage cannot survive 8-bit quantisation even under a
   heavy adjustment, so the profile is snapped to flat there -- which is what lets it
   terminate (and lets Lut bound its table). Both ends are snapped, so the profile and
   the table agree to the table's interpolation error rather than to this. */
inline constexpr double kCutoff = 1.0 / 512.0;

/* Reference profile. The hot paths go through Lut instead. */
inline float coverage(double t, const Shape &s)
{
    if (t <= 0.0) return 1.0f;
    const double v = std::exp2(-std::pow(t / s.h, s.p));
    if (v < kCutoff) return 0.0f;
    return v > 1.0 - kCutoff ? 1.0f : float(v);
}

/*
    Sampled coverage for the per-pixel loops: 512 steps, linearly interpolated. Built once
    per mask component (Radial) or per dab (Brush) -- a few hundred pow calls against
    millions of pixels.

    The table spans only the TRANSITION, t0 (where coverage leaves 1) to tMax (where it
    reaches 0); outside that at() answers 1 or 0 without a lookup. That matters at low
    feather, where the transition is a sliver of the radius: spreading 512 samples over
    the whole profile put barely 5 of them across a feather-1 edge and cost 1.5% of
    coverage in interpolation error. Spanning the transition instead holds every feather
    to ~1e-5.
*/
class Lut
{
public:
    void build(double featherPct)
    {
        shape_ = shapeFor(featherPct);
        /* 2^-((t/h)^p) == kCutoff  => t = h*9^(1/p)
           2^-((t/h)^p) == 1-kCutoff => t = h*0.002888^(1/p) */
        const double invP = 1.0 / shape_.p;
        t0_   = shape_.h * std::pow(-std::log2(1.0 - kCutoff), invP);
        tMax_ = shape_.h * std::pow(9.0, invP);
        const double span = tMax_ - t0_;
        inv_  = span > 0.0 ? double(N) / span : 0.0;
        for (int i = 0; i <= N; ++i) {
            const double t = t0_ + span * double(i) / double(N);
            const double v = std::exp2(-std::pow(t / shape_.h, shape_.p));
            v_[i] = float(v);
        }
        /* Entries are NOT clamped to the cutoff: the span already ends where the profile
           reaches it, and at() answers a flat 1/0 outside. Clamping the last entry to 0
           would make the final cell ramp down while the real profile is still flat --
           a 1/512 disagreement with coverage() across that one cell. */
    }

    double tMax() const { return tMax_; }
    const Shape &shape() const { return shape_; }

    float at(double t) const
    {
        if (t <= t0_) return 1.0f;
        if (t >= tMax_) return 0.0f;
        const double x = (t - t0_) * inv_;
        const int    i = int(x);
        const float  a = float(x - i);
        return v_[i] * (1.0f - a) + v_[i + 1] * a;
    }

private:
    static constexpr int N = 512;
    float  v_[N + 1] = { 1.0f };
    double t0_   = 1.0;
    double tMax_ = 1.0;
    double inv_  = 0.0;
    Shape  shape_;
};

/* ---- Linear Gradient: Gaussian CDF ---- */

namespace detail {
/* Phi(x) sampled over +/-4 sigma; outside that it is 0/1 to better than 1/32000. */
inline const std::vector<float> &cdfTable()
{
    static const std::vector<float> t = [] {
        std::vector<float> v(1025);
        for (int i = 0; i < 1025; ++i) {
            const double x = -4.0 + 8.0 * double(i) / 1024.0;
            v[size_t(i)] = float(0.5 * std::erfc(-x * 0.7071067811865476));
        }
        return v;
    }();
    return t;
}
} // namespace detail

/* Gaussian CDF in sigma units: 0 far below, 0.5 at 0, 1 far above. */
inline float cdf(double x)
{
    if (x <= -4.0) return 0.0f;
    if (x >=  4.0) return 1.0f;
    const std::vector<float> &t = detail::cdfTable();
    const double u = (x + 4.0) * 128.0;         // 1024 steps over 8 sigma
    const int    i = int(u);
    const float  a = float(u - i);
    return t[size_t(i)] * (1.0f - a) + t[size_t(i) + 1] * a;
}

/* Sigma (in units of the p1->p2 length) for a Linear Gradient at this feather. 0.25*f
   keeps the smootherstep's old 10-90 width (2.563*sigma vs ~0.62*f). */
inline double gradientSigma(double featherPct)
{
    return 0.25 * std::clamp(featherPct / 100.0, 0.0, 1.0);
}

/* ---- Fixed-band tools: one-sided taper ---- */

namespace detail {
inline const std::vector<float> &taperTable()
{
    static const std::vector<float> t = [] {
        std::vector<float> v(513);
        for (int i = 0; i < 513; ++i) {
            const double u = double(i) / 512.0;
            v[size_t(i)] = float(std::exp2(-8.0 * u * u * u));   // 1 at 0, 1/2 at 1/2
        }
        v[512] = 0.0f;
        return v;
    }();
    return t;
}
} // namespace detail

/* u = how far outside the selected range, in band widths. 1 at the range edge, 0.5 at
   mid-band (as the smootherstep was), ~0 at u = 1 -- but falling from the first step
   instead of leaving a flat shoulder inside the range. */
inline float taper(double u)
{
    if (u <= 0.0) return 1.0f;
    if (u >= 1.0) return 0.0f;
    const std::vector<float> &t = detail::taperTable();
    const double x = u * 512.0;
    const int    i = int(x);
    const float  a = float(x - i);
    return t[size_t(i)] * (1.0f - a) + t[size_t(i) + 1] * a;
}

} // namespace MaskFalloff

#endif // MASKFALLOFF_H
