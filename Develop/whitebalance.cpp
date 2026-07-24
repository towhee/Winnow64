#include "Develop/whitebalance.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace {

/* XYZ -> linear sRGB (D65). The inverse of the rgbToXyz used by RawColor. */
const float kXyzToSrgb[3][3] = {
    { 3.2404542f, -1.5371385f, -0.4985314f},
    {-0.9692660f,  1.8760108f,  0.0415560f},
    { 0.0556434f, -0.2040259f,  1.0572252f}
};

/*
    Kelvin -> CIE 1931 xy. TWO loci, because no single one is right across the range:

      < 4000 K   the PLANCKIAN (blackbody) locus. Tungsten and candlelight really are
                 incandescent, so a blackbody is what they sit on. (Kim et al., 1999.)
      > 5000 K   the CIE DAYLIGHT locus. Daylight is emphatically NOT a blackbody -- it
                 sits measurably above the Planckian locus -- and the standard D
                 illuminants are defined on it. Using Planckian here puts D65 about
                 550 K and 22 tint units off, so an untouched sRGB file would open
                 reading "7050 K, -22" instead of "6500, 0".

    Between them the two are crossfaded, because a step change part-way along the Temp
    slider would show up as a colour jump while dragging through 4000 K.

    Nominally valid to 25000 K; above that the daylight branch is extrapolated, which is
    well behaved (every term decays as 1/T, so x tends to a constant ~0.237) -- the
    25000..50000 K end of the slider keeps moving the right way even though no real
    illuminant lives there.
*/
void planckianXY(double T, double &x, double &y)
{
    const double t1 = 1.0 / T, t2 = t1 * t1, t3 = t2 * t1;
    if (T < 4000.0)
        x = -0.2661239e9 * t3 - 0.2343589e6 * t2 + 0.8776956e3 * t1 + 0.179910;
    else
        x = -3.0258469e9 * t3 + 2.1070379e6 * t2 + 0.2226347e3 * t1 + 0.240390;

    const double x2 = x * x, x3 = x2 * x;
    if (T < 2222.0)
        y = -1.1063814 * x3 - 1.34811020 * x2 + 2.18555832 * x - 0.20219683;
    else if (T < 4000.0)
        y = -0.9549476 * x3 - 1.37418593 * x2 + 2.09137015 * x - 0.16748867;
    else
        y =  3.0817580 * x3 - 5.87338670 * x2 + 3.75112997 * x - 0.37001483;
}

void daylightXY(double T, double &x, double &y)
{
    const double t1 = 1.0 / T, t2 = t1 * t1, t3 = t2 * t1;
    if (T <= 7000.0)
        x = -4.6070e9 * t3 + 2.9678e6 * t2 + 0.09911e3 * t1 + 0.244063;
    else
        x = -2.0064e9 * t3 + 1.9018e6 * t2 + 0.24748e3 * t1 + 0.237040;
    y = -3.000 * x * x + 2.870 * x - 0.275;
}

void locusXY(double T, double &x, double &y)
{
    T = std::clamp(T, 1667.0, 100000.0);
    if (T <= 4000.0) { planckianXY(T, x, y); return; }
    if (T >= 5000.0) { daylightXY(T, x, y);  return; }

    double px, py, dx, dy;
    planckianXY(T, px, py);
    daylightXY(T, dx, dy);
    const double u = (T - 4000.0) / 1000.0;
    const double w = u * u * (3.0 - 2.0 * u);       // smoothstep: no kink at either end
    x = px + (dx - px) * w;
    y = py + (dy - py) * w;
}

/*
    Tint displaces the chromaticity perpendicular to the locus, which is what the CIE
    1960 (u,v) v axis is: the classic Duv.

    SIGN: these controls describe the LIGHT, and the correction is its opposite -- the
    same logic as temperature, where a higher Kelvin says "the light was bluer" and so
    renders the image warmer. Positive tint therefore assumes a GREENER illuminant
    (+v, above the locus), which renders the image more MAGENTA -- matching what the
    Lightroom slider does under the hand. kTintDuv scales the +/-150 slider to a Duv of
    about +/-0.037, a full-strength correction.
*/
constexpr double kTintDuv = 0.00025;

void applyTint(double &x, double &y, double tint)
{
    if (tint == 0.0) return;
    const double d = -2.0 * x + 12.0 * y + 3.0;
    if (std::fabs(d) < 1e-12) return;
    double u = 4.0 * x / d;
    double v = 6.0 * y / d;
    v += tint * kTintDuv;
    const double d2 = 2.0 * u - 8.0 * v + 4.0;
    if (std::fabs(d2) < 1e-12) return;
    x = 3.0 * u / d2;
    y = 2.0 * v / d2;
}

/* The rendered linear-sRGB colour of a neutral surface under illuminant (K, tint),
   carried through the same chain the decode used. */
bool renderIlluminant(const CameraColor &cam, double kelvin, double tint, double s[3])
{
    double x, y;
    locusXY(kelvin, x, y);
    applyTint(x, y, tint);
    if (y < 1e-9) return false;

    const double XYZ[3] = {x / y, 1.0, (1.0 - x - y) / y};

    double camRaw[3];
    for (int i = 0; i < 3; ++i)
        camRaw[i] = cam.xyzToCam[i][0] * XYZ[0] + cam.xyzToCam[i][1] * XYZ[1]
                  + cam.xyzToCam[i][2] * XYZ[2];

    /* The white balance the decode already baked in. */
    for (int i = 0; i < 3; ++i) camRaw[i] *= cam.asShotMul[i];

    for (int i = 0; i < 3; ++i)
        s[i] = cam.camToSrgb[i][0] * camRaw[0] + cam.camToSrgb[i][1] * camRaw[1]
             + cam.camToSrgb[i][2] * camRaw[2];

    /* An extreme temperature/tint combination can put the illuminant outside the sRGB
       gamut, giving a non-positive channel. Clamp rather than fail: the solve's
       bisections need a usable value at every probe, and a hard failure there would
       abandon the whole search over one out-of-gamut trial point. The ratios stay
       monotonic through the clamp, so the bisection still converges from the inside. */
    for (int i = 0; i < 3; ++i)
        if (!(s[i] > 1e-9)) s[i] = 1e-9;
    return true;
}

/* Chromaticity error axes used by the bisections. warmth falls as K rises (hotter =
   bluer), so it is monotonic and safe to bisect; green is the residual after warmth. */
double warmthOf(const double c[3]) { return std::log(c[0] / c[2]); }
double greenOf (const double c[3]) { return std::log(c[1] / std::sqrt(c[0] * c[2])); }

} // namespace

namespace WhiteBalance {

QString presetName(Preset p)
{
    switch (p) {
    case Preset::AsShot:      return "As Shot";
    case Preset::Auto:        return "Auto";
    case Preset::Daylight:    return "Daylight";
    case Preset::Cloudy:      return "Cloudy";
    case Preset::Shade:       return "Shade";
    case Preset::Tungsten:    return "Tungsten";
    case Preset::Fluorescent: return "Fluorescent";
    case Preset::Flash:       return "Flash";
    case Preset::Custom:      return "Custom";
    }
    return "As Shot";
}

Preset presetFromName(const QString &name)
{
    for (int i = 0; i <= int(Preset::Custom); ++i) {
        const Preset p = static_cast<Preset>(i);
        if (presetName(p).compare(name, Qt::CaseInsensitive) == 0) return p;
    }
    return Preset::AsShot;
}

/* Adobe's standard illuminants for the named balances. */
bool presetValues(Preset p, float &kelvin, float &tint)
{
    switch (p) {
    case Preset::Daylight:    kelvin = 5500;  tint = 10;  return true;
    case Preset::Cloudy:      kelvin = 6500;  tint = 10;  return true;
    case Preset::Shade:       kelvin = 7500;  tint = 10;  return true;
    case Preset::Tungsten:    kelvin = 2850;  tint =  0;  return true;
    case Preset::Fluorescent: kelvin = 3800;  tint = 21;  return true;
    case Preset::Flash:       kelvin = 5500;  tint =  0;  return true;
    default: return false;      // AsShot / Auto / Custom are not fixed illuminants
    }
}

void gains(const CameraColor &cam, float kelvin, float tint, float out[3])
{
    out[0] = out[1] = out[2] = 1.0f;
    if (!cam.valid) return;

    double s[3];
    if (!renderIlluminant(cam, kelvin, tint, s)) return;

    /* Gains that make the illuminant's neutral grey again, green pinned to 1 so
       exposure does not drift as temperature changes. */
    out[0] = static_cast<float>(s[1] / s[0]);
    out[1] = 1.0f;
    out[2] = static_cast<float>(s[1] / s[2]);
}

void relativeGains(const CameraColor &cam, float kelvin, float tint, float out[3])
{
    out[0] = out[1] = out[2] = 1.0f;
    if (!cam.valid) return;

    float g[3], ref[3];
    gains(cam, kelvin, tint, g);
    gains(cam, cam.asShotK, cam.asShotTint, ref);
    for (int i = 0; i < 3; ++i)
        out[i] = (ref[i] > 1e-9f) ? g[i] / ref[i] : g[i];

    /* Re-pin green: the reference division can leave it a hair off 1. */
    if (out[1] > 1e-9f) {
        const float n = 1.0f / out[1];
        out[0] *= n; out[1] = 1.0f; out[2] *= n;
    }
}

void resolve(const CameraColor &cam, float storedTemp, float storedTint,
             float &kelvin, float &tint)
{
    /* Storage is ABSOLUTE, with temp == 0 the sentinel for "untouched / as shot" (not
       0 K). The panel always writes temp and tint together -- moving Tint alone still
       commits the resolved Kelvin -- so temp == 0 only ever means a pristine image and
       the two values can never disagree about which reference they are relative to. */
    if (storedTemp > 0.0f) {
        kelvin = storedTemp;
        tint   = storedTint;
    } else {
        kelvin = (cam.asShotK > 0.0f) ? cam.asShotK : 6500.0f;
        tint   = cam.asShotTint;
    }
    kelvin = std::clamp(kelvin, kMinKelvin, kMaxKelvin);
    tint   = std::clamp(tint, kMinTint, kMaxTint);
}

bool solve(const CameraColor &cam, float r, float g, float b,
           float &kelvin, float &tint)
{
    if (!cam.valid) return false;
    if (r <= 1e-9f || g <= 1e-9f || b <= 1e-9f) return false;

    const double target[3] = {r, g, b};
    const double wantWarmth = warmthOf(target);
    const double wantGreen  = greenOf(target);

    double s[3];

    /*
        NESTED, not alternating. The two axes are only nearly orthogonal, and treating
        them as independent (bisect K, then bisect tint, repeat) leaves each K step
        evaluated against a stale tint. That converges fine near the locus but drifts
        badly at strong tints -- a +80 tint at 3400 K came back as 2742 K / +42.

        So: for EVERY candidate K, solve the tint exactly first, then judge that K on
        the warmth it produces with its own matched tint. The outer bisection is then
        monotonic in K and converges to the true pair. It costs ~1600 evaluations of a
        30-flop function, which is irrelevant here -- this runs on a dropper click, an
        Auto pick and once per decode, never per rendered pixel.
    */
    auto tintFor = [&](double K) {
        double tlo = kMinTint, thi = kMaxTint, T = 0.0;
        for (int it = 0; it < 40; ++it) {
            T = 0.5 * (tlo + thi);
            renderIlluminant(cam, K, T, s);
            /* Positive tint assumes a greener light, so the rendered green RISES with
               it -- the opposite direction to warmth against K. */
            if (greenOf(s) < wantGreen) tlo = T;
            else                        thi = T;
        }
        return T;
    };

    double lo = kMinKelvin, hi = kMaxKelvin, K = 6500.0, T = 0.0;
    for (int it = 0; it < 40; ++it) {
        K = 0.5 * (lo + hi);
        T = tintFor(K);
        renderIlluminant(cam, K, T, s);
        /* warmth decreases as K rises. */
        if (warmthOf(s) > wantWarmth) lo = K;
        else                          hi = K;
    }
    T = tintFor(K);

    kelvin = static_cast<float>(std::clamp(K, double(kMinKelvin), double(kMaxKelvin)));
    tint   = static_cast<float>(std::clamp(T, double(kMinTint), double(kMaxTint)));
    return true;
}

SkinPick solveSkin(const CameraColor &cam, float r, float g, float b,
                   float &kelvin, float &tint, float *hueErrorDeg)
{
    if (hueErrorDeg) *hueErrorDeg = 0.0f;
    if (!cam.valid) return SkinPick::Degenerate;
    if (r <= 1e-9f || g <= 1e-9f || b <= 1e-9f) return SkinPick::Degenerate;

    /* The sample in the (warmth, green) plane. Neutral is the ORIGIN here, so the
       vector's direction is the hue and its length the saturation. */
    const double s[3] = {r, g, b};
    const double w = warmthOf(s);
    const double gr = greenOf(s);
    const double mag = std::hypot(w, gr);
    if (mag < kSkinMinChroma) return SkinPick::TooNeutral;

    const double th = kSkinHueDeg * 0.017453292519943295;
    const double ux = std::cos(th), uy = std::sin(th);

    /* Signed angle from the skin direction, wrapped to [-180, 180]. */
    double err = std::atan2(gr, w) - th;
    while (err >  M_PI) err -= 2.0 * M_PI;
    while (err < -M_PI) err += 2.0 * M_PI;
    const double errDeg = std::fabs(err) * 57.29577951308232;
    if (hueErrorDeg) *hueErrorDeg = static_cast<float>(errDeg);

    /* along <= 0 means the sample points the OPPOSITE way down the line (a cyan/blue
       patch); projecting would flip it through neutral, so treat it as not-skin. */
    const double along = w * ux + gr * uy;
    if (errDeg > kSkinHueToleranceDeg || along <= 0.0) return SkinPick::NotSkin;

    /* Project onto the line: keep the along-line component (the skin's own saturation),
       drop the perpendicular error (the cast). */
    const double tw = along * ux, tgr = along * uy;

    /* Back to a colour, green pinned to 1 -- the inverse of warmthOf/greenOf:
           tw  = log(r/b)              -> log r - log b = tw
           tgr = log(1/sqrt(r*b))      -> log r + log b = -2*tgr                */
    const double tr = std::exp((tw - 2.0 * tgr) * 0.5);
    const double tb = std::exp((-tw - 2.0 * tgr) * 0.5);
    if (!(tr > 1e-9) || !(tb > 1e-9)) return SkinPick::Degenerate;

    /*
        Reduce to the neutral case. The correction that carries sample s onto target T
        is the one that NEUTRALISES the component-wise ratio s/T: the gains solve()
        yields for a colour c are (c.g/c.r, 1, c.g/c.b), and substituting c = s/T makes
        the corrected sample proportional to T. Picking a neutral is just T = (1,1,1).
    */
    if (!solve(cam, static_cast<float>(r / tr), g, static_cast<float>(b / tb),
               kelvin, tint))
        return SkinPick::Degenerate;
    return SkinPick::Ok;
}

void resolveAsShot(CameraColor &cam)
{
    if (!cam.valid) return;
    /* The as-shot render is neutral by construction (RawColor's row-normalised matrix
       maps the as-shot neutral to (1,1,1)), so the illuminant that renders neutral is
       the one the shot was balanced for. */
    float k = 0.0f, t = 0.0f;
    if (solve(cam, 1.0f, 1.0f, 1.0f, k, t)) {
        cam.asShotK = k;
        cam.asShotTint = t;
    } else {
        cam.asShotK = 6500.0f;
        cam.asShotTint = 0.0f;
    }
}

bool autoWhiteBalance(const WorkingImage &img, float &kelvin, float &tint)
{
    if (!img.isValid() || !img.cam.valid) return false;

    /*
        A grey-world average over everything is easily dragged off by a large
        saturated subject (a red barn, a blue sky). Averaging only the BRIGHT,
        UNCLIPPED pixels is far more robust: highlights and near-whites carry the
        illuminant's colour most faithfully. Two passes -- find the luminance
        threshold, then average what clears it.
    */
    const size_t n = static_cast<size_t>(img.width) * img.height;
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    const float clip  = white * 0.98f;      // ignore blown pixels: they have no colour

    /* Subsample big images; the estimate does not need every pixel. */
    const size_t stride = std::max<size_t>(1, n / 200000);

    std::vector<float> luma;
    luma.reserve(n / stride + 1);
    for (size_t i = 0; i < n; i += stride) {
        const float r = img.rgb[i * 3], g = img.rgb[i * 3 + 1], b = img.rgb[i * 3 + 2];
        if (r >= clip || g >= clip || b >= clip) continue;
        if (r <= 0.0f || g <= 0.0f || b <= 0.0f) continue;
        luma.push_back(0.2126f * r + 0.7152f * g + 0.0722f * b);
    }
    if (luma.size() < 64) return false;

    /* Threshold at the 80th percentile of what survived. */
    const size_t k = luma.size() * 4 / 5;
    std::nth_element(luma.begin(), luma.begin() + k, luma.end());
    const float thresh = luma[k];

    double sum[3] = {0, 0, 0};
    size_t count = 0;
    for (size_t i = 0; i < n; i += stride) {
        const float r = img.rgb[i * 3], g = img.rgb[i * 3 + 1], b = img.rgb[i * 3 + 2];
        if (r >= clip || g >= clip || b >= clip) continue;
        if (r <= 0.0f || g <= 0.0f || b <= 0.0f) continue;
        if (0.2126f * r + 0.7152f * g + 0.0722f * b < thresh) continue;
        sum[0] += r; sum[1] += g; sum[2] += b;
        ++count;
    }
    if (count < 32) return false;

    return solve(img.cam,
                 static_cast<float>(sum[0] / count),
                 static_cast<float>(sum[1] / count),
                 static_cast<float>(sum[2] / count),
                 kelvin, tint);
}

} // namespace WhiteBalance
