/*
    Develop's STAGE 0 -- the camera-native -> working-space input profile.

    WHY THIS TEST EXISTS. RawColor deliberately stops at the sensor's own primaries so
    that the WorkingImageCache boundary sits UPSTREAM of every colour decision: changing a
    camera profile or the white balance is then a re-render, not a re-decode. The
    conversion it no longer does became Develop's first stage, and that stage has two
    implementations which MUST agree:

      Develop::ToWorkingSpace   a standalone pass, used when the fused point pass cannot
                                carry the conversion -- an identity edit (there is no
                                point pass at all) and an active Denoise (op #1 runs
                                BEFORE the point pass and must see working-space pixels).

      PointCoeffs::preMat       the same transform folded into one 3x3 together with the
                                white balance, exposure, the Colour RGB sliders and the
                                Calibrate matrix, so the conversion costs nothing on the
                                path virtually every render takes.

    THE LOAD-BEARING PROPERTY is that the FOLD EQUALS THE SEQUENCE. A fused matrix that
    quietly disagrees with applying the stages one at a time would make an image render
    differently depending on whether Denoise happened to be switched on -- the kind of
    bug that looks like "the denoise slider changes my colours".

    The other thing pinned here is that the conversion is NOT OPTIONAL: it must run even
    when the edit is identity, because it is not an edit. Miss that and an untouched raw
    renders in sensor primaries, which looks like a strong colour cast.
*/

#include <QtTest>
#include <vector>
#include <algorithm>
#include <cmath>

#include "Develop/develop.h"
#include "Develop/cameraprofile.h"
#include <QImage>
#include "Develop/workingimage.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"
#include "Develop/colorspace.h"
#include "Develop/outputtransform.h"
#include "Develop/outputlook.h"
#include "Develop/baselineexposure.h"

namespace {

/*
    A deliberately NON-symmetric stand-in for a camera matrix, with row sums of 1 so a
    neutral sensor reading maps to a neutral working colour (the property a real
    camera->working matrix has after white balance). Asymmetric so a transposed or
    mis-indexed matrix cannot pass by accident.
*/
const float kCamToWorking[3][3] = {
    { 1.30f, -0.25f, -0.05f},
    {-0.18f,  1.32f, -0.14f},
    { 0.05f, -0.42f,  1.37f}
};
const float kAsShot[3] = {1.9f, 1.0f, 1.45f};

WorkingImage makeCameraNative(int w, int h)
{
    WorkingImage img;
    img.width  = w;
    img.height = h;
    img.white  = 1.0f;
    img.sceneReferred = true;
    img.space  = ColorSpaceMath::ColorSpace::CameraNative;
    img.cam.valid = true;
    for (int i = 0; i < 3; ++i) {
        img.cam.asShotMul[i] = kAsShot[i];
        for (int j = 0; j < 3; ++j) img.cam.camToWorking[i][j] = kCamToWorking[i][j];
    }
    img.rgb.resize(size_t(w) * size_t(h) * 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t j = (size_t(y) * w + x) * 3;
            const float u = float(x) / float(w - 1);
            const float v = float(y) / float(h - 1);
            img.rgb[j + 0] = 0.05f + 0.60f * u;
            img.rgb[j + 1] = 0.07f + 0.55f * v;
            img.rgb[j + 2] = 0.09f + 0.30f * (u + v);
        }
    }
    return img;
}

/* The conversion, computed independently of the code under test. */
void refConvert(const float in[3], float out[3])
{
    const float c[3] = {in[0] * kAsShot[0], in[1] * kAsShot[1], in[2] * kAsShot[2]};
    for (int i = 0; i < 3; ++i)
        out[i] = kCamToWorking[i][0] * c[0] + kCamToWorking[i][1] * c[1]
               + kCamToWorking[i][2] * c[2];
}

float worstDiff(const WorkingImage &a, const WorkingImage &b)
{
    float worst = 0.0f;
    const size_t n = a.rgb.size();
    for (size_t i = 0; i < n; ++i)
        worst = std::max(worst, std::fabs(a.rgb[i] - b.rgb[i]));
    return worst;
}

} // namespace

class TestInputProfile : public QObject
{
    Q_OBJECT

private slots:
    void toWorkingSpaceMatchesReference();
    void toWorkingSpaceIsIdempotent();
    void identityEditStillConverts();
    void foldEqualsSequenceForExposure();
    void foldEqualsSequenceWithCalibrate();
    void denoisePathAgreesWithFoldedPath();
    void nonRawIsUntouched();
    void defaultViewTransformIsFilmic();
    void viewTransformSanitisesToTheDefault();
    void cameraProfileReplacesTheInputMatrix();
    void cameraProfileWhiteBalancesExactlyOnce();
    void cameraProfileSurvivesTheDenoiseRoute();
    void stageZeroRunsOnlyOnce();
    void cameraBaseRendersWithoutTheLook();
    void profileCurveIsOneToneMappingAmongOthers();
    void profileCurveRollsOffAPushedExposure();
    void lookSurvivesACopy();
    void baselineExposureAppliesOncePerBody();
};

/*
    THE DEFAULT IS FILMIC, AND IT IS NOT THE IDENTITY.

    This test used to assert the opposite -- that the default was None -- on the reasoning
    that a default-constructed EditParams is what "no edits" means, so the default had to
    be the identity or every reset and isIdentity() would carry a special case. The
    reasoning was sound and the result was wrong: with no view transform a scene-referred
    raw renders radiometrically linear, which is dark and flat, and "no edits" was
    therefore a look nobody would choose rather than a neutral starting point. Filmic
    matches Lightroom's rendering of the same raw to within about one level of 255 across
    the luminance histogram; None is off by six.

    So the special case is real and this is where it is pinned. Three things have to stay
    true together, and getting any two right is the trap:

      - the PUBLISHED NUMBERING, which is the sidecar format: added to, never renumbered.
      - the IDENTITY is still None == 0, because zero is what an absent field means.
      - the DEFAULT is Filmic, and everything that asks "is this untouched?" compares
        against EditParams::kDefaultViewTransform, never against a literal 0.
*/
void TestInputProfile::defaultViewTransformIsFilmic()
{
    const EditParams def;
    QCOMPARE(def.viewTransform, EditParams::kDefaultViewTransform);
    QCOMPARE(OutputTransform::ViewFromInt(def.viewTransform),
             OutputTransform::ViewTransform::Filmic);

    /* THE POINT OF THE CHANGE: an untouched raw is tone mapped. */
    QVERIFY(OutputTransform::ViewFromInt(def.viewTransform)
            != OutputTransform::ViewTransform::None);

    /* isIdentity() tracks the DEFAULT, not zero. Both directions, because a literal 0
       gets exactly these two backwards: the untouched image reads as edited, and the one
       deliberately set to None reads as untouched. */
    QVERIFY2(def.isIdentity(), "a default-constructed EditParams must be identity");
    EditParams none;
    none.viewTransform = int(OutputTransform::ViewTransform::None);
    QVERIFY2(!none.isIdentity(), "choosing None is an edit -- it is not the default");

    /* Reset Basic restores the default, i.e. Filmic, not the identity. */
    EditParams p;
    p.viewTransform = int(OutputTransform::ViewTransform::AgX);
    EditParams::resetGroup(p, EditParams::Group::Basic);
    QCOMPARE(OutputTransform::ViewFromInt(p.viewTransform),
             OutputTransform::ViewTransform::Filmic);

    /* The published numbering itself. */
    QCOMPARE(int(OutputTransform::ViewTransform::None),   0);
    QCOMPARE(int(OutputTransform::ViewTransform::Filmic), 1);
    QCOMPARE(int(OutputTransform::ViewTransform::AgX),    2);
    QCOMPARE(int(OutputTransform::ViewTransform::CameraContrast), 3);
    QCOMPARE(int(OutputTransform::ViewTransform::ProfileCurve),   4);
    QCOMPARE(OutputTransform::ViewFromInt(3), OutputTransform::ViewTransform::CameraContrast);
    QCOMPARE(OutputTransform::ViewFromInt(4), OutputTransform::ViewTransform::ProfileCurve);

    /* An unknown value (a sidecar from a later build) resolves to the IDENTITY rather
       than to whatever the cast happens to land on -- the output stage does not invent a
       look when it cannot tell what was asked for. Restoring the DEFAULT is the
       sanitiser's job, one layer up; see viewTransformSanitisesToTheDefault. */
    QCOMPARE(OutputTransform::ViewFromInt(99), OutputTransform::ViewTransform::None);
    QCOMPARE(OutputTransform::ViewFromInt(-1), OutputTransform::ViewTransform::None);
}

/*
    A SIDECAR WITHOUT THE FIELD MUST NOT SILENTLY GO FLAT.

    Every sidecar Winnow has ever written carries viewTransform explicitly, so an older
    one restores the look it was saved with -- including None, which is what those files
    were rendered as and must keep rendering as. But a hand-edited or truncated sidecar
    that omits the key falls back to the value already in the struct, which is the
    default. That fallback is the reason the JSON reader must read INTO a
    default-constructed EditParams rather than a zeroed one.
*/
void TestInputProfile::viewTransformSanitisesToTheDefault()
{
    const EditParams def;

    /* Absent -> the default, via the reader's own fallback argument. */
    QJsonObject o;
    EditParams p;
    p.viewTransform = EditStack::paramsFromJson(o).viewTransform;
    QCOMPARE(p.viewTransform, def.viewTransform);

    /* Present -> honoured exactly, None included: an already-edited image does not move. */
    o["viewTransform"] = int(OutputTransform::ViewTransform::None);
    QCOMPARE(EditStack::paramsFromJson(o).viewTransform,
             int(OutputTransform::ViewTransform::None));

    /* Out of range -> repaired to the DEFAULT (not to the identity), which is where the
       "show the default look rather than refuse to load" contract lives. */
    EditParams bad;
    bad.viewTransform = 99;
    EditStack::sanitizeParams(bad, QStringLiteral("Global"), nullptr);
    QCOMPARE(bad.viewTransform, def.viewTransform);
}

/* The standalone pass must reproduce asShotMul then camToWorking, and re-tag. */
void TestInputProfile::toWorkingSpaceMatchesReference()
{
    WorkingImage img = makeCameraNative(16, 12);
    const WorkingImage src = img;
    Develop::ToWorkingSpace(img);

    QCOMPARE(img.space, ColorSpaceMath::kWorking);
    float worst = 0.0f;
    for (size_t i = 0; i < src.rgb.size(); i += 3) {
        const float in[3] = {src.rgb[i], src.rgb[i + 1], src.rgb[i + 2]};
        float want[3];
        refConvert(in, want);
        for (int c = 0; c < 3; ++c)
            worst = std::max(worst, std::fabs(img.rgb[i + c] - want[c]));
    }
    QVERIFY2(worst < 1e-5f, qPrintable(QString("worst %1").arg(worst)));
}

/* Calling it twice must not convert twice -- the tag is what stops it, and a double
   conversion would be a hard-to-spot colour error rather than a crash. */
void TestInputProfile::toWorkingSpaceIsIdempotent()
{
    WorkingImage once = makeCameraNative(8, 8);
    Develop::ToWorkingSpace(once);
    WorkingImage twice = once;
    Develop::ToWorkingSpace(twice);
    QCOMPARE(worstDiff(once, twice), 0.0f);
}

/* The input profile is NOT an edit: an identity EditParams must still convert. */
void TestInputProfile::identityEditStillConverts()
{
    WorkingImage img = makeCameraNative(8, 8);
    WorkingImage want = img;
    Develop::ToWorkingSpace(want);

    EditParams p;                       // untouched == identity
    QVERIFY(p.isIdentity());
    Develop d;
    QVERIFY(d.Apply(img, p));

    QCOMPARE(img.space, ColorSpaceMath::kWorking);
    QVERIFY2(worstDiff(img, want) < 1e-6f, "identity edit did not convert");
}

/*
    THE CENTRAL PROPERTY. Applying the conversion as its own pass and then developing must
    equal developing with the conversion folded into preMat. Exposure is a pure
    per-channel gain, so the fold is diag(gain) . camToWorking . diag(asShotMul).
*/
void TestInputProfile::foldEqualsSequenceForExposure()
{
    EditParams p;
    p.exposure = 0.75f;                 // non-identity, so the fused point pass runs

    /* Folded: Apply() sees camera-native input and folds stage 0 into preMat. */
    WorkingImage folded = makeCameraNative(24, 18);
    Develop d1;
    QVERIFY(d1.Apply(folded, p));

    /* Sequential: convert first, so Apply() sees working-space input and takes the
       ordinary diagonal-gain path with no fold. */
    WorkingImage seq = makeCameraNative(24, 18);
    Develop::ToWorkingSpace(seq);
    Develop d2;
    QVERIFY(d2.Apply(seq, p));

    const float worst = worstDiff(folded, seq);
    QVERIFY2(worst < 1e-5f, qPrintable(QString("fold vs sequence worst %1").arg(worst)));
    QCOMPARE(folded.space, ColorSpaceMath::kWorking);
}

/* Same property with the Calibrate matrix in play, which is the other 3x3 in the fold --
   this is where a multiply in the wrong ORDER would show up (matrices do not commute). */
void TestInputProfile::foldEqualsSequenceWithCalibrate()
{
    EditParams p;
    p.exposure    = -0.4f;
    p.calRedHue   = 40.0f;
    p.calGreenSat = -35.0f;
    p.calBlueHue  = 25.0f;

    WorkingImage folded = makeCameraNative(24, 18);
    Develop d1;
    QVERIFY(d1.Apply(folded, p));

    WorkingImage seq = makeCameraNative(24, 18);
    Develop::ToWorkingSpace(seq);
    Develop d2;
    QVERIFY(d2.Apply(seq, p));

    const float worst = worstDiff(folded, seq);
    QVERIFY2(worst < 1e-5f, qPrintable(QString("fold vs sequence worst %1").arg(worst)));
}

/*
    Denoise runs BEFORE the point pass, so Apply() converts up front instead of folding.
    Both routes must still agree -- otherwise switching denoise on would shift colour,
    which no user would attribute to the denoise slider.
*/
void TestInputProfile::denoisePathAgreesWithFoldedPath()
{
    EditParams p;
    p.exposure = 0.3f;

    WorkingImage noDenoise = makeCameraNative(24, 18);
    Develop d1;
    QVERIFY(d1.Apply(noDenoise, p));

    /* Same params plus a denoise amount, applied to an ALREADY-converted image, so the
       only difference between the two runs is denoise itself rather than the space. */
    EditParams pd = p;
    pd.localDenoiseLuma = 0.0f;         // zero: Denoise is a no-op, path choice unchanged
    WorkingImage viaEarly = makeCameraNative(24, 18);
    Develop::ToWorkingSpace(viaEarly);  // force the early-convert route
    Develop d2;
    QVERIFY(d2.Apply(viaEarly, pd));

    const float worst = worstDiff(noDenoise, viaEarly);
    QVERIFY2(worst < 1e-5f, qPrintable(QString("denoise route worst %1").arg(worst)));
}

/* A non-raw image arrives already in the working space: stage 0 must leave it alone. */
void TestInputProfile::nonRawIsUntouched()
{
    WorkingImage img = makeCameraNative(8, 8);
    img.space = ColorSpaceMath::kWorking;       // as InputTransform tags it
    const WorkingImage before = img;
    Develop::ToWorkingSpace(img);
    QCOMPARE(worstDiff(img, before), 0.0f);
}

/* ------------------------------------------------------------------------------------
   Camera profiles -- stage 0 replaced, and the white balance that comes with it
   ------------------------------------------------------------------------------------ */

namespace {

/* A real Adobe Standard profile's numbers (Canon EOS 77D), attached directly rather than
   read from a file: these tests are about what the RENDER does with a profile, not about
   parsing one. */
std::shared_ptr<const Dcp::Profile> makeProfile()
{
    static const double color1[9] = { 0.7952, -0.1689, -0.0575,
                                     -0.3746,  1.0825,  0.3378,
                                     -0.0405,  0.1362,  0.6120 };
    static const double color2[9] = { 0.7377, -0.0742, -0.0998,
                                     -0.4235,  1.1981,  0.2549,
                                     -0.0673,  0.1918,  0.5538 };
    static const double fwd1[9]   = { 0.5407,  0.2506,  0.1730,
                                      0.3306,  0.6136,  0.0558,
                                      0.1852,  0.0007,  0.6392 };
    static const double fwd2[9]   = { 0.5388,  0.1799,  0.2457,
                                      0.3091,  0.6107,  0.0802,
                                      0.1438,  0.0001,  0.6812 };
    Dcp::Profile p;
    p.valid = true;
    p.uniqueCameraModel = "Canon EOS 77D";
    p.name = "Adobe Standard";
    const double *src[4] = {color1, color2, fwd1, fwd2};
    for (int c = 0; c < 2; ++c) {
        p.cal[c].illuminant = c ? 21 : 17;          // D65 / Standard light A
        p.cal[c].haveColor = true;
        p.cal[c].haveForward = true;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                p.cal[c].color.m[i][j]   = float(src[c][i * 3 + j]);
                p.cal[c].forward.m[i][j] = float(src[2 + c][i * 3 + j]);
            }
    }
    return std::make_shared<const Dcp::Profile>(std::move(p));
}

} // namespace

/*
    A CAMERA PROFILE REPLACES STAGE 0 and reports that it carries the white balance.

    The second half is the part that cannot be left to inspection: InputMatrix's caller
    uses wbIncluded to decide whether to ALSO apply per-channel gains, and a matrix that
    quietly claimed not to contain a white balance would be balanced twice -- which looks
    like a slightly-too-warm picture, not like a bug.
*/
void TestInputProfile::cameraProfileReplacesTheInputMatrix()
{
    WorkingImage img = makeCameraNative(4, 4);
    EditParams p;

    float builtIn[3][3];
    bool wbIncluded = true;
    QVERIFY(Develop::InputMatrix(img, &p, builtIn, wbIncluded));
    QVERIFY2(!wbIncluded, "the built-in path does NOT contain the white balance");
    /* It is camToWorking . diag(asShotMul) -- the product the fold has always used. */
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            QVERIFY(qAbs(builtIn[i][j] - kCamToWorking[i][j] * kAsShot[j]) < 1e-6f);

    img.cam.profile = makeProfile();
    p.cameraProfile = "Adobe Standard";
    float withProfile[3][3];
    QVERIFY(Develop::InputMatrix(img, &p, withProfile, wbIncluded));
    QVERIFY2(wbIncluded, "a camera profile DOES contain the white balance");

    /* And it is a different matrix -- otherwise the test proves nothing. */
    float worst = 0.0f;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            worst = std::max(worst, std::fabs(withProfile[i][j] - builtIn[i][j]));
    QVERIFY(worst > 1e-3f);
}

/*
    THE WHITE BALANCE IS APPLIED EXACTLY ONCE.

    Under DNG the chosen temperature sets diag(1/referenceNeutral) INSIDE the profile's
    matrix, so buildPointCoeffs must drop the per-channel gains it applies on the built-in
    path. The test feeds the camera the exact colour a neutral surface under the chosen
    illuminant produces: if the white balance runs once it renders neutral, and if it runs
    twice the correction is squared and the pixel comes out visibly tinted.
*/
void TestInputProfile::cameraProfileWhiteBalancesExactlyOnce()
{
    const auto profile = makeProfile();

    for (float kelvin : {3000.0f, 5000.0f, 6504.0f}) {
        double n[3];
        QVERIFY(CameraProfile::neutralCam(*profile, kelvin, 0.0f, n));

        WorkingImage img = makeCameraNative(2, 2);
        img.cam.profile = profile;
        for (size_t i = 0; i < img.rgb.size(); i += 3)
            for (int c = 0; c < 3; ++c) img.rgb[i + c] = float(n[c]);

        EditParams p;
        p.cameraProfile = "Adobe Standard";
        p.temp = kelvin;
        p.tint = 0.0f;

        Develop d;
        QVERIFY(d.Apply(img, p));

        for (int c = 0; c < 3; ++c)
            QVERIFY2(qAbs(img.rgb[c] - 1.0f) < 1e-3f,
                     qPrintable(QString("channel %1 = %2 at %3 K -- white balanced twice?")
                                    .arg(c).arg(double(img.rgb[c])).arg(double(kelvin))));
    }
}

/*
    THE TWO ROUTES STILL AGREE WITH A PROFILE, which is what pins the least obvious rule
    in this feature: ToWorkingSpace LEAVES img.cam.profile set after converting.

    With an active local Denoise the conversion happens early, as its own pass, and the
    pixels are re-tagged as working-space; buildPointCoeffs then cannot tell from the tag
    that a profile already did the white balance. It tests cam.profile instead. Clear the
    profile after converting -- the tidy-looking thing to do -- and this test fails,
    because the early route would white-balance a second time while the folded route
    would not.
*/
void TestInputProfile::cameraProfileSurvivesTheDenoiseRoute()
{
    const auto profile = makeProfile();

    EditParams p;
    p.cameraProfile = "Adobe Standard";
    p.temp = 4200.0f;
    p.exposure = 0.3f;

    WorkingImage folded = makeCameraNative(24, 18);
    folded.cam.profile = profile;
    Develop d1;
    QVERIFY(d1.Apply(folded, p));

    WorkingImage early = makeCameraNative(24, 18);
    early.cam.profile = profile;
    Develop::ToWorkingSpace(early, &p);          // force the early-convert route
    QVERIFY2(early.cam.profile != nullptr,
             "ToWorkingSpace must LEAVE the profile on cam -- it is how buildPointCoeffs "
             "knows the white balance is already applied");
    Develop d2;
    QVERIFY(d2.Apply(early, p));

    const float worst = worstDiff(folded, early);
    QVERIFY2(worst < 1e-5f, qPrintable(QString("profile route worst %1").arg(worst)));
}

/*
    STAGE 0 RUNS ONCE PER IMAGE, NOT ONCE PER SCOPE.

    The scope compositor develops each mask layer by COPYING the base result and calling
    Apply again with that scope's params. A WorkingImage copy carries `cam` wholesale, so
    a profile left on it after the base develop would be spent a second time on every
    scope: the HueSatMap re-applied on top of itself, and the scope's own white balance
    silently dropped (buildPointCoeffs reads cam.profile to mean "already balanced", which
    is true of the base and false of a layer above it).

    Apply therefore clears the profile when it is done. This is what that looks like from
    outside: develop once, then develop the result again exactly as a scope layer would,
    and the second pass must behave as though there were no profile at all.
*/
void TestInputProfile::stageZeroRunsOnlyOnce()
{
    const auto profile = makeProfile();

    EditParams base;
    base.cameraProfile = "Adobe Standard";
    base.temp = 4200.0f;

    WorkingImage img = makeCameraNative(16, 12);
    img.cam.profile = profile;
    Develop d1;
    QVERIFY(d1.Apply(img, base));
    QVERIFY2(img.cam.profile == nullptr,
             "Apply must drop the profile once stage 0 has been spent");

    /* Now a scope layer: the developed image, re-developed with the scope's own params.
       Its result must equal the same params applied to an image that never had a profile,
       because stage 0 is behind it either way. */
    EditParams scope;
    scope.exposure = 0.4f;
    scope.temp = 7000.0f;               // a per-mask white balance

    WorkingImage layer = img;           // exactly what renderStack copies
    Develop d2;
    QVERIFY(d2.Apply(layer, scope));

    WorkingImage reference = img;
    reference.cam.profile = nullptr;    // already null; stated so the intent is explicit
    Develop d3;
    QVERIFY(d3.Apply(reference, scope));

    const float worst = worstDiff(layer, reference);
    QVERIFY2(worst < 1e-6f, qPrintable(QString("scope layer worst %1").arg(worst)));

    /* And the scope's white balance actually did something -- otherwise the comparison
       above would pass with both sides equally broken. */
    WorkingImage noWb = img;
    EditParams scopeNoWb = scope;
    scopeNoWb.temp = 0.0f;
    Develop d4;
    QVERIFY(d4.Apply(noWb, scopeNoWb));
    QVERIFY2(worstDiff(layer, noWb) > 1e-3f,
             "the scope's own white balance was dropped");
}

/* The look tags, added to a copy of the shared test profile. */
static Dcp::Profile withLook(const Dcp::Profile &base, float offsetEv)
{
    Dcp::Profile p = base;
    p.baselineExposureOffset = offsetEv;
    p.lookTable.hueDivs = 4;
    p.lookTable.satDivs = 2;
    p.lookTable.valDivs = 1;
    p.lookTable.v.assign(4 * 2 * 1 * 3, 0.0f);
    for (size_t i = 0; i < p.lookTable.v.size(); i += 3) {
        p.lookTable.v[i + 1] = 1.0f;        // identity table, so the offset is the only
        p.lookTable.v[i + 2] = 1.0f;        // thing whose effect is an exact number
    }
    p.toneCurve = {0.0f, 0.0f, 1.0f, 1.0f};                 // the diagonal
    return p;
}

/* The largest per-channel difference between two 8-bit renders, in levels. */
static int worstLevels(const QImage &a, const QImage &b)
{
    int worst = 0;
    for (int y = 0; y < a.height(); ++y) {
        const uchar *pa = a.constScanLine(y), *pb = b.constScanLine(y);
        for (int x = 0; x < a.width() * 3; ++x) worst = qMax(worst, qAbs(pa[x] - pb[x]));
    }
    return worst;
}

static QImage renderOut(const WorkingImage &img, OutputTransform::ViewTransform vt)
{
    QImage out;
    OutputTransform ot;
    ot.ToImage(img, out, OutputTransform::Space::sRGB, vt);
    return out;
}

/*
    A "CAMERA BASE" IS THE PROFILE WITH ITS LOOK STRIPPED, and must render exactly as a
    profile that never had one.

    The look (LookTable, offset, curve) no longer touches stage 0 at all -- it rides to
    the output stage on img.look -- so the developed pixels match outright, and under Winnow's
    own tone mapping the offset is undone after the (here identity) LookTable, so the
    rendered output matches too. Only "Profile curve" keeps the offset, because that curve
    was built for it.
*/
void TestInputProfile::cameraBaseRendersWithoutTheLook()
{
    const auto plain = makeProfile();                       // no look tags at all
    const auto full  = std::make_shared<const Dcp::Profile>(withLook(*plain, 1.0f));

    Dcp::Profile stripped = *full;
    stripped.lookTable = Dcp::Table3D();
    stripped.toneCurve.clear();
    stripped.baselineExposureOffset = 0.0f;
    const auto base = std::make_shared<const Dcp::Profile>(stripped);

    EditParams p;
    p.cameraProfile = "Camera Base";

    WorkingImage viaBase = makeCameraNative(12, 9);
    viaBase.cam.profile = base;
    Develop d1;
    QVERIFY(d1.Apply(viaBase, p));

    WorkingImage viaPlain = makeCameraNative(12, 9);
    viaPlain.cam.profile = plain;
    Develop d2;
    QVERIFY(d2.Apply(viaPlain, p));
    QVERIFY2(worstDiff(viaBase, viaPlain) < 1e-6f,
             "a stripped profile is not the bare characterisation");
    QVERIFY(!viaBase.look);

    /* The full profile: the same developed pixels, its look carried for the output. */
    WorkingImage viaFull = makeCameraNative(12, 9);
    viaFull.cam.profile = full;
    Develop d3;
    QVERIFY(d3.Apply(viaFull, p));
    QVERIFY2(worstDiff(viaFull, viaBase) < 1e-6f, "the look leaked into stage 0");
    QVERIFY2(viaFull.look, "the look was not attached for the output stage");
    QVERIFY(qAbs(viaFull.look->exposureScale - 2.0f) < 1e-4f);

    /* Under Winnow's own tone mapping the offset nets out around the identity table. */
    const QImage a = renderOut(viaFull, OutputTransform::ViewTransform::Filmic);
    const QImage b = renderOut(viaBase, OutputTransform::ViewTransform::Filmic);
    QVERIFY2(worstLevels(a, b) <= 1,
             qPrintable(QString("look changed a Standard roll-off render by %1 levels")
                            .arg(worstLevels(a, b))));
}

/*
    THE PROFILE'S CURVE IS A TONE MAPPING CHOICE, NOT AN OVERRIDE. It applies under
    "Profile curve" and only there; every other choice renders the profile's colour with
    Winnow's own tone. A profile without a curve renders "Profile curve" as Standard
    roll-off rather than as unmapped data.
*/
void TestInputProfile::profileCurveIsOneToneMappingAmongOthers()
{
    using VT = OutputTransform::ViewTransform;
    const auto plain = makeProfile();
    Dcp::Profile curved = withLook(*plain, 0.0f);
    curved.toneCurve = {0.0f, 0.0f, 0.25f, 0.55f, 0.5f, 0.78f, 1.0f, 1.0f};
    const auto profile = std::make_shared<const Dcp::Profile>(curved);

    WorkingImage img = makeCameraNative(8, 6);
    img.cam.profile = profile;
    EditParams p;
    p.cameraProfile = "Camera NT";
    Develop d;
    QVERIFY(d.Apply(img, p));
    QVERIFY(img.look && img.look->hasProfileCurve());

    const QImage curve  = renderOut(img, VT::ProfileCurve);
    const QImage filmic = renderOut(img, VT::Filmic);
    const QImage none   = renderOut(img, VT::None);
    const QImage camera = renderOut(img, VT::CameraContrast);
    QVERIFY2(curve != filmic && curve != none && curve != camera,
             "the profile curve is not its own rendering");

    /* No curve in the profile: Profile curve falls back to Standard roll-off. */
    WorkingImage bare = makeCameraNative(8, 6);
    bare.cam.profile = plain;
    Develop d2;
    QVERIFY(d2.Apply(bare, p));
    QCOMPARE(renderOut(bare, VT::ProfileCurve), renderOut(bare, VT::Filmic));
}

/*
    THE BLOWOUT THIS WAS BUILT FOR: Camera Neutral at +3 EV turned a face into a flat
    white patch, because the curve ran BEFORE exposure and the push multiplied pixels it
    had already taken to white. Now the curve runs last, with a roll-off above its knee,
    so a pushed exposure keeps separation: through two stops over white (where a +3 EV
    face lands) no channel reaches 255 and brighter input renders brighter. Far beyond
    that the roll-off may round to 255 in 8 bits -- as Standard roll-off does from 2.6
    stops over -- so that is not asserted.
*/
void TestInputProfile::profileCurveRollsOffAPushedExposure()
{
    const auto plain = makeProfile();
    Dcp::Profile curved = withLook(*plain, 0.0f);
    curved.toneCurve = {0.0f, 0.0f, 0.18f, 0.45f, 0.5f, 0.85f, 0.9f, 0.99f, 1.0f, 1.0f};
    const auto profile = std::make_shared<const Dcp::Profile>(curved);

    WorkingImage img = makeCameraNative(16, 12);
    img.cam.profile = profile;
    EditParams p;
    p.cameraProfile = "Camera Neutral";
    p.exposure = 3.0f;
    Develop d;
    QVERIFY(d.Apply(img, p));

    using VT = OutputTransform::ViewTransform;
    const int y = img.height / 2;
    int tested = 0;
    for (VT vt : {VT::ProfileCurve, VT::CameraContrast}) {
        const QImage out = renderOut(img, vt);
        const uchar *row = out.constScanLine(y);
        int prevSum = -1;
        for (int x = 0; x < img.width; ++x) {
            const float *px = &img.rgb[(size_t(y) * img.width + x) * 3];
            if (std::max({px[0], px[1], px[2]}) > 4.0f) continue;   // past two stops over
            const uchar *o = row + x * 3;
            QVERIFY2(o[0] < 255 && o[1] < 255 && o[2] < 255,
                     qPrintable(QString("view %1 clipped to white at x %2 (max %3)")
                                    .arg(int(vt)).arg(x)
                                    .arg(double(std::max({px[0], px[1], px[2]})))));
            const int sum = o[0] + o[1] + o[2];
            QVERIFY2(sum > prevSum, qPrintable(QString("view %1 lost separation at x %2")
                                                   .arg(int(vt)).arg(x)));
            prevSum = sum;
            ++tested;
        }
    }
    QVERIFY2(tested >= 6, "too few pixels in range -- the test is not testing anything");

    /* The old behaviour for contrast: no tone mapping at all clips the same pixels. */
    const QImage none = renderOut(img, VT::None);
    int noneClipped = 0;
    const uchar *nrow = none.constScanLine(y);
    for (int x = 0; x < img.width; ++x)
        if (nrow[x * 3] == 255 || nrow[x * 3 + 1] == 255 || nrow[x * 3 + 2] == 255)
            ++noneClipped;
    QVERIFY2(noneClipped > 0, "the exposure push is not over white -- test is too weak");
}

/*
    THE LOOK MUST SURVIVE copyMetadata. A WorkingImage field not copied there does not
    fail to compile and does not fail loudly: the scope compositor copies the developed
    accumulator into every mask layer, and WorkingImageCache::downscaled builds the
    interactive proxy the same way. A missed field there once cost a whole-image green
    cast on every proxy render.
*/
void TestInputProfile::lookSurvivesACopy()
{
    WorkingImage src = makeCameraNative(4, 4);
    src.look = std::make_shared<const OutputLook>();

    WorkingImage viaMetadata;
    copyMetadata(viaMetadata, src);
    QVERIFY2(viaMetadata.look == src.look, "copyMetadata dropped the look");

    WorkingImage viaAssign = makeCameraNative(4, 4);
    assignReusing(viaAssign, src);
    QVERIFY2(viaAssign.look == src.look, "assignReusing dropped the look");
}

/*
    ADOBE'S PER-CAMERA BASELINE EXPOSURE is applied at stage 0, once. An ILCE-1 (+0.15 EV)
    develops exactly 2^0.15 brighter than the same pixels with no camera named; an
    unlisted body gets the common +0.35; an empty model (non-raw, synthetic) gets nothing.
    And a mask layer -- developed from a copy of the converted base, with cam copied too --
    must NOT add it again.
*/
void TestInputProfile::baselineExposureAppliesOncePerBody()
{
    QCOMPARE(BaselineExposure::ForModel(QString()), 0.0f);
    QCOMPARE(BaselineExposure::ForModel("Sony ILCE-1"), 0.15f);
    QCOMPARE(BaselineExposure::ForModel("nikon d7200"), 0.35f);
    QCOMPARE(BaselineExposure::ForModel("Olympus OM-1 Mark II"),
             BaselineExposure::ForModel("OM Digital Solutions OM-1 Mark II"));
    QCOMPARE(BaselineExposure::ForModel("Acme Nonesuch 9"), BaselineExposure::kUnlistedEv);

    EditParams p;
    p.exposure = 0.3f;                  // non-identity, so the fused pass runs
    WorkingImage none = makeCameraNative(10, 8);
    WorkingImage sony = makeCameraNative(10, 8);
    sony.cam.cameraModel = "Sony ILCE-1";
    Develop d1, d2;
    QVERIFY(d1.Apply(none, p));
    QVERIFY(d2.Apply(sony, p));
    const float k = std::exp2(0.15f);
    for (size_t i = 0; i < none.rgb.size(); ++i) {
        if (std::fabs(none.rgb[i]) < 1e-4f) continue;
        QVERIFY2(std::fabs(sony.rgb[i] / none.rgb[i] - k) < 1e-3f,
                 "BaselineExposure was not applied as a uniform 2^EV at stage 0");
    }

    /* A scope layer: the converted base, developed again with neutral params. */
    WorkingImage layer = sony;
    EditParams scope;
    scope.exposure = 0.0f;
    scope.contrast = 1.0f;              // non-identity, but no exposure change
    WorkingImage layerRef = none;
    Develop d3, d4;
    QVERIFY(d3.Apply(layer, scope));
    QVERIFY(d4.Apply(layerRef, scope));
    /* Same ratio still: the layer pass did not apply it a second time. Contrast is not a
       linear scale, so compare on a pixel the two passes treat alike -- the ratio of the
       means of the two whole images must stay well under 2^0.30. */
    double a = 0, b = 0;
    for (size_t i = 0; i < layer.rgb.size(); ++i) { a += layer.rgb[i]; b += layerRef.rgb[i]; }
    QVERIFY2(a / b < std::exp2(0.25), "BaselineExposure applied twice on a scope layer");
}

QTEST_APPLESS_MAIN(TestInputProfile)
#include "tst_inputprofile.moc"
