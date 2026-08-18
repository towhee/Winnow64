/*
    OutputTransform::ToImage -- the float -> 8-bit pack, and the transfer LUT that
    replaced its per-pixel pow.

    WHY THIS TEST EXISTS. The sRGB path is a TABLE plus linear interpolation, not the
    exact maths: it was ~90% of an interactive Develop tick (35 ms of 38 ms on a 5.9 MP
    proxy) and the table is ~4.8x faster. That trade is deliberate but it is also silent
    -- nothing on screen would show the table drifting -- so the bound it was accepted
    under is asserted here rather than left as a comment: every output byte within 1 of
    the exact computation, and only a hair of them off at all.

    The reference below re-implements the exact chain independently of the code under
    test, so a change to either side has to be made twice to pass.
*/
#include <QtTest>
#include <cmath>
#include <vector>
#include "Develop/outputtransform.h"
#include "Develop/workingimage.h"

namespace {

float Clamp01Ref(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/* Linear -> sRGB transfer (IEC 61966-2-1). */
float SrgbGammaRef(float v)
{
    v = Clamp01Ref(v);
    return v <= 0.0031308f ? 12.92f * v
                           : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

/* ACES (Narkowicz) shoulder after a fixed +0.68 EV lift; scene-referred input only. */
float BaselineToneRef(float v)
{
    v *= 1.6f;
    if (v < 0.0f) v = 0.0f;
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return Clamp01Ref((v * (a * v + b)) / (v * (c * v + d) + e));
}

int ExactByte(float v, bool sceneReferred)
{
    return int(std::lround(SrgbGammaRef(sceneReferred ? BaselineToneRef(v)
                                                      : Clamp01Ref(v)) * 255.0f));
}

/* A ramp across the whole domain the LUT covers, laid out so consecutive pixels differ
   (the table is indexed per channel, so the three channels sample different points). */
WorkingImage makeRamp(int w, int h, float vMax, bool sceneReferred)
{
    WorkingImage img;
    img.width = w;
    img.height = h;
    img.white = 1.0f;
    img.sceneReferred = sceneReferred;
    img.rgb.resize(size_t(w) * size_t(h) * 3);
    const size_t n = size_t(w) * size_t(h);
    for (size_t i = 0; i < n; ++i) {
        const float t = float(i) / float(n - 1);
        img.rgb[i * 3 + 0] = vMax * t;
        img.rgb[i * 3 + 1] = vMax * (1.0f - t);
        img.rgb[i * 3 + 2] = vMax * std::fabs(0.5f - t) * 2.0f;
    }
    return img;
}

} // namespace

class TestOutputTransform : public QObject
{
    Q_OBJECT

private slots:
    void sceneReferredMatchesExactWithinOne();
    void displayReferredMatchesExactWithinOne();
    void blackAndWhiteAreExact();
    void overRangeSaturatesToWhite();

private:
    /* Renders `img` and compares every byte with the reference. Returns the worst
       absolute difference; `offBy` counts the bytes that differ at all. */
    static int compare(const WorkingImage &img, qint64 &offBy, qint64 &total);
};

int TestOutputTransform::compare(const WorkingImage &img, qint64 &offBy, qint64 &total)
{
    QImage out;
    OutputTransform t;
    if (!t.ToImage(img, out)) return -1;

    int worst = 0;
    offBy = 0;
    total = 0;
    for (int y = 0; y < img.height; ++y) {
        const uchar *line = out.constScanLine(y);
        for (int x = 0; x < img.width; ++x) {
            const size_t o = (size_t(y) * size_t(img.width) + size_t(x)) * 3;
            for (int c = 0; c < 3; ++c) {
                const int got = line[x * 3 + c];
                const int want = ExactByte(img.rgb[o + c], img.sceneReferred);
                const int d = std::abs(got - want);
                if (d > worst) worst = d;
                if (d) ++offBy;
                ++total;
            }
        }
    }
    return worst;
}

/* RAW (scene-referred): the baseline tone curve runs, so the rolloff can never fire and
   EVERY pixel goes through the table. Domain covers BaselineTone's saturation at 4.53. */
void TestOutputTransform::sceneReferredMatchesExactWithinOne()
{
    const WorkingImage img = makeRamp(512, 384, 5.0f, /*sceneReferred*/true);
    qint64 offBy = 0, total = 0;
    const int worst = compare(img, offBy, total);
    QVERIFY(worst >= 0);                       // render succeeded
    QVERIFY2(worst <= 1, qPrintable(QString("worst delta %1").arg(worst)));
    /* The bound the table was accepted under; measured ~0.001%, so 0.05% is loose enough
       to absorb platform pow differences and tight enough to catch a real drift. */
    QVERIFY2(offBy * 10000 <= total * 5,
             qPrintable(QString("%1 of %2 bytes differ").arg(offBy).arg(total)));
}

/* Display-referred (JPEG/TIFF): no baseline curve, so a channel CAN exceed 1 and the
   cross-channel rolloff takes over. Values stay in range here so the table is exercised;
   the over-range fallback is covered by overRangeSaturatesToWhite. */
void TestOutputTransform::displayReferredMatchesExactWithinOne()
{
    const WorkingImage img = makeRamp(512, 384, 1.0f, /*sceneReferred*/false);
    qint64 offBy = 0, total = 0;
    const int worst = compare(img, offBy, total);
    QVERIFY(worst >= 0);
    QVERIFY2(worst <= 1, qPrintable(QString("worst delta %1").arg(worst)));
    QVERIFY2(offBy * 10000 <= total * 5,
             qPrintable(QString("%1 of %2 bytes differ").arg(offBy).arg(total)));
}

/* The two ends must be EXACT, not merely within one: a black frame that renders as 1, or
   a white one as 254, is visible. Both are the table's clamped edges. */
void TestOutputTransform::blackAndWhiteAreExact()
{
    WorkingImage img;
    img.width = 4;
    img.height = 1;
    img.white = 1.0f;
    img.sceneReferred = true;
    img.rgb = { 0.0f, 0.0f, 0.0f,        // black
                -1.0f, -1.0f, -1.0f,     // negative: clamps to black
                4.6f, 4.6f, 4.6f,        // the table's last entry
                1e30f, 1e30f, 1e30f };   // far past it
    QImage out;
    OutputTransform t;
    QVERIFY(t.ToImage(img, out));
    const uchar *line = out.constScanLine(0);
    QCOMPARE(int(line[0]), 0);
    QCOMPARE(int(line[3]), 0);
    QCOMPARE(int(line[6]), 255);
    QCOMPARE(int(line[9]), 255);
}

/* Display-referred over-range takes the exact rolloff branch, not the table. */
void TestOutputTransform::overRangeSaturatesToWhite()
{
    WorkingImage img;
    img.width = 1;
    img.height = 1;
    img.white = 1.0f;
    img.sceneReferred = false;
    img.rgb = { 0.5f, 2.0f, 0.25f };     // green over range: bleeds into red and blue
    QImage out;
    OutputTransform t;
    QVERIFY(t.ToImage(img, out));
    const uchar *line = out.constScanLine(0);
    QCOMPARE(int(line[1]), 255);                 // the over-range channel clips
    QVERIFY(int(line[0]) == 255);                // and bleeds the excess into the others
    QVERIFY(int(line[2]) == 255);
}

QTEST_MAIN(TestOutputTransform)
#include "tst_outputtransform.moc"
