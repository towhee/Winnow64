#include <QtTest>
#include <cmath>
#include "Develop/pointcolor.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"

/*
    Point Color (Develop/pointcolor.h) -- the per-point selection + shift behind the
    Point Color panel, and the string encoding the sidecar, presets and multi-image edit
    share. These pin the properties that are easy to break and invisible in review: the
    OKLab round trip is lossless (so an untouched pixel is not moved by the trip), the
    sample selects itself fully and does not reach the opposite hue or a neutral, the
    panel's drag inverses agree with the render's forward shifts, and the encoding and
    the "no key when no points" rule that keeps every older sidecar byte-identical.
*/
class tst_pointcolor : public QObject
{
    Q_OBJECT

    /* A sample's colour as the pipeline sees it: display-linear, working primaries. */
    static void sampleToWorking(const PointColor::Point &p, float v[3])
    {
        const float lin[3] = {PointColor::srgbDecode(p.r), PointColor::srgbDecode(p.g),
                              PointColor::srgbDecode(p.b)};
        float m[3][3];
        ColorSpaceMath::matrixF(ColorSpaceMath::ColorSpace::LinearSRGB,
                                ColorSpaceMath::kWorking, m);
        for (int r = 0; r < 3; ++r)
            v[r] = m[r][0] * lin[0] + m[r][1] * lin[1] + m[r][2] * lin[2];
    }

    static float weightOf(const PointColor::Coef &k, const float toLms[9], const float v[3])
    {
        float lab[3];
        PointColor::toLab(toLms, v, lab);
        return PointColor::weight(k, lab[0], std::hypot(lab[1], lab[2]),
                                  std::atan2(lab[2], lab[1]));
    }

    static PointColor::Point orange()
    {
        PointColor::Point p;
        p.r = 0.85f; p.g = 0.45f; p.b = 0.20f;
        return p;
    }

private slots:
    void labRoundTripIsLossless();
    void sampleSelectsItself();
    void oppositeHueAndNeutralAreNotSelected();
    void inertPointMovesNothing();
    void dragInversesMatchForwardShifts();
    void encodeDecodeRoundTrip();
    void decodeClampsAndDropsMalformed();
    void sidecarOmitsKeyWithoutPoints();
    void paramsGroupAndIdentity();
    void addedSampleWidensHue();
    void envelopeWrapsAcrossPi();
    void encodeDecodeAddedSamples();
};

void tst_pointcolor::labRoundTripIsLossless()
{
    float to[9], from[9];
    PointColor::workingToLms(to);
    PointColor::lmsToWorking(from);
    const float cases[][3] = {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.18f, 0.18f, 0.18f},
                              {0.9f, 0.1f, 0.05f}, {0.02f, 0.4f, 0.8f}, {0.5f, 0.6f, 0.1f}};
    for (const auto &c : cases) {
        float lab[3], back[3];
        PointColor::toLab(to, c, lab);
        PointColor::fromLab(from, lab, back);
        for (int k = 0; k < 3; ++k) QVERIFY(std::fabs(back[k] - c[k]) < 2e-4f);
    }
    /* White is L = 1 and neutral, whatever the working primaries. */
    const float white[3] = {1.0f, 1.0f, 1.0f};
    float lab[3];
    PointColor::toLab(to, white, lab);
    QVERIFY(std::fabs(lab[0] - 1.0f) < 1e-3f);
    QVERIFY(std::hypot(lab[1], lab[2]) < 1e-3f);
}

void tst_pointcolor::sampleSelectsItself()
{
    float to[9];
    PointColor::workingToLms(to);
    const PointColor::Point p = orange();
    const PointColor::Coef k = PointColor::makeCoef(p);
    float v[3];
    sampleToWorking(p, v);
    QVERIFY(std::fabs(weightOf(k, to, v) - 1.0f) < 1e-3f);
}

void tst_pointcolor::oppositeHueAndNeutralAreNotSelected()
{
    float to[9];
    PointColor::workingToLms(to);
    PointColor::Point p = orange();
    p.range = 100.0f;                        // even at the widest overall range
    const PointColor::Coef k = PointColor::makeCoef(p);
    PointColor::Point blue;
    blue.r = 0.20f; blue.g = 0.45f; blue.b = 0.85f;
    float v[3];
    sampleToWorking(blue, v);
    QCOMPARE(weightOf(k, to, v), 0.0f);
    PointColor::Point grey;
    grey.r = grey.g = grey.b = 0.55f;
    sampleToWorking(grey, v);
    QCOMPARE(weightOf(k, to, v), 0.0f);
}

void tst_pointcolor::inertPointMovesNothing()
{
    float to[9];
    PointColor::workingToLms(to);
    const PointColor::Point p = orange();
    QVERIFY(p.isInert());
    const PointColor::Coef k = PointColor::makeCoef(p);
    float v[3], lab[3];
    sampleToWorking(p, v);
    PointColor::toLab(to, v, lab);
    float d[3] = {0.0f, 0.0f, 0.0f};
    const float w = PointColor::accumulate(k, lab, std::hypot(lab[1], lab[2]),
                                           std::atan2(lab[2], lab[1]), d);
    QVERIFY(w > 0.99f);
    for (float x : d) QVERIFY(std::fabs(x) < 1e-6f);
}

void tst_pointcolor::dragInversesMatchForwardShifts()
{
    PointColor::Point p = orange();
    float L, C, h;
    PointColor::sampleLch(p, L, C, h);
    for (float s : {-100.0f, -40.0f, 0.0f, 25.0f, 100.0f}) {
        p.hueShift = p.satShift = p.lumShift = s;
        QVERIFY(std::fabs(PointColor::hueShiftFor(h, PointColor::shiftedHue(p, h)) - s) < 0.01f);
        QVERIFY(std::fabs(PointColor::satShiftFor(C, PointColor::shiftedChroma(p, C)) - s)
                < 0.01f);
        QVERIFY(std::fabs(PointColor::lumShiftFor(L, PointColor::shiftedLightness(p, L)) - s)
                < 0.01f);
    }
}

void tst_pointcolor::encodeDecodeRoundTrip()
{
    PointColor::Point pts[PointColor::kMaxPts];
    pts[0] = orange();
    pts[0].hueShift = -15; pts[0].satShift = -11; pts[0].lumShift = 29;
    pts[0].variance = 47;  pts[0].range = 63;
    pts[1].r = 0.1f; pts[1].g = 0.5f; pts[1].b = 0.2f;
    pts[1].hueRange = 80; pts[1].satRange = 10; pts[1].lumRange = 0;
    const QString enc = PointColor::encode(2, pts);
    int n = 0;
    PointColor::Point out[PointColor::kMaxPts];
    out[5].hueShift = 99;                     // stale tail must be cleared
    QVERIFY(PointColor::decode(enc, n, out));
    QCOMPARE(n, 2);
    for (int i = 0; i < PointColor::kMaxPts; ++i) QVERIFY(out[i] == pts[i]);
    QCOMPARE(PointColor::encode(0, pts), QString());
    QVERIFY(PointColor::decode(QString(), n, out));
    QCOMPARE(n, 0);
}

void tst_pointcolor::decodeClampsAndDropsMalformed()
{
    int n = 0;
    PointColor::Point out[PointColor::kMaxPts];
    /* Group 1 out of range everywhere, group 2 short, group 3 not numbers. */
    const QString s = "2,-1,0.5,500,-500,0,0,150,50,50,50;1,2,3;a,b,c,d,e,f,g,h,i,j,k";
    QVERIFY(!PointColor::decode(s, n, out));
    QCOMPARE(n, 1);
    QCOMPARE(out[0].r, 1.0f);
    QCOMPARE(out[0].g, 0.0f);
    QCOMPARE(out[0].hueShift, 100.0f);
    QCOMPARE(out[0].satShift, -100.0f);
    QCOMPARE(out[0].range, 100.0f);
    /* A clean set is left alone. */
    PointColor::Point pts[PointColor::kMaxPts];
    pts[0] = orange();
    int m = 1;
    QVERIFY(!PointColor::sanitize(m, pts));
}

void tst_pointcolor::sidecarOmitsKeyWithoutPoints()
{
    EditParams p;
    QVERIFY(!EditStack::paramsToJson(p).contains("pointColors"));
    p.pointColor[0] = orange();
    p.pointColor[0].lumShift = 20;
    p.pointColorN = 1;
    p.pointColorVisualize = 0;                // view state: never serialised
    const QJsonObject o = EditStack::paramsToJson(p);
    QVERIFY(o.contains("pointColors"));
    EditParams q = EditStack::paramsFromJson(o);
    QCOMPARE(q.pointColorN, 1);
    QVERIFY(q.pointColor[0] == p.pointColor[0]);
    QCOMPARE(q.pointColorVisualize, -1);
}

void tst_pointcolor::paramsGroupAndIdentity()
{
    EditParams p;
    QVERIFY(p.isIdentity());
    QVERIFY(EditParams::groupIsDefault(p, EditParams::Group::PointColor));
    p.pointColor[0] = orange();               // a bare sample is still the user's work
    p.pointColorN = 1;
    QVERIFY(!p.isIdentity());
    QVERIFY(p.pointColorIsIdentity());        // ...but it moves no pixel
    QVERIFY(!EditParams::groupIsDefault(p, EditParams::Group::PointColor));
    EditParams::resetGroup(p, EditParams::Group::PointColor);
    QVERIFY(p == EditParams());
}

/* A second sample a hue step away fully selects everything between the two -- which a
   one-sample point at the same Range reaches only partly -- and nothing beyond the
   falloff past either end. */
void tst_pointcolor::addedSampleWidensHue()
{
    float to[9];
    PointColor::workingToLms(to);
    PointColor::Point one = orange();
    one.range = 25.0f;                        // narrow, so the gap is outside its reach
    PointColor::Point two = one;
    QVERIFY(two.addSample(0.85f, 0.30f, 0.45f));   // a pinker red
    QCOMPARE(two.sampleCount(), 2);
    const PointColor::Coef k1 = PointColor::makeCoef(one);
    const PointColor::Coef k2 = PointColor::makeCoef(two);
    /* Both samples are fully selected by the two-sample point. */
    PointColor::Point s2;
    s2.r = 0.85f; s2.g = 0.30f; s2.b = 0.45f;
    float v[3];
    sampleToWorking(one, v);
    QVERIFY(std::fabs(weightOf(k2, to, v) - 1.0f) < 1e-3f);
    sampleToWorking(s2, v);
    QVERIFY(std::fabs(weightOf(k2, to, v) - 1.0f) < 1e-3f);
    QVERIFY(weightOf(k1, to, v) < 0.5f);      // the one-sample point barely reaches it
    /* The opposite hue is still untouched. */
    PointColor::Point blue;
    blue.r = 0.20f; blue.g = 0.45f; blue.b = 0.85f;
    sampleToWorking(blue, v);
    QCOMPARE(weightOf(k2, to, v), 0.0f);
    /* One sample's envelope is the sample: the coefficients are unchanged. */
    const PointColor::Envelope e = PointColor::envelope(one);
    float L, C, h;
    PointColor::sampleLch(one, L, C, h);
    QCOMPARE(e.hh, 0.0f);
    QCOMPARE(e.Lmin, L);
    QCOMPARE(e.Cmax, C);
    QCOMPARE(e.hc, h);
    two.removeLastSample();
    QVERIFY(two == one);
}

/* Samples either side of the -pi/+pi seam span the short arc through it, not the long
   way round. */
void tst_pointcolor::envelopeWrapsAcrossPi()
{
    PointColor::Point p;
    /* Two blue-greens straddling OKLab's h = +/-pi (a < 0, b ~ 0). */
    float r, g, b;
    PointColor::lchToSrgb(0.6f, 0.08f, PointColor::kPi - 0.15f, r, g, b);
    p.r = r; p.g = g; p.b = b;
    PointColor::lchToSrgb(0.6f, 0.08f, -PointColor::kPi + 0.15f, r, g, b);
    QVERIFY(p.addSample(r, g, b));
    const PointColor::Envelope e = PointColor::envelope(p);
    QVERIFY(e.hueGate);
    QVERIFY(e.hh < 0.25f);
    QVERIFY(std::fabs(std::fabs(e.hc) - PointColor::kPi) < 0.05f);
}

void tst_pointcolor::encodeDecodeAddedSamples()
{
    PointColor::Point pts[PointColor::kMaxPts];
    pts[0] = orange();
    pts[0].addSample(0.9f, 0.3f, 0.4f);
    pts[0].addSample(0.6f, 0.35f, 0.2f);
    pts[1].r = 0.1f; pts[1].g = 0.5f; pts[1].b = 0.2f;
    const QString enc = PointColor::encode(2, pts);
    /* The one-sample point's group keeps its eleven fields. */
    QCOMPARE(enc.split(';').at(0).split(',').size(), PointColor::kFields + 6);
    QCOMPARE(enc.split(';').at(1).split(',').size(), PointColor::kFields);
    int n = 0;
    PointColor::Point out[PointColor::kMaxPts];
    QVERIFY(PointColor::decode(enc, n, out));
    QCOMPARE(n, 2);
    for (int i = 0; i < PointColor::kMaxPts; ++i) QVERIFY(out[i] == pts[i]);
    /* A trailing partial triple is malformed: the group is dropped. */
    QVERIFY(!PointColor::decode("0.5,0.4,0.3,0,0,0,0,50,50,50,50,0.1,0.2", n, out));
    QCOMPARE(n, 0);
    /* A stale tail past extraN is cleared by sanitize. */
    PointColor::Point s[PointColor::kMaxPts];
    s[0] = orange();
    s[0].extra[5] = 0.7f;
    int m = 1;
    QVERIFY(PointColor::sanitize(m, s));
    QCOMPARE(s[0].extra[5], 0.0f);
}

QTEST_APPLESS_MAIN(tst_pointcolor)
#include "tst_pointcolor.moc"
