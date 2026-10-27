#include "Metadata/colorspaces.h"
#include "Lcms2/lcms2.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
/*  A space's colorants as an ICC profile states them: the RGB -> XYZ columns adapted to
    the D50 PCS. Built by lcms from the space's primaries and white, so the adaptation is
    the same one a real profile's tags carry and the comparison is like with like. */
using Colorants = std::array<double, 9>;

bool readColorants(cmsHPROFILE p, Colorants &c)
{
    const cmsTagSignature tag[3] = {cmsSigRedColorantTag, cmsSigGreenColorantTag,
                                    cmsSigBlueColorantTag};
    for (int i = 0; i < 3; ++i) {
        const auto *xyz = static_cast<const cmsCIEXYZ *>(cmsReadTag(p, tag[i]));
        if (!xyz) return false;
        c[size_t(i) * 3 + 0] = xyz->X;
        c[size_t(i) * 3 + 1] = xyz->Y;
        c[size_t(i) * 3 + 2] = xyz->Z;
    }
    return true;
}

struct Reference { int id; Colorants c; };

Colorants referenceFor(const cmsCIExyY &white, const cmsCIExyYTRIPLE &primaries)
{
    cmsToneCurve *g = cmsBuildGamma(nullptr, 1.0);
    cmsToneCurve *curves[3] = {g, g, g};
    cmsHPROFILE p = cmsCreateRGBProfile(&white, &primaries, curves);
    cmsFreeToneCurve(g);
    Colorants c{};
    if (p) {
        readColorants(p, c);
        cmsCloseProfile(p);
    }
    return c;
}

const std::vector<Reference> &references()
{
    static const std::vector<Reference> refs = [] {
        const cmsCIExyY d65 = {0.3127, 0.3290, 1.0};
        const cmsCIExyY d50 = {0.3457, 0.3585, 1.0};
        std::vector<Reference> r;
        r.push_back({ColorSpaces::sRGB, referenceFor(
            d65, {{0.64, 0.33, 1.0}, {0.30, 0.60, 1.0}, {0.15, 0.06, 1.0}})});
        r.push_back({ColorSpaces::AdobeRGB, referenceFor(
            d65, {{0.64, 0.33, 1.0}, {0.21, 0.71, 1.0}, {0.15, 0.06, 1.0}})});
        r.push_back({ColorSpaces::DisplayP3, referenceFor(
            d65, {{0.680, 0.320, 1.0}, {0.265, 0.690, 1.0}, {0.150, 0.060, 1.0}})});
        r.push_back({ColorSpaces::ProPhotoRGB, referenceFor(
            d50, {{0.7347, 0.2653, 1.0}, {0.1596, 0.8404, 1.0}, {0.0366, 0.0001, 1.0}})});
        r.push_back({ColorSpaces::Rec2020, referenceFor(
            d65, {{0.708, 0.292, 1.0}, {0.170, 0.797, 1.0}, {0.131, 0.046, 1.0}})});
        return r;
    }();
    return refs;
}

/*  Real profiles round their tags to s15Fixed16 and adapt with whatever their author
    used (Bradford almost always, occasionally von Kries), which moves a colorant by a
    few thousandths. The named spaces are tens of thousandths apart (sRGB and Display P3
    red differ by 0.08 in X), so 0.01 separates them with room on both sides. */
constexpr double kTolerance = 0.01;

int byDescription(cmsHPROFILE p)
{
    char buf[256] = "";
    cmsGetProfileInfoASCII(p, cmsInfoDescription, "en", "US", buf, sizeof buf);
    const QString d = QString::fromLatin1(buf).toLower();
    if (d.contains("srgb") || d.contains("61966-2")) return ColorSpaces::sRGB;
    if (d.contains("adobe rgb") || d.contains("adobergb")) return ColorSpaces::AdobeRGB;
    if (d.contains("display p3")) return ColorSpaces::DisplayP3;
    if (d.contains("prophoto") || d.contains("romm")) return ColorSpaces::ProPhotoRGB;
    if (d.contains("2020")) return ColorSpaces::Rec2020;
    return ColorSpaces::OtherProfile;
}
} // namespace

namespace ColorSpaces
{

int fromIcc(const QByteArray &profile)
{
    if (profile.size() < 132) return None;      // shorter than an ICC header + tag count
    cmsHPROFILE p = cmsOpenProfileFromMem(profile.constData(),
                                          cmsUInt32Number(profile.size()));
    if (!p) return OtherProfile;
    int id = OtherProfile;
    switch (cmsGetColorSpace(p)) {
    case cmsSigGrayData: id = Gray; break;
    case cmsSigCmykData: id = CMYK; break;
    case cmsSigRgbData: {
        Colorants c;
        if (readColorants(p, c)) {
            for (const Reference &r : references()) {
                double worst = 0.0;
                for (size_t i = 0; i < c.size(); ++i)
                    worst = std::max(worst, std::abs(c[i] - r.c[i]));
                if (worst <= kTolerance) { id = r.id; break; }
            }
            /*  sRGB is the one id that also DROPS the profile (the readers keep no
                bytes for it), so the colorants alone are not enough: a linear-light
                profile on sRGB primaries would be decoded as gamma-encoded. Its curve
                must be sRGB's too -- 0.214 at half scale (a pure 2.2 gamma, 0.218, is
                close enough to show identically; linear, 0.5, is not). */
            if (id == sRGB) {
                const auto *trc =
                    static_cast<const cmsToneCurve *>(cmsReadTag(p, cmsSigRedTRCTag));
                if (!trc || std::abs(cmsEvalToneCurveFloat(trc, 0.5f) - 0.214f) > 0.01f)
                    id = OtherProfile;
            }
        }
        else {
            id = byDescription(p);              // a CLUT-only RGB profile
        }
        break;
    }
    default: break;
    }
    cmsCloseProfile(p);
    return id;
}

int fromExif(int tag)
{
    switch (tag) {
    case 1:      return sRGB;
    case 2:      return AdobeRGB;
    case 0xFFFF: return Uncalibrated;
    default:     return None;
    }
}

int fromDngPreview(int tag)
{
    switch (tag) {
    case 1:  return Gray;
    case 2:  return sRGB;
    case 3:  return AdobeRGB;
    case 4:  return ProPhotoRGB;
    default: return None;
    }
}

int fromNclx(int primaries)
{
    switch (primaries) {
    case 1:  return sRGB;               // BT.709 primaries
    case 12: return DisplayP3;          // P3 D65
    case 9:  return Rec2020;
    case 2:  return None;               // unspecified
    default: return OtherProfile;       // e.g. 11, DCI-P3 (a different white)
    }
}

} // namespace ColorSpaces
