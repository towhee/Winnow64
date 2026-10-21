#include <QtTest>
#include <QElapsedTimer>

#include "Develop/lensblur.h"

/*
    Lens > Lens Blur (Develop/lensblur.h): the depth-of-field blur. Pins what makes it a
    LENS blur rather than a variable blur -- an in-focus subject stays exactly sharp and
    never bleeds into the blurred background behind it, while a blurred foreground does
    spread over what is behind it -- plus the bookkeeping the render depends on: the CoC
    map (focal range, sign, resolution independence), the refine, energy conservation
    (a flat field stays flat whatever the CoC), the kernels, and the highlight boost.
*/

namespace {

using namespace LensBlur;

cv::Mat flat(int w, int h, float v)
{
    return cv::Mat(h, w, CV_32FC3, cv::Scalar(v, v, v));
}

float at(const cv::Mat &m, int x, int y) { return m.at<cv::Vec3f>(y, x)[1]; }

}   // namespace

class TstLensBlur : public QObject
{
    Q_OBJECT

private slots:
    void zeroCocIsIdentity()
    {
        cv::Mat img(60, 80, CV_32FC3);
        cv::randu(img, 0.0f, 1.0f);
        const cv::Mat coc(img.size(), CV_32F, cv::Scalar(0.0f));
        const cv::Mat out = render(img, coc, Circle, 0.0f);
        QCOMPARE(cv::norm(out, img, cv::NORM_INF), 0.0);
    }

    void cocMapFocalRangeAndSign()
    {
        Settings s;
        s.amount = 1.0f; s.focus = 0.4f; s.range = 0.2f;
        const double L = 1000.0;
        QCOMPARE(cocAt(0.4f, s, L), 0.0f);             // the focal plane
        QCOMPARE(cocAt(0.49f, s, L), 0.0f);            // inside the range
        QCOMPARE(cocAt(0.31f, s, L), 0.0f);
        QVERIFY(cocAt(0.9f, s, L) > 0.0f);             // behind: positive
        QVERIFY(cocAt(0.1f, s, L) < 0.0f);             // in front: negative
        /* Linear beyond the range (disparity), and amount 0 is off. */
        QVERIFY(qAbs(cocAt(0.9f, s, L) - float(kMaxCoc * L * 0.4)) < 1e-3f);
        s.amount = 0.0f;
        QCOMPARE(cocAt(0.9f, s, L), 0.0f);
    }

    void cocScalesWithTheFrame()
    {
        Settings s;
        s.amount = 0.7f; s.focus = 0.2f; s.range = 0.0f;
        const cv::Mat d(10, 10, CV_32F, cv::Scalar(0.8f));
        const cv::Mat a = cocMap(d, s, 500.0), b = cocMap(d, s, 1000.0);
        QVERIFY(qAbs(b.at<float>(5, 5) - 2.0f * a.at<float>(5, 5)) < 1e-4f);
    }

    void refinePullsAndPushes()
    {
        cv::Mat coc(20, 20, CV_32F, cv::Scalar(12.0f));
        const cv::Mat full(coc.size(), CV_32F, cv::Scalar(1.0f));
        cv::Mat a = coc.clone();
        refine(a, full, -1.0f, 30.0);
        QCOMPARE(a.at<float>(3, 3), 0.0f);             // forced in focus
        cv::Mat b = coc.clone();
        refine(b, full, 1.0f, 30.0);
        QCOMPARE(b.at<float>(3, 3), 30.0f);            // pushed to the strong blur
        cv::Mat c = -coc;
        refine(c, full, 0.5f, 30.0);
        QVERIFY(c.at<float>(3, 3) < -12.0f);           // keeps its side (in front)
        /* Outside the mask nothing moves. */
        cv::Mat d = coc.clone();
        refine(d, cv::Mat(coc.size(), CV_32F, cv::Scalar(0.0f)), 1.0f, 30.0);
        QCOMPARE(cv::norm(d, coc, cv::NORM_INF), 0.0);
    }

    void kernelsSumToOne()
    {
        for (int sh = 0; sh < ShapeCount; ++sh)
            for (double r : {1.0, 3.5, 9.0}) {
                const cv::Mat k = kernel(sh, r, catOffset(0.7, 0.4));
                QVERIFY2(qAbs(cv::sum(k)[0] - 1.0) < 1e-5,
                         qPrintable(QString("shape %1 r %2").arg(sh).arg(r)));
            }
    }

    void shapesHaveTheirForm()
    {
        auto taps = [](const cv::Mat &k) { return cv::countNonZero(k > 1e-6f); };
        auto extent = [](const cv::Mat &k, bool cols) {
            cv::Mat red;
            cv::reduce(k, red, cols ? 0 : 1, cv::REDUCE_SUM);
            return cv::countNonZero(red > 1e-6f);
        };
        const cv::Mat circle = kernel(Circle, 9.0);
        const cv::Mat ana = kernel(Anamorphic, 9.0);
        QVERIFY(extent(ana, true) < extent(ana, false));      // a tall oval
        QVERIFY(taps(kernel(Blade5, 9.0)) < taps(circle));     // a polygon in the circle
        /* The cat's eye is round in the middle and clipped toward a corner. */
        QCOMPARE(taps(kernel(CatsEye, 9.0, catOffset(0, 0))), taps(circle));
        QVERIFY(taps(kernel(CatsEye, 9.0, catOffset(0.7, 0.7))) < taps(circle) * 0.8);
        /* The soap bubble's rim outweighs its centre. */
        const cv::Mat sb = kernel(SoapBubble, 9.0);
        const int c = sb.rows / 2;
        QVERIFY(sb.at<float>(c, c + 8) > 2.0f * sb.at<float>(c, c));
    }

    void flatFieldStaysFlat()
    {
        /* Energy conservation under any CoC: renormalised coverage, no dark seams
           between slices, for every shape. */
        const cv::Mat img = flat(120, 90, 0.4f);
        cv::Mat coc(img.size(), CV_32F);
        for (int y = 0; y < coc.rows; ++y)
            for (int x = 0; x < coc.cols; ++x)
                coc.at<float>(y, x) = float(x - 40) * 0.5f;      // -20 .. +40
        for (int sh = 0; sh < ShapeCount; ++sh) {
            const cv::Mat out = render(img, coc, sh, 0.0f);
            QVERIFY2(cv::norm(out, img, cv::NORM_INF) < 1e-3,
                     qPrintable(QString("shape %1").arg(sh)));
        }
    }

    void inFocusSubjectIsSharpAndDoesNotBleed()
    {
        /* A bright in-focus square on a dark blurred background. */
        cv::Mat img = flat(160, 160, 0.2f);
        cv::Mat coc(img.size(), CV_32F, cv::Scalar(15.0f));
        const cv::Rect sq(60, 60, 40, 40);
        img(sq).setTo(cv::Scalar(1, 1, 1));
        coc(sq).setTo(0.0f);
        const cv::Mat out = render(img, coc, Circle, 0.0f);
        /* Inside: untouched. */
        for (int y = sq.y; y < sq.y + sq.height; ++y)
            for (int x = sq.x; x < sq.x + sq.width; ++x)
                QCOMPARE(at(out, x, y), 1.0f);
        /* Just outside: background, not a halo of the subject. */
        QVERIFY(qAbs(at(out, sq.x - 4, 80) - 0.2f) < 0.01f);
        QVERIFY(qAbs(at(out, 80, sq.y + sq.height + 3) - 0.2f) < 0.01f);
    }

    void blurredForegroundSpreadsOverTheBackground()
    {
        /* A bright square IN FRONT (negative CoC) over a sharp background: its blur
           covers the background beside it, as a real lens does. */
        cv::Mat img = flat(160, 160, 0.2f);
        cv::Mat coc(img.size(), CV_32F, cv::Scalar(0.0f));
        const cv::Rect sq(60, 60, 40, 40);
        img(sq).setTo(cv::Scalar(1, 1, 1));
        coc(sq).setTo(-15.0f);
        const cv::Mat out = render(img, coc, Circle, 0.0f);
        QVERIFY(at(out, sq.x - 4, 80) > 0.35f);
        /* ... while the background further out is untouched. */
        QCOMPARE(at(out, 10, 10), 0.2f);
    }

    void boostBrightensBlurredHighlights()
    {
        cv::Mat img = flat(120, 120, 0.05f);
        img(cv::Rect(58, 58, 4, 4)).setTo(cv::Scalar(1, 1, 1));
        const cv::Mat coc(img.size(), CV_32F, cv::Scalar(12.0f));
        const cv::Mat a = render(img, coc, Circle, 0.0f);
        const cv::Mat b = render(img, coc, Circle, 1.0f);
        const cv::Rect around(40, 40, 40, 40);         // the highlight's disc
        QVERIFY(cv::sum(b(around))[1] > cv::sum(a(around))[1] * 1.3);
        /* An in-focus highlight is not boosted. */
        const cv::Mat sharp(img.size(), CV_32F, cv::Scalar(0.0f));
        QCOMPARE(cv::norm(render(img, sharp, Circle, 1.0f), img, cv::NORM_INF), 0.0);
    }

    void sliceRadiiCoverTheRange()
    {
        const std::vector<double> s = sliceRadii(-7.0, 40.0, Quality::final());
        QVERIFY(s.front() <= -7.0);
        QVERIFY(s.back() >= 40.0);
        QVERIFY(std::is_sorted(s.begin(), s.end()));
        QVERIFY(std::find(s.begin(), s.end(), 0.0) != s.end());
    }

    void timing()
    {
        /* Not a gate: what a 3 MP tick costs, for the record. */
        cv::Mat img(1512, 2016, CV_32FC3);
        cv::randu(img, 0.0f, 1.0f);
        cv::Mat d(img.size(), CV_32F);
        for (int y = 0; y < d.rows; ++y) d.row(y).setTo(float(y) / d.rows);
        Settings s;
        s.amount = 0.6f; s.focus = 0.3f; s.range = 0.1f;
        const cv::Mat coc = cocMap(d, s, 2016);
        for (int sh : {int(Circle), int(CatsEye)}) {
            QElapsedTimer t;
            t.start();
            render(img, coc, sh, 0.3f, Quality::draft());
            const qint64 draft = t.restart();
            render(img, coc, sh, 0.3f, Quality::final());
            qInfo("shape %d at 3 MP: draft %lld ms, final %lld ms", sh, draft, t.elapsed());
        }
    }
};

QTEST_GUILESS_MAIN(TstLensBlur)
#include "tst_lensblur.moc"
