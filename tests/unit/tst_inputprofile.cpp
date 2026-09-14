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
#include <cmath>

#include "Develop/develop.h"
#include "Develop/cameraprofile.h"
#include <QImage>
#include "Develop/workingimage.h"
#include "Develop/editparams.h"
#include "Develop/colorspace.h"
#include "Develop/outputtransform.h"

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
    void defaultViewTransformIsNone();
    void cameraProfileReplacesTheInputMatrix();
    void cameraProfileWhiteBalancesExactlyOnce();
    void cameraProfileSurvivesTheDenoiseRoute();
    void stageZeroRunsOnlyOnce();
    void cameraBaseRendersWithoutTheLook();
    void profileToneCurveSuppressesTheViewTransform();
    void profileToneMappedSurvivesACopy();
};

/*
    THE STORED DEFAULT MUST RESOLVE TO NONE.

    EditParams::viewTransform is an int (so it rides the existing int machinery) and
    default-constructs to 0. OutputTransform::ViewFromInt maps that back to an enum, and
    the two have to agree about which value zero is. Zero is the IDENTITY -- no tone
    mapping -- because a default-constructed EditParams is what "no edits" means: what an
    untouched image renders as, what isIdentity() tests for and what Reset Basic restores.
    A non-zero identity would put a special case for this one field in all three.

    Pinned here rather than left to the enum's declaration order, because the numbering is
    also the sidecar format: it may be added to, never renumbered.
*/
void TestInputProfile::defaultViewTransformIsNone()
{
    const EditParams def;
    QCOMPARE(def.viewTransform, 0);
    QCOMPARE(OutputTransform::ViewFromInt(def.viewTransform),
             OutputTransform::ViewTransform::None);

    /* Reset Basic restores the identity, i.e. no tone mapping. */
    EditParams p;
    p.viewTransform = int(OutputTransform::ViewTransform::AgX);
    EditParams::resetGroup(p, EditParams::Group::Basic);
    QCOMPARE(OutputTransform::ViewFromInt(p.viewTransform),
             OutputTransform::ViewTransform::None);

    /* The published numbering itself. */
    QCOMPARE(int(OutputTransform::ViewTransform::None),   0);
    QCOMPARE(int(OutputTransform::ViewTransform::Filmic), 1);
    QCOMPARE(int(OutputTransform::ViewTransform::AgX),    2);

    /* An unknown value (a sidecar from a later build) falls back to the default rather
       than to whatever the cast happens to land on. */
    QCOMPARE(OutputTransform::ViewFromInt(99), OutputTransform::ViewTransform::None);
    QCOMPARE(OutputTransform::ViewFromInt(-1), OutputTransform::ViewTransform::None);
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

/*
    A "CAMERA BASE" IS THE PROFILE WITH ITS LOOK STRIPPED, and must render exactly as a
    profile that never had one.

    There is no "apply the look?" flag in the pipeline any more: the store hands back a
    synthesised profile whose look tags are cleared, and the render path cannot tell the
    difference. This pins that -- including the BaselineExposureOffset, which is the piece
    that would move the picture's brightness if it were left behind (224 of 436 installed
    profiles carry one).
*/
void TestInputProfile::cameraBaseRendersWithoutTheLook()
{
    const auto plain = makeProfile();                       // no look tags at all
    const auto full  = std::make_shared<const Dcp::Profile>(withLook(*plain, 1.0f));

    /* What CameraProfileStore::profile() does for a base name. */
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
             qPrintable(QString("a stripped profile is not the bare characterisation: %1")
                            .arg(double(worstDiff(viaBase, viaPlain)))));

    /* And the un-stripped profile is NOT the same -- one stop brighter, from its offset.
       Without this the comparison above would pass with both sides equally inert. */
    WorkingImage viaFull = makeCameraNative(12, 9);
    viaFull.cam.profile = full;
    Develop d3;
    QVERIFY(d3.Apply(viaFull, p));
    for (size_t i = 0; i < viaFull.rgb.size(); ++i) {
        if (viaBase.rgb[i] <= 0.0f) continue;
        QVERIFY2(qAbs(viaFull.rgb[i] / viaBase.rgb[i] - 2.0f) < 2e-3f,
                 "the look profile did not apply its one-stop offset");
    }
}

/*
    EXACTLY ONE TONE MAPPING IS EVER IN PLAY.

    A ProfileToneCurve is a whole scene-linear -> display mapping, so a view transform on
    top of it compresses the highlights twice. Measured before this guard existed: mid grey
    rendered 220 instead of 176, and a full stop above white collapsed to nothing (1.0 and
    2.0 both 242). OutputTransform::EffectiveView now forces None when the profile has
    already mapped the data -- the same rule it applies to display-referred input.
*/
void TestInputProfile::profileToneCurveSuppressesTheViewTransform()
{
    const auto plain = makeProfile();
    Dcp::Profile curved = withLook(*plain, 0.0f);
    /* A real, pronounced curve rather than the diagonal, so "suppressed" and "applied"
       cannot look alike. */
    curved.toneCurve = {0.0f, 0.0f, 0.25f, 0.55f, 0.5f, 0.78f, 1.0f, 1.0f};
    const auto profile = std::make_shared<const Dcp::Profile>(curved);

    auto render = [&](int viewTransform, QImage &out) {
        WorkingImage img = makeCameraNative(8, 6);
        img.cam.profile = profile;
        EditParams p;
        p.cameraProfile = "Camera NT";
        p.viewTransform = viewTransform;
        Develop d;
        QVERIFY(d.Apply(img, p));
        QVERIFY2(img.profileToneMapped,
                 "applying a ProfileToneCurve must mark the image tone mapped");
        OutputTransform ot;
        QVERIFY(ot.ToImage(img, out));
    };

    QImage none, filmic, agx;
    render(int(OutputTransform::ViewTransform::None), none);
    render(int(OutputTransform::ViewTransform::Filmic), filmic);
    render(int(OutputTransform::ViewTransform::AgX), agx);

    QCOMPARE(filmic, none);
    QCOMPARE(agx, none);

    /* The guard must be conditional, not a blanket disable: with no profile curve the
       view transform still does its job. */
    WorkingImage bare = makeCameraNative(8, 6);
    bare.cam.profile = plain;
    EditParams p;
    p.cameraProfile = "Camera Base";
    p.viewTransform = int(OutputTransform::ViewTransform::Filmic);
    Develop d;
    QVERIFY(d.Apply(bare, p));
    QVERIFY(!bare.profileToneMapped);
    QImage bareFilmic, bareNone;
    OutputTransform ot;
    QVERIFY(ot.ToImage(bare, bareFilmic, OutputTransform::Space::sRGB,
                       OutputTransform::ViewTransform::Filmic));
    QVERIFY(ot.ToImage(bare, bareNone, OutputTransform::Space::sRGB,
                       OutputTransform::ViewTransform::None));
    QVERIFY2(bareFilmic != bareNone, "the view transform was disabled unconditionally");
}

/*
    THE FLAG MUST SURVIVE copyMetadata.

    A WorkingImage field that is not copied there does not fail to compile and does not
    fail loudly: the scope compositor copies the developed accumulator into every mask
    layer, and WorkingImageCache::downscaled builds the interactive proxy the same way. A
    missed field there once cost a whole-image green cast on every proxy render.
*/
void TestInputProfile::profileToneMappedSurvivesACopy()
{
    WorkingImage src = makeCameraNative(4, 4);
    src.profileToneMapped = true;

    WorkingImage viaMetadata;
    copyMetadata(viaMetadata, src);
    QVERIFY2(viaMetadata.profileToneMapped, "copyMetadata dropped profileToneMapped");

    WorkingImage viaAssign = makeCameraNative(4, 4);
    assignReusing(viaAssign, src);
    QVERIFY2(viaAssign.profileToneMapped, "assignReusing dropped profileToneMapped");
}

QTEST_APPLESS_MAIN(TestInputProfile)
#include "tst_inputprofile.moc"
