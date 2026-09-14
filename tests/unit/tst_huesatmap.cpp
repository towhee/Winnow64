/*
    HueSatMap -- the 3-D HSV correction table a DNG camera profile carries.

    WHY THIS TEST EXISTS. A lookup table misapplied does not fail, it renders: a table read
    with its axes transposed shears hue against saturation and produces colours that look
    deliberate, and a value axis clamped at 1.0 silently flattens every specular highlight
    the moment a profile is selected. Neither shows up as an error anywhere.

    The storage order is pinned HERE by construction (a table whose entries encode their
    own indices), having first been established EMPIRICALLY against 279 installed
    HueSatMaps and real LookTables -- see the note in Develop/huesatmap.h. Both matter: the
    empirical check says the order is right for real files, this one says the code still
    implements the order that was checked.
*/

#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <cmath>

#include "Develop/huesatmap.h"
#include "Develop/cameraprofile.h"
#include "Develop/profiletone.h"

class TstHueSatMap : public QObject
{
    Q_OBJECT

private slots:
    void identityTableChangesNothing();
    void storageOrderIsValueHueSat();
    void hueShiftRotatesAndWraps();
    void highlightHeadroomSurvives();
    void saturationIsClampedButValueIsNot();
    void blendEndpointsAndMidpoint();
    void blendTakesTheOtherWhenOneIsEmpty();
    void srgbEncodedTableRoundTrips();
    void srgbTransferTableMatchesTheExactCurve();
    void neutralSurvivesARealProfile();
    void toneCurveIdentityIsIdentity();
    void toneCurveHoldsHueAndSaturation();
    void toneCurveExtendsAboveWhiteInsteadOfClamping();
    void toneCurveRejectsUnusableData();
    void lookStripsCleanlyFromTheCharacterisation();

private:
    /* A table of the given shape, every entry the identity (no shift, unit scales). */
    static HueSatMap::Table identity(int h, int s, int v)
    {
        HueSatMap::Table t;
        t.hueDivs = h; t.satDivs = s; t.valDivs = v;
        t.v.assign(size_t(h) * s * v * 3, 0.0f);
        for (size_t i = 0; i < t.v.size(); i += 3) { t.v[i + 1] = 1.0f; t.v[i + 2] = 1.0f; }
        return t;
    }
    static float *entry(HueSatMap::Table &t, int h, int s, int v)
    {
        return &t.v[size_t((v * t.hueDivs + h) * t.satDivs + s) * 3];
    }
};

void TstHueSatMap::identityTableChangesNothing()
{
    const HueSatMap::Table t = identity(6, 4, 3);

    /* Including the awkward ones: pure primaries (hue exactly on a division), a neutral
       (no hue at all), black (no hue and no value) and an over-white value. */
    const float probes[][3] = {
        {0.80f, 0.20f, 0.10f}, {0.20f, 0.80f, 0.30f}, {0.10f, 0.25f, 0.90f},
        {0.50f, 0.50f, 0.50f}, {0.00f, 0.00f, 0.00f}, {3.00f, 2.50f, 1.20f},
        {1.00f, 0.00f, 0.00f}, {0.00f, 1.00f, 1.00f}
    };
    for (const auto &p : probes) {
        float r = p[0], g = p[1], b = p[2];
        HueSatMap::Apply(t, r, g, b);
        QVERIFY2(qAbs(r - p[0]) < 1e-5f && qAbs(g - p[1]) < 1e-5f && qAbs(b - p[2]) < 1e-5f,
                 qPrintable(QString("(%1,%2,%3) -> (%4,%5,%6)")
                     .arg(double(p[0])).arg(double(p[1])).arg(double(p[2]))
                     .arg(double(r)).arg(double(g)).arg(double(b))));
    }
}

/*
    THE STORAGE ORDER: index = ((val * hueDivs + hue) * satDivs + sat) * 3.

    Every entry is given a value that identifies WHICH entry it is, then one colour whose
    hue, saturation and value land exactly on a known grid node is pushed through. If the
    axes were ordered any other way a different entry would be fetched and the three
    returned numbers would not be the ones this node holds.
*/
void TstHueSatMap::storageOrderIsValueHueSat()
{
    HueSatMap::Table t = identity(4, 3, 2);
    for (int v = 0; v < 2; ++v)
        for (int h = 0; h < 4; ++h)
            for (int s = 0; s < 3; ++s) {
                float *e = entry(t, h, s, v);
                e[0] = 10.0f * h;               // hue shift  == 10 x the hue index
                e[1] = 1.0f + 0.1f * s;         // sat scale  == 1 + 0.1 x the sat index
                e[2] = 1.0f + 0.5f * v;         // val scale  == 1 + 0.5 x the value index
            }

    /* hue 90 of 4 divisions = node 1 exactly; sat 0.5 of 3 = node 1; value 1.0 = node 1. */
    float r, g, b;
    HueSatMap::FromHsv(90.0f, 0.5f, 1.0f, r, g, b);
    HueSatMap::Apply(t, r, g, b);

    float h, s, v;
    HueSatMap::ToHsv(r, g, b, h, s, v);
    QVERIFY2(qAbs(h - 100.0f) < 1e-3f,      // 90 + 10 x 1
             qPrintable(QString("hue %1, expected 100 -- hue axis misread").arg(double(h))));
    QVERIFY2(qAbs(s - 0.55f) < 1e-4f,       // 0.5 x (1 + 0.1 x 1)
             qPrintable(QString("sat %1, expected 0.55 -- saturation axis misread").arg(double(s))));
    QVERIFY2(qAbs(v - 1.5f) < 1e-4f,        // 1.0 x (1 + 0.5 x 1)
             qPrintable(QString("val %1, expected 1.5 -- value axis misread").arg(double(v))));
}

void TstHueSatMap::hueShiftRotatesAndWraps()
{
    HueSatMap::Table t = identity(4, 2, 1);
    for (size_t i = 0; i < t.v.size(); i += 3) t.v[i] = 60.0f;

    /* Red (hue 0) shifted +60 is yellow. */
    float r = 1.0f, g = 0.0f, b = 0.0f;
    HueSatMap::Apply(t, r, g, b);
    float h, s, v;
    HueSatMap::ToHsv(r, g, b, h, s, v);
    QVERIFY(qAbs(h - 60.0f) < 1e-3f);
    QVERIFY(qAbs(s - 1.0f) < 1e-5f);
    QVERIFY(qAbs(v - 1.0f) < 1e-5f);

    /* And the shift WRAPS rather than running off the end: magenta (300) + 60 is red (0),
       not 360. A hue of 360 is not invalid, but it would index past the last division. */
    for (size_t i = 0; i < t.v.size(); i += 3) t.v[i] = 90.0f;
    HueSatMap::FromHsv(300.0f, 1.0f, 1.0f, r, g, b);
    HueSatMap::Apply(t, r, g, b);
    HueSatMap::ToHsv(r, g, b, h, s, v);
    QVERIFY2(h < 180.0f, qPrintable(QString("hue %1 did not wrap").arg(double(h))));
    QVERIFY(qAbs(h - 30.0f) < 1e-3f);
}

/*
    THE HIGHLIGHT HEADROOM SURVIVES. DNG describes a display-referred pipeline where value
    lives in 0..1; Winnow's working data is SCENE-REFERRED and carries several stops above
    white, which is precisely what the view transform exists to roll off. Clamping value
    here would blow every specular highlight flat the moment a profile was selected -- and
    it would look like a property of the profile, not like a bug.
*/
void TstHueSatMap::highlightHeadroomSurvives()
{
    const HueSatMap::Table t = identity(4, 2, 1);
    float r = 6.0f, g = 5.0f, b = 4.0f;         // ~2.5 stops above white
    HueSatMap::Apply(t, r, g, b);
    QVERIFY2(qAbs(r - 6.0f) < 1e-4f,
             qPrintable(QString("over-white red came back as %1").arg(double(r))));
    QVERIFY(qAbs(g - 5.0f) < 1e-4f);
    QVERIFY(qAbs(b - 4.0f) < 1e-4f);
}

void TstHueSatMap::saturationIsClampedButValueIsNot()
{
    HueSatMap::Table t = identity(4, 2, 1);
    for (size_t i = 0; i < t.v.size(); i += 3) { t.v[i + 1] = 4.0f; t.v[i + 2] = 3.0f; }

    float r, g, b;
    HueSatMap::FromHsv(30.0f, 0.5f, 0.4f, r, g, b);
    HueSatMap::Apply(t, r, g, b);
    float h, s, v;
    HueSatMap::ToHsv(r, g, b, h, s, v);

    /* 0.5 x 4 = 2, clamped to 1: saturation is bounded by its own definition
       ((max-min)/max), and a value past 1 would produce a negative channel. */
    QVERIFY2(qAbs(s - 1.0f) < 1e-5f,
             qPrintable(QString("sat %1 -- should clamp at 1").arg(double(s))));
    QVERIFY(b >= 0.0f);
    /* 0.4 x 3 = 1.2, NOT clamped. */
    QVERIFY2(qAbs(v - 1.2f) < 1e-4f,
             qPrintable(QString("val %1 -- should not clamp").arg(double(v))));
}

void TstHueSatMap::blendEndpointsAndMidpoint()
{
    HueSatMap::Table warm = identity(4, 2, 1);
    HueSatMap::Table cool = identity(4, 2, 1);
    for (size_t i = 0; i < warm.v.size(); i += 3) { warm.v[i] = 10.0f; cool.v[i] = 30.0f; }

    HueSatMap::Table out;
    QVERIFY(HueSatMap::Blend(warm, cool, 0.0f, out));
    QVERIFY(qAbs(out.v[0] - 10.0f) < 1e-6f);
    QVERIFY(HueSatMap::Blend(warm, cool, 1.0f, out));
    QVERIFY(qAbs(out.v[0] - 30.0f) < 1e-6f);
    QVERIFY(HueSatMap::Blend(warm, cool, 0.25f, out));
    QVERIFY(qAbs(out.v[0] - 15.0f) < 1e-5f);

    /* Out-of-range weights clamp rather than extrapolating a fitted table. */
    QVERIFY(HueSatMap::Blend(warm, cool, 2.0f, out));
    QVERIFY(qAbs(out.v[0] - 30.0f) < 1e-6f);
}

void TstHueSatMap::blendTakesTheOtherWhenOneIsEmpty()
{
    HueSatMap::Table warm = identity(4, 2, 1);
    for (size_t i = 0; i < warm.v.size(); i += 3) warm.v[i] = 7.0f;
    const HueSatMap::Table none;

    HueSatMap::Table out;
    /* A single-illuminant profile: the one table it has stands at every temperature. */
    QVERIFY(HueSatMap::Blend(warm, none, 0.9f, out));
    QVERIFY(qAbs(out.v[0] - 7.0f) < 1e-6f);
    QVERIFY(HueSatMap::Blend(none, warm, 0.1f, out));
    QVERIFY(qAbs(out.v[0] - 7.0f) < 1e-6f);
    /* Neither: there is no table, and the caller must be told so rather than handed an
       empty one to apply. */
    QVERIFY(!HueSatMap::Blend(none, none, 0.5f, out));
    QVERIFY(out.isEmpty());
}

void TstHueSatMap::srgbEncodedTableRoundTrips()
{
    HueSatMap::Table t = identity(4, 2, 1);
    t.encoding = HueSatMap::kEncodingSrgb;

    /* An identity table must be identity whatever encoding it declares -- which is what
       proves the transfer curve and its inverse are actually inverses. */
    const float probes[][3] = {{0.8f, 0.2f, 0.1f}, {0.02f, 0.01f, 0.005f}, {0.5f, 0.5f, 0.5f}};
    for (const auto &p : probes) {
        float r = p[0], g = p[1], b = p[2];
        HueSatMap::Apply(t, r, g, b);
        QVERIFY2(qAbs(r - p[0]) < 1e-4f && qAbs(g - p[1]) < 1e-4f && qAbs(b - p[2]) < 1e-4f,
                 qPrintable(QString("sRGB-encoded identity moved (%1,%2,%3) to (%4,%5,%6)")
                     .arg(double(p[0])).arg(double(p[1])).arg(double(p[2]))
                     .arg(double(r)).arg(double(g)).arg(double(b))));
    }
}

/*
    END TO END ON REAL PROFILES: a camera neutral must still render neutral AFTER the
    HueSatMap, and the table must actually do something to a colour that is not neutral.

    The first half is a real invariant, not a coincidence: the saturation == 0 slice of
    every installed HueSatMap is exactly (sat 1.0, val 1.0), because a profile that tinted
    greys would be unusable. It is also the check that would catch the bracketing matrices
    being wrong -- a working -> ProPhoto -> working round trip that did not close would
    move the neutral even through an identity slice.
*/
void TstHueSatMap::neutralSurvivesARealProfile()
{
#ifdef Q_OS_MAC
    const QString root = "/Library/Application Support/Adobe/CameraRaw/CameraProfiles";
#else
    const QString root = "C:/ProgramData/Adobe/CameraRaw/CameraProfiles";
#endif
    if (!QDir(root).exists()) QSKIP("no camera profiles installed on this machine");

    QStringList paths;
    QDirIterator it(root, QStringList() << "*.dcp", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) paths << it.next();
    if (paths.isEmpty()) QSKIP("no camera profiles installed on this machine");
    paths.sort();

    const int stride = qMax(1, paths.size() / 120);
    int withTable = 0, movedAColour = 0;
    double worstNeutral = 0.0;

    for (int i = 0; i < paths.size(); i += stride) {
        Dcp::Profile p;
        if (!Dcp::parseFile(paths[i], p)) continue;

        CameraProfile::Tables t;
                if (!CameraProfile::tables(p, 5000.0f, t) || !t.active) continue;
        /* This test is about the BASELINE table, so skip the profiles that have none. */
        if (t.hueSatMap.isEmpty()) continue;
        ++withTable;

        auto through = [&t](float in[3], float out[3]) {
            float pr = t.toTable[0][0]*in[0] + t.toTable[0][1]*in[1] + t.toTable[0][2]*in[2];
            float pg = t.toTable[1][0]*in[0] + t.toTable[1][1]*in[1] + t.toTable[1][2]*in[2];
            float pb = t.toTable[2][0]*in[0] + t.toTable[2][1]*in[1] + t.toTable[2][2]*in[2];
            HueSatMap::Apply(t.hueSatMap, pr, pg, pb);
            out[0] = t.fromTable[0][0]*pr + t.fromTable[0][1]*pg + t.fromTable[0][2]*pb;
            out[1] = t.fromTable[1][0]*pr + t.fromTable[1][1]*pg + t.fromTable[1][2]*pb;
            out[2] = t.fromTable[2][0]*pr + t.fromTable[2][1]*pg + t.fromTable[2][2]*pb;
        };

        for (float grey : {0.18f, 0.5f, 1.0f}) {
            float in[3] = {grey, grey, grey}, out[3];
            through(in, out);
            for (int c = 0; c < 3; ++c)
                worstNeutral = qMax(worstNeutral, qAbs(double(out[c]) - double(grey)));
        }

        float colour[3] = {0.62f, 0.21f, 0.17f}, moved[3];      // a saturated red
        through(colour, moved);
        double d = 0.0;
        for (int c = 0; c < 3; ++c) d = qMax(d, qAbs(double(moved[c]) - double(colour[c])));
        if (d > 1e-4) ++movedAColour;
    }

    qInfo() << "profiles with a HueSatMap:" << withTable << "; worst neutral drift"
            << worstNeutral << "; moved a saturated colour:" << movedAColour;

    QVERIFY2(withTable > 0, "no installed profile carried a HueSatMap -- nothing was tested");
    QVERIFY2(worstNeutral < 1e-5,
             qPrintable(QString("a real profile tinted a neutral by %1").arg(worstNeutral)));
    /* If NO profile moved a saturated colour the table is being read but not applied,
       which the neutral test above would happily pass. */
    QVERIFY2(movedAColour > withTable / 2,
             qPrintable(QString("only %1 of %2 profiles changed a saturated colour")
                            .arg(movedAColour).arg(withTable)));
}

/*
    THE TABLED sRGB TRANSFER MUST BE THE CURVE IT REPLACED.

    The exact formula was swapped for a 4096-entry table because the round trip was six
    pow() calls per pixel and measured as HALF the cost of the whole profile stage. A
    table is only a valid substitute while it agrees with the function, and it is the toe
    -- where the curve is steep and a 16-bit level is small -- that a coarse table would
    get wrong first.
*/
void TstHueSatMap::srgbTransferTableMatchesTheExactCurve()
{
    double worstEnc = 0.0, worstDec = 0.0;
    for (int i = 0; i <= 20000; ++i) {
        const float v = float(i) / 20000.0f;
        worstEnc = qMax(worstEnc, qAbs(double(HueSatMap::SrgbEncode(v))
                                     - double(HueSatMap::SrgbEncodeExact(v))));
        worstDec = qMax(worstDec, qAbs(double(HueSatMap::SrgbDecode(v))
                                     - double(HueSatMap::SrgbDecodeExact(v))));
    }
    /* THE BAR IS ONE 16-BIT LEVEL (1.53e-5) -- the accuracy the rest of Develop's colour
       invariants are asserted at -- not a number chosen to fit the result. Measured at the
       table's current size: 2.4e-6 encoding, and less decoding. At 4096 samples encoding
       measured 1.6e-5 and would fail this, which is how the size was chosen. */
    QVERIFY2(worstEnc < 1.0e-5, qPrintable(QString("encode worst %1").arg(worstEnc)));
    QVERIFY2(worstDec < 1.0e-5, qPrintable(QString("decode worst %1").arg(worstDec)));

    /* Above white the table does not apply at all -- the exact formula is used, so the
       headroom the rest of this file preserves is not quietly clamped by a lookup. */
    for (float v : {1.0f, 1.5f, 4.0f, 9.0f}) {
        QCOMPARE(HueSatMap::SrgbEncode(v), HueSatMap::SrgbEncodeExact(v));
        QCOMPARE(HueSatMap::SrgbDecode(v), HueSatMap::SrgbDecodeExact(v));
    }
    /* ...and over-white really does stay over white. Tested ABOVE 1.0, not at it: the
       formula evaluates to 1.0 there only to within a float ULP (1.055f - 0.055f is
       0.99999994), and asserting exactness on that would be testing the arithmetic, not
       the headroom. */
    for (float v : {1.001f, 1.5f, 4.0f, 9.0f}) {
        QVERIFY2(HueSatMap::SrgbEncode(v) > 1.0f,
                 qPrintable(QString("%1 encoded to %2").arg(double(v))
                                .arg(double(HueSatMap::SrgbEncode(v)))));
        QVERIFY(HueSatMap::SrgbDecode(v) > 1.0f);
    }
}

/* ------------------------------------------------------------------------------------
   The look half: ProfileToneCurve, and the switch that separates it from the
   characterisation
   ------------------------------------------------------------------------------------ */

void TstHueSatMap::toneCurveIdentityIsIdentity()
{
    ProfileTone::Lut l;
    QVERIFY(ProfileTone::Build({0.0f, 0.0f, 1.0f, 1.0f}, l));
    for (float v : {0.0f, 0.05f, 0.18f, 0.5f, 0.9f, 1.0f})
        QVERIFY2(qAbs(l.eval(v) - v) < 1e-5f,
                 qPrintable(QString("diagonal moved %1 to %2").arg(double(v)).arg(double(l.eval(v)))));
    QVERIFY(qAbs(l.endSlope - 1.0f) < 1e-3f);
}

/*
    A curve run independently on R, G and B pulls colour toward the primaries as it
    steepens -- the familiar per-channel-curve hue shift, and a real defect in a control
    whose whole job is tone. Winnow applies it to the VALUE and scales the triple, so hue
    and saturation come through untouched.
*/
void TstHueSatMap::toneCurveHoldsHueAndSaturation()
{
    /* A pronounced S-curve: darkens the low end, lifts the high end. */
    ProfileTone::Lut l;
    QVERIFY(ProfileTone::Build({0.0f, 0.0f, 0.25f, 0.15f, 0.5f, 0.5f,
                                0.75f, 0.87f, 1.0f, 1.0f}, l));

    const float probes[][3] = {{0.60f, 0.22f, 0.18f}, {0.20f, 0.55f, 0.30f},
                               {0.15f, 0.30f, 0.80f}, {0.40f, 0.40f, 0.40f}};
    for (const auto &probe : probes) {
        float r = probe[0], g = probe[1], b = probe[2];
        float h0, s0, v0;
        HueSatMap::ToHsv(r, g, b, h0, s0, v0);

        ProfileTone::ApplyToValue(l, r, g, b);

        float h1, s1, v1;
        HueSatMap::ToHsv(r, g, b, h1, s1, v1);
        QVERIFY2(qAbs(h1 - h0) < 1e-3f,
                 qPrintable(QString("hue moved %1 -> %2").arg(double(h0)).arg(double(h1))));
        QVERIFY2(qAbs(s1 - s0) < 1e-4f,
                 qPrintable(QString("saturation moved %1 -> %2").arg(double(s0)).arg(double(s1))));
        QVERIFY2(qAbs(v1 - l.eval(v0)) < 1e-4f, "value did not follow the curve");
    }

    /* ...and it really is a curve, not a no-op. */
    QVERIFY(l.eval(0.25f) < 0.20f);
    QVERIFY(l.eval(0.75f) > 0.80f);
}

/*
    ABOVE WHITE THE CURVE CONTINUES, it does not stop. A ProfileToneCurve is defined over
    0..1 because DNG describes a display-referred pipeline; Winnow's data is scene-referred
    and carries the headroom the view transform exists to roll off. Clamping at the top
    would flatten every specular highlight the moment a look was switched on -- and it
    would read as a property of the profile rather than as a bug.
*/
void TstHueSatMap::toneCurveExtendsAboveWhiteInsteadOfClamping()
{
    ProfileTone::Lut l;
    QVERIFY(ProfileTone::Build({0.0f, 0.0f, 0.5f, 0.42f, 1.0f, 1.0f}, l));

    const float atWhite = l.eval(1.0f);
    QVERIFY(qAbs(atWhite - 1.0f) < 1e-4f);

    /* Strictly increasing well past white, and by the slope the curve ended with. */
    float prev = atWhite;
    for (float v : {1.5f, 2.0f, 4.0f, 8.0f}) {
        const float y = l.eval(v);
        QVERIFY2(y > prev, qPrintable(QString("curve stopped rising at %1").arg(double(v))));
        QVERIFY2(qAbs(y - (atWhite + (v - 1.0f) * l.endSlope)) < 1e-3f,
                 "the extension is not the end slope");
        prev = y;
    }

    /* And a pixel 2.5 stops over white keeps its headroom through the whole apply. */
    float r = 6.0f, g = 5.0f, b = 4.0f;
    ProfileTone::ApplyToValue(l, r, g, b);
    QVERIFY2(r > 4.0f, qPrintable(QString("over-white red collapsed to %1").arg(double(r))));
}

void TstHueSatMap::toneCurveRejectsUnusableData()
{
    ProfileTone::Lut l;
    QVERIFY(!ProfileTone::Build({}, l));                            // absent
    QVERIFY(!ProfileTone::Build({0.0f, 0.0f}, l));                  // one point
    QVERIFY(!ProfileTone::Build({0.0f, 0.0f, 1.0f}, l));            // odd count
    QVERIFY(!ProfileTone::Build({0.0f, 0.0f, 0.7f, 0.5f, 0.3f, 0.9f}, l));   // x goes back

    /* A rejected curve must leave the Lut empty and inert, not half-built: the render
       applies no curve rather than a broken one. */
    QVERIFY(l.isEmpty());
    float r = 0.4f, g = 0.3f, b = 0.2f;
    ProfileTone::ApplyToValue(l, r, g, b);
    QVERIFY(qAbs(r - 0.4f) < 1e-6f && qAbs(g - 0.3f) < 1e-6f && qAbs(b - 0.2f) < 1e-6f);
}

/*
    A "CAMERA BASE" IS A PROFILE WITH ITS LOOK REMOVED, and the two halves have to come
    apart cleanly at the point the store strips them.

    There is no longer a switch in the pipeline -- a profile is applied whole, and the base
    is a synthesised profile that simply has no look. So what is pinned here is the
    equivalence that makes that substitution valid: stripping the look tags must leave the
    characterisation (matrix + HueSatMap) untouched, and must remove ALL THREE creative
    pieces -- the table, the curve, and the exposure offset. Leaving the offset behind is
    the easy one to miss, and it is the one that would move the picture's brightness.
*/
void TstHueSatMap::lookStripsCleanlyFromTheCharacterisation()
{
#ifdef Q_OS_MAC
    const QString root = "/Library/Application Support/Adobe/CameraRaw/CameraProfiles";
#else
    const QString root = "C:/ProgramData/Adobe/CameraRaw/CameraProfiles";
#endif
    if (!QDir(root).exists()) QSKIP("no camera profiles installed on this machine");

    QStringList paths;
    QDirIterator it(root, QStringList() << "*.dcp", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) paths << it.next();
    if (paths.isEmpty()) QSKIP("no camera profiles installed on this machine");
    paths.sort();

    int withLook = 0, withCurve = 0, withOffset = 0;
    const int stride = qMax(1, paths.size() / 120);
    for (int i = 0; i < paths.size(); i += stride) {
        Dcp::Profile p;
        if (!Dcp::parseFile(paths[i], p)) continue;

        CameraProfile::Tables whole;
        if (!CameraProfile::tables(p, 5000.0f, whole)) continue;
        if (!whole.lookTable.isEmpty()) ++withLook;
        if (!whole.toneCurve.isEmpty()) ++withCurve;
        if (!qFuzzyCompare(whole.exposureScale, 1.0f)) ++withOffset;

        /* The strip the store performs for a "Camera Base". */
        Dcp::Profile base = p;
        base.lookTable = Dcp::Table3D();
        base.toneCurve.clear();
        base.baselineExposureOffset = 0.0f;

        CameraProfile::Tables stripped;
        const bool any = CameraProfile::tables(base, 5000.0f, stripped);

        QVERIFY2(stripped.lookTable.isEmpty(), "a look table survived the strip");
        QVERIFY2(stripped.toneCurve.isEmpty(), "a tone curve survived the strip");
        QVERIFY2(qFuzzyCompare(stripped.exposureScale, 1.0f),
                 "an exposure offset survived the strip -- the base would change brightness");

        /* ...and the characterisation is the SAME table, not merely present. */
        if (any && !whole.hueSatMap.isEmpty()) {
            QCOMPARE(stripped.hueSatMap.v.size(), whole.hueSatMap.v.size());
            for (size_t k = 0; k < stripped.hueSatMap.v.size(); ++k)
                QVERIFY(qAbs(stripped.hueSatMap.v[k] - whole.hueSatMap.v[k]) < 1e-6f);
        }
        /* A profile whose only per-pixel content was its look has nothing left, which is
           exactly the common "Camera *" case: matrix only, and the stage is skipped. */
        if (whole.hueSatMap.isEmpty()) QVERIFY(!any);
    }

    qInfo() << "applied whole --" << withLook << "look tables," << withCurve
            << "tone curves," << withOffset << "exposure offsets";
    QVERIFY(withLook > 0);
    QVERIFY(withCurve > 0);
    QVERIFY(withOffset > 0);
}

QTEST_MAIN(TstHueSatMap)
#include "tst_huesatmap.moc"
