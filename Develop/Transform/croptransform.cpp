#include "Develop/Transform/croptransform.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <QTransform>
#include <QPolygonF>
#include <vector>
#include <cmath>
#include <cstring>

namespace {

/* Largest all-valid (non-zero) axis-aligned rectangle in a binary mask, by the classic
   per-row histogram + monotonic-stack method (O(rows*cols)). mask is CV_8U (0 / non-zero). */
cv::Rect largestValidRect(const cv::Mat &mask)
{
    const int rows = mask.rows, cols = mask.cols;
    std::vector<int> h(cols, 0);
    int bestArea = 0;
    cv::Rect best(0, 0, 0, 0);
    std::vector<int> st;
    st.reserve(cols + 1);

    for (int y = 0; y < rows; ++y) {
        const uchar *m = mask.ptr<uchar>(y);
        for (int x = 0; x < cols; ++x) h[x] = m[x] ? h[x] + 1 : 0;

        st.clear();
        for (int x = 0; x <= cols; ++x) {
            const int curH = (x == cols) ? 0 : h[x];
            while (!st.empty() && h[st.back()] >= curH) {
                const int height = h[st.back()];
                st.pop_back();
                const int left = st.empty() ? 0 : st.back() + 1;
                const int width = x - left;
                const int area = height * width;
                if (area > bestArea) {
                    bestArea = area;
                    best = cv::Rect(left, y - height + 1, width, height);
                }
            }
            st.push_back(x);
        }
    }
    return best;
}

/* QImage -> owned BGRA cv::Mat (ARGB32's little-endian byte order IS B,G,R,A). */
cv::Mat toBgra(const QImage &img)
{
    const QImage c = img.convertToFormat(QImage::Format_ARGB32);
    cv::Mat view(c.height(), c.width(), CV_8UC4,
                 const_cast<uchar *>(c.bits()), c.bytesPerLine());
    return view.clone();
}

/* QImage -> owned 16-bit RGBA cv::Mat, the 16-bit twin of toBgra for a deep (export)
   image. RGBA64 is R,G,B,A as quint16 -- a different channel ORDER to toBgra's BGRA,
   which does not matter here: warpPerspective is geometric, and only channel 3 (alpha,
   the same index in both) is read, for the validity mask. */
cv::Mat toRgba64(const QImage &img)
{
    const QImage c = img.convertToFormat(QImage::Format_RGBA64);
    cv::Mat view(c.height(), c.width(), CV_16UC4,
                 const_cast<uchar *>(c.bits()), c.bytesPerLine());
    return view.clone();
}

/* True for the 16-bit-per-channel QImage formats the export path produces. Everything
   downstream that has to pick a working format branches on this so a 16-bit export is not
   silently flattened to 8 bits by an intermediate stage. */
bool isDeep(const QImage &img)
{
    return img.format() == QImage::Format_RGBX64 ||
           img.format() == QImage::Format_RGBA64 ||
           img.format() == QImage::Format_RGBA64_Premultiplied;
}

inline double dist(const cv::Point2f &a, const cv::Point2f &b)
{
    return std::hypot(double(a.x) - b.x, double(a.y) - b.y);
}

} // namespace

QImage CropTransform::rectifyPerspective(const QImage &src, const QPointF quad[4],
                                         QRectF &outCropNorm)
{
    if (src.isNull()) return QImage();

    std::vector<cv::Point2f> srcPts = {
        {float(quad[0].x()), float(quad[0].y())},
        {float(quad[1].x()), float(quad[1].y())},
        {float(quad[2].x()), float(quad[2].y())},
        {float(quad[3].x()), float(quad[3].y())}
    };

    /* Destination rectangle size = average of opposite edge lengths (preserves the selected
       region's rough scale). Reject a degenerate quad. */
    const double wTop = dist(srcPts[0], srcPts[1]);
    const double wBot = dist(srcPts[3], srcPts[2]);
    const double hLft = dist(srcPts[0], srcPts[3]);
    const double hRgt = dist(srcPts[1], srcPts[2]);
    const double W = (wTop + wBot) / 2.0;
    const double H = (hLft + hRgt) / 2.0;
    if (W < 2.0 || H < 2.0) return QImage();

    const std::vector<cv::Point2f> dstPts = {
        {0.f, 0.f}, {float(W), 0.f}, {float(W), float(H)}, {0.f, float(H)}
    };
    const cv::Mat Hm = cv::getPerspectiveTransform(srcPts, dstPts);

    /* Warp the whole image: find the bounds of the four image corners under Hm so nothing is
       clipped, then translate the transform into a positive-origin output canvas. */
    const std::vector<cv::Point2f> imgCorners = {
        {0.f, 0.f}, {float(src.width()), 0.f},
        {float(src.width()), float(src.height())}, {0.f, float(src.height())}
    };
    std::vector<cv::Point2f> warpedCorners;
    cv::perspectiveTransform(imgCorners, warpedCorners, Hm);

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (const auto &p : warpedCorners) {
        minX = std::min(minX, p.x); minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y);
    }
    /* Clamp the output canvas so an extreme/near-degenerate warp can't blow up memory. */
    const int outW = std::min(20000, std::max(2, int(std::ceil(maxX - minX))));
    const int outH = std::min(20000, std::max(2, int(std::ceil(maxY - minY))));

    cv::Mat T = (cv::Mat_<double>(3, 3) << 1, 0, -double(minX),
                                            0, 1, -double(minY),
                                            0, 0, 1);
    const cv::Mat Hfull = T * Hm;

    /* Warp at the SOURCE's bit depth: a 16-bit export image goes through a CV_16UC4
       warp, an 8-bit one through CV_8UC4 as before. Routing everything through 8 bits
       here would silently flatten a 16-bit export as soon as the recipe held a warp. */
    const bool deep = isDeep(src);
    cv::Mat dst;
    cv::warpPerspective(deep ? toRgba64(src) : toBgra(src), dst, Hfull,
                        cv::Size(outW, outH), cv::INTER_LINEAR, cv::BORDER_CONSTANT,
                        cv::Scalar(0, 0, 0, 0));

    /* Validity mask = solidly opaque pixels (drop the interpolated semi-transparent
       fringe). The largest-inscribed-rectangle search runs on a downscaled mask for
       speed, then scales back. */
    std::vector<cv::Mat> ch;
    cv::split(dst, ch);
    cv::Mat mask = ch[3] >= (deep ? 250 * 257 : 250);   // same cut-off at either depth

    const int kMaxSearch = 1000;
    const double s = std::min(1.0, double(kMaxSearch) / std::max(outW, outH));
    cv::Rect best;
    if (s < 1.0) {
        cv::Mat small;
        cv::resize(mask, small, cv::Size(), s, s, cv::INTER_NEAREST);
        const cv::Rect r = largestValidRect(small);
        best = cv::Rect(int(r.x / s), int(r.y / s),
                        int(r.width / s), int(r.height / s));
    } else {
        best = largestValidRect(mask);
    }
    best &= cv::Rect(0, 0, outW, outH);
    if (best.width <= 0 || best.height <= 0) best = cv::Rect(0, 0, outW, outH);

    outCropNorm = QRectF(double(best.x) / outW, double(best.y) / outH,
                         double(best.width) / outW, double(best.height) / outH);

    /* cv::Mat -> QImage, same byte order in both cases (BGRA -> ARGB32's little-endian
       bytes; RGBA-16 -> RGBA64). */
    QImage out(dst.cols, dst.rows, deep ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
    const size_t rowBytes = size_t(dst.cols) * (deep ? 8 : 4);
    for (int y = 0; y < dst.rows; ++y)
        std::memcpy(out.scanLine(y), dst.ptr(y), rowBytes);
    return out;
}

QImage CropTransform::applyGeometry(const QImage &src, const Geometry &g)
{
    if (src.isNull() || g.isIdentity()) return src;

    QImage img = src;

    /* 1) Straighten: rotate about the centre, expanding the canvas (the crop trims the
       corners). Convert to an ALPHA format first so the exposed wedges are TRANSPARENT
       (not black), matching the warp and letting straightenCropNorm / the view background
       behave. A 16-bit (export) image takes RGBA64 so the rotation does not flatten it to
       8 bits; everything else keeps ARGB32. */
    if (g.straighten != 0.0) {
        QTransform t;
        t.rotate(g.straighten);
        const QImage::Format alphaFmt =
            isDeep(img) ? QImage::Format_RGBA64 : QImage::Format_ARGB32;
        img = img.convertToFormat(alphaFmt).transformed(t, Qt::SmoothTransformation);
    }

    /* 2) Warp: rectify the 4-point quad (its corners are in this image's normalized space). */
    if (g.hasWarp) {
        QPointF quadPx[4];
        for (int i = 0; i < 4; ++i)
            quadPx[i] = QPointF(g.quad[i * 2] * img.width(), g.quad[i * 2 + 1] * img.height());
        QRectF ignore;
        const QImage warped = rectifyPerspective(img, quadPx, ignore);
        if (!warped.isNull()) img = warped;
    }

    /* 3) Crop: g.crop is normalized in the post-warp output space. */
    if (!g.cropIsIdentity()) {
        const QRect cri = QRectF(g.cropX * img.width(),  g.cropY * img.height(),
                                 g.cropW * img.width(),  g.cropH * img.height())
                              .toRect().intersected(img.rect());
        if (cri.isValid() && cri != img.rect()) img = img.copy(cri);
    }
    return img;
}

QTransform CropTransform::geometryTransform(double srcW, double srcH, const Geometry &g,
                                            QSizeF *outSize)
{
    /* Mirrors applyGeometry step for step, in points instead of pixels. QTransform
       composes in APPLICATION order (p * A * B), so each stage appends on the right. */
    QTransform t;
    double W = srcW, H = srcH;
    if (W <= 0.0 || H <= 0.0) { if (outSize) *outSize = QSizeF(); return t; }

    /* 1) Straighten. QImage::transformed() rotates and then shifts the result into a
       positive-origin canvas -- the rotation centre drops out of that pair, so rotating
       about the origin and translating by -boundingRect.topLeft() gives the same map. */
    if (g.straighten != 0.0) {
        QTransform r;
        r.rotate(g.straighten);
        const QRectF bb = r.mapRect(QRectF(0.0, 0.0, W, H));
        QTransform shift;
        shift.translate(-bb.left(), -bb.top());
        t = t * r * shift;
        W = bb.width(); H = bb.height();
    }

    /* 2) Warp: the same quad -> upright-rectangle homography rectifyPerspective builds,
       plus its translation into the positive-origin output canvas. */
    if (g.hasWarp) {
        QPolygonF srcQuad;
        for (int i = 0; i < 4; ++i)
            srcQuad << QPointF(g.quad[i * 2] * W, g.quad[i * 2 + 1] * H);
        auto len = [](const QPointF &a, const QPointF &b) {
            return std::hypot(a.x() - b.x(), a.y() - b.y());
        };
        const double dW = (len(srcQuad[0], srcQuad[1]) + len(srcQuad[3], srcQuad[2])) / 2.0;
        const double dH = (len(srcQuad[0], srcQuad[3]) + len(srcQuad[1], srcQuad[2])) / 2.0;
        QTransform h;
        if (dW >= 2.0 && dH >= 2.0) {
            QPolygonF dstQuad;
            dstQuad << QPointF(0, 0) << QPointF(dW, 0) << QPointF(dW, dH) << QPointF(0, dH);
            if (QTransform::quadToQuad(srcQuad, dstQuad, h)) {
                const QRectF bb = h.mapRect(QRectF(0.0, 0.0, W, H));
                QTransform shift;
                shift.translate(-bb.left(), -bb.top());
                t = t * h * shift;
                W = std::min(20000.0, std::max(2.0, std::ceil(bb.width())));
                H = std::min(20000.0, std::max(2.0, std::ceil(bb.height())));
            }
        }
    }

    /* 3) Crop: g.crop is normalized in the post-warp output space. */
    if (!g.cropIsIdentity()) {
        QTransform c;
        c.translate(-g.cropX * W, -g.cropY * H);
        t = t * c;
        W *= g.cropW; H *= g.cropH;
    }

    if (outSize) *outSize = QSizeF(W, H);
    return t;
}

QRectF CropTransform::straightenCropNorm(double W, double H, double deg)
{
    if (W <= 0.0 || H <= 0.0 || deg == 0.0) return QRectF(0, 0, 1, 1);

    const double a = deg * 0.017453292519943295;   // radians
    const double c = std::abs(std::cos(a));
    const double s = std::abs(std::sin(a));
    const double canvasW = W * c + H * s;           // bounding box of the rotated frame
    const double canvasH = W * s + H * c;

    /* rotatedRectWithMaxArea: largest axis-aligned rect (canvas axes) of pure content. */
    const double longSide  = std::max(W, H);
    const double shortSide = std::min(W, H);
    const bool   wLonger   = (W >= H);
    double wr, hr;
    if (shortSide <= 2.0 * s * c * longSide || std::abs(s - c) < 1e-10) {
        const double x = 0.5 * shortSide;
        if (wLonger) { wr = (s > 1e-9) ? x / s : W; hr = (c > 1e-9) ? x / c : H; }
        else         { wr = (c > 1e-9) ? x / c : W; hr = (s > 1e-9) ? x / s : H; }
    } else {
        const double cos2a = c * c - s * s;
        wr = (W * c - H * s) / cos2a;
        hr = (H * c - W * s) / cos2a;
    }
    wr = std::max(0.0, std::min(wr, canvasW));
    hr = std::max(0.0, std::min(hr, canvasH));

    return QRectF((canvasW - wr) / 2.0 / canvasW, (canvasH - hr) / 2.0 / canvasH,
                  wr / canvasW, hr / canvasH);
}
