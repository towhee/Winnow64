#include <QtTest>
#include <QJsonObject>
#include "Develop/lenscorrect.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"

/*
    Pins Develop/lenscorrect.h -- lateral CA correction and defringe -- on synthetic
    scenes whose truth is known, the same ground-truth set the prototype was judged on
    (2026-10-07):

      - estimateCA recovers a KNOWN radial red/blue scale, applyCA undoes it, and an
        image with no scale is reported as having none (a camera JPEG must not be moved);
      - defringe leaves CLEAN edges alone -- sky/trees, yellow leaf/blue sky, green leaf/
        shadow, red/grey, white/navy, every channel blurred identically -- which is what
        keeps it from eating real colour;
      - defringe removes an axial (blue blurred wider) fringe whose hue is in its band;
      - a clipped highlight is never modified (its true colour is unknown);
      - the RECIPE: an untouched image serializes exactly as before (the JSON is also
        the devPreview key and the render-cache key, so writing defaults would re-key
        every image ever edited), set values round-trip, damaged ones are clamped, and
        an unset removeCA follows the preference for raws only.
*/

namespace {

float lin(float v) { return std::pow(v, 2.2f); }

/* A two-colour scene with a wavy edge, blurred in LINEAR light like a lens. fringe:
   0 = clean, 1 = blue blurred wider (axial CA). Returned display-referred. */
cv::Mat edgeScene(const cv::Vec3f &c1, const cv::Vec3f &c2, int fringe)
{
    const int H = 160, W = 240;
    cv::Mat ch[3];
    for (int c = 0; c < 3; ++c) {
        cv::Mat m(H, W, CV_32F);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                m.at<float>(y, x) = (y < 70 + 12 * std::sin(x / 25.0)) ? lin(c1[c]) : lin(c2[c]);
        cv::GaussianBlur(m, ch[c], cv::Size(0, 0), (fringe == 1 && c == 2) ? 2.6 : 1.0);
        cv::pow(ch[c], 1.0 / 2.2, ch[c]);
    }
    cv::Mat out;
    cv::merge(ch, 3, out);
    return out;
}

double maxChromaError(const cv::Mat &a, const cv::Mat &ref)
{
    cv::Mat la, lr;
    cv::cvtColor(a, la, cv::COLOR_RGB2Lab);
    cv::cvtColor(ref, lr, cv::COLOR_RGB2Lab);
    double mx = 0;
    for (int y = 0; y < a.rows; ++y)
        for (int x = 0; x < a.cols; ++x) {
            const cv::Vec3f p = la.at<cv::Vec3f>(y, x), q = lr.at<cv::Vec3f>(y, x);
            mx = std::max(mx, double(std::hypot(p[1] - q[1], p[2] - q[2])));
        }
    return mx;
}

/* A busy, radially varied pattern (rotated grid of squares) for the CA estimator. */
cv::Mat caScene()
{
    const int H = 900, W = 1200;
    cv::Mat g(H, W, CV_32F);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const double u = (x * 0.8 + y * 0.6) / 37.0, v = (-x * 0.6 + y * 0.8) / 29.0;
            g.at<float>(y, x) = ((int(std::floor(u)) + int(std::floor(v))) & 1) ? 0.85f : 0.1f;
        }
    cv::GaussianBlur(g, g, cv::Size(0, 0), 1.2);
    cv::Mat out;
    cv::merge(std::vector<cv::Mat>{g, g, g}, out);
    return out;
}

}   // namespace

class TstLensCorrect : public QObject
{
    Q_OBJECT
private slots:
    void caRecoversKnownScale();
    void caNoneWhenAligned();
    void defringeLeavesCleanEdges();
    void defringeRemovesAxialFringe();
    void defringeSkipsClippedHighlights();
    void recipeFields();
};

void TstLensCorrect::caRecoversKnownScale()
{
    const cv::Mat clean = caScene();
    /* Make the aberration with the correction's own resampler, inverted. */
    cv::Mat aberrated = clean.clone();
    LensCorrect::CACoeffs make;
    make.valid = true;
    make.kR = -4.0e-3f;              // content of R displaced OUTWARD ~3 px at the corner
    make.kB = +2.0e-3f;
    LensCorrect::applyCA(aberrated, make);

    const LensCorrect::CACoeffs k = LensCorrect::estimateCA(aberrated);
    QVERIFY(k.valid);
    QVERIFY2(std::abs(k.kR - 4.0e-3f) < 0.6e-3f, qPrintable(QString("kR %1").arg(k.kR)));
    QVERIFY2(std::abs(k.kB + 2.0e-3f) < 0.6e-3f, qPrintable(QString("kB %1").arg(k.kB)));

    cv::Mat fixed = aberrated.clone();
    LensCorrect::applyCA(fixed, k);
    auto rgMisfit = [](const cv::Mat &m) {
        cv::Mat ch[3];
        cv::split(m, ch);
        return cv::norm(ch[0], ch[1], cv::NORM_L1) + cv::norm(ch[2], ch[1], cv::NORM_L1);
    };
    QVERIFY2(rgMisfit(fixed) < 0.3 * rgMisfit(aberrated),
             qPrintable(QString("misfit %1 -> %2").arg(rgMisfit(aberrated)).arg(rgMisfit(fixed))));
}

void TstLensCorrect::caNoneWhenAligned()
{
    const LensCorrect::CACoeffs k = LensCorrect::estimateCA(caScene());
    QVERIFY(!k.any());
}

void TstLensCorrect::defringeLeavesCleanEdges()
{
    const cv::Vec3f pairs[][2] = {
        {{0.40f, 0.60f, 0.90f}, {0.05f, 0.12f, 0.06f}},     // sky / dark trees
        {{0.80f, 0.70f, 0.10f}, {0.35f, 0.55f, 0.90f}},     // yellow leaf / blue sky
        {{0.30f, 0.70f, 0.15f}, {0.03f, 0.05f, 0.03f}},     // green leaf / shadow
        {{0.85f, 0.15f, 0.10f}, {0.45f, 0.45f, 0.45f}},     // red / grey
        {{0.95f, 0.95f, 0.95f}, {0.05f, 0.08f, 0.25f}}};    // white / navy
    for (const auto &p : pairs) {
        const cv::Mat clean = edgeScene(p[0], p[1], 0);
        cv::Mat out = clean.clone();
        LensCorrect::defringe(out, 1.0f, 1.0f, 1.0f);
        const double e = maxChromaError(out, clean);
        QVERIFY2(e < 1.0, qPrintable(QString("clean edge changed by %1").arg(e)));
    }
}

void TstLensCorrect::defringeRemovesAxialFringe()
{
    const cv::Vec3f pairs[][2] = {{{0.40f, 0.60f, 0.90f}, {0.05f, 0.12f, 0.06f}},
                                  {{0.95f, 0.95f, 0.95f}, {0.05f, 0.08f, 0.25f}}};
    for (const auto &p : pairs) {
        const cv::Mat clean = edgeScene(p[0], p[1], 0);
        const cv::Mat fringed = edgeScene(p[0], p[1], 1);
        cv::Mat out = fringed.clone();
        LensCorrect::defringe(out, 1.0f, 0.0f, 1.0f);
        const double before = maxChromaError(fringed, clean), after = maxChromaError(out, clean);
        QVERIFY2(after < 0.4 * before,
                 qPrintable(QString("fringe %1 -> %2").arg(before).arg(after)));
    }
}

void TstLensCorrect::defringeSkipsClippedHighlights()
{
    cv::Mat img = edgeScene({0.95f, 0.95f, 0.95f}, {0.05f, 0.08f, 0.25f}, 1);
    /* Clip a band across the edge: its pixels must come back bit-identical. */
    for (int y = 60; y < 80; ++y)
        for (int x = 0; x < img.cols; ++x) img.at<cv::Vec3f>(y, x)[2] = 1.0f;
    const cv::Mat before = img.clone();
    LensCorrect::defringe(img, 1.0f, 1.0f, 1.0f);
    for (int y = 60; y < 80; ++y)
        for (int x = 0; x < img.cols; ++x)
            QCOMPARE(img.at<cv::Vec3f>(y, x), before.at<cv::Vec3f>(y, x));
}

void TstLensCorrect::recipeFields()
{
    const EditParams def;
    QVERIFY(def.isIdentity());
    const QJsonObject o = EditStack::paramsToJson(def);
    QVERIFY(!o.contains("removeCA"));
    QVERIFY(!o.contains("defringePurple"));
    QVERIFY(!o.contains("defringeGreen"));

    EditParams p;
    p.removeCA = 1;
    p.defringePurple = 0.6f;
    QVERIFY(!p.isIdentity());
    const EditParams back = EditStack::paramsFromJson(EditStack::paramsToJson(p));
    QCOMPARE(back.removeCA, 1);
    QCOMPARE(back.defringePurple, 0.6f);
    QCOMPARE(back.defringeGreen, 0.0f);

    EditStack s;
    EditScope g;
    g.params.removeCA = 7;
    g.params.defringeGreen = 3.0f;
    s.scopes.append(g);
    EditStack::sanitize(s);
    QCOMPARE(s.scopes.at(0).params.removeCA, -1);
    QCOMPARE(s.scopes.at(0).params.defringeGreen, 1.0f);    // clampF clamps to the bound

    EditParams u;                                   // unset: follows the preference
    QVERIFY(u.wantsRemoveCA(true, true));           //   raw, preference on
    QVERIFY(!u.wantsRemoveCA(true, false));         //   never a non-raw automatically
    QVERIFY(!u.wantsRemoveCA(false, true));
    u.removeCA = 0;
    QVERIFY(!u.wantsRemoveCA(true, true));          // an explicit off beats it
    u.removeCA = 1;
    QVERIFY(u.wantsRemoveCA(false, false));         // an explicit on applies to anything
}

QTEST_MAIN(TstLensCorrect)
#include "tst_lenscorrect.moc"
