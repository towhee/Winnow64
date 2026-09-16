#include "Develop/whitebalance.h"
#include "Develop/colorspace.h"
#include <cmath>
#include <algorithm>
#include <vector>

/* Working-space luma weights (see Develop/colorspace.h) -- used by the auto-WB and skin
   samplers to weight and threshold pixels by brightness. */
using ColorSpaceMath::kLumR;
using ColorSpaceMath::kLumG;
using ColorSpaceMath::kLumB;

namespace {

/* XYZ -> linear sRGB (D65). The inverse of the rgbToXyz used by RawColor. */
const float kXyzToSrgb[3][3] = {
    { 3.2404542f, -1.5371385f, -0.4985314f},
    {-0.9692660f,  1.8760108f,  0.0415560f},
    { 0.0556434f, -0.2040259f,  1.0572252f}
};

/*
    Kelvin + tint <-> CIE 1931 xy, using ADOBE'S DEFINITION -- the DNG SDK's
    dng_temperature, which is Robertson's isotemperature-line method over the PLANCKIAN
    locus in CIE 1960 (u,v).

    WHY ADOBE'S AND NOT OUR OWN. The number in the Temp box is only useful if it means
    the same thing as the number in everyone else's Temp box. The previous code measured
    Kelvin on a crossfade of the Planckian and CIE daylight loci and scaled tint at 4000
    units per Duv; Adobe measures Kelvin on the Planckian locus alone and scales tint at
    3000 units per Duv (kTintScale). Those are not small differences: a D850 frame whose
    as-shot neutral Lightroom reads as 6300 K / +3 came out of the old chain as
    5959 K / -18. The same chromaticity read through this table gives 6315 K / +1.9.

    The cost, accepted deliberately: a stored tint now means a different displacement
    than it did, so images edited before this change shift green/magenta. Kelvin barely
    moves; tint moves by roughly 10-20 units at daylight temperatures. There is no
    sidecar version key to migrate against -- see notes/Documentation.txt.

    CONSEQUENCE WORTH KNOWING: D65 is not 6500/0 in this convention, it is 6503 / +9.8.
    Daylight genuinely sits about 0.003 Duv ABOVE the Planckian locus, and Adobe reports
    it that way too. So a display-referred file (InputTransform's synthetic D65 camera)
    now opens reading a tint near +10 rather than 0. Rendering is unaffected:
    relativeGains divides by the as-shot reference, so as-shot is still an exact no-op.

    The table is 31 (mired, u, v, slope) rows, mired 0 (infinite K) to 600 (1667 K).
    Winnow's slider range, 2000 K (500 mired) to 50000 K (20 mired), sits inside it.
*/
struct TempTableEntry { double r, u, v, t; };

const TempTableEntry kTempTable[31] = {
    {   0, 0.18006, 0.26352,   -0.24341 },
    {  10, 0.18066, 0.26589,   -0.25479 },
    {  20, 0.18133, 0.26846,   -0.26876 },
    {  30, 0.18208, 0.27119,   -0.28539 },
    {  40, 0.18293, 0.27407,   -0.30470 },
    {  50, 0.18388, 0.27709,   -0.32675 },
    {  60, 0.18494, 0.28021,   -0.35156 },
    {  70, 0.18611, 0.28342,   -0.37915 },
    {  80, 0.18740, 0.28668,   -0.40955 },
    {  90, 0.18880, 0.28997,   -0.44278 },
    { 100, 0.19032, 0.29326,   -0.47888 },
    { 125, 0.19462, 0.30141,   -0.58204 },
    { 150, 0.19962, 0.30921,   -0.70471 },
    { 175, 0.20525, 0.31647,   -0.84901 },
    { 200, 0.21142, 0.32312,   -1.01820 },
    { 225, 0.21807, 0.32909,   -1.21680 },
    { 250, 0.22511, 0.33439,   -1.45120 },
    { 275, 0.23247, 0.33904,   -1.72980 },
    { 300, 0.24010, 0.34308,   -2.06370 },
    { 325, 0.24792, 0.34655,   -2.46810 },
    { 350, 0.25591, 0.34951,   -2.96410 },
    { 375, 0.26400, 0.35200,   -3.58140 },
    { 400, 0.27218, 0.35407,   -4.36330 },
    { 425, 0.28039, 0.35577,   -5.37620 },
    { 450, 0.28863, 0.35714,   -6.72620 },
    { 475, 0.29685, 0.35823,   -8.59550 },
    { 500, 0.30505, 0.35907,  -11.32400 },
    { 525, 0.31320, 0.35968,  -15.62800 },
    { 550, 0.32129, 0.36011,  -23.32500 },
    { 575, 0.32931, 0.36038,  -40.77000 },
    { 600, 0.33724, 0.36051, -116.45000 }
};

/*
    Tint units per unit of displacement along the isotemperature line. Adobe's constant,
    and NEGATIVE: a positive tint moves the chromaticity in the -offset direction, which
    is ABOVE the locus in v. That is the same sign the old code had, and the same sign
    Lightroom's slider has under the hand -- a positive tint assumes a GREENER
    illuminant, so the correction renders the image more MAGENTA.
*/
constexpr double kTintScale = -3000.0;

/* (K, tint) -> xy. The DNG SDK's dng_temperature::Get_xy_coord. */
void locusXY(double T, double tint, double &x, double &y)
{
    T = std::clamp(T, 1667.0, 100000.0);
    const double r = 1.0e6 / T;
    const double offset = tint * (1.0 / kTintScale);

    for (int i = 0; i <= 29; ++i) {
        if (r >= kTempTable[i + 1].r && i != 29) continue;

        /* Where r falls between the two bracketing mired rows. */
        const double f = (kTempTable[i + 1].r - r)
                       / (kTempTable[i + 1].r - kTempTable[i].r);
        double u = kTempTable[i].u * f + kTempTable[i + 1].u * (1.0 - f);
        double v = kTempTable[i].v * f + kTempTable[i + 1].v * (1.0 - f);

        /* The isotemperature line's direction at each end, normalised, then blended --
           blending the NORMALISED ends rather than the raw slopes is what keeps the
           tint displacement continuous across a row boundary. */
        double u1 = 1.0, v1 = kTempTable[i].t;
        double u2 = 1.0, v2 = kTempTable[i + 1].t;
        const double l1 = std::sqrt(1.0 + v1 * v1);
        const double l2 = std::sqrt(1.0 + v2 * v2);
        u1 /= l1; v1 /= l1;
        u2 /= l2; v2 /= l2;
        double u3 = u1 * f + u2 * (1.0 - f);
        double v3 = v1 * f + v2 * (1.0 - f);
        const double l3 = std::sqrt(u3 * u3 + v3 * v3);
        u3 /= l3; v3 /= l3;

        u += u3 * offset;
        v += v3 * offset;

        const double d = u - 4.0 * v + 2.0;
        if (std::fabs(d) < 1e-12) { x = 0.3127; y = 0.3290; return; }
        x = 1.5 * u / d;
        y = v / d;
        return;
    }
    x = 0.3127; y = 0.3290;         // unreachable; D65 rather than an uninitialised pair
}

/*
    xy -> (K, tint). The DNG SDK's dng_temperature::Set_xy_coord: walk the table until
    the point falls on the far side of an isotemperature line, then interpolate both the
    mired value and the perpendicular offset between that row and the last.

    This is the EXACT inverse of locusXY, so it is also the exact answer solve()'s
    bisection converges to for the neutral case; it is kept separate because solve() has
    to work on an arbitrary rendered colour, not just a chromaticity.
*/
void xyToTempTint(double x, double y, double &kelvin, double &tint)
{
    const double den = 1.5 - x + 6.0 * y;
    if (std::fabs(den) < 1e-12) { kelvin = 6500.0; tint = 0.0; return; }
    const double u = 2.0 * x / den;
    const double v = 3.0 * y / den;

    double lastDt = 0.0, lastDu = 0.0, lastDv = 0.0;
    for (int i = 1; i <= 30; ++i) {
        double du = 1.0, dv = kTempTable[i].t;
        const double len = std::sqrt(1.0 + dv * dv);
        du /= len; dv /= len;

        double uu = u - kTempTable[i].u;
        double vv = v - kTempTable[i].v;
        double dt = -uu * dv + vv * du;

        if (dt <= 0.0 || i == 30) {
            if (dt > 0.0) dt = 0.0;
            dt = -dt;
            const double f = (i == 1) ? 0.0 : dt / (lastDt + dt);
            kelvin = 1.0e6 / (kTempTable[i - 1].r * f + kTempTable[i].r * (1.0 - f));

            uu = u - (kTempTable[i - 1].u * f + kTempTable[i].u * (1.0 - f));
            vv = v - (kTempTable[i - 1].v * f + kTempTable[i].v * (1.0 - f));
            du = du * (1.0 - f) + lastDu * f;
            dv = dv * (1.0 - f) + lastDv * f;
            const double l = std::sqrt(du * du + dv * dv);
            du /= l; dv /= l;

            tint = (uu * du + vv * dv) * kTintScale;
            return;
        }
        lastDt = dt; lastDu = du; lastDv = dv;
    }
    kelvin = 6500.0; tint = 0.0;    // unreachable
}

/* The rendered linear-sRGB colour of a neutral surface under illuminant (K, tint),
   carried through the same chain the decode used. */
bool renderIlluminant(const CameraColor &cam, double kelvin, double tint, double s[3])
{
    double XYZ[3];
    if (!WhiteBalance::illuminantXYZ(float(kelvin), float(tint), XYZ)) return false;

    double camRaw[3];
    for (int i = 0; i < 3; ++i)
        camRaw[i] = cam.xyzToCam[i][0] * XYZ[0] + cam.xyzToCam[i][1] * XYZ[1]
                  + cam.xyzToCam[i][2] * XYZ[2];

    /* The white balance the decode already baked in. */
    for (int i = 0; i < 3; ++i) camRaw[i] *= cam.asShotMul[i];

    for (int i = 0; i < 3; ++i)
        s[i] = cam.camToWorking[i][0] * camRaw[0] + cam.camToWorking[i][1] * camRaw[1]
             + cam.camToWorking[i][2] * camRaw[2];

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

bool illuminantXYZ(float kelvin, float tint, double xyz[3])
{
    double x, y;
    locusXY(kelvin, tint, x, y);
    if (y < 1e-9) return false;
    xyz[0] = x / y;
    xyz[1] = 1.0;
    xyz[2] = (1.0 - x - y) / y;
    return true;
}

/*
    The ANALYTIC inverse of illuminantXYZ: the (kelvin, tint) whose illuminant sits at
    chromaticity (x, y). Exact, where solve() bisects -- but solve() is given a RENDERED
    colour and has to walk the camera chain backwards, so the two are not
    interchangeable. Used by the profile-aware as-shot solve and by the tests.
*/
void tempTintFromXY(double x, double y, float &kelvin, float &tint)
{
    double k = 6500.0, t = 0.0;
    xyToTempTint(x, y, k, t);
    kelvin = float(std::clamp(k, double(kMinKelvin), double(kMaxKelvin)));
    tint   = float(std::clamp(t, double(kMinTint), double(kMaxTint)));
}

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

    /* Brightness weights for the space img is ACTUALLY in -- camera-native for a raw
       (see lumaWeightsFor, workingimage.h). Only used to rank pixels by brightness for
       the percentile threshold, but ranking sensor channels with sRGB weights would pick
       a measurably different "brightest 20%" to average. */
    const ColorSpaceMath::Luma LW = lumaWeightsFor(img);

    std::vector<float> luma;
    luma.reserve(n / stride + 1);
    for (size_t i = 0; i < n; i += stride) {
        const float r = img.rgb[i * 3], g = img.rgb[i * 3 + 1], b = img.rgb[i * 3 + 2];
        if (r >= clip || g >= clip || b >= clip) continue;
        if (r <= 0.0f || g <= 0.0f || b <= 0.0f) continue;
        luma.push_back(LW.r * r + LW.g * g + LW.b * b);
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
        if (LW.r * r + LW.g * g + LW.b * b < thresh) continue;
        sum[0] += r; sum[1] += g; sum[2] += b;
        ++count;
    }
    if (count < 32) return false;

    /* The average is sampled from the PRE-develop image, which for a raw is still in
       camera-native primaries; solve() expects a working-space colour. Converting the
       AVERAGE is exact because the transform is linear, and it is one matrix instead of
       one per sampled pixel. No-op for non-raw. */
    float ar = static_cast<float>(sum[0] / count);
    float ag = static_cast<float>(sum[1] / count);
    float ab = static_cast<float>(sum[2] / count);
    toWorkingColor(img, ar, ag, ab);
    return solve(img.cam, ar, ag, ab, kelvin, tint);
}

} // namespace WhiteBalance
