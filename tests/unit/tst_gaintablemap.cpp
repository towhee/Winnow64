/*
    The DNG PROFILE GAIN TABLE MAP (Develop/gaintablemap.h) and where Develop applies it.

    WHY THIS TEST EXISTS. iPhone ProRAW pairs a +5.16 EV BaselineExposure with a gain
    table map that pulls the highlights back; either without the other renders the picture
    blown out. Three things have to hold and none of them fails loudly:

      - the PARSE: the tag is UNDEFINED bytes in the file's own byte order, in two layouts
        (version 1: float gains; version 2: uint8 / uint16 / half / float with a gamma);
      - the LOOKUP: pixel-centred relative positions, edge replication outside the grid,
        index t * N, bilinear across the four surrounding tables;
      - the PLACEMENT: applied exactly ONCE, when pixels leave camera-native
        (Develop::ToWorkingSpace), after the file's BaselineExposure, and even for an
        identity edit -- it is not an edit.
*/

#include <QtTest>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <vector>

#include "Develop/develop.h"
#include "Develop/editparams.h"
#include "Develop/gaintablemap.h"
#include "Develop/workingimage.h"

namespace {

/* Serialise a map in either byte order and either layout. v2 with dataType 0 stores
   gains as uint8 between gainMin and gainMax. */
QByteArray makeMap(bool big, bool v2, int V, int H, int N,
                   double spV, double spH, double orV, double orH,
                   const float w[5], const std::vector<float> &gains,
                   int dataType = 3, float gamma = 1.0f,
                   float gainMin = 0.0f, float gainMax = 1.0f)
{
    QByteArray d;
    auto put32 = [&](quint32 v) {
        char b[4];
        if (big) qToBigEndian(v, b); else qToLittleEndian(v, b);
        d.append(b, 4);
    };
    auto putF = [&](float f) { quint32 u; std::memcpy(&u, &f, 4); put32(u); };
    auto putD = [&](double v) {
        quint64 u; std::memcpy(&u, &v, 8);
        char b[8];
        if (big) qToBigEndian(u, b); else qToLittleEndian(u, b);
        d.append(b, 8);
    };
    put32(quint32(V)); put32(quint32(H));
    putD(spV); putD(spH); putD(orV); putD(orH);
    put32(quint32(N));
    for (int i = 0; i < 5; ++i) putF(w[i]);
    if (v2) { put32(quint32(dataType)); putF(gamma); putF(gainMin); putF(gainMax); }
    for (float g : gains) {
        if (v2 && dataType == 0)
            d.append(char(qRound((g - gainMin) / (gainMax - gainMin) * 255.0f)));
        else putF(g);
    }
    return d;
}

const float kLumWeights[5] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f};   // t = green (RIMM)

/* A camera-native image whose input profile is the identity -- so the working-space
   pixels are exactly the camera values -- with a gain map stamped on, as the DNG decode
   stamps it. */
WorkingImage makeImage(int w, int h, float v, std::shared_ptr<const GainTableMap> map)
{
    WorkingImage img;
    img.width = w;
    img.height = h;
    img.white = 1.0f;
    img.sceneReferred = true;
    img.space = ColorSpaceMath::ColorSpace::CameraNative;
    img.cam.valid = true;
    img.cam.cameraModel = "Test DNG";
    img.cam.hasBaselineExposure = true;     // 0 EV: keep the per-model table out of it
    img.cam.baselineExposure = 0.0f;
    img.cam.gainMap = std::move(map);
    img.rgb.assign(size_t(w) * size_t(h) * 3, v);
    return img;
}

std::shared_ptr<const GainTableMap> constantMap(float gain)
{
    std::vector<float> g(2 * 2 * 4, gain);
    GainTableMap m;
    const bool ok = GainTableMap::Parse(
        makeMap(true, false, 2, 2, 4, 1.0, 1.0, 0.0, 0.0, kLumWeights, g), true, false, m);
    Q_ASSERT(ok);
    return std::make_shared<const GainTableMap>(m);
}

} // namespace

class TstGainTableMap : public QObject
{
    Q_OBJECT
private slots:
    void parsesVersion1BigEndian();
    void parsesVersion2Uint8LittleEndian();
    void rejectsMalformed();
    void lookupInterpolatesAndReplicatesEdges();
    void appliedOnceWhenLeavingCameraNative();
    void appliedEvenForAnIdentityEdit();
    void fileBaselineExposureReplacesTheTable();
    void positionsAreRelativeToTheFrame();
};

void TstGainTableMap::parsesVersion1BigEndian()
{
    std::vector<float> g;
    for (int i = 0; i < 2 * 3 * 4; ++i) g.push_back(0.1f * i);
    const QByteArray d = makeMap(true, false, 2, 3, 4, 0.5, 0.25, 0.1, 0.2, kLumWeights, g);
    QCOMPARE(d.size(), 64 + 4 * 2 * 3 * 4);     // the spec's size formula
    GainTableMap m;
    QVERIFY(GainTableMap::Parse(d, true, false, m));
    QVERIFY(m.isValid());
    QCOMPARE(m.pointsV, 2);
    QCOMPARE(m.pointsH, 3);
    QCOMPARE(m.pointsN, 4);
    QCOMPARE(m.spacingV, 0.5);
    QCOMPARE(m.originH, 0.2);
    QCOMPARE(m.weights[1], 1.0f);
    QCOMPARE(m.gamma, 1.0f);
    for (size_t i = 0; i < g.size(); ++i) QCOMPARE(m.gains[i], g[i]);

    /* The same bytes read in the wrong order are not a valid map. */
    GainTableMap wrong;
    QVERIFY(!GainTableMap::Parse(d, false, false, wrong));
}

void TstGainTableMap::parsesVersion2Uint8LittleEndian()
{
    std::vector<float> g = {0.5f, 2.5f, 1.5f, 0.5f};    // 1 x 1 x 4
    const QByteArray d = makeMap(false, true, 1, 1, 4, 0.0, 0.0, 0.0, 0.0, kLumWeights, g,
                                 0, 2.0f, 0.5f, 2.5f);
    QCOMPARE(d.size(), 80 + 1 * 4);
    GainTableMap m;
    QVERIFY(GainTableMap::Parse(d, false, true, m));
    QCOMPARE(m.gamma, 2.0f);
    /* uint8 -> GainMin + (G / 255) * (GainMax - GainMin), so the ends are exact. */
    QCOMPARE(m.gains[0], 0.5f);
    QCOMPARE(m.gains[1], 2.5f);
    QVERIFY(std::fabs(m.gains[2] - 1.5f) < 0.01f);
    /* Gamma applies to the input: green 0.25 -> 0.0625. */
    QVERIFY(std::fabs(m.Input(0.0f, 0.25f, 0.0f) - 0.0625f) < 1e-6f);
}

void TstGainTableMap::rejectsMalformed()
{
    std::vector<float> g(2 * 2 * 4, 1.0f);
    const QByteArray d = makeMap(true, false, 2, 2, 4, 1.0, 1.0, 0.0, 0.0, kLumWeights, g);
    GainTableMap m;
    QVERIFY(!GainTableMap::Parse(d.left(d.size() - 1), true, false, m));   // truncated
    QVERIFY(!GainTableMap::Parse(d.left(40), true, false, m));             // no header
    /* Zero spacing with more than one point cannot place the grid. */
    const QByteArray z = makeMap(true, false, 2, 2, 4, 0.0, 1.0, 0.0, 0.0, kLumWeights, g);
    QVERIFY(!GainTableMap::Parse(z, true, false, m));
    /* Version 2's gamma is bounded 0.25..4. */
    const QByteArray bad = makeMap(true, true, 1, 1, 4, 0.0, 0.0, 0.0, 0.0, kLumWeights,
                                   std::vector<float>(4, 1.0f), 3, 8.0f);
    QVERIFY(!GainTableMap::Parse(bad, true, true, m));
}

void TstGainTableMap::lookupInterpolatesAndReplicatesEdges()
{
    /* 1 x 2 grid at u = 0 and u = 1; left table 1.0 everywhere, right 0.5; N = 3 so the
       index is t * 3 clamped to 2. */
    std::vector<float> g = {1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f};
    GainTableMap m;
    QVERIFY(GainTableMap::Parse(
        makeMap(true, false, 1, 2, 3, 0.0, 1.0, 0.0, 0.0, kLumWeights, g), true, false, m));
    int x0 = -1; float wx = -1;
    GainTableMap::Axis(0.5, m.originH, m.spacingH, m.pointsH, x0, wx);
    QCOMPARE(x0, 0);
    QCOMPARE(wx, 0.5f);
    QCOMPARE(m.Gain(0, 0.0f, x0, wx, 0.3f), 0.75f);             // halfway between tables
    GainTableMap::Axis(-0.4, m.originH, m.spacingH, m.pointsH, x0, wx);   // left of grid
    QCOMPARE(m.Gain(0, 0.0f, x0, wx, 0.3f), 1.0f);
    GainTableMap::Axis(1.7, m.originH, m.spacingH, m.pointsH, x0, wx);    // right of grid
    QCOMPARE(m.Gain(0, 0.0f, x0, wx, 0.3f), 0.5f);

    /* Along the table: 0, 1, 2 at indices 0..2; t = 0.5 -> index 1.5 -> 1.5. */
    std::vector<float> ramp = {0.0f, 1.0f, 2.0f};
    GainTableMap r;
    QVERIFY(GainTableMap::Parse(
        makeMap(true, false, 1, 1, 3, 0.0, 0.0, 0.0, 0.0, kLumWeights, ramp), true, false, r));
    QCOMPARE(r.Gain(0, 0.0f, 0, 0.0f, 0.5f), 1.5f);
    QCOMPARE(r.Gain(0, 0.0f, 0, 0.0f, 1.0f), 2.0f);              // clamped to the last
}

void TstGainTableMap::appliedOnceWhenLeavingCameraNative()
{
    WorkingImage img = makeImage(8, 6, 0.4f, constantMap(0.5f));
    Develop::ToWorkingSpace(img);
    QCOMPARE(img.space, ColorSpaceMath::kWorking);
    for (float v : img.rgb) QVERIFY(std::fabs(v - 0.2f) < 1e-6f);
    /* A second conversion is a no-op -- a mask scope works on the converted base and
       must not take the gain again. */
    Develop::ToWorkingSpace(img);
    for (float v : img.rgb) QVERIFY(std::fabs(v - 0.2f) < 1e-6f);
}

void TstGainTableMap::appliedEvenForAnIdentityEdit()
{
    WorkingImage img = makeImage(8, 6, 0.4f, constantMap(0.5f));
    const EditParams identity;
    QVERIFY(identity.isIdentity());
    Develop d;
    QVERIFY(d.Apply(img, identity));
    for (float v : img.rgb) QVERIFY(std::fabs(v - 0.2f) < 1e-6f);

    /* And through the fused path an edit takes: still exactly once. */
    WorkingImage edited = makeImage(8, 6, 0.4f, constantMap(0.5f));
    EditParams p;
    p.saturation = 10.0f;               // neutral pixels: saturation leaves them alone
    QVERIFY(!p.isIdentity());
    Develop d2;
    QVERIFY(d2.Apply(edited, p));
    for (size_t i = 0; i < edited.rgb.size(); ++i)
        QVERIFY2(std::fabs(edited.rgb[i] - 0.2f) < 2e-3f,
                 qPrintable(QString::number(edited.rgb[i])));
}

void TstGainTableMap::fileBaselineExposureReplacesTheTable()
{
    WorkingImage img = makeImage(4, 4, 0.1f, nullptr);
    img.cam.baselineExposure = 1.0f;    // +1 EV, from the file
    Develop::ToWorkingSpace(img);
    for (float v : img.rgb) QVERIFY(std::fabs(v - 0.2f) < 1e-6f);
}

void TstGainTableMap::positionsAreRelativeToTheFrame()
{
    /* Left half of the frame gain 1, right half 0.5: a 1 x 2 grid, a table centred on
       each half. A crop of the RIGHT half (a Detail patch: frameX/frameW) must take the
       right half's gain, not the left's. */
    std::vector<float> g = {1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f};
    GainTableMap m;
    QVERIFY(GainTableMap::Parse(
        makeMap(true, false, 1, 2, 3, 0.0, 0.5, 0.0, 0.25, kLumWeights, g), true, false, m));
    auto map = std::make_shared<const GainTableMap>(m);

    WorkingImage whole = makeImage(16, 2, 0.4f, map);
    Develop::ToWorkingSpace(whole);
    QVERIFY(std::fabs(whole.rgb[0] - 0.4f) < 1e-6f);                    // far left
    QVERIFY(std::fabs(whole.rgb[(16 - 1) * 3] - 0.2f) < 1e-6f);         // far right

    WorkingImage patch = makeImage(4, 2, 0.4f, map);
    patch.frameX = 12; patch.frameY = 0; patch.frameW = 16; patch.frameH = 2;
    Develop::ToWorkingSpace(patch);
    for (int x = 0; x < 4; ++x)
        QVERIFY2(std::fabs(patch.rgb[x * 3] - 0.2f) < 1e-6f,
                 qPrintable(QString::number(patch.rgb[x * 3])));
}

QTEST_GUILESS_MAIN(TstGainTableMap)
#include "tst_gaintablemap.moc"
