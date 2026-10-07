#include <QtTest>
#include <QImage>
#include "Utilities/skyrefine.h"

/*
    Pins Utilities/skyrefine.h, the two-model sky matte behind Select Sky.

    The synthetic cases pin the three RULES that were each added for a failure seen on
    real pictures: calibrated probabilities (a no-sky picture used to be min-max
    stretched into a full false sky), the region gate (smooth bokeh that only skyseg
    calls sky is dropped), and agreement (sky both models see is kept).

    The fixture case is EVIDENCE, not a mock: fixtures/sky/oak.png is a real photograph
    of a bare oak against blue sky (640 px), and oak_skyseg.png / oak_segformer.png are
    the two models' REAL outputs on it at their native 320x320 and 128x128 -- the exact
    inputs the app hands refine(). Ground truth comes from colour (clearly blue = sky,
    red >= blue = branch), scored only over the canopy, where the halo lives. Measured
    after the 2026-10-07 halo passes (spread-aware colours, half-width edge snaps,
    decontaminated alpha): canopy error 0.154 refined vs 0.223 for the raw upsampled
    skyseg map, sky mean alpha 0.83, branch 0.14, 49% fractional. Bounds leave a margin.
*/

namespace {

cv::Mat toRgb32(const QImage &img)
{
    const QImage im = img.convertToFormat(QImage::Format_RGB888);
    cv::Mat m(im.height(), im.width(), CV_32FC3);
    for (int y = 0; y < im.height(); ++y) {
        const uchar *s = im.constScanLine(y);
        cv::Vec3f *d = m.ptr<cv::Vec3f>(y);
        for (int x = 0; x < im.width(); ++x)
            d[x] = cv::Vec3f(s[3 * x] / 255.0f, s[3 * x + 1] / 255.0f, s[3 * x + 2] / 255.0f);
    }
    return m;
}

cv::Mat toGray32(const QImage &img)
{
    const QImage im = img.convertToFormat(QImage::Format_Grayscale8);
    cv::Mat m(im.height(), im.width(), CV_32F);
    for (int y = 0; y < im.height(); ++y) {
        const uchar *s = im.constScanLine(y);
        float *d = m.ptr<float>(y);
        for (int x = 0; x < im.width(); ++x) d[x] = s[x] / 255.0f;
    }
    return m;
}

/* A smooth, featureless picture -- the bokeh / overcast case. */
cv::Mat smoothPicture(int w, int h)
{
    cv::Mat m(h, w, CV_32FC3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            m.at<cv::Vec3f>(y, x) = cv::Vec3f(0.45f + 0.1f * x / w, 0.55f, 0.35f + 0.1f * y / h);
    return m;
}

}   // namespace

class TstSkyRefine : public QObject
{
    Q_OBJECT
private slots:
    void noSkyStaysEmpty();
    void skysegOnlyRegionIsDropped();
    void agreedSkyIsKept();
    void realCanopyMatte();
};

/* A picture where skyseg peaks at 0.016 (a real no-sky case had exactly that) must not
   select anything -- min-max used to stretch it into a full sky. */
void TstSkyRefine::noSkyStaysEmpty()
{
    const cv::Mat rgb = smoothPicture(400, 300);
    const cv::Mat sky(320, 320, CV_32F, cv::Scalar(0.016));
    const cv::Mat seg(128, 128, CV_32F, cv::Scalar(0.0));
    const cv::Mat a = SkyRefine::refine(sky, seg, rgb);
    double mx = 0;
    cv::minMaxLoc(a, nullptr, &mx);
    QVERIFY2(mx < 0.05, qPrintable(QString("max alpha %1").arg(mx)));
}

/* skyseg calls the whole upper half sky at full confidence; SegFormer disagrees. That is
   the bokeh false positive, and the whole region must go. */
void TstSkyRefine::skysegOnlyRegionIsDropped()
{
    const cv::Mat rgb = smoothPicture(400, 300);
    cv::Mat sky(320, 320, CV_32F, cv::Scalar(0.0));
    sky(cv::Rect(0, 0, 320, 160)).setTo(1.0);
    const cv::Mat seg(128, 128, CV_32F, cv::Scalar(0.05));
    const cv::Mat a = SkyRefine::refine(sky, seg, rgb);
    const double upper = cv::mean(a(cv::Rect(0, 0, 400, 140)))[0];
    QVERIFY2(upper < 0.05, qPrintable(QString("upper mean alpha %1").arg(upper)));
}

/* Same picture, but both models agree: the region is sky. */
void TstSkyRefine::agreedSkyIsKept()
{
    const cv::Mat rgb = smoothPicture(400, 300);
    cv::Mat sky(320, 320, CV_32F, cv::Scalar(0.0));
    sky(cv::Rect(0, 0, 320, 160)).setTo(1.0);
    cv::Mat seg(128, 128, CV_32F, cv::Scalar(0.0));
    seg(cv::Rect(0, 0, 128, 64)).setTo(0.9);
    const cv::Mat a = SkyRefine::refine(sky, seg, rgb);
    const double upper = cv::mean(a(cv::Rect(0, 0, 400, 130)))[0];
    const double lower = cv::mean(a(cv::Rect(0, 170, 400, 130)))[0];
    QVERIFY2(upper > 0.95, qPrintable(QString("upper mean alpha %1").arg(upper)));
    QVERIFY2(lower < 0.05, qPrintable(QString("lower mean alpha %1").arg(lower)));
}

/* The halo case on a real photograph: branches against blue sky. */
void TstSkyRefine::realCanopyMatte()
{
    const QImage photo(QFINDTESTDATA("../fixtures/sky/oak.png"));
    const QImage skyseg(QFINDTESTDATA("../fixtures/sky/oak_skyseg.png"));
    const QImage segformer(QFINDTESTDATA("../fixtures/sky/oak_segformer.png"));
    QVERIFY(!photo.isNull() && !skyseg.isNull() && !segformer.isNull());
    QCOMPARE(skyseg.size(), QSize(320, 320));
    QCOMPARE(segformer.size(), QSize(128, 128));

    const cv::Mat rgb = toRgb32(photo);
    const cv::Mat rawSky = toGray32(skyseg);
    const cv::Mat a = SkyRefine::refine(rawSky, toGray32(segformer), rgb);
    QCOMPARE(a.size(), rgb.size());

    cv::Mat raw;                                    // what the old path started from
    cv::resize(rawSky, raw, rgb.size(), 0, 0, cv::INTER_CUBIC);

    /* The canopy: the 512x384 patch at (524,287) of the 2048 px original. */
    const double k = rgb.cols / 2048.0;
    const cv::Rect canopy(int(524 * k), int(287 * k), int(512 * k), int(384 * k));
    double skySum = 0, nSky = 0, brSum = 0, nBr = 0, errRefined = 0, errRaw = 0, frac = 0;
    for (int y = canopy.y; y < canopy.y + canopy.height; ++y)
        for (int x = canopy.x; x < canopy.x + canopy.width; ++x) {
            const cv::Vec3f c = rgb.at<cv::Vec3f>(y, x);
            const float al = a.at<float>(y, x);
            const float r = std::clamp(raw.at<float>(y, x), 0.0f, 1.0f);
            if (al > 0.1f && al < 0.9f) ++frac;
            if (c[2] - c[0] > 0.15f) {                      // clearly sky
                skySum += al; ++nSky;
                errRefined += 1.0 - al; errRaw += 1.0 - r;
            } else if (c[0] >= c[2]) {                      // branch
                brSum += al; ++nBr;
                errRefined += al; errRaw += r;
            }
        }
    QVERIFY(nSky > 1000 && nBr > 1000);
    const double skyMean = skySum / nSky, brMean = brSum / nBr;
    const double ratio = errRefined / errRaw;
    const double fractional = frac / canopy.area();
    const QString msg = QString("sky %1 branch %2 err ratio %3 fractional %4")
                            .arg(skyMean).arg(brMean).arg(ratio).arg(fractional);
    QVERIFY2(skyMean > 0.70, qPrintable(msg));
    QVERIFY2(brMean < 0.22, qPrintable(msg));
    QVERIFY2(ratio < 0.90, qPrintable(msg));       // beats the raw map in the canopy
    QVERIFY2(fractional > 0.40, qPrintable(msg));  // a real matte, not a binary cutout
}

QTEST_MAIN(TstSkyRefine)
#include "tst_skyrefine.moc"
