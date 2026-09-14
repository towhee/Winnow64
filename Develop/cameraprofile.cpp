#include "Develop/cameraprofile.h"
#include "Develop/whitebalance.h"

#include <cmath>

namespace CameraProfile {

using ColorSpaceMath::Matrix3;
using ColorSpaceMath::kIdentity3;
using ColorSpaceMath::multiply;
using ColorSpaceMath::invert3x3;

namespace {

Matrix3 lerp(const Matrix3 &a, const Matrix3 &b, double t)
{
    Matrix3 out{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) out.m[i][j] = a.m[i][j] * (1.0 - t) + b.m[i][j] * t;
    return out;
}

Matrix3 toMatrix3(const Dcp::Matrix3 &m)
{
    Matrix3 out{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) out.m[i][j] = double(m.m[i][j]);
    return out;
}

Matrix3 diag(const double d[3])
{
    Matrix3 out = kIdentity3;
    for (int i = 0; i < 3; ++i) out.m[i][i] = d[i];
    return out;
}

void applyM(const Matrix3 &m, const double in[3], double out[3])
{
    for (int i = 0; i < 3; ++i)
        out[i] = m.m[i][0] * in[0] + m.m[i][1] * in[1] + m.m[i][2] * in[2];
}

bool inverse(const Matrix3 &m, Matrix3 &out)
{
    double inv[3][3];
    if (!invert3x3(m.m, inv)) return false;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) out.m[i][j] = inv[i][j];
    return true;
}

} // namespace

bool resolve(const Dcp::Profile &p, float kelvin, Resolved &out)
{
    out = Resolved();
    if (!p.valid || !p.cal[0].haveColor) return false;

    const float k0 = Dcp::illuminantKelvin(p.cal[0].illuminant);
    const float k1 = Dcp::illuminantKelvin(p.cal[1].illuminant);

    /* Single illuminant, or a second one we cannot key on (no definite temperature, or
       the same temperature as the first): there is nothing to interpolate towards, so the
       first calibration stands at every temperature. */
    if (!p.cal[1].haveColor || k0 <= 0.0f || k1 <= 0.0f || k0 == k1) {
        const Dcp::Calibration &c = p.cal[0];
        out.color = toMatrix3(c.color);
        out.calibration = c.haveCalibration ? toMatrix3(c.calibration) : kIdentity3;
        out.haveForward = c.haveForward;
        if (c.haveForward) out.forward = toMatrix3(c.forward);
        out.weight = 0.0f;
        out.warmKelvin = out.coolKelvin = (k0 > 0.0f ? k0 : 0.0f);
        return true;
    }

    /* Order WARM first. Adobe writes illuminant 1 = Standard A and illuminant 2 = D65, but
       nothing in the format requires it, and a profile written the other way round would
       otherwise interpolate backwards -- silently, and worst exactly at the ends. */
    const int warm = (k0 <= k1) ? 0 : 1;
    const int cool = 1 - warm;
    const Dcp::Calibration &cw = p.cal[warm];
    const Dcp::Calibration &cc = p.cal[cool];
    out.warmKelvin = (warm == 0) ? k0 : k1;
    out.coolKelvin = (cool == 0) ? k0 : k1;

    /* INTERPOLATE IN MIRED, not in kelvin: 1/T is the scale on which a step of colour
       temperature is perceptually even, and it is what the DNG specification specifies.
       In kelvin the weighting would be badly skewed -- halfway between 2856 K and 6504 K
       is 4680 K by kelvin but 3982 K by mired, and the second is the one that looks
       halfway. Outside the two calibration temperatures the nearer one stands alone;
       extrapolating a fitted matrix is not meaningful. */
    const double t  = (kelvin > 1.0f) ? 1.0 / double(kelvin) : 1.0 / 1.0;
    const double tw = 1.0 / double(out.warmKelvin);
    const double tc = 1.0 / double(out.coolKelvin);
    double g = (t - tw) / (tc - tw);         // 0 at the warm end, 1 at the cool end
    if (!(g > 0.0)) g = 0.0;
    if (g > 1.0) g = 1.0;
    out.weight = float(g);

    out.color = lerp(toMatrix3(cw.color), toMatrix3(cc.color), g);

    /* CameraCalibration defaults to identity per illuminant, so a profile carrying only
       one still interpolates towards identity rather than snapping. */
    out.calibration = lerp(cw.haveCalibration ? toMatrix3(cw.calibration) : kIdentity3,
                           cc.haveCalibration ? toMatrix3(cc.calibration) : kIdentity3, g);

    /* ForwardMatrix is used only when BOTH calibrations have one. Mixing a fitted forward
       matrix with one derived from the inverse of a colour matrix would put a seam in the
       middle of the temperature range, where the two disagree by more than either is
       worth; deriving both is consistent, if slightly less accurate. */
    if (cw.haveForward && cc.haveForward) {
        out.forward = lerp(toMatrix3(cw.forward), toMatrix3(cc.forward), g);
        out.haveForward = true;
    }
    return true;
}

bool neutralCam(const Dcp::Profile &p, float kelvin, float tint, double n[3])
{
    Resolved r;
    if (!resolve(p, kelvin, r)) return false;

    double xyz[3];
    if (!WhiteBalance::illuminantXYZ(kelvin, tint, xyz)) return false;

    applyM(r.color, xyz, n);
    /* Normalised to GREEN == 1, the convention CameraColor::asShotMul already uses, so the
       two are directly comparable and the exposure anchor below is the familiar one. */
    if (!(std::fabs(n[1]) > 1e-9)) return false;
    const double g = n[1];
    for (int i = 0; i < 3; ++i) n[i] /= g;
    return true;
}

bool camToWorking(const Dcp::Profile &p, float kelvin, float tint, float out[3][3])
{
    Resolved r;
    if (!resolve(p, kelvin, r)) return false;

    double n[3];
    if (!neutralCam(p, kelvin, tint, n)) return false;

    /* AnalogBalance and CameraCalibration are the per-INDIVIDUAL-camera correction, applied
       to camera values before anything else; the rendering chain has to undo them. Both are
       identity on almost every profile, so this whole step is usually a no-op -- but a
       profile written for a calibrated body is exactly the case where getting it wrong
       would be invisible to everyone except the one user it was made for. */
    const double ab[3] = {double(p.analogBalance[0]), double(p.analogBalance[1]),
                          double(p.analogBalance[2])};
    const Matrix3 abcc = multiply(diag(ab), r.calibration);
    Matrix3 invAbcc;
    if (!inverse(abcc, invAbcc)) return false;

    Matrix3 camToXyzD50;

    if (r.haveForward) {
        /* The reference neutral is the camera neutral with the per-camera correction taken
           off; dividing by it is the white balance. ForwardMatrix is built so that the
           result, (1,1,1), maps to the D50 white -- which is what anchors exposure. */
        double ref[3];
        applyM(invAbcc, n, ref);
        for (int i = 0; i < 3; ++i)
            if (!(std::fabs(ref[i]) > 1e-9)) return false;
        const double invRef[3] = {1.0 / ref[0], 1.0 / ref[1], 1.0 / ref[2]};
        camToXyzD50 = multiply(r.forward, multiply(diag(invRef), invAbcc));
    } else {
        /* NO ForwardMatrix -- legal, and common on older and third-party profiles. Invert
           the colour matrix instead, then adapt from the white the camera neutral actually
           lands on to D50. The adaptation carries that white EXACTLY onto D50 by
           construction, so this path anchors exposure identically to the one above; it is
           less accurate in the colours away from neutral, not in the neutral itself. */
        Matrix3 camToXyz;
        if (!inverse(multiply(abcc, r.color), camToXyz)) return false;
        double w[3];
        applyM(camToXyz, n, w);
        if (!(w[1] > 1e-9)) return false;
        camToXyzD50 = multiply(ColorSpaceMath::adapt(w, ColorSpaceMath::kWhiteD50), camToXyz);
    }

    /* D50 -> the working space's OWN white (taken from its primaries, not from a published
       D65 triple, so a neutral lands on exactly (1,1,1)), then XYZ -> its primaries. */
    double workWhite[3];
    ColorSpaceMath::whiteOf(ColorSpaceMath::kWorking, workWhite);
    const Matrix3 xyzToWork =
        multiply(ColorSpaceMath::xyzToRgb(ColorSpaceMath::kWorking),
                 ColorSpaceMath::adapt(ColorSpaceMath::kWhiteD50, workWhite));

    const Matrix3 m = multiply(xyzToWork, camToXyzD50);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) out[i][j] = float(m.m[i][j]);
    return true;
}

/*
    LINEAR ProPhoto RGB (ROMM), whose white IS D50 -- the space the DNG specification
    applies both profile lookup tables in. Written out here rather than added to
    ColorSpaceMath::ColorSpace because every space in that enum is D65 by design; see the
    note on Tables in the header.
*/
namespace {

constexpr Matrix3 kProPhotoToXyzD50 = {{
    {0.7976749, 0.1351917, 0.0313534},
    {0.2880402, 0.7118741, 0.0000857},
    {0.0000000, 0.0000000, 0.8252100}
}};

HueSatMap::Table toHsmTable(const Dcp::Table3D &t)
{
    HueSatMap::Table out;
    if (t.isEmpty()) return out;
    out.hueDivs = t.hueDivs;
    out.satDivs = t.satDivs;
    out.valDivs = t.valDivs;
    out.encoding = t.encoding;
    out.v = t.v;
    return out;
}

} // namespace

bool tables(const Dcp::Profile &p, float kelvin, Tables &out)
{
    out = Tables();

    Resolved r;
    if (!resolve(p, kelvin, r)) return false;

    /* The tables are interpolated with the SAME weight the matrices were, and the warm /
       cool ordering has to match too -- resolve() reports which calibration is which by
       temperature, not by the order the file lists them in. */
    const bool warmIsZero =
        Dcp::illuminantKelvin(p.cal[0].illuminant) <= Dcp::illuminantKelvin(p.cal[1].illuminant);
    const int warm = warmIsZero ? 0 : 1;
    const int cool = 1 - warm;

    /* The baseline correction. Absent on every creative profile, which is fine -- the look
       below may still give this stage something to do. */
    HueSatMap::Blend(toHsmTable(p.cal[warm].hueSatMap),
                     toHsmTable(p.cal[cool].hueSatMap), r.weight, out.hueSatMap);

    /*
        THE LOOK. Unlike the HueSatMap the LookTable is a SINGLE tag, not one per
        illuminant -- a creative grade is not a function of the light -- so there is
        nothing to blend. Empty on a profile that carries no look.
    */
    out.lookTable = toHsmTable(p.lookTable);
    ProfileTone::Build(p.toneCurve, out.toneCurve);
    if (p.baselineExposureOffset != 0.0f)
        out.exposureScale = std::exp2(p.baselineExposureOffset);

    if (out.hueSatMap.isEmpty() && out.lookTable.isEmpty() &&
        out.toneCurve.isEmpty() && out.exposureScale == 1.0f)
        return false;                           // nothing per-pixel to do

    double workWhite[3];
    ColorSpaceMath::whiteOf(ColorSpaceMath::kWorking, workWhite);

    /*
        THE DESTINATION WHITE COMES FROM ProPhoto's OWN MATRIX -- its RGB->XYZ row sums --
        NOT from the published D50 triple, for exactly the reason the working-space hop
        takes its white the same way: only then does a neutral land on EXACTLY neutral.

        The two differ in the fourth decimal (ProPhoto's rows sum to 0.96422 / 1 / 0.82521
        against D50's 0.9642 / 1 / 0.8249), which sounds ignorable and is not. A neutral
        that arrives 3e-4 off neutral has a small but non-zero SATURATION, and the table's
        saturation == 0 slice -- the one that is exactly identity, and the reason a profile
        cannot tint greys -- is then no longer the slice being read. Measured before this
        was fixed: real profiles moved a neutral by 1.5e-4, fifteen times the tolerance
        this is asserted at.
    */
    double proPhotoWhite[3];
    for (int i = 0; i < 3; ++i)
        proPhotoWhite[i] = kProPhotoToXyzD50.m[i][0] + kProPhotoToXyzD50.m[i][1]
                         + kProPhotoToXyzD50.m[i][2];

    Matrix3 proPhotoFromXyz;
    if (!inverse(kProPhotoToXyzD50, proPhotoFromXyz)) return false;

    /* working -> XYZ(working white) -> XYZ(ProPhoto's white) -> linear ProPhoto. */
    const Matrix3 to = multiply(proPhotoFromXyz,
                          multiply(ColorSpaceMath::adapt(workWhite, proPhotoWhite),
                                   ColorSpaceMath::rgbToXyz(ColorSpaceMath::kWorking)));
    Matrix3 from;
    if (!inverse(to, from)) return false;

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            out.toTable[i][j]   = float(to.m[i][j]);
            out.fromTable[i][j] = float(from.m[i][j]);
        }
    out.active = true;
    return true;
}

} // namespace CameraProfile
