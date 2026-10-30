#include <QtTest>
#include <QElapsedTimer>

#include "Develop/artblur.h"
#include "Develop/editrotate.h"     // EditParams / EditStack / EditRotate (header-only)

/*
    Effects > Blur (Develop/artblur.h): the artistic blurs. Pins what each one is for --
    the plain blur keeps a flat field flat, Motion blurs ALONG its angle only, Spin and
    Zoom leave the centre alone and blur rings vs. spokes respectively, Tilt-shift keeps
    its band sharp, Glow never darkens -- plus the two properties the render depends on:
    a proxy-sized render matches a full one (resolution independence) and a padded
    Detail patch matches the same window of the full frame. Effects > Orton
    (applyOrton) is pinned here too: a NORMAL blend toward the soft copy, so unlike Glow
    it darkens bright detail, and Brightness lifts the copy.
*/

namespace {

using namespace ArtBlur;

cv::Mat flat(int w, int h, float v)
{
    return cv::Mat(h, w, CV_32FC3, cv::Scalar(v, v, v));
}

/* A white line on black: horizontal (row) or vertical (column). */
cv::Mat line(int w, int h, bool horizontal)
{
    cv::Mat m = flat(w, h, 0.0f);
    if (horizontal) m.row(h / 2).setTo(cv::Scalar(1, 1, 1));
    else            m.col(w / 2).setTo(cv::Scalar(1, 1, 1));
    return m;
}

/* A fine checkerboard: every blur should flatten it, unless it is sharp-preserving. */
cv::Mat checker(int w, int h, int cell = 2)
{
    cv::Mat m(h, w, CV_32FC3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float v = (((x / cell) + (y / cell)) & 1) ? 1.0f : 0.0f;
            m.at<cv::Vec3f>(y, x) = cv::Vec3f(v, v, v);
        }
    return m;
}

/* Local contrast in a window: max - min of channel 0. */
float spread(const cv::Mat &m, cv::Rect r)
{
    double lo, hi;
    std::vector<cv::Mat> ch;
    cv::split(m(r), ch);
    cv::minMaxLoc(ch[0], &lo, &hi);
    return float(hi - lo);
}

Settings blur(int type, float amount)
{
    Settings s;
    s.type = type;
    s.amount = amount;
    return s;
}

}  // namespace

class TestArtBlur : public QObject
{
    Q_OBJECT
private slots:
    void amountZeroIsIdentity();
    void gaussianKeepsFlatAndMean();
    void motionBlursAlongItsAngleOnly();
    void spinKeepsSpokesBlursRings();
    void zoomKeepsRingsBlursSpokes();
    void tiltShiftKeepsTheBand();
    void glowNeverDarkens();
    void ortonBlendsTowardTheSoftCopy();
    void ortonPatchJsonAndEye();
    void proxyMatchesFullRes();
    void paddedPatchMatchesFullFrame();
    void reachCoversTheBlur();
    void paramsJsonSanitizeAndEye();
    void rotationTurnsTheBlur();
    void timing();
};

void TestArtBlur::amountZeroIsIdentity()
{
    for (int t = 0; t < TypeCount; ++t) {
        cv::Mat m = checker(40, 30), ref = m.clone();
        apply(m, blur(t, 0.0f));
        QVERIFY(cv::norm(m, ref, cv::NORM_INF) == 0.0);
    }
}

void TestArtBlur::gaussianKeepsFlatAndMean()
{
    cv::Mat f = flat(120, 80, 0.37f);
    apply(f, blur(Gaussian, 1.0f));
    QVERIFY(cv::norm(f, flat(120, 80, 0.37f), cv::NORM_INF) < 1e-4);

    cv::Mat c = checker(200, 160);
    const double before = cv::mean(c)[0];
    apply(c, blur(Gaussian, 0.5f));
    QVERIFY(qAbs(cv::mean(c)[0] - before) < 0.01);
    QVERIFY(spread(c, cv::Rect(80, 60, 40, 40)) < 0.05f);   // checker flattened
}

void TestArtBlur::motionBlursAlongItsAngleOnly()
{
    /* Horizontal motion: a horizontal line stays put, a vertical one smears sideways. */
    cv::Mat hl = line(200, 200, true), vl = line(200, 200, false);
    Settings s = blur(Motion, 0.5f);       // 10 px streak
    s.angle = 0.0f;
    apply(hl, s);
    apply(vl, s);
    QVERIFY(hl.at<cv::Vec3f>(100, 100)[0] > 0.9f);
    QVERIFY(hl.at<cv::Vec3f>(95, 100)[0] < 0.05f);         // not spread vertically
    QVERIFY(vl.at<cv::Vec3f>(100, 100)[0] < 0.2f);         // spread...
    QVERIFY(vl.at<cv::Vec3f>(100, 103)[0] > 0.05f);        // ...sideways
    /* At 90 degrees the roles swap. */
    cv::Mat hl2 = line(200, 200, true), vl2 = line(200, 200, false);
    s.angle = 90.0f;
    apply(hl2, s);
    apply(vl2, s);
    QVERIFY(vl2.at<cv::Vec3f>(100, 100)[0] > 0.9f);
    QVERIFY(hl2.at<cv::Vec3f>(100, 100)[0] < 0.2f);
}

void TestArtBlur::spinKeepsSpokesBlursRings()
{
    /* About the centre: a ring (constant radius) is unchanged by a rotation, a spoke
       (constant angle) is smeared round it. */
    const int W = 201, H = 201;
    cv::Mat ring = flat(W, H, 0.0f), spoke = flat(W, H, 0.0f);
    cv::circle(ring, cv::Point(100, 100), 60, cv::Scalar(1, 1, 1), 3);
    cv::line(spoke, cv::Point(100, 100), cv::Point(200, 100), cv::Scalar(1, 1, 1), 3);
    const Settings s = blur(Spin, 0.5f);   // 30 degrees
    apply(ring, s);
    apply(spoke, s);
    QVERIFY(ring.at<cv::Vec3f>(100, 160)[0] > 0.7f);        // on the ring, still bright
    QVERIFY(spoke.at<cv::Vec3f>(100, 180)[0] < 0.3f);       // the spoke smeared...
    QVERIFY(spoke.at<cv::Vec3f>(100 + 20, 178)[0] > 0.02f); // ...round the arc
}

void TestArtBlur::zoomKeepsRingsBlursSpokes()
{
    const int W = 201, H = 201;
    cv::Mat ring = flat(W, H, 0.0f), spoke = flat(W, H, 0.0f);
    cv::circle(ring, cv::Point(100, 100), 60, cv::Scalar(1, 1, 1), 3);
    cv::line(spoke, cv::Point(100, 100), cv::Point(200, 100), cv::Scalar(1, 1, 1), 3);
    const Settings s = blur(Zoom, 0.5f);
    apply(ring, s);
    apply(spoke, s);
    QVERIFY(spoke.at<cv::Vec3f>(100, 160)[0] > 0.7f);       // along the spoke: unchanged
    QVERIFY(ring.at<cv::Vec3f>(100, 160)[0] < 0.6f);        // the ring smeared radially
    /* The centre pixel never moves. */
    cv::Mat c = checker(W, H, 5), ref = c.clone();
    apply(c, s);
    QVERIFY(qAbs(c.at<cv::Vec3f>(100, 100)[0] - ref.at<cv::Vec3f>(100, 100)[0]) < 0.2f);
}

void TestArtBlur::tiltShiftKeepsTheBand()
{
    cv::Mat c = checker(300, 300);
    Settings s = blur(TiltShift, 1.0f);
    s.angle = 0.0f;          // horizontal band through the centre
    s.size = 0.2f;
    s.feather = 0.1f;
    apply(c, s);
    QVERIFY(spread(c, cv::Rect(140, 145, 20, 10)) > 0.9f);  // in the band: sharp
    QVERIFY(spread(c, cv::Rect(140, 5, 20, 10)) < 0.1f);    // far out: blurred
}

void TestArtBlur::glowNeverDarkens()
{
    cv::Mat c = checker(150, 150, 3), ref = c.clone();
    Settings s = blur(Glow, 1.0f);
    s.size = 0.5f;
    apply(c, s);
    cv::Mat diff = c - ref;
    double lo, hi;
    cv::minMaxLoc(diff.reshape(1), &lo, &hi);
    QVERIFY(lo >= -1e-6);
    QVERIFY(hi > 0.1);                                      // and it does glow
}

void TestArtBlur::ortonBlendsTowardTheSoftCopy()
{
    cv::Mat c = checker(150, 150, 3), ref = c.clone();
    OrtonSettings s;
    applyOrton(c, s);                                       // opacity 0: off
    QVERIFY(cv::norm(c, ref, cv::NORM_INF) == 0.0);

    /* Opacity 1, no lift: the picture IS the soft copy -- mean kept, checker flattened,
       and (unlike Glow) the white cells darken. */
    s.opacity = 1.0f;
    s.size = 0.5f;
    applyOrton(c, s);
    QVERIFY(qAbs(cv::mean(c)[0] - cv::mean(ref)[0]) < 0.01);
    QVERIFY(spread(c, cv::Rect(60, 60, 30, 30)) < 0.05f);
    cv::Mat diff = c - ref;
    double lo, hi;
    cv::minMaxLoc(diff.reshape(1), &lo, &hi);
    QVERIFY(lo < -0.4);

    /* Half opacity: halfway between the photo and the soft copy. */
    cv::Mat h = ref.clone();
    s.opacity = 0.5f;
    applyOrton(h, s);
    cv::Mat mid = (ref + c) * 0.5f;
    QVERIFY(cv::norm(h, mid, cv::NORM_INF) < 1e-4);

    /* Brightness lifts the copy by 2^EV: +1 EV at full opacity doubles a flat field. */
    cv::Mat f = flat(80, 60, 0.2f);
    s.opacity = 1.0f;
    s.brightness = 1.0f;
    applyOrton(f, s);
    QVERIFY(cv::norm(f, flat(80, 60, 0.4f), cv::NORM_INF) < 1e-4);
}

void TestArtBlur::ortonPatchJsonAndEye()
{
    /* A padded Detail patch matches the same window of the whole frame. */
    cv::Mat full(300, 400, CV_32FC3);
    cv::randu(full, cv::Scalar::all(0), cv::Scalar::all(1));
    cv::GaussianBlur(full, full, cv::Size(0, 0), 1.5);
    OrtonSettings o;
    o.opacity = 0.7f; o.size = 0.6f; o.brightness = 0.5f;
    cv::Mat whole = full.clone();
    applyOrton(whole, o);
    const cv::Rect win(150, 100, 60, 60);
    const int pad = ortonReach(o, full.cols, full.rows) + 4;
    const cv::Rect padded = cv::Rect(win.x - pad, win.y - pad, win.width + 2 * pad,
                                     win.height + 2 * pad) & cv::Rect(0, 0, 400, 300);
    cv::Mat patch = full(padded).clone();
    Frame f;
    f.fx = padded.x; f.fy = padded.y; f.fw = full.cols; f.fh = full.rows;
    applyOrton(patch, o, f);
    const cv::Mat got = patch(cv::Rect(win.x - padded.x, win.y - padded.y,
                                       win.width, win.height));
    QVERIFY(cv::norm(got, whole(win), cv::NORM_INF) < 0.03);

    /* Off: nothing written. On: round trips; Reset Effects and the eye take it. */
    QVERIFY(!EditStack::paramsToJson(EditParams()).contains("ortonOpacity"));
    EditParams p;
    p.ortonOpacity = 0.4f; p.ortonSize = 0.3f; p.ortonBrightness = 1.25f;
    QVERIFY(!p.isIdentity());
    const EditParams b = EditStack::paramsFromJson(EditStack::paramsToJson(p));
    QCOMPARE(b.ortonOpacity, 0.4f);
    QCOMPARE(b.ortonSize, 0.3f);
    QCOMPARE(b.ortonBrightness, 1.25f);
    EditParams r = p;
    EditParams::resetGroup(r, EditParams::Group::Effects);
    QCOMPARE(r.ortonOpacity, 0.0f);
    EditScope l;
    l.params = p;
    l.showOrton = false;
    QCOMPARE(effectiveScopeParams(l).ortonOpacity, 0.0f);
    QCOMPARE(effectiveScopeParams(l).ortonSize, 0.3f);
    EditStack hidden;
    EditScope hs;
    hs.showOrton = false;
    hidden.scopes.append(hs);
    QVERIFY(!hidden.isIdentity());                          // a shut eye is state
    QVERIFY(!EditStack::fromJson(hidden.toJson()).scopes.at(0).showOrton);
    /* Out of range is repaired. */
    EditStack s;
    EditScope sc;
    sc.params.ortonOpacity = 2.0f;
    sc.params.ortonBrightness = 5.0f;
    s.scopes.append(sc);
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.ortonOpacity, 1.0f);
    QCOMPARE(s.scopes[0].params.ortonBrightness, 2.0f);
}

void TestArtBlur::proxyMatchesFullRes()
{
    /* The same picture at 1/2 scale, blurred with the same settings, then scaled up,
       matches the full-size blur: every distance is relative to the frame. */
    cv::Mat full(240, 320, CV_32FC3);
    cv::randu(full, cv::Scalar::all(0), cv::Scalar::all(1));
    cv::GaussianBlur(full, full, cv::Size(0, 0), 3.0);     // what a half-size copy holds
    for (int t : {int(Gaussian), int(Motion), int(Spin), int(Zoom)}) {
        Settings s = blur(t, 0.6f);
        s.angle = 30.0f;
        s.x = 0.4f; s.y = 0.6f;
        cv::Mat a = full.clone(), half;
        cv::resize(full, half, cv::Size(160, 120), 0, 0, cv::INTER_AREA);
        apply(a, s);
        apply(half, s);
        cv::Mat up;
        cv::resize(half, up, full.size(), 0, 0, cv::INTER_LINEAR);
        const double err = cv::norm(a, up, cv::NORM_L1) / (double(a.total()) * 3.0);
        QVERIFY2(err < 0.02, qPrintable(QString("type %1 mean err %2").arg(t).arg(err)));
    }
}

void TestArtBlur::paddedPatchMatchesFullFrame()
{
    /* Band-limited, like a photo: Motion turns the frame to blur it, and on per-pixel
       noise the resampling PHASE of a crop vs. the whole frame would dominate. */
    cv::Mat full(300, 400, CV_32FC3);
    cv::randu(full, cv::Scalar::all(0), cv::Scalar::all(1));
    cv::GaussianBlur(full, full, cv::Size(0, 0), 1.5);
    for (int t : {int(Gaussian), int(Motion), int(TiltShift), int(Glow)}) {
        Settings s = blur(t, 0.5f);
        s.angle = 20.0f;
        cv::Mat whole = full.clone();
        apply(whole, s);
        const cv::Rect win(150, 100, 60, 60);
        const int pad = reach(s, full.cols, full.rows) + 4;
        const cv::Rect padded = cv::Rect(win.x - pad, win.y - pad, win.width + 2 * pad,
                                         win.height + 2 * pad) & cv::Rect(0, 0, 400, 300);
        cv::Mat patch = full(padded).clone();
        Frame f;
        f.fx = padded.x; f.fy = padded.y; f.fw = full.cols; f.fh = full.rows;
        apply(patch, s, f);
        const cv::Mat got = patch(cv::Rect(win.x - padded.x, win.y - padded.y,
                                           win.width, win.height));
        const double err = cv::norm(got, whole(win), cv::NORM_INF);
        QVERIFY2(err < 0.03, qPrintable(QString("type %1 max err %2").arg(t).arg(err)));
    }
}

void TestArtBlur::reachCoversTheBlur()
{
    QCOMPARE(reach(blur(Gaussian, 0.0f), 1000, 800), 0);
    QVERIFY(reach(blur(Gaussian, 1.0f), 1000, 800) >= 150);   // 3 x 5% of 1000
    QVERIFY(reach(blur(Spin, 1.0f), 1000, 800) > reach(blur(Spin, 0.2f), 1000, 800));
}

void TestArtBlur::paramsJsonSanitizeAndEye()
{
    /* Off: nothing written, so existing recipes keep their JSON (and hash). */
    QVERIFY(!EditStack::paramsToJson(EditParams()).contains("blurAmount"));
    EditParams p;
    p.blurType = 4; p.blurAmount = 0.6f; p.blurAngle = -30.0f;
    p.blurX = 0.3f; p.blurY = 0.7f; p.blurSize = 0.25f; p.blurFeather = 0.8f;
    QVERIFY(!p.isIdentity());
    QVERIFY(!EditParams::groupIsDefault(p, EditParams::Group::Effects));
    const EditParams b = EditStack::paramsFromJson(EditStack::paramsToJson(p));
    QCOMPARE(b.blurType, 4);
    QCOMPARE(b.blurAmount, 0.6f);
    QCOMPARE(b.blurAngle, -30.0f);
    QCOMPARE(b.blurX, 0.3f);
    QCOMPARE(b.blurFeather, 0.8f);
    /* Reset Effects takes it; the per-effect eye folds it out, settings kept. */
    EditParams r = p;
    EditParams::resetGroup(r, EditParams::Group::Effects);
    QCOMPARE(r.blurAmount, 0.0f);
    EditScope l;
    l.params = p;
    l.showBlur = false;
    QCOMPARE(effectiveScopeParams(l).blurAmount, 0.0f);
    QCOMPARE(effectiveScopeParams(l).blurAngle, -30.0f);
    /* Out of range is repaired. */
    EditStack s;
    EditScope sc;
    sc.params.blurType = 9;
    sc.params.blurAmount = 3.0f;
    s.scopes.append(sc);
    EditStack::sanitize(s);
    QCOMPARE(s.scopes[0].params.blurType, 0);
    QCOMPARE(s.scopes[0].params.blurAmount, 1.0f);
}

void TestArtBlur::rotationTurnsTheBlur()
{
    /* The photo turned 90: an ON blur's centre and direction turn with it. */
    EditStack s;
    EditScope g;
    g.params.blurAmount = 0.5f;
    g.params.blurType = 1;
    g.params.blurX = 0.2f; g.params.blurY = 0.3f;
    g.params.blurAngle = 0.0f;
    s.scopes.append(g);
    QVERIFY(EditRotate::stack(s, 90));
    QCOMPARE(s.scopes[0].params.blurX, 0.7f);
    QCOMPARE(s.scopes[0].params.blurY, 0.2f);
    QCOMPARE(s.scopes[0].params.blurAngle, 90.0f);
    /* An untouched scope is left at its defaults. */
    EditStack plain;
    plain.scopes.append(EditScope());
    QVERIFY(!EditRotate::stack(plain, 90));
    QCOMPARE(plain.scopes[0].params.blurAngle, 0.0f);
    /* The render's conversion (oriented -> source) turns even DEFAULT values: a Motion
       blur at angle 0 on a frame shown turned 90 is vertical in the source. */
    EditParams m;
    m.blurAmount = 0.5f;
    EditRotate::turnBlur(m, (360 - 90) % 360);
    QCOMPARE(m.blurAngle, -90.0f);
}

void TestArtBlur::timing()
{
    /* Not a gate: the numbers for the record -- a 2.7 MP proxy (an interactive tick)
       and a 24 MP frame (the settle render / export). */
    const char *names[] = {"Gaussian", "Motion", "Spin", "Zoom", "TiltShift", "Glow"};
    for (const cv::Size sz : {cv::Size(2000, 1350), cv::Size(6000, 4000)}) {
        cv::Mat base(sz, CV_32FC3);
        cv::randu(base, cv::Scalar::all(0), cv::Scalar::all(1));
        for (int t = 0; t < TypeCount; ++t) {
            cv::Mat m = base.clone();
            Settings s = blur(t, 0.5f);
            s.angle = 25.0f;
            QElapsedTimer et;
            et.start();
            apply(m, s);
            qInfo("%s: %lld ms at %.1f MP", names[t], et.elapsed(),
                  sz.area() / 1.0e6);
        }
        cv::Mat m = base.clone();
        OrtonSettings o;
        o.opacity = 0.5f;
        QElapsedTimer et;
        et.start();
        applyOrton(m, o);
        qInfo("Orton: %lld ms at %.1f MP", et.elapsed(), sz.area() / 1.0e6);
    }
}

QTEST_GUILESS_MAIN(TestArtBlur)
#include "tst_artblur.moc"
