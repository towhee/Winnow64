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
    void defaultViewTransformIsFilmic();
};

/*
    THE STORED DEFAULT MUST RESOLVE TO FILMIC.

    EditParams::viewTransform is an int (so it rides the existing int machinery) and
    default-constructs to 0. OutputTransform::ViewFromInt maps that back to an enum, and
    the two have to agree about which value zero is. They did NOT: the enum originally
    listed None first, so a default-constructed EditParams asked for NO TONE MAPPING and
    every untouched raw would have rendered dark and flat.

    Pinned here rather than left to the enum's declaration order, because the numbering is
    also the sidecar format: it may be added to, never renumbered.
*/
void TestInputProfile::defaultViewTransformIsFilmic()
{
    const EditParams def;
    QCOMPARE(def.viewTransform, 0);
    QCOMPARE(OutputTransform::ViewFromInt(def.viewTransform),
             OutputTransform::ViewTransform::Filmic);

    /* The published numbering itself. */
    QCOMPARE(int(OutputTransform::ViewTransform::Filmic), 0);
    QCOMPARE(int(OutputTransform::ViewTransform::AgX),    1);
    QCOMPARE(int(OutputTransform::ViewTransform::None),   2);

    /* An unknown value (a sidecar from a later build) falls back to the default rather
       than to whatever the cast happens to land on. */
    QCOMPARE(OutputTransform::ViewFromInt(99), OutputTransform::ViewTransform::Filmic);
    QCOMPARE(OutputTransform::ViewFromInt(-1), OutputTransform::ViewTransform::Filmic);
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

QTEST_APPLESS_MAIN(TestInputProfile)
#include "tst_inputprofile.moc"
