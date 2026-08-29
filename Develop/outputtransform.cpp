#include "Develop/outputtransform.h"
#include <QtConcurrent>
#include <QThreadPool>
#include <QFuture>
#include <QVector>
#include <QtGlobal>
#include <QMutex>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>
#include <array>

namespace {

inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/* Linear -> sRGB transfer (IEC 61966-2-1). */
inline float SrgbGamma(float v)
{
    v = Clamp01(v);
    return v <= 0.0031308f ? 12.92f * v
                           : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

/* Adobe RGB (1998) transfer: a pure power curve, gamma 563/256 = 2.19921875 exactly (the
   spec's odd number, not 2.2 -- the difference is visible in a gradient). */
constexpr float kAdobeGammaInv = 256.0f / 563.0f;

/*
    The working space -> linear output primaries, row major. Both are the product of
    source->XYZ and XYZ->target with matching D65 white, so no chromatic adaptation is
    involved and each row sums to 1 (white -> white).

    Adobe RGB shares sRGB's red and blue primaries and differs only in green, which is why
    its matrix is mostly identity when the source is sRGB -- red borrows from green, and
    blue borrows a little green back.
*/
struct Encoding {
    float m[9];         // primaries matrix; unused when identity
    bool  identity;     // source primaries == output primaries: skip the matrix entirely
    float gammaInv;     // 0 => sRGB piecewise transfer, else a pure power curve
};

/* The linear space an output space's primaries correspond to. */
ColorSpaceMath::ColorSpace LinearOf(OutputTransform::Space space)
{
    using CS = ColorSpaceMath::ColorSpace;
    switch (space) {
    case OutputTransform::Space::DisplayP3: return CS::LinearP3;
    case OutputTransform::Space::AdobeRGB:  return CS::LinearAdobeRGB;
    case OutputTransform::Space::sRGB:      break;
    }
    return CS::LinearSRGB;
}

/*
    The primaries matrix is DERIVED from the source space rather than tabulated, because
    the source is no longer always sRGB: it is whatever ColorSpaceMath::kWorking is. The
    hardcoded sRGB->P3 and sRGB->AdobeRGB matrices this replaces are what
    matrix(LinearSRGB, ...) produces, so nothing changes while the working space is sRGB
    -- but the day it widens, these follow instead of silently mis-converting.
*/
Encoding EncodingFor(OutputTransform::Space space, ColorSpaceMath::ColorSpace from)
{
    const ColorSpaceMath::ColorSpace to = LinearOf(space);
    const float gammaInv =
        (space == OutputTransform::Space::AdobeRGB) ? kAdobeGammaInv : 0.0f;

    Encoding e{};
    e.identity = (from == to);
    e.gammaInv = gammaInv;
    if (!e.identity) {
        float m[3][3];
        ColorSpaceMath::matrixF(from, to, m);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) e.m[i * 3 + j] = m[i][j];
    } else {
        e.m[0] = e.m[4] = e.m[8] = 1.0f;
    }
    return e;
}

/* Primaries conversion, in LINEAR -- before the transfer function, after the tone curve.
   Values above 1 are left alone here; the transfer clamps. */
inline void ToPrimaries(const float m[9], float &r, float &g, float &b)
{
    const float R = m[0]*r + m[1]*g + m[2]*b;
    const float G = m[3]*r + m[4]*g + m[5]*b;
    const float B = m[6]*r + m[7]*g + m[8]*b;
    r = R; g = G; b = B;
}

/* Linear -> the output space's transfer function. */
inline float Transfer(float v, float gammaInv)
{
    return gammaInv > 0.0f ? std::pow(Clamp01(v), gammaInv) : SrgbGamma(v);
}

/*
    THE FILMIC VIEW TRANSFORM: a fixed exposure lift followed by the ACES (Narkowicz)
    shoulder, applied in linear before the transfer function. A raw render is scene-linear
    and otherwise lands dark and flat next to the camera's JPEG; this lifts the midtones
    (~+0.7 EV) and rolls highlights off smoothly instead of hard-clipping. Validated
    against the A9 II embedded preview.

    This WAS the pipeline's only look, applied unconditionally to scene-referred data. It
    is now one value of OutputTransform::ViewTransform and stays the default, so every
    existing render is unchanged. Display-referred input still skips it -- see the header:
    a JPEG already carries its camera's tone curve.
*/
constexpr float kBaselineExposure = 1.6f;       // ~ +0.68 EV

inline float FilmicTone(float v)
{
    v *= kBaselineExposure;
    if (v < 0.0f) v = 0.0f;
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return Clamp01((v * (a * v + b)) / (v * (c * v + d) + e));
}

/*
    AgX -- a wide-latitude view transform whose point is HIGHLIGHT HUE INTEGRITY.

    THE PROBLEM IT SOLVES. A per-channel filmic curve (Filmic above) compresses each
    channel independently, so a saturated colour loses its channels one at a time as it
    brightens: a saturated red marches to orange, a blue to purple. That is where the
    "notorious six" skews come from, and it is also why a brightened radial mask clips to
    a flat patch -- the falloff calibrated in maskfalloff.h becomes invisible in the core.
    HighlightRolloff was reaching for the same thing, but only above 1.0 and only as a
    threshold hack (and on the Filmic path it is dead code, because Filmic clamps first).

    THE SHAPE. Inset the primaries toward the achromatic axis, apply the curve in that
    compressed space over a wide log2 window, then outset back. Because the curve runs in
    a space where the primaries are pulled together, a bright saturated colour desaturates
    smoothly toward white over many stops instead of clipping channel by channel.

        working -> sRGB primaries -> INSET -> log2 window -> sigmoid -> OUTSET -> working

    The matrices fold into their neighbours (see ViewMatrices) so the hot loop stays at
    matrix, table, matrix.

    PROVENANCE. The inset matrix and the log2 window are AgX's published constants. THE
    SIGMOID IS OURS -- a two-sided power curve pinned through the mid-grey pivot, written
    here rather than ported, so nothing in this file carries a third-party licence. That
    makes this AgX's structure and behaviour rather than a bit-exact reproduction of any
    particular AgX build; it will not match a reference render pixel for pixel, and it is
    not meant to.
*/
constexpr float kAgxMinEv = -12.47393f;      // published AgX log2 window
constexpr float kAgxMaxEv =   4.026069f;
constexpr float kAgxEvRange = kAgxMaxEv - kAgxMinEv;      // 16.5 stops

/* AgX inset: sRGB/Rec.709 primaries -> the compressed space the curve runs in. Rows sum
   to 1, so white maps to white and the transform cannot tint a neutral. */
constexpr float kAgxInset[9] = {
    0.842479062253094f, 0.078433599999999f, 0.079223745147764f,
    0.042328242261012f, 0.878468636469772f, 0.079166127460543f,
    0.042375654905705f, 0.078433600000000f, 0.879142973793104f
};

/*
    OUR SIGMOID. Two power segments meeting at the mid-grey pivot, which is what pins the
    thing that matters perceptually: 18% scene grey must come out as 18% display grey, or
    every exposure the user has dialled changes meaning.

    In the normalised log window, 0.18 linear sits at
        x = (log2(0.18) - kAgxMinEv) / kAgxEvRange = 10.0 / 16.5 = 0.60606
    and it must map to display-encoded 0.18^(1/2.2) = 0.4626, which the curve then raises
    to display-LINEAR by ^2.2 on the way out (this stage owes the caller linear light --
    the transfer function is applied later, by Transfer()).

    Both segments pass exactly through (0,0), (pivot), (1,1) and share the same slope at
    the pivot, so the curve is C1 and MONOTONIC BY CONSTRUCTION -- no overshoot, which in
    a tone curve would be a locally inverted tone. kAgxSlope > 1.364 is what makes it an S
    rather than a bulge; 2.4 is a contrast close to AgX's own.
*/
constexpr float kAgxPivotX = 0.60606061f;    // where 0.18 linear lands in the log window
constexpr float kAgxPivotY = 0.45865637f;    // 0.18^(1/2.2), i.e. mid grey out
constexpr float kAgxSlope  = 2.4f;

/*
    The inset with its ROWS NORMALISED TO SUM TO 1. The published constants are rounded
    and their rows sum to 1 +/- 1.4e-4, which is a 0.014% tint on every neutral -- small,
    but it is exactly the error that has no upper bound once other stages compound it,
    and neutrals staying neutral is the one property this matrix must not break. Same
    discipline as Calibrate::buildMatrix, which normalises for the same reason.
*/
const float *AgxInsetNormalised()
{
    static const std::array<float, 9> m = []{
        std::array<float, 9> t{};
        for (int i = 0; i < 3; ++i) {
            const float sum = kAgxInset[i*3] + kAgxInset[i*3+1] + kAgxInset[i*3+2];
            const float k = (std::fabs(sum) > 1e-9f) ? 1.0f / sum : 1.0f;
            for (int j = 0; j < 3; ++j) t[i*3+j] = kAgxInset[i*3+j] * k;
        }
        return t;
    }();
    return m.data();
}

inline float AgxSigmoid(float x)
{
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    if (x < kAgxPivotX) {
        const float a = kAgxSlope * kAgxPivotX / kAgxPivotY;
        return kAgxPivotY * std::pow(x / kAgxPivotX, a);
    }
    const float b = kAgxSlope * (1.0f - kAgxPivotX) / (1.0f - kAgxPivotY);
    return 1.0f - (1.0f - kAgxPivotY) * std::pow((1.0f - x) / (1.0f - kAgxPivotX), b);
}

/* Scene-linear (already in the inset space) -> display-linear 0..1. */
inline float AgxCurve(float v)
{
    if (!(v > 0.0f)) return 0.0f;            // negated: catches NaN as well as <= 0
    const float x = (std::log2(v) - kAgxMinEv) / kAgxEvRange;
    const float s = AgxSigmoid(x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x));
    return std::pow(s, 2.2f);                // display-encoded -> display-linear
}

/*
    The view transform's per-channel curve: scene-linear in, display-linear 0..1 out.

    PER-CHANNEL IS THE POINT. Every stage of the output chain is either a 3x3 or a
    function of one channel, which is what lets the whole thing collapse into matrices
    plus 1-D tables (see the LUT note below) instead of needing a 3-D LUT and its
    interpolation error.
*/
inline float ViewCurve(OutputTransform::ViewTransform vt, float v)
{
    switch (vt) {
    case OutputTransform::ViewTransform::Filmic: return FilmicTone(v);
    case OutputTransform::ViewTransform::AgX:    return AgxCurve(v);
    case OutputTransform::ViewTransform::None:   break;
    }
    return v;
}

/* Does this transform compress everything to <= 1.0? When it does, HighlightRolloff
   provably cannot fire downstream, which is what lets the fused fast path skip its
   per-pixel over-range test. Filmic Clamp01s its own output; None does not. */
inline bool ViewClampsToWhite(OutputTransform::ViewTransform vt)
{
    return vt == OutputTransform::ViewTransform::Filmic
        || vt == OutputTransform::ViewTransform::AgX;
}

/* The transform actually applied: a view transform tone-maps, so display-referred input
   (which already carries a camera tone curve) is forced to None. One rule, one place. */
inline OutputTransform::ViewTransform EffectiveView(OutputTransform::ViewTransform vt,
                                                    bool sceneReferred)
{
    return sceneReferred ? vt : OutputTransform::ViewTransform::None;
}

/*
    THE VIEW TRANSFORM'S OWN COLOUR SPACE.

    A per-channel curve is not the whole story for the interesting transforms. AgX and
    its relatives are: pull the primaries toward the achromatic axis (an INSET matrix),
    apply the curve in that compressed space -- which is what makes highlights desaturate
    toward white smoothly instead of channels clipping one at a time and marching the hue
    round -- then restore some purity with an OUTSET matrix.

    So the general shape of the output stage is MATRIX -> 1-D CURVE -> MATRIX, and this
    struct carries the two matrices. None and Filmic are per-channel and set both to
    identity, which collapses the chain back to a single curve at no cost.

    The OUTSET FOLDS FORWARD. Nothing per-channel sits between it and the output-primaries
    matrix, so the two multiply into one 3x3 built once per render (see ToImage). That is
    what keeps the hot loop at "matrix, table, matrix, table" no matter how many
    conceptual stages the transform has.
*/
/* dst = a . b, both row-major 3x3 -- the fold that keeps a matrix run at one matrix. */
inline void Mul3x3(const float a[9], const float b[9], float dst[9])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float s = 0.0f;
            for (int k = 0; k < 3; ++k) s += a[i * 3 + k] * b[k * 3 + j];
            dst[i * 3 + j] = s;
        }
}

struct ViewMatrices {
    float pre[9]  = {1,0,0, 0,1,0, 0,0,1};
    float post[9] = {1,0,0, 0,1,0, 0,0,1};
    bool  preIdentity  = true;
    bool  postIdentity = true;
};

ViewMatrices ViewMatricesFor(OutputTransform::ViewTransform vt,
                             ColorSpaceMath::ColorSpace working)
{
    switch (vt) {
    /* Both per-channel: no space of their own, so no matrices. */
    case OutputTransform::ViewTransform::None:
    case OutputTransform::ViewTransform::Filmic:
        break;

    case OutputTransform::ViewTransform::AgX: {
        /* AgX's inset is defined against sRGB/Rec.709 primaries, so compose the
           working->sRGB hop into it rather than assuming the working space IS sRGB. That
           is what lets the working space widen without silently mis-insetting. */
        float toSrgb[3][3], fromSrgb[3][3];
        ColorSpaceMath::matrixF(working, ColorSpaceMath::ColorSpace::LinearSRGB, toSrgb);
        ColorSpaceMath::matrixF(ColorSpaceMath::ColorSpace::LinearSRGB, working, fromSrgb);
        float toSrgb9[9], fromSrgb9[9];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                toSrgb9[i * 3 + j]   = toSrgb[i][j];
                fromSrgb9[i * 3 + j] = fromSrgb[i][j];
            }

        const float *inset = AgxInsetNormalised();
        ViewMatrices vm;
        Mul3x3(inset, toSrgb9, vm.pre);
        vm.preIdentity = false;

        /* The outset is the inset's inverse: the curve runs in the compressed space and
           the result is brought back out of it. Inverted numerically so the two can never
           drift apart the way two hand-tabulated matrices would. */
        double in[3][3], inv[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) in[i][j] = inset[i * 3 + j];
        float outset9[9] = {1,0,0, 0,1,0, 0,0,1};
        if (ColorSpaceMath::invert3x3(in, inv))
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    outset9[i * 3 + j] = static_cast<float>(inv[i][j]);
        Mul3x3(fromSrgb9, outset9, vm.post);
        vm.postIdentity = false;
        return vm;
    }
    }
    return ViewMatrices{};
}


/*
    Highlight rolloff: applied to every render in LINEAR, in the OUTPUT primaries (after
    the matrix, before the transfer) -- that is the space the clip actually happens in.

    Without it the only thing standing between an over-range value and the file is the
    transfer function's Clamp01 -- a hard per-channel clip. That is what makes a
    brightened radial mask look wrong: the core of the mask clips flat, so the smooth
    smootherstep falloff is invisible there and re-appears with a derivative kink at the
    radius where the channel drops back under 1.0 (a Mach-band ring), and because the
    channels clip one at a time the hue swings on the way out.

    Instead the energy the peak channel loses to the clip is bled into the other two, so
    an over-range colour desaturates toward white the way Lightroom's does: pushing a
    saturated green past white lifts red and blue instead of pinning green at 255 and
    freezing the pixel. Measured on the green test frame that puts the mask core at
    ~(154,255,151) against Lightroom's ~(172,255,164), and it removes the flat plateau --
    brightness keeps rising smoothly through the region where green alone is clipped.

    Everything at or below white is left BIT-EXACT: nothing but the over-range part of a
    render can change, so an unedited image (and every export of one) is untouched. That
    rules out a compressive shoulder here -- any curve with a soft knee below 1.0 would
    have to darken existing whites to make room.
*/
inline void HighlightRolloff(float &r, float &g, float &b)
{
    const float mx = qMax(r, qMax(g, b));
    if (mx <= 1.0f) return;                     // in range: the common case, untouched
    const float bleed = mx - 1.0f;              // the part that would have been clipped
    r = qMin(1.0f, r + bleed);
    g = qMin(1.0f, g + bleed);
    b = qMin(1.0f, b + bleed);
}

/*
    TRANSFER LUT (8-bit sRGB output only).

    For the sRGB path the per-channel chain collapses to a pure 1-D function of the
    channel value: the primaries matrix is identity (skipped), and the only cross-channel
    step, HighlightRolloff, cannot fire when the baseline tone curve ran -- BaselineTone
    Clamp01s its own output, so nothing reaches the rolloff above 1.0. That leaves
    SrgbGamma(BaselineTone(v)) (or SrgbGamma(v) for display-referred input), which a table
    plus linear interpolation evaluates ~4.8x faster than the pow it replaces. Measured:
    toImage was ~90% of an interactive Develop tick, 35 ms of 38 ms on a 5.9 MP proxy.

    NOT BIT-EXACT, deliberately and with the difference bounded: about 0.001% of inputs
    (1 in 90,000) land one level of 255 away from the pow, always by exactly 1, at values
    that fall on a rounding boundary. A denser table does not remove them -- the residue
    is float rounding, and an alternative formulation using exact per-level thresholds was
    measured with the same residue and only 2.9x -- so this is the faster of two equally
    approximate options, not a shortcut past an exact one. It is used for BOTH the preview
    and 8-bit export so the two always agree; ToImage16 keeps the exact maths, since 16-bit
    output is where precision is the point.

    kToneDomainMax is where BaselineTone saturates: solving f(u)=1 for u=1.6v gives
    u=7.2416, i.e. v=4.53. Past the table's end the function is flat at white, so a clamp
    to the last entry is exact there.
*/
constexpr int   kLutSize       = 16384;
constexpr float kToneDomainMax = 4.6f;      // covers FilmicTone's saturation at v=4.53

/*
    INDEXING. Most curves are sampled on a LINEAR axis (scale = index per unit value).
    AgX is not: it is a function of log2(v) over a 16.5-stop window, so a linear axis
    would spend almost the whole table on highlights and leave the shadows with about ONE
    entry -- 0.001 linear lands at index 1 of 16384. logIndexed switches the axis to the
    normalised log coordinate the curve is actually built on, which costs one log2 per
    channel (the same order as the pow the Develop tone LUT already does per channel) and
    buys uniform resolution across the whole window.
*/
struct TransferLut {
    float v[kLutSize + 1];                  // encoded 0..1, indexed per `logIndexed`
    float scale;                            // linear axis: value -> table index
    float domainMax;
    bool  logIndexed = false;
    float logMin = 0.0f;                    // log2 value at table index 0
    float logInvRange = 1.0f;               // 1 / (log2 span the table covers)
};

/* Where v falls in the table, as a float index in [0, kLutSize]. */
inline float LutIndex(const TransferLut &t, float v)
{
    if (!t.logIndexed) return v * t.scale;
    if (!(v > 0.0f)) return 0.0f;           // negated: catches NaN too
    const float u = (std::log2(v) - t.logMin) * t.logInvRange;
    return (u <= 0.0f ? 0.0f : (u >= 1.0f ? 1.0f : u)) * float(kLutSize);
}

/*
    The axis a transform's table is sampled on. AgX gets the log axis its curve is
    defined on; everything else a linear one. A transform that compresses to white only
    needs the domain up to where it saturates; one that does not is only ever asked for
    0..1 (anything above is handled by the rolloff, off this path).
*/
void SetLutAxis(TransferLut &t, OutputTransform::ViewTransform vt)
{
    if (vt == OutputTransform::ViewTransform::AgX) {
        t.logIndexed  = true;
        t.logMin      = kAgxMinEv;
        t.logInvRange = 1.0f / kAgxEvRange;
        t.domainMax   = std::exp2(kAgxMaxEv);
        t.scale       = float(kLutSize) / t.domainMax;   // unused on the log axis
        return;
    }
    t.logIndexed = false;
    t.domainMax  = ViewClampsToWhite(vt) ? kToneDomainMax : 1.0f;
    t.scale      = float(kLutSize) / t.domainMax;
}

/* The input value entry i of the table stands for -- the inverse of LutIndex. */
float LutSampleValue(const TransferLut &t, int i)
{
    const float f = float(i) / float(kLutSize);
    if (!t.logIndexed) return t.domainMax * f;
    return std::exp2(t.logMin + f / t.logInvRange);
}

/*
    Built once per (view transform, output space) combination and cached. Function-local
    statics were enough while there were exactly two tables; a selectable view transform
    makes the set open-ended, so this is a small keyed cache under a mutex. Building a
    table is ~16k evaluations -- trivial next to the megapixels it then serves, and it
    happens once per combination for the life of the process.

    The proxy worker, the settle worker and the export worker all reach this from
    different threads, hence the lock. Entries are never evicted (there is a handful) and
    never mutated after construction, so handing back a const reference is safe.
*/
const TransferLut &LutFor(OutputTransform::ViewTransform vt, float gammaInv)
{
    struct Key {
        OutputTransform::ViewTransform vt;
        float gammaInv;
        bool operator==(const Key &o) const { return vt == o.vt && gammaInv == o.gammaInv; }
    };
    static QMutex mutex;
    static std::vector<std::pair<Key, std::unique_ptr<TransferLut>>> cache;

    const Key key{vt, gammaInv};
    QMutexLocker lock(&mutex);
    for (const auto &e : cache)
        if (e.first == key) return *e.second;

    auto t = std::make_unique<TransferLut>();
    SetLutAxis(*t, vt);
    for (int i = 0; i <= kLutSize; ++i)
        t->v[i] = Transfer(ViewCurve(vt, LutSampleValue(*t, i)), gammaInv);
    const TransferLut &ref = *t;
    cache.emplace_back(key, std::move(t));
    return ref;
}

/*
    The VIEW-ONLY table: scene-linear in, display-linear out, WITHOUT the transfer
    function. Needed whenever the primaries matrix is not the identity, because the matrix
    sits between the view curve and the transfer and so the two cannot be fused.

    That case used to be export-only (P3 / Adobe RGB), which is why the original code
    simply fell back to exact maths there. It stops being rare the moment the working
    space is wider than the output space -- then EVERY sRGB render needs a matrix -- so
    the fallback needs to be a table too, not a pow. This plus the transfer table keeps
    the chain at two lookups and a matrix, with no transcendental in the hot loop.
*/
const TransferLut &ViewOnlyLut(OutputTransform::ViewTransform vt)
{
    static QMutex mutex;
    static std::vector<std::pair<OutputTransform::ViewTransform,
                                 std::unique_ptr<TransferLut>>> cache;
    QMutexLocker lock(&mutex);
    for (const auto &e : cache)
        if (e.first == vt) return *e.second;

    auto t = std::make_unique<TransferLut>();
    SetLutAxis(*t, vt);
    for (int i = 0; i <= kLutSize; ++i)
        t->v[i] = ViewCurve(vt, LutSampleValue(*t, i));
    const TransferLut &ref = *t;
    cache.emplace_back(vt, std::move(t));
    return ref;
}

/* Interpolated lookup returning the table's float value (not quantised). */
inline float LutValue(const TransferLut &t, float v)
{
    const float fi = LutIndex(t, v);
    if (!(fi > 0.0f)) return t.v[0];        // negated: catches NaN too
    if (fi >= float(kLutSize)) return t.v[kLutSize];
    const int i = static_cast<int>(fi);
    const float fr = fi - float(i);
    return t.v[i] + (t.v[i + 1] - t.v[i]) * fr;
}

inline uchar LutSample(const TransferLut &t, float v)
{
    const float fi = LutIndex(t, v);
    /* Negated compare so a NaN takes this branch too: (int)NaN is undefined behaviour,
       and a stray NaN pixel must not be able to index off the table. */
    if (!(fi > 0.0f)) return 0;
    if (fi >= float(kLutSize)) return static_cast<uchar>(std::lround(t.v[kLutSize] * 255.0f));
    const int i = static_cast<int>(fi);
    const float fr = fi - float(i);
    return static_cast<uchar>(std::lround((t.v[i] + (t.v[i + 1] - t.v[i]) * fr) * 255.0f));
}

/*
    Run processRows(y0, y1) over the image's rows, parallelised over disjoint row chunks
    (QtConcurrent + the global pool) exactly as Develop::applyPointOps is. Rows are
    disjoint, so the threads write into the output buffer without contention. Shared by
    the 8-bit and 16-bit transforms -- only the per-pixel packing differs between them.
*/
template <typename RowFn>
void RunRows(int H, RowFn processRows)
{
    const int maxThreads = qMax(1, QThreadPool::globalInstance()->maxThreadCount());
    const int kMinRowsPerChunk = 64;
    if (maxThreads == 1 || H < kMinRowsPerChunk * 2) {
        processRows(0, H);
        return;
    }

    const int chunks = qMin(maxThreads, (H + kMinRowsPerChunk - 1) / kMinRowsPerChunk);
    const int rowsPerChunk = (H + chunks - 1) / chunks;
    QVector<QFuture<void>> futures;
    futures.reserve(chunks);
    for (int k = 0; k < chunks; ++k) {
        const int y0 = k * rowsPerChunk;
        const int y1 = qMin(H, y0 + rowsPerChunk);
        if (y0 >= y1) break;
        futures.append(QtConcurrent::run(QThreadPool::globalInstance(),
                                         [processRows, y0, y1]() { processRows(y0, y1); }));
    }
    for (QFuture<void> &f : futures) f.waitForFinished();
}

} // namespace

QColorSpace OutputTransform::ColorSpaceOf(Space space)
{
    switch (space) {
    case Space::DisplayP3: return QColorSpace(QColorSpace::DisplayP3);
    case Space::AdobeRGB:  return QColorSpace(QColorSpace::AdobeRgb);
    case Space::sRGB:      break;
    }
    return QColorSpace(QColorSpace::SRgb);
}

bool OutputTransform::ToImage(const WorkingImage &img, QImage &out, Space space,
                              ViewTransform view)
{
    if (!img.isValid()) return false;

    const int W = img.width;
    const int H = img.height;
    const float scale = img.white > 0.0f ? 1.0f / img.white : 1.0f;

    out = QImage(W, H, QImage::Format_RGB888);
    if (out.isNull()) return false;

    const OutputTransform::ViewTransform vt = EffectiveView(view, img.sceneReferred);
    const bool clampsToWhite = ViewClampsToWhite(vt);
    Encoding enc = EncodingFor(space, img.space);
    const float *rgb = img.rgb.data();
    uchar *bits = out.bits();
    const qsizetype bpl = out.bytesPerLine();

    /* Fold the view transform's OUTSET into the output-primaries matrix: nothing
       per-channel separates them, so they are one 3x3. For a per-channel transform
       (None, Filmic) the outset is the identity and enc is untouched. */
    const ViewMatrices vm = ViewMatricesFor(vt, img.space);
    if (!vm.postIdentity) {
        float folded[9];
        Mul3x3(enc.m, vm.post, folded);
        for (int i = 0; i < 9; ++i) enc.m[i] = folded[i];
        enc.identity = false;
    }

    /* FUSED fast path -- taken when the primaries matrix is the identity, so the whole
       per-channel chain (view curve then transfer) is 1-D and one table replaces it. See
       the TRANSFER LUT note above for what it costs in accuracy and why it is not exact.

       When the view transform compresses to white the rolloff provably cannot fire, so
       every pixel takes the table. When it does not -- ViewTransform::None, i.e. any
       display-referred file -- a channel can exceed 1 and the rolloff is cross-channel,
       so those pixels fall back to the exact maths; one compare per pixel buys that, and
       in-range pixels (the overwhelming majority) still take the table. */
    /* vm.preIdentity is part of the condition: an inset matrix ahead of the curve breaks
       the 1-D chain just as surely as a primaries matrix behind it. (enc.identity has
       already absorbed the outset above.) */
    if (enc.identity && vm.preIdentity) {
        const TransferLut &lut = LutFor(vt, enc.gammaInv);
        auto processRowsLut = [=, &lut](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                uchar *line = bits + static_cast<qsizetype>(y) * bpl;
                const size_t base = static_cast<size_t>(y) * W * 3;
                for (int x = 0; x < W; ++x) {
                    const size_t o = base + static_cast<size_t>(x) * 3;
                    const float v0 = rgb[o + 0] * scale;
                    const float v1 = rgb[o + 1] * scale;
                    const float v2 = rgb[o + 2] * scale;
                    if (!clampsToWhite) {
                        const float mx = qMax(v0, qMax(v1, v2));
                        if (mx > 1.0f) {          // rolloff applies: exact maths
                            float w[3] = {v0, v1, v2};
                            HighlightRolloff(w[0], w[1], w[2]);
                            for (int c = 0; c < 3; ++c)
                                line[x * 3 + c] = static_cast<uchar>(
                                    std::lround(Transfer(w[c], enc.gammaInv) * 255.0f));
                            continue;
                        }
                    }
                    line[x * 3 + 0] = LutSample(lut, v0);
                    line[x * 3 + 1] = LutSample(lut, v1);
                    line[x * 3 + 2] = LutSample(lut, v2);
                }
            }
        };
        RunRows(H, processRowsLut);
        return true;
    }

    /*
        SPLIT path -- the primaries matrix is cross-channel and sits between the view
        curve and the transfer, so the two cannot fuse. Still no transcendental in the
        loop: the view curve is one table, the transfer another, with the matrix and the
        rolloff between them.

        This used to be export-only (P3 / Adobe RGB) and ran exact pow maths. It becomes
        the path EVERY render takes as soon as the working space is wider than the output
        space, which is why it is now table-driven rather than a fallback.
    */
    /* None is a genuine pass-through and must NOT go through a table: its domain stops at
       1.0, so tabulating it would clamp exactly the over-range values HighlightRolloff
       exists to handle, and the rolloff would never fire. */
    const bool applyView = (vt != OutputTransform::ViewTransform::None);
    const TransferLut &viewLut = ViewOnlyLut(vt);
    const TransferLut &encLut  = LutFor(OutputTransform::ViewTransform::None, enc.gammaInv);
    auto processRows = [=, &viewLut, &encLut](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            uchar *line = bits + static_cast<qsizetype>(y) * bpl;
            const size_t base = static_cast<size_t>(y) * W * 3;
            for (int x = 0; x < W; ++x) {
                const size_t o = base + static_cast<size_t>(x) * 3;
                float v[3];
                for (int c = 0; c < 3; ++c) v[c] = rgb[o + c] * scale;
                /* INSET, then the curve, then the folded outset+primaries matrix. */
                if (!vm.preIdentity) ToPrimaries(vm.pre, v[0], v[1], v[2]);
                if (applyView)
                    for (int c = 0; c < 3; ++c) v[c] = LutValue(viewLut, v[c]);
                ToPrimaries(enc.m, v[0], v[1], v[2]);
                HighlightRolloff(v[0], v[1], v[2]);
                for (int c = 0; c < 3; ++c)
                    line[x * 3 + c] = static_cast<uchar>(
                        std::lround(LutValue(encLut, v[c]) * 255.0f));
            }
        }
    };

    RunRows(H, processRows);
    return true;
}

bool OutputTransform::ToImage16(const WorkingImage &img, QImage &out, Space space,
                                ViewTransform view)
{
/*
    16-bit twin of ToImage, for the export path. Identical maths -- the same baseline tone
    curve, primaries and transfer -- quantised to 16 bits per channel into
    Format_RGBX64 (four quint16 per pixel: R, G, B, unused-opaque). The develop pipeline
    is float
    throughout, so this is the only stage that loses precision; at 16 bits the loss is
    below what any downstream edit can reveal.
*/
    if (!img.isValid()) return false;

    const int W = img.width;
    const int H = img.height;
    const float scale = img.white > 0.0f ? 1.0f / img.white : 1.0f;

    out = QImage(W, H, QImage::Format_RGBX64);
    if (out.isNull()) return false;

    const OutputTransform::ViewTransform vt = EffectiveView(view, img.sceneReferred);
    Encoding enc = EncodingFor(space, img.space);
    const float *rgb = img.rgb.data();
    uchar *bits = out.bits();
    const qsizetype bpl = out.bytesPerLine();

    /* Same outset fold as ToImage -- the two must stay one set of maths, which is what
       guarantees a 16-bit export matches the loupe. */
    const ViewMatrices vm = ViewMatricesFor(vt, img.space);
    if (!vm.postIdentity) {
        float folded[9];
        Mul3x3(enc.m, vm.post, folded);
        for (int i = 0; i < 9; ++i) enc.m[i] = folded[i];
        enc.identity = false;
    }

    auto processRows = [=](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            quint16 *line = reinterpret_cast<quint16*>(bits + static_cast<qsizetype>(y) * bpl);
            const size_t base = static_cast<size_t>(y) * W * 3;
            for (int x = 0; x < W; ++x) {
                const size_t o = base + static_cast<size_t>(x) * 3;
                float v[3];
                for (int c = 0; c < 3; ++c) v[c] = rgb[o + c] * scale;
                if (!vm.preIdentity) ToPrimaries(vm.pre, v[0], v[1], v[2]);
                for (int c = 0; c < 3; ++c) v[c] = ViewCurve(vt, v[c]);
                if (!enc.identity) ToPrimaries(enc.m, v[0], v[1], v[2]);
                HighlightRolloff(v[0], v[1], v[2]);
                for (int c = 0; c < 3; ++c)
                    line[x * 4 + c] = static_cast<quint16>(
                        std::lround(Transfer(v[c], enc.gammaInv) * 65535.0f));
                line[x * 4 + 3] = 0xFFFF;   // RGBX64: the fourth channel is unused/opaque
            }
        }
    };

    RunRows(H, processRows);
    return true;
}
