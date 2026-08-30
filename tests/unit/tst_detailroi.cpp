#include <QtTest>
#include "Develop/detailroi.h"

/*
    Detail-panel 1:1 preview geometry (Develop/detailroi.h) -- the mapping that turns the
    point the user picked on the loupe into a rectangle of SOURCE pixels to render at full
    resolution.

    The interesting failure here is silent: an off-by-one or a swapped sign in the
    rotation inverse still returns a plausible-looking rect, and the preview simply shows
    the wrong part of the picture -- or, worse, a rect that runs off the end of the pixel
    buffer. So these tests check the rotation inverse against the FORWARD mapping (the one
    documented in the header, and the one the render's EXIF rotate implements) rather than
    against hand-copied expected numbers, and they check the clamping that keeps a crop
    inside the buffer.
*/
class tst_detailroi : public QObject
{
    Q_OBJECT

private:
    /* The forward source -> oriented mapping the header documents, written out
       independently so the inverse is checked against it rather than against itself. */
    static void fwd(int x, int y, int w, int h, int degrees, int &ox, int &oy)
    {
        switch (((degrees % 360) + 360) % 360) {
        case 90:  ox = h - 1 - y; oy = x;         break;
        case 180: ox = w - 1 - x; oy = h - 1 - y; break;
        case 270: ox = y;         oy = w - 1 - x; break;
        default:  ox = x;         oy = y;         break;
        }
    }

private slots:

    /* The oriented frame swaps axes at 90 / 270 and only there. */
    void orientedSizeSwapsAtRightAngles()
    {
        int fw = 0, fh = 0;
        DetailRoi::orientedSize(6000, 4000, 0, fw, fh);
        QCOMPARE(fw, 6000); QCOMPARE(fh, 4000);
        DetailRoi::orientedSize(6000, 4000, 180, fw, fh);
        QCOMPARE(fw, 6000); QCOMPARE(fh, 4000);
        DetailRoi::orientedSize(6000, 4000, 90, fw, fh);
        QCOMPARE(fw, 4000); QCOMPARE(fh, 6000);
        DetailRoi::orientedSize(6000, 4000, 270, fw, fh);
        QCOMPARE(fw, 4000); QCOMPARE(fh, 6000);
    }

    /* A centred pick yields a full-size window centred on the point. */
    void centredPickIsCentred()
    {
        const DetailRoi::Rect r = DetailRoi::orientedRoi(0.5, 0.5, 6000, 4000, 300);
        QCOMPARE(r.w, 300);
        QCOMPARE(r.h, 300);
        QCOMPARE(r.x, 3000 - 150);
        QCOMPARE(r.y, 2000 - 150);
    }

    /* Near an edge the window slides inside the frame at FULL size rather than shrinking
       -- a magnifier that showed less image near the border would be worse than useless
       for judging a corner. */
    void edgePickClampsWithoutShrinking()
    {
        for (auto pt : {QPointF(0.0, 0.0), QPointF(1.0, 1.0), QPointF(0.0, 1.0)}) {
            const DetailRoi::Rect r =
                DetailRoi::orientedRoi(pt.x(), pt.y(), 6000, 4000, 300);
            QCOMPARE(r.w, 300);
            QCOMPARE(r.h, 300);
            QVERIFY(r.x >= 0 && r.x + r.w <= 6000);
            QVERIFY(r.y >= 0 && r.y + r.h <= 4000);
        }
    }

    /* A frame smaller than the window gives the whole frame, not a rect hanging off
       the end of it. */
    void tinyFrameYieldsWholeFrame()
    {
        const DetailRoi::Rect r = DetailRoi::orientedRoi(0.5, 0.5, 120, 90, 300);
        QCOMPARE(r.x, 0); QCOMPARE(r.y, 0);
        QCOMPARE(r.w, 120); QCOMPARE(r.h, 90);
    }

    /* THE test: every corner of the mapped SOURCE rect must land on the corresponding
       corner of the ORIENTED rect under the forward mapping. Run for all four rotations
       and for off-centre rects, which is where a sign error shows and a centred one
       hides. */
    void rotationInverseMatchesForwardMapping()
    {
        const int w = 600, h = 400;
        for (int degrees : {0, 90, 180, 270}) {
            int fw = 0, fh = 0;
            DetailRoi::orientedSize(w, h, degrees, fw, fh);
            for (auto pt : {QPointF(0.5, 0.5), QPointF(0.1, 0.2), QPointF(0.9, 0.15)}) {
                const DetailRoi::Rect o =
                    DetailRoi::orientedRoi(pt.x(), pt.y(), fw, fh, 100);
                const DetailRoi::Rect s =
                    DetailRoi::orientedRectToSource(o, w, h, degrees);

                /* Same pixel count, axes swapped at right angles. */
                QCOMPARE(s.w * s.h, o.w * o.h);
                if (degrees == 90 || degrees == 270) {
                    QCOMPARE(s.w, o.h);
                    QCOMPARE(s.h, o.w);
                }
                else {
                    QCOMPARE(s.w, o.w);
                    QCOMPARE(s.h, o.h);
                }

                /* Every source corner maps into the oriented rect. */
                const int xs[2] = {s.x, s.x + s.w - 1};
                const int ys[2] = {s.y, s.y + s.h - 1};
                for (int xi = 0; xi < 2; ++xi)
                    for (int yi = 0; yi < 2; ++yi) {
                        int ox = 0, oy = 0;
                        fwd(xs[xi], ys[yi], w, h, degrees, ox, oy);
                        QVERIFY2(ox >= o.x && ox < o.x + o.w && oy >= o.y && oy < o.y + o.h,
                                 qPrintable(QString("deg=%1 src(%2,%3) -> (%4,%5) outside "
                                                    "oriented rect %6,%7 %8x%9")
                                                .arg(degrees).arg(xs[xi]).arg(ys[yi])
                                                .arg(ox).arg(oy)
                                                .arg(o.x).arg(o.y).arg(o.w).arg(o.h)));
                    }
            }
        }
    }

    /* The crop must always be inside the pixel buffer -- this is the one that turns a
       geometry slip into a read past the end of WorkingImage::rgb. */
    void sourceRoiStaysInsideTheBuffer()
    {
        const int w = 613, h = 409;                 // deliberately not round
        for (int degrees : {0, 90, 180, 270})
            for (double nx : {-0.5, 0.0, 0.33, 1.0, 1.7})
                for (double ny : {-0.5, 0.0, 0.5, 1.0, 2.0}) {
                    const DetailRoi::Rect s =
                        DetailRoi::sourceRoi(nx, ny, w, h, degrees, 200);
                    QVERIFY(s.x >= 0 && s.y >= 0);
                    QVERIFY(s.w > 0 && s.h > 0);
                    QVERIFY(s.x + s.w <= w);
                    QVERIFY(s.y + s.h <= h);
                }
    }

    /* Degenerate inputs must yield an empty rect, not a negative-size one that would be
       read as a huge unsigned length by a crop loop. */
    void degenerateInputsAreEmpty()
    {
        QVERIFY(DetailRoi::orientedRoi(0.5, 0.5, 0, 0, 300).isEmpty());
        QVERIFY(DetailRoi::sourceRoi(0.5, 0.5, 0, 100, 0, 300).isEmpty());
        const DetailRoi::Rect empty;
        QVERIFY(DetailRoi::orientedRectToSource(empty, 100, 100, 90).isEmpty());
    }

    /* The requested side is clamped, so a caller cannot ask for a 1-pixel or a
       whole-image "preview". */
    void sizeIsClamped()
    {
        QCOMPARE(DetailRoi::orientedRoi(0.5, 0.5, 6000, 4000, 1).w, DetailRoi::kMinSize);
        QCOMPARE(DetailRoi::orientedRoi(0.5, 0.5, 6000, 4000, 99999).w,
                 DetailRoi::kMaxSize);
    }
};

QTEST_APPLESS_MAIN(tst_detailroi)
#include "tst_detailroi.moc"
