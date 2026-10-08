#ifndef MASKREFINE_H
#define MASKREFINE_H

/*
    REFINE BRUSH: re-matte the mask folded so far, inside a painted region, against the
    photo's own colours.

    THE PROBLEM. Every automatic mask (Select Sky, Subject, Object, ...) gets some areas
    semantically wrong -- a glacier or a moon counted as sky, snow on branches counted as
    sky, sky holes in a treeline missed -- or leaves a halo where its edge does not sit on
    the real one. A plain Brush add/subtract fixes coverage but not edges: painting a
    skyline by hand is tedious and never lands on the pixel boundary.

    WHAT THIS DOES. The user paints a REGION (a Refine Brush submask,
    MaskTool::RefineBrush). Inside it, the mask is re-matted with colour models taken only
    from the neighbourhood of the stroke -- where sky and foreground are separable even
    when they are not across the whole frame. Three modes:

      ADD     the unmasked pixels under the stroke are split RELATIVELY (k-means in a
              chroma-weighted space; the cluster nearest the mask colour is the mask's),
              the mask-like ones seed a LOCAL S and the rest F, and the matte between
              them adds the holes (sky through branches) but not the branches or snow.
              The mask colour comes from masked pixels near the stroke, or the nearest
              masked pixels anywhere when none are near. A fixed "looks like S"
              distance (0.08) added nothing on Paradise Meadows, where the sky through
              the trees is hazier and paler than the open sky (0.13-0.31 away).
      REMOVE  the TRUE mask colour is learned from masked pixels OUTSIDE the stroke;
              masked pixels under the stroke that are clearly unlike it are the
              intruders (glacier, moon, foreground snow) and are matted out. Painting
              over real sky changes nothing. "Clearly unlike" is adaptive -- Otsu on the
              under-stroke distances -- because sky near a horizon is paler than the sky
              the reference comes from, and a fixed threshold removed real sky
              (measured on Joffrey Lake).
      EDGE    the existing edge under the stroke is re-matted with local S and F, then
              snapped beyond half the photo's edge width and decontaminated -- the same
              steps that removed the ridgeline halos in the sky matte.

    IT IS A FOLD STEP, not a coverage: buildMaskBuffer runs it on the buffer holding
    every submask ABOVE it (so it refines any mask), and the result is blended in by the
    stroke's own coverage, so a soft brush edge fades the refinement out. Pure function of the
    stroke, the mode, the folded mask and the guide -- what the per-scope render cache and
    the fold-prefix cache assume.

    THE GUIDE is the UNDEVELOPED working image converted to the working space and
    display gamma (MW::buildRefineGuide), for the same reason MaskHalo's is: a guide that
    moved with the sliders would make the mask chase the adjustment it masks.

    All radii scale with the buffer's long edge L, so a proxy and a full-res render agree.
    Prototyped and measured in Python first (2026-10-07): foreground snow and snow clumps
    removed with the sky between them untouched; glacier removed without the paler sky
    above it; ridge halos under a Fix Edge stroke -66% (Dempster) / -36% (Joffrey).
    Header-only (all inline), OpenCV core + imgproc, no Qt. Pinned by
    tests/unit/tst_maskrefine.cpp.
*/

#include "Utilities/skyrefine.h"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace MaskRefine {

enum class Mode { Add, Remove, Edge };

namespace detail {

using SkyRefine::smoothstep;
using SkyRefine::forRows;

/* A smooth colour field filled in from the pixels where `wt` is solid, at half
   resolution (a local mean is all the projection needs). count = solid pixels used. */
inline cv::Mat fill(const cv::Mat &img, const cv::Mat &wt, int &count)
{
    const cv::Size hs(std::max(1, img.cols / 2), std::max(1, img.rows / 2));
    cv::Mat s, ws;
    cv::resize(img, s, hs, 0, 0, cv::INTER_AREA);
    cv::resize(wt, ws, hs, 0, 0, cv::INTER_AREA);
    cv::threshold(ws, ws, 0.99, 1.0, cv::THRESH_BINARY);
    count = cv::countNonZero(ws);
    cv::Mat f;
    cv::resize(SkyRefine::pushPull(s, ws), f, img.size(), 0, 0, cv::INTER_LINEAR);
    return f;
}

/* Projection of each pixel on the local F->S line: t (clamped), the line's separation,
   and how much to trust t (well separated AND the pixel near the line). */
inline void project(const cv::Mat &rgb, const cv::Mat &S, const cv::Mat &F,
                    cv::Mat &t, cv::Mat &sep, cv::Mat &conf)
{
    t.create(rgb.size(), CV_32F); sep.create(rgb.size(), CV_32F); conf.create(rgb.size(), CV_32F);
    forRows(rgb.rows, [&](int y) {
        const cv::Vec3f *ip = rgb.ptr<cv::Vec3f>(y), *sp = S.ptr<cv::Vec3f>(y),
                        *fp = F.ptr<cv::Vec3f>(y);
        float *tp = t.ptr<float>(y), *qp = sep.ptr<float>(y), *cp = conf.ptr<float>(y);
        for (int x = 0; x < rgb.cols; ++x) {
            const cv::Vec3f d = sp[x] - fp[x];
            const float dd = d.dot(d);
            const float a = std::clamp((ip[x] - fp[x]).dot(d) / std::max(dd, 1e-6f), 0.0f, 1.0f);
            const float r = float(cv::norm(ip[x] - (fp[x] + a * d)));
            tp[x] = a;
            qp[x] = std::sqrt(dd);
            cp[x] = smoothstep(0.06f, 0.18f, qp[x]) * (1.0f - smoothstep(0.05f, 0.15f, r));
        }
    });
}

/* Per-pixel colour distance |rgb - S|. */
inline cv::Mat distTo(const cv::Mat &rgb, const cv::Mat &S)
{
    cv::Mat d(rgb.size(), CV_32F);
    forRows(rgb.rows, [&](int y) {
        const cv::Vec3f *ip = rgb.ptr<cv::Vec3f>(y), *sp = S.ptr<cv::Vec3f>(y);
        float *dp = d.ptr<float>(y);
        for (int x = 0; x < rgb.cols; ++x) dp[x] = float(cv::norm(ip[x] - sp[x]));
    });
    return d;
}

/* The colour alpha snapped beyond half the photo's edge width; lerped toward `fallback`
   (0/1 by "looks like the mask colour") where the projection is not trusted. */
inline cv::Mat matte(const cv::Mat &rgb, const cv::Mat &S, const cv::Mat &F,
                     const cv::Mat &likeMask, int L, bool decon)
{
    cv::Mat t, sep, conf;
    project(rgb, S, F, t, sep, conf);
    SkyRefine::snapBeyondEdgeWidth(t, SkyRefine::photoEdgeWidth(rgb, sep, L));
    if (decon) SkyRefine::decontaminate(t, rgb);
    forRows(t.rows, [&](int y) {
        float *tp = t.ptr<float>(y);
        const float *cp = conf.ptr<float>(y);
        const uchar *lp = likeMask.ptr<uchar>(y);
        for (int x = 0; x < t.cols; ++x)
            tp[x] = tp[x] * cp[x] + (1.0f - cp[x]) * (lp[x] ? 1.0f : 0.0f);
    });
    return t;
}

/* ADD's colour space: Y plus chroma weighted kChromaW times. Pale horizon sky and lit
   snow have nearly the same RGB (and the same Y); they differ in chroma, and in plain RGB
   k-means put them in one cluster and added the snowy treetops. The brush Auto mask
   weights chroma for the same reason. */
constexpr float kChromaW = 3.0f;
inline cv::Vec3f chroma(const cv::Vec3f &c)
{
    const float Y = 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
    return {Y, kChromaW * 0.564f * (c[2] - Y), kChromaW * 0.713f * (c[0] - Y)};
}

/* Mean colour of the masked pixels nearest the stroke's box (sx0..sx1, sy0..sy1), for an
   Add stroke with no masked pixel in its own context. The search widens (2M, 4M, ...)
   until it finds some, then keeps the K nearest -- NOT every masked pixel in reach: on
   Paradise Meadows that mean was the deep zenith blue, not the nearer sky. False if the
   mask is empty. */
inline bool nearestMaskColour(const std::vector<float> &m, const cv::Mat &rgb, int w, int h,
                              int sx0, int sy0, int sx1, int sy1, int M, cv::Vec3f &out)
{
    const int L = std::max(w, h);
    const size_t K = size_t(std::max(50, (L / 100) * (L / 100)));
    for (int r = 2 * M; ; r *= 2) {
        const int x0 = std::max(0, sx0 - r), y0 = std::max(0, sy0 - r);
        const int x1 = std::min(w - 1, sx1 + r), y1 = std::min(h - 1, sy1 + r);
        std::vector<std::pair<long long, int>> hits;     // (distance^2, pixel index)
        for (int y = y0; y <= y1; ++y) {
            const float *mp = m.data() + size_t(y) * w;
            const long long dy = y < sy0 ? sy0 - y : (y > sy1 ? y - sy1 : 0);
            for (int x = x0; x <= x1; ++x)
                if (mp[x] > 0.95f) {
                    const long long dx = x < sx0 ? sx0 - x : (x > sx1 ? x - sx1 : 0);
                    hits.push_back({dx * dx + dy * dy, y * w + x});
                }
        }
        if (hits.size() >= 20) {
            const size_t k = std::min(K, hits.size());
            std::nth_element(hits.begin(), hits.begin() + long(k - 1), hits.end());
            cv::Vec3d acc(0, 0, 0);
            for (size_t i = 0; i < k; ++i)
                acc += cv::Vec3d(rgb.at<cv::Vec3f>(hits[i].second / w, hits[i].second % w));
            out = cv::Vec3f(acc / double(k));
            return true;
        }
        if (x0 == 0 && y0 == 0 && x1 == w - 1 && y1 == h - 1) return false;
    }
}

/*
    ADD's split of the unmasked pixels under the stroke into "mask-like" and foreground.
    A fixed distance from the mask colour (0.08, the old rule) failed in the field: sky
    seen through branches is hazier and paler than the sky the reference comes from
    (0.13-0.31 away on Paradise Meadows), so the holes counted as foreground and nothing
    was added. The split is RELATIVE instead: k-means (k = 4, chroma space) on the
    pixels under the stroke, the cluster nearest the mask colour s0 is the mask's (merged
    with any cluster within kMerge of it -- a sky gradient is one class), and each pixel
    goes to whichever side's nearest centre is closer. A nearest cluster more than kGuard
    from s0 means nothing under the stroke looks like the mask: returns false. (The guard
    is loose on purpose: on the real image the chosen cluster was <= 0.33 even for a
    stroke over snow only, while a saturated test sky put real haze at 0.41-0.51. It
    cannot tell shaded snow from hazy sky; the user's stroke is the semantic cue.)

    far   = foreground (CV_8U, the box), clear = clearly mask-like (seeds the local S).
    k-means is seeded from a fixed RNG state: the render caches need a pure function.
*/
constexpr int   kClusters = 4;
constexpr float kMerge = 0.15f, kGuard = 0.6f;
inline bool splitUnderStroke(const cv::Mat &ic, const cv::Mat &under, const cv::Vec3f &s0,
                             cv::Mat &far, cv::Mat &clear)
{
    const int n = cv::countNonZero(under);
    if (n < 50) return false;
    const int stride = std::max(1, n / 20000);
    std::vector<cv::Vec3f> pts;
    pts.reserve(size_t(n / stride + 1));
    int k = 0;
    for (int y = 0; y < ic.rows; ++y) {
        const cv::Vec3f *ip = ic.ptr<cv::Vec3f>(y);
        const uchar *up = under.ptr<uchar>(y);
        for (int x = 0; x < ic.cols; ++x)
            if (up[x] && k++ % stride == 0) pts.push_back(chroma(ip[x]));
    }
    const cv::Mat samples = cv::Mat(pts).reshape(1);   // N x 3, CV_32F
    cv::Mat labels, C;
    cv::RNG &rng = cv::theRNG();
    const uint64 saved = rng.state;
    rng.state = 0x2545F4914F6CDD1DULL;
    cv::kmeans(samples, kClusters, labels,
               cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::COUNT, 20, 1e-3),
               3, cv::KMEANS_PP_CENTERS, C);
    rng.state = saved;

    auto centre = [&](int j) { return cv::Vec3f(C.ptr<float>(j)); };
    int best = 0;
    for (int j = 1; j < C.rows; ++j)
        if (cv::norm(centre(j) - s0) < cv::norm(centre(best) - s0)) best = j;
    if (cv::norm(centre(best) - s0) > kGuard) return false;
    std::vector<cv::Vec3f> mask, other;
    for (int j = 0; j < C.rows; ++j)
        (j == best || cv::norm(centre(j) - centre(best)) < kMerge ? mask : other)
            .push_back(centre(j));

    far.create(ic.size(), CV_8U); clear.create(ic.size(), CV_8U);
    forRows(ic.rows, [&](int y) {
        const cv::Vec3f *ip = ic.ptr<cv::Vec3f>(y);
        uchar *fp = far.ptr<uchar>(y), *cp = clear.ptr<uchar>(y);
        for (int x = 0; x < ic.cols; ++x) {
            const cv::Vec3f c = chroma(ip[x]);
            float dm = 1e9f, dn = 1e9f;
            for (const auto &q : mask)  dm = std::min(dm, float(cv::norm(c - q)));
            for (const auto &q : other) dn = std::min(dn, float(cv::norm(c - q)));
            fp[x] = dm >= dn ? 255 : 0;
            cp[x] = dm < 0.5f * dn ? 255 : 0;
        }
    });
    return true;
}

}   // namespace detail

/*
    Refine m (w*h, the mask folded so far) in place, inside roi (w*h brush coverage 0..1),
    using rgb (CV_32FC3 w x h, RGB, display-referred 0..1) as the guide.
*/
inline void apply(std::vector<float> &m, const std::vector<float> &roi, const cv::Mat &rgb,
                  int w, int h, Mode mode)
{
    using namespace detail;
    if (w <= 0 || h <= 0 || m.size() != size_t(w) * h || roi.size() != m.size()
        || rgb.cols != w || rgb.rows != h || rgb.type() != CV_32FC3) return;

    /* The stroke's bounding box, widened by a context margin the colour models are drawn
       from. Everything below runs on that crop only -- a stroke costs its own area. */
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y) {
        const float *rp = roi.data() + size_t(y) * w;
        for (int x = 0; x < w; ++x)
            if (rp[x] > 0.01f) { x0 = std::min(x0, x); x1 = std::max(x1, x);
                                 y0 = std::min(y0, y); y1 = std::max(y1, y); }
    }
    if (x1 < 0) return;
    const int L = std::max(w, h);
    const int M = std::max(16, L / 30);
    const int sx0 = x0, sy0 = y0, sx1 = x1, sy1 = y1;   // the stroke's own box
    x0 = std::max(0, x0 - M); y0 = std::max(0, y0 - M);
    x1 = std::min(w - 1, x1 + M); y1 = std::min(h - 1, y1 + M);
    const cv::Rect box(x0, y0, x1 - x0 + 1, y1 - y0 + 1);

    const cv::Mat mFull(h, w, CV_32F, m.data());
    const cv::Mat rFull(h, w, CV_32F, const_cast<float *>(roi.data()));
    const cv::Mat mc = mFull(box).clone(), rc = rFull(box).clone();
    const cv::Mat ic = rgb(box).clone();

    const cv::Mat inside = mc > 0.95f, outside = mc < 0.05f;
    const float tau = 0.08f;
    auto toF = [](const cv::Mat &b) { cv::Mat f; b.convertTo(f, CV_32F, 1.0 / 255.0); return f; };
    cv::Mat result;

    if (mode == Mode::Remove) {
        const cv::Mat refSet = inside & (rc < 0.05f);
        int nS = 0;
        const cv::Mat Sref = fill(ic, toF(refSet), nS);
        if (nS < 20) {                                   // no reference: plain subtract
            result = mc.mul(1.0f - rc);
        }
        else {
            const cv::Mat dist = distTo(ic, Sref);
            /* Adaptive "clearly unlike the mask colour": at least tau, at least twice
               how far the reference itself strays from its local estimate, and at least
               the Otsu split of the masked pixels UNDER the stroke. */
            std::vector<float> refD, inD;
            const cv::Mat under = inside & (rc > 0.5f);
            for (int y = 0; y < dist.rows; ++y) {
                const float *dp = dist.ptr<float>(y);
                const uchar *rp = refSet.ptr<uchar>(y), *up = under.ptr<uchar>(y);
                for (int x = 0; x < dist.cols; ++x) {
                    if (rp[x]) refD.push_back(dp[x]);
                    if (up[x]) inD.push_back(dp[x]);
                }
            }
            float tauA = tau;
            if (!refD.empty()) {
                const size_t k = size_t(0.95 * double(refD.size() - 1));
                std::nth_element(refD.begin(), refD.begin() + long(k), refD.end());
                tauA = std::max(tauA, 2.0f * refD[k]);
            }
            if (inD.size() > 50) {
                const float mx = std::max(*std::max_element(inD.begin(), inD.end()), 1e-6f);
                cv::Mat q(int(inD.size()), 1, CV_8U);
                for (size_t i = 0; i < inD.size(); ++i)
                    q.at<uchar>(int(i)) = uchar(std::clamp(inD[i] / mx * 255.0f, 0.0f, 255.0f));
                cv::Mat dummy;
                const double th = cv::threshold(q, dummy, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
                /* Otsu's lower class is the bins AT OR BELOW th, so the cut is that bin's
                   UPPER edge. th/255 itself rounded a whole uniform class (pale sky all in
                   one bin) just past the cut and removed it -- tst_maskrefine caught it. */
                tauA = std::max(tauA, float((th + 1.0) / 255.0) * mx);
            }
            const cv::Mat far = dist > tauA;
            int nF = 0;
            const cv::Mat F = fill(ic, toF(inside & (rc > 0.5f) & far), nF);
            if (nF < 20) {
                result = mc.clone();                    // all of it IS mask colour
            }
            else {
                const cv::Mat a = matte(ic, Sref, F, ~far, L, /*decon*/false);
                cv::min(mc, a, result);
            }
        }
    }
    else if (mode == Mode::Add) {
        int nS = 0;
        const cv::Mat S = fill(ic, toF(inside), nS);
        cv::Vec3f s0;                                    // the mask colour, chroma space
        bool haveS = nS >= 20;
        if (haveS) {
            cv::Vec3d acc(0, 0, 0); int n = 0;
            for (int y = 0; y < ic.rows; ++y) {
                const cv::Vec3f *sp = S.ptr<cv::Vec3f>(y);
                const uchar *op = outside.ptr<uchar>(y);
                const float *rp = rc.ptr<float>(y);
                for (int x = 0; x < ic.cols; ++x)
                    if (op[x] && rp[x] > 0.5f) { acc += cv::Vec3d(chroma(sp[x])); ++n; }
            }
            if (n) s0 = cv::Vec3f(acc / n);
            else   haveS = false;
        }
        if (!haveS) {
            cv::Vec3f c;
            if (nearestMaskColour(m, rgb, w, h, sx0, sy0, sx1, sy1, M, c)) {
                s0 = chroma(c); haveS = true;
            }
        }
        if (!haveS) cv::max(mc, rc, result);             // empty mask: plain add
        else {
            cv::Mat far, clear;
            if (!splitUnderStroke(ic, outside & (rc > 0.5f), s0, far, clear)) {
                result = mc.clone();                     // nothing mask-like under it
            }
            else {
                /* The LOCAL mask colour: masked context plus the clearly mask-like
                   pixels under the stroke -- the hole's own sky, not the reference. */
                int nS2 = 0, nF = 0;
                const cv::Mat S2 = fill(ic, toF(inside | (outside & (rc > 0.5f) & clear)), nS2);
                const cv::Mat F = fill(ic, toF(outside & far), nF);
                cv::Mat a;
                if (nF < 20) toF(~far).copyTo(a);        // nothing unlike S: lookalikes
                else         a = matte(ic, S2, F, ~far, L, /*decon*/true);
                cv::max(mc, a, result);
            }
        }
    }
    else {                                               // Fix Edge
        int nS = 0, nF = 0;
        const cv::Mat S = fill(ic, toF(inside), nS);
        const cv::Mat F = fill(ic, toF(outside), nF);
        if (nS < 20 || nF < 20) return;                  // no edge to work from
        cv::Mat t, sep, conf;
        project(ic, S, F, t, sep, conf);
        cv::Mat band = (mc > 0.02f) & (mc < 0.98f);
        cv::dilate(band, band, SkyRefine::ellipse(std::max(2, L / 200)));
        result = mc.clone();
        forRows(result.rows, [&](int y) {
            float *op = result.ptr<float>(y);
            const float *tp = t.ptr<float>(y), *cp = conf.ptr<float>(y);
            const uchar *bp = band.ptr<uchar>(y);
            for (int x = 0; x < result.cols; ++x)
                if (bp[x]) op[x] = op[x] * (1.0f - cp[x]) + tp[x] * cp[x];
        });
        SkyRefine::snapBeyondEdgeWidth(result, SkyRefine::photoEdgeWidth(ic, sep, L));
        SkyRefine::decontaminate(result, ic);
    }

    /* Blend by the stroke's own coverage: a soft brush edge fades the refinement out. */
    forRows(box.height, [&](int y) {
        float *op = m.data() + size_t(y + box.y) * w + box.x;
        const float *mp = mc.ptr<float>(y), *rp = rc.ptr<float>(y), *np = result.ptr<float>(y);
        for (int x = 0; x < box.width; ++x)
            op[x] = mp[x] * (1.0f - rp[x]) + std::clamp(np[x], 0.0f, 1.0f) * rp[x];
    });
}

}   // namespace MaskRefine

#endif // MASKREFINE_H
