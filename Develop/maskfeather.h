#ifndef MASKFEATHER_H
#define MASKFEATHER_H

/*
    SPATIAL feather for the alpha-map masks -- Subject/Background, Sky, Object and
    Segmenter.

    WHY THIS EXISTS. Those tools used to feather by re-thresholding their alpha with a
    Gaussian band around 0.5 (MaskFalloff::cdf). That is a curve on the alpha VALUE, so it
    can only reshape the transition the model already produced -- and the models produce
    a near-binary cutout, edge-snapped by SegRefine to a pixel or two. At Feather 100 the
    band was roughly the identity curve, and below it the band SHARPENED the matte. The
    slider moved and the edge did not, unlike Radial and Brush, whose feather is a
    distance.

    WHAT IT IS NOW. A Gaussian blur of the coverage map with sigma a fixed fraction of the
    IMAGE'S SHORT SIDE, so a feather value means the same softness at any render size, on
    the proxy and the export alike:

        sigma = kSigmaAt100 * (feather / 100) * short side
        feather 100 -> 10-90 transition ~10% of the short side
        feather  50 -> ~5%,   feather 10 -> ~1%

    Feather 0 is the map as it was (each tool's own feather-0 rule). Blurring is
    symmetric, so the half-coverage line stays where the cutout is and the transition
    spreads both ways, as Lightroom's does.

    BUILT ONCE PER (reference, feather), not per pixel. The render and the ImageView
    overlay both call cached() and sample the SAME map, so preview == render. A large
    sigma is blurred on a downscaled copy (sigma ~kWorkSigmaPx there) and upsampled
    bilinearly at sample time: a Gaussian that wide has nothing left at the scale the
    downscale throws away, and it keeps a 4096 px Sky matte at feather 100 to a few ms
    instead of a 1000-tap kernel over 11 MP.

    A CROPPED MAP (Object) IS PADDED by 3 sigma before the blur, inside the frame, so the
    feather can spread past the crop's edge instead of being cut off by it.
*/

#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <functional>
#include <list>
#include <QMutex>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace MaskFeather {

inline constexpr double kSigmaAt100 = 0.04;   // fraction of the image short side
inline constexpr double kWorkSigmaPx = 6.0;   // blur at most this sigma, in work pixels
inline constexpr float  kMinFeather = 0.05f;  // below this the map is used unfeathered

/* A feathered coverage map over [x0,x1]x[y0,y1] of the OUTPUT-normalized frame. */
struct Map {
    std::vector<float> cov;     // 0..1, row-major
    int w = 0, h = 0;
    double x0 = 0.0, y0 = 0.0, x1 = 1.0, y1 = 1.0;

    /* Bilinear; 0 outside the extent (only a padded Object crop has an outside). */
    float sample(double onx, double ony) const
    {
        if (onx < x0 || onx > x1 || ony < y0 || ony > y1) return 0.0f;
        const double fx = std::clamp((onx - x0) / (x1 - x0), 0.0, 1.0) * (w - 1);
        const double fy = std::clamp((ony - y0) / (y1 - y0), 0.0, 1.0) * (h - 1);
        const int ix0 = int(fx), iy0 = int(fy);
        const int ix1 = std::min(ix0 + 1, w - 1), iy1 = std::min(iy0 + 1, h - 1);
        const float tx = float(fx - ix0), ty = float(fy - iy0);
        const float *c = cov.data();
        const float c00 = c[size_t(iy0) * w + ix0], c10 = c[size_t(iy0) * w + ix1];
        const float c01 = c[size_t(iy1) * w + ix0], c11 = c[size_t(iy1) * w + ix1];
        const float top = c00 + (c10 - c00) * tx;
        const float bot = c01 + (c11 - c01) * tx;
        return top + (bot - top) * ty;
    }
};

/*
    Blur `base` (0..1, covering [x0,x1]x[y0,y1] of the frame) by the feather. The map's
    pixels are assumed square, so its full-frame size is base.cols / (x1-x0) by
    base.rows / (y1-y0).
*/
inline std::shared_ptr<const Map> build(const cv::Mat1f &base, double x0, double y0,
                                        double x1, double y1, double featherPct)
{
    auto out = std::make_shared<Map>();
    if (base.empty() || x1 <= x0 || y1 <= y0) return out;
    const double f = std::clamp(featherPct / 100.0, 0.0, 1.0);
    const double fullW = base.cols / (x1 - x0), fullH = base.rows / (y1 - y0);
    const double sigmaPx = kSigmaAt100 * f * std::min(fullW, fullH);

    /* Pad a crop by 3 sigma, never past the frame. A full-frame map gets no pad: its edge
       is the photo's edge, and BORDER_REPLICATE below keeps a subject that touches the
       frame from fading there. */
    const double reach = std::ceil(3.0 * sigmaPx);
    const int padL = int(std::min(reach, std::floor(x0 * fullW)));
    const int padT = int(std::min(reach, std::floor(y0 * fullH)));
    const int padR = int(std::min(reach, std::floor((1.0 - x1) * fullW)));
    const int padB = int(std::min(reach, std::floor((1.0 - y1) * fullH)));
    cv::Mat1f work;
    cv::copyMakeBorder(base, work, padT, padB, padL, padR, cv::BORDER_CONSTANT, 0);
    out->x0 = x0 - padL / fullW;
    out->y0 = y0 - padT / fullH;
    out->x1 = x1 + padR / fullW;
    out->y1 = y1 + padB / fullH;

    double s = sigmaPx;
    if (sigmaPx > kWorkSigmaPx) {
        const double scale = kWorkSigmaPx / sigmaPx;
        const int sw = std::max(2, int(std::lround(work.cols * scale)));
        const int sh = std::max(2, int(std::lround(work.rows * scale)));
        cv::Mat1f small;
        cv::resize(work, small, cv::Size(sw, sh), 0, 0, cv::INTER_AREA);
        work = small;
        s = sigmaPx * (double(sw) / (base.cols + padL + padR));
    }
    if (s > 0.0)
        cv::GaussianBlur(work, work, cv::Size(0, 0), s, s, cv::BORDER_REPLICATE);

    out->w = work.cols;
    out->h = work.rows;
    out->cov.assign(size_t(out->w) * size_t(out->h), 0.0f);
    for (int y = 0; y < out->h; ++y)
        std::copy(work.ptr<float>(y), work.ptr<float>(y) + out->w,
                  out->cov.data() + size_t(y) * out->w);
    return out;
}

/*
    Feather a whole-frame coverage buffer IN PLACE -- the mask-level Feather
    (EditScope::maskFeather), run on the folded mask in buildMaskBuffer. Same sigma rule
    as build() (a fraction of the buffer's short side), so the proxy, the veil and the
    export agree, and the same downscale for a wide sigma. The buffer is the whole frame,
    so its edge is the photo's edge: BORDER_REPLICATE keeps a mask that touches it from
    fading there.
*/
inline void blurInPlace(std::vector<float> &buf, int w, int h, double featherPct)
{
    if (featherPct < kMinFeather || w < 2 || h < 2) return;
    if (buf.size() != size_t(w) * size_t(h)) return;
    const double f = std::clamp(featherPct / 100.0, 0.0, 1.0);
    const double sigmaPx = kSigmaAt100 * f * std::min(w, h);
    cv::Mat1f full(h, w, buf.data());          // header over buf, no copy
    if (sigmaPx <= kWorkSigmaPx) {
        cv::GaussianBlur(full, full, cv::Size(0, 0), sigmaPx, sigmaPx, cv::BORDER_REPLICATE);
        return;
    }
    const double scale = kWorkSigmaPx / sigmaPx;
    const int sw = std::max(2, int(std::lround(w * scale)));
    const int sh = std::max(2, int(std::lround(h * scale)));
    cv::Mat1f small;
    cv::resize(full, small, cv::Size(sw, sh), 0, 0, cv::INTER_AREA);
    const double s = sigmaPx * (double(sw) / w);
    cv::GaussianBlur(small, small, cv::Size(0, 0), s, s, cv::BORDER_REPLICATE);
    cv::resize(small, full, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
}

/*
    The feathered map for (owner, feather), built by `make` on a miss. Keyed on the
    owner's ADDRESS and checked through a weak_ptr, so a ref freed and another allocated
    at the same address is a miss rather than a stale hit. A feather drag makes a map per
    tick, so the cache is small: the render and the overlay of the same tick are what it
    is for.
*/
inline std::shared_ptr<const Map> cached(const std::shared_ptr<const void> &owner,
                                         double featherPct,
                                         const std::function<std::shared_ptr<const Map>()> &make)
{
    struct Entry {
        const void *owner;
        std::weak_ptr<const void> alive;
        long key;
        std::shared_ptr<const Map> map;
    };
    static QMutex mutex;
    static std::list<Entry> lru;              // front = most recent
    const long key = std::lround(featherPct * 10.0);
    QMutexLocker lk(&mutex);
    for (auto it = lru.begin(); it != lru.end(); ) {
        if (it->alive.expired()) { it = lru.erase(it); continue; }
        if (it->owner == owner.get() && it->key == key) {
            lru.splice(lru.begin(), lru, it);
            return lru.front().map;
        }
        ++it;
    }
    /* Built under the lock: the render and the overlay usually ask for the same map at
       the same moment, and one of them waiting beats both building it. */
    auto map = make();
    lru.push_front({owner.get(), owner, key, map});
    while (lru.size() > 4) lru.pop_back();
    return map;
}

} // namespace MaskFeather

#endif // MASKFEATHER_H
