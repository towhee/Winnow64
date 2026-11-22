#include "ImageFormats/Raw/highlightrecon.h"
#include <opencv2/core.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <functional>
#include <vector>

/*
    See highlightrecon.h. A line-for-line port of LightCraft's highlight.rs (MIT): same
    constants, same order of operations. Row loops run on OpenCV's pool (cv::parallel_for_,
    as Demosaic: this runs on ImageCache decoder threads); every row writes only its own
    cells or pixels, so the result does not depend on the thread count.
*/

namespace {

/* Unclipped pixels lend their chromaticity in proportion to how close they come to
   clipping: nothing below SOURCE_LO of the clip, fully above SOURCE_HI. */
constexpr float kSourceLo = 0.25f;
constexpr float kSourceHi = 0.6f;
/* Sources on a strong edge (a 3x3 half-resolution neighbour brighter or darker by this
   many stops) are left out, fading from EDGE_LO to EDGE_HI. */
constexpr float kEdgeLo = 0.15f;
constexpr float kEdgeHi = 0.4f;
/* Source cells within this many half-resolution cells of a clipped pixel are left out
   (their channels were demosaiced partly from clipped samples). */
constexpr int kMargin = 1;
/* Weight of the coarser estimate against a cell's own sources when pushing down. */
constexpr float kPrior = 0.01f;
/* The rebuilt color fades to neutral where reliable content covers less than SUPPORT of
   the surroundings at every scale up to REACH of the long edge. */
constexpr float kReach = 1.0f / 4.0f;
constexpr float kSupport = 0.05f;
/* An unclipped channel above FLOOR_LO of its clip counts more and more as the floor it
   is about to become. */
constexpr float kFloorLo = 0.75f;
/* The rim: unclipped pixels above RIM_LO of the clip, weighted towards it. Up to
   NEAR_RIM stops past it a pixel takes its color, then over DEPTH stops goes over to the
   estimate. */
constexpr float kRimLo = 0.85f;
constexpr float kNearRim = 0.03f;
constexpr float kDepth = 0.2f;
/* Borrowed color counts less as pixel and rim contradict it by AGREE0..AGREE1 stops. */
constexpr float kAgree0 = 0.15f, kAgree1 = 0.4f;
/* Saturation below PALE0 fades its color to neutral from the first clip; above PALE1
   only in the second half of the way. */
constexpr float kPale0 = 0.2f, kPale1 = 0.4f;

using V3 = std::array<float, 3>;

inline float Smoothstep(float e0, float e1, float x)
{
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float Max3(const V3 &v) { return std::max(v[0], std::max(v[1], v[2])); }

/* One pyramid level, w x h cells of N-1 values and a weight. Pulled: per cell the
   weighted sums and the weight, as densities. Pushed: the estimated values, and still the
   weight. */
template <int N>
struct Level {
    int w = 0, h = 0;
    std::vector<std::array<float, N>> cells;
};

/* Bilinear sample of a level at the centre of pixel (x, y) of a grid scale times finer. */
template <int N>
std::array<float, N> Sample(const Level<N> &l, float scale, int x, int y)
{
    const int cw = l.w, ch = l.h;
    const float fx = std::clamp((x + 0.5f) / scale - 0.5f, 0.0f, static_cast<float>(std::max(cw - 1, 0)));
    const float fy = std::clamp((y + 0.5f) / scale - 0.5f, 0.0f, static_cast<float>(std::max(ch - 1, 0)));
    const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
    const int x1 = std::min(x0 + 1, std::max(cw - 1, 0)), y1 = std::min(y0 + 1, std::max(ch - 1, 0));
    const float tx = fx - x0, ty = fy - y0;
    auto g = [&](int xx, int yy) {
        const size_t i = static_cast<size_t>(yy) * cw + xx;
        return i < l.cells.size() ? l.cells[i] : std::array<float, N>{};
    };
    const auto a = g(x0, y0), b = g(x1, y0), c = g(x0, y1), d = g(x1, y1);
    std::array<float, N> out;
    for (int k = 0; k < N; ++k) {
        const float top = a[k] + (b[k] - a[k]) * tx;
        const float bot = c[k] + (d[k] - c[k]) * tx;
        out[k] = top + (bot - top) * ty;
    }
    return out;
}

struct Image {
    float *d;
    int w, h;
    V3 Px(size_t i) const { return { d[i * 3], d[i * 3 + 1], d[i * 3 + 2] }; }
};

/* Which channels of p are clipped. */
inline std::array<bool, 3> Clipped(const V3 &p, float clip)
{
    return { p[0] >= clip, p[1] >= clip, p[2] >= clip };
}
inline bool Any(const std::array<bool, 3> &c) { return c[0] || c[1] || c[2]; }

/* The half-resolution source level: every 2x2 block's source-weighted chromaticity sums,
   blocks near clipped pixels or on strong edges left out. near[cell] = a clipped pixel
   lies in its row within kMargin cells (the vertical half is left to the readers). */
Level<3> Sources(const Image &img, const V3 &wb, float clip, std::vector<char> &near)
{
    const int w = img.w, h = img.h;
    const int cw = (w + 1) / 2, ch = (h + 1) / 2;
    Level<3> L;
    L.w = cw;
    L.h = ch;
    L.cells.assign(static_cast<size_t>(cw) * ch, {0, 0, 0});
    std::vector<float> lum(static_cast<size_t>(cw) * ch, 0.0f);
    near.assign(static_cast<size_t>(cw) * ch, 0);
    const float lo = kSourceLo * clip;

    cv::parallel_for_(cv::Range(0, ch), [&](const cv::Range &r) {
        std::vector<char> touched(cw);
        for (int y = r.start; y < r.end; ++y) {
            for (int x = 0; x < cw; ++x) {
                float a[3] = {0, 0, 0}, sum = 0;
                int n = 0;
                bool hit = false;
                for (int sy = 2 * y; sy < std::min(2 * y + 2, h); ++sy)
                    for (int sx = 2 * x; sx < std::min(2 * x + 2, w); ++sx) {
                        const size_t i = static_cast<size_t>(sy) * w + sx;
                        const V3 p = img.Px(i);
                        const V3 q = { p[0] * wb[0], p[1] * wb[1], p[2] * wb[2] };
                        const float s = std::max(q[0], 0.0f) + std::max(q[1], 0.0f) + std::max(q[2], 0.0f);
                        sum += s;
                        ++n;
                        const float top = Max3(p);
                        if (Any(Clipped(p, clip))) { hit = true; continue; }
                        if (top > lo && s > 1e-4f) {
                            const float b = Smoothstep(kSourceLo, kSourceHi, top / clip) / s;
                            a[0] += b * std::max(q[0], 0.0f);
                            a[1] += b * std::max(q[2], 0.0f);
                            a[2] += b * s;
                        }
                    }
                const float k = 1.0f / std::max(n, 1);
                L.cells[static_cast<size_t>(y) * cw + x] = { a[0] * k, a[1] * k, a[2] * k };
                lum[static_cast<size_t>(y) * cw + x] = std::max(sum * k, 1e-6f);
                touched[x] = hit;
            }
            for (int x = 0; x < cw; ++x) {
                bool any = false;
                for (int xx = std::max(x - kMargin, 0); xx <= std::min(x + kMargin, cw - 1); ++xx)
                    any = any || touched[xx];
                near[static_cast<size_t>(y) * cw + x] = any;
            }
        }
    });

    /* Leave out cells near clipped pixels and on strong edges (luminance ratios). */
    const float edgeLo = std::exp2(kEdgeLo), edgeHi = std::exp2(kEdgeHi);
    cv::parallel_for_(cv::Range(0, ch), [&](const cv::Range &r) {
        for (int y = r.start; y < r.end; ++y) {
            const int ya = std::max(y - kMargin, 0), yb = std::min(y + kMargin, ch - 1);
            const int ea = std::max(y - 1, 0), eb = std::min(y + 1, ch - 1);
            for (int x = 0; x < cw; ++x) {
                auto &cell = L.cells[static_cast<size_t>(y) * cw + x];
                if (cell[2] <= 0.0f) continue;
                bool isNear = false;
                for (int yy = ya; yy <= yb; ++yy) isNear = isNear || near[static_cast<size_t>(yy) * cw + x];
                if (isNear) { cell = {0, 0, 0}; continue; }
                const float l = lum[static_cast<size_t>(y) * cw + x];
                const int xa = std::max(x - 1, 0), xb = std::min(x + 1, cw - 1);
                float mn = l, mx = l;
                for (int yy = ea; yy <= eb; ++yy)
                    for (int xx = xa; xx <= xb; ++xx) {
                        const float v = lum[static_cast<size_t>(yy) * cw + xx];
                        mn = std::min(mn, v);
                        mx = std::max(mx, v);
                    }
                const float keep = 1.0f - Smoothstep(edgeLo, edgeHi, std::max(mx / l, l / mn));
                for (float &v : cell) v *= keep;
            }
        }
    });
    return L;
}

/* The quarter-resolution rim level: every 4x4 block's white-balanced RGB of its unclipped
   pixels above kRimLo of the clip, weighted steeply towards it, as densities. Pixels next
   to clipped ones (margin(half-res x, y)) are left out. */
template <class Margin>
Level<4> Rim(const Image &img, const V3 &wb, float clip, const Margin &margin)
{
    const int w = img.w, h = img.h;
    const int cw = (w + 3) / 4, ch = (h + 3) / 4;
    Level<4> L;
    L.w = cw;
    L.h = ch;
    L.cells.assign(static_cast<size_t>(cw) * ch, {0, 0, 0, 0});
    cv::parallel_for_(cv::Range(0, ch), [&](const cv::Range &r) {
        for (int y = r.start; y < r.end; ++y)
            for (int x = 0; x < cw; ++x) {
                float a[4] = {0, 0, 0, 0};
                int n = 0;
                for (int sy = 4 * y; sy < std::min(4 * y + 4, h); ++sy)
                    for (int sx = 4 * x; sx < std::min(4 * x + 4, w); ++sx) {
                        ++n;
                        const V3 p = img.Px(static_cast<size_t>(sy) * w + sx);
                        const float top = Max3(p) / clip;
                        if (!(top >= kRimLo && top < 1.0f) || margin(sx / 2, sy / 2) || Any(Clipped(p, clip)))
                            continue;
                        const float t = (top - kRimLo) / (1.0f - kRimLo);
                        const float k = t * t * t * t;
                        for (int c = 0; c < 3; ++c) a[c] += k * p[c] * wb[c];
                        a[3] += k;
                    }
                const float inv = 1.0f / std::max(n, 1);
                L.cells[static_cast<size_t>(y) * cw + x] = { a[0] * inv, a[1] * inv, a[2] * inv, a[3] * inv };
            }
    });
    return L;
}

/* The next coarser level: each 2x2 block's mean densities. */
template <int N>
Level<N> Pull(const Level<N> &l)
{
    Level<N> o;
    o.w = (l.w + 1) / 2;
    o.h = (l.h + 1) / 2;
    o.cells.assign(static_cast<size_t>(o.w) * o.h, std::array<float, N>{});
    cv::parallel_for_(cv::Range(0, o.h), [&](const cv::Range &r) {
        for (int y = r.start; y < r.end; ++y)
            for (int x = 0; x < o.w; ++x) {
                std::array<float, N> s{};
                int n = 0;
                for (int k = 0; k < 4; ++k) {
                    const int sx = 2 * x + (k & 1), sy = 2 * y + (k >> 1);
                    if (sx < l.w && sy < l.h) {
                        const auto &c = l.cells[static_cast<size_t>(sy) * l.w + sx];
                        for (int j = 0; j < N; ++j) s[j] += c[j];
                        ++n;
                    }
                }
                for (int j = 0; j < N; ++j) s[j] /= std::max(n, 1);
                o.cells[static_cast<size_t>(y) * o.w + x] = s;
            }
    });
    return o;
}

template <int N>
std::vector<Level<N>> Pyramid(Level<N> first)
{
    std::vector<Level<N>> levels;
    levels.push_back(std::move(first));
    while (levels.back().w > 1 || levels.back().h > 1) levels.push_back(Pull(levels.back()));
    return levels;
}

/* Estimates of levels[0]'s cells that needed marks (all when null), and of every cell of
   the coarser levels: each level's own sources blended with the next coarser estimate.
   The top level is its own weighted mean, or empty (at zero weight) when it has none. */
template <int N>
void Push(std::vector<Level<N>> &levels, const std::array<float, N> &empty,
          const std::function<bool(int, int)> *needed)
{
    if (levels.empty()) return;
    for (auto &c : levels.back().cells) {
        const float weight = c[N - 1];
        if (weight > 0.0f) {
            for (int k = 0; k < N - 1; ++k) c[k] /= weight;
        } else {
            c = empty;
        }
    }
    for (int i = static_cast<int>(levels.size()) - 2; i >= 0; --i) {
        Level<N> &fine = levels[i];
        const Level<N> &coarse = levels[i + 1];
        const std::function<bool(int, int)> *need = (i == 0) ? needed : nullptr;
        cv::parallel_for_(cv::Range(0, fine.h), [&](const cv::Range &r) {
            for (int y = r.start; y < r.end; ++y)
                for (int x = 0; x < fine.w; ++x) {
                    if (need && !(*need)(x, y)) continue;
                    auto &cell = fine.cells[static_cast<size_t>(y) * fine.w + x];
                    const auto prior = Sample(coarse, 2.0f, x, y);
                    const float weight = cell[N - 1];
                    const float d = weight + kPrior;
                    for (int k = 0; k < N - 1; ++k) cell[k] = (cell[k] + kPrior * prior[k]) / d;
                    cell[N - 1] = weight;
                }
        });
    }
}

/* How many stops two chromaticities differ by in any channel ratio. */
float Unlike(const V3 &a, const V3 &b)
{
    auto ratio = [&](int i, int j) {
        return (std::max(a[i], 1e-6f) * std::max(b[j], 1e-6f)) / (std::max(a[j], 1e-6f) * std::max(b[i], 1e-6f));
    };
    float m = 0.0f;
    for (auto [i, j] : { std::pair{0, 1}, std::pair{0, 2}, std::pair{1, 2} })
        m = std::max(m, std::fabs(std::log2(ratio(i, j))));
    return m;
}

/* How far channel c only bounds its true value from below: fully once clipped, and
   increasingly from kFloorLo of the clip on. */
inline float Bound(const V3 &p, const std::array<bool, 3> &cl, float clip, int c)
{
    return cl[c] ? 1.0f : Smoothstep(kFloorLo, 1.0f, p[c] / clip);
}

/* How many stops the channels of a partly clipped pixel contradict chromaticity r. */
float Contradiction(const V3 &p, const V3 &q, const std::array<bool, 3> &cl, const V3 &r, float clip)
{
    float lo = INFINITY, hi = 0.0f;
    for (int c = 0; c < 3; ++c) {
        const float level = q[c] / std::max(r[c], 1e-3f);
        hi = std::max(hi, level);
        const float w = Bound(p, cl, clip, c);
        if (r[c] > 1e-3f && w < 1.0f) lo = std::min(lo, level / (1.0f - w));
    }
    return (hi > 0.0f && lo > 0.0f && std::isfinite(lo)) ? std::max(std::log2(hi / lo), 0.0f) : 0.0f;
}

/* The white-balanced color of a partly clipped pixel estimated at chromaticity r:
   clipped channels rebuilt, unclipped kept. False when no unclipped channel can tell the
   level. */
bool Rebuild(const V3 &p, const V3 &q, const std::array<bool, 3> &cl, const V3 &r, float clip,
             float maxLevel, V3 &out)
{
    float sum = 0.0f, nearClip = 0.0f;
    int k = 0;
    for (int c = 0; c < 3; ++c) {
        if (!cl[c] && r[c] > 1e-3f) {
            const float s = q[c] / r[c];
            sum += s;
            nearClip = std::max(nearClip, s * Smoothstep(kFloorLo, 1.0f, p[c] / clip));
            ++k;
        }
    }
    if (k == 0) return false;
    /* Two clipped channels must not be left at their clip levels (they differ by the
       white-balance gains and would read magenta or red): they keep the estimate's ratio
       at the level that lifts each to its clip, and so does a channel about to clip. */
    float floor = 0.0f;
    for (int c = 0; c < 3; ++c)
        if (cl[c]) floor = std::max(floor, q[c] / std::max(r[c], 1e-3f));
    const float top = std::max(Max3(r), 1e-3f);
    sum = std::max(sum / k, std::min(std::max(nearClip, floor), maxLevel / top));
    for (int c = 0; c < 3; ++c) out[c] = cl[c] ? std::max(q[c], sum * r[c]) : q[c];
    return true;
}

} // namespace

int HighlightRecon::Reconstruct(float *rgb, int w, int h, const float wbIn[3], float clip,
                                const float whiteIn[3])
{
    if (!rgb || w <= 0 || h <= 0) return 0;
    const Image img{ rgb, w, h };
    const V3 wb = { wbIn[0], wbIn[1], wbIn[2] };

    std::atomic<int> count{0};
    cv::parallel_for_(cv::Range(0, h), [&](const cv::Range &r) {
        int k = 0;
        for (int y = r.start; y < r.end; ++y)
            for (int x = 0; x < w; ++x)
                if (Any(Clipped(img.Px(static_cast<size_t>(y) * w + x), clip))) ++k;
        count += k;
    });
    if (count == 0) return 0;

    /* a white far from the camera's neutral (or not a number) is a broken color model */
    bool plausible = true;
    for (int c = 0; c < 3; ++c)
        plausible = plausible && std::isfinite(whiteIn[c]) && whiteIn[c] >= 1.0f / 16.0f && whiteIn[c] <= 16.0f;
    const V3 white = plausible ? V3{ whiteIn[0], whiteIn[1], whiteIn[2] } : V3{ 1.0f, 1.0f, 1.0f };
    const float total = white[0] + white[1] + white[2];
    const V3 neutral = { white[0] / total, white[1] / total, white[2] / total };

    /* The color field: half-resolution sources diffused coarse to fine. */
    std::vector<char> near;
    std::vector<Level<3>> levels = Pyramid(Sources(img, wb, clip, near));
    const int cw = (w + 1) / 2, ch = (h + 1) / 2;
    /* a clipped pixel samples the cells within one of its own, which are within one of a
       clipped cell */
    const std::function<bool(int, int)> needed = [&](int x, int y) {
        for (int yy = std::max(y - 1, 0); yy <= std::min(y + 1, ch - 1); ++yy)
            if (near[static_cast<size_t>(yy) * cw + x]) return true;
        return false;
    };
    Push(levels, { neutral[0], neutral[2], 0.0f }, &needed);

    /* How much reliable content lies around each needed cell, at any scale up to kReach;
       kept in the third entry of the level-0 estimates. */
    const float reach = kReach * static_cast<float>(std::max(w, h)) / 2.0f;
    int widest = static_cast<int>(levels.size()) - 1;
    for (int i = 0; i < static_cast<int>(levels.size()); ++i)
        if (static_cast<float>(1ULL << std::min(i, 62)) >= reach) { widest = i; break; }
    {
        Level<3> &first = levels[0];
        std::vector<std::pair<const Level<3> *, float>> support;
        for (int i = 0; i < widest && i + 1 < static_cast<int>(levels.size()); ++i)
            support.push_back({ &levels[i + 1], static_cast<float>(1ULL << std::min(i + 1, 62)) });
        cv::parallel_for_(cv::Range(0, first.h), [&](const cv::Range &r) {
            for (int y = r.start; y < r.end; ++y)
                for (int x = 0; x < first.w; ++x) {
                    if (!needed(x, y)) continue;
                    float t = 0.0f;
                    for (const auto &[l, scale] : support) {
                        t = std::max(t, Smoothstep(0.0f, kSupport, Sample(*l, scale, x, y)[2]));
                        if (t >= 1.0f) break;
                    }
                    first.cells[static_cast<size_t>(y) * first.w + x][2] = t;
                }
        });
    }
    const Level<3> &field = levels[0];

    /* The rim (level 0 of its pyramid holds the estimate), if there is one anywhere. */
    std::vector<Level<4>> rims = Pyramid(Rim(img, wb, clip, needed));
    Push<4>(rims, { 0, 0, 0, 0 }, nullptr);
    bool haveRim = false;
    for (const auto &c : rims.back().cells) haveRim = haveRim || c[3] > 0.0f;
    const Level<4> *rimField = haveRim ? &rims[0] : nullptr;

    const float maxLevel = std::max(wb[0], std::max(wb[1], wb[2])) * clip;

    cv::parallel_for_(cv::Range(0, h), [&](const cv::Range &r) {
        for (int y = r.start; y < r.end; ++y)
            for (int x = 0; x < w; ++x) {
                const size_t i = static_cast<size_t>(y) * w + x;
                const V3 p = img.Px(i);
                const std::array<bool, 3> cl = Clipped(p, clip);
                if (!Any(cl)) continue;
                const V3 q = { p[0] * wb[0], p[1] * wb[1], p[2] * wb[2] };

                /* rebuilt with the color of the reliable content around, and with
                   neutral, blended by how much of it there is and how well the pixel
                   agrees with it */
                const auto e = Sample(field, 2.0f, x, y);
                const V3 borrowed = { e[0], 1.0f - e[0] - e[1], e[1] };
                std::array<float, 4> edge{};
                const bool hasEdge = rimField != nullptr;
                if (hasEdge) edge = Sample(*rimField, 4.0f, x, y);
                float trust = std::clamp(e[2], 0.0f, 1.0f);
                if (trust > 0.0f) {
                    float off = Contradiction(p, q, cl, borrowed, clip);
                    if (off > kAgree0) {
                        float rimOff = 0.0f;
                        if (hasEdge) {
                            const float s = edge[0] + edge[1] + edge[2];
                            if (s > 1e-6f) rimOff = Unlike({ edge[0] / s, edge[1] / s, edge[2] / s }, borrowed);
                        }
                        off = std::min(off, rimOff);
                    }
                    trust *= 1.0f - Smoothstep(kAgree0, kAgree1, off);
                }

                /* either applies only as far as the pixel lies past the rim nearest its
                   clip; nearer, the pixel is rebuilt with the rim's color */
                bool atRim = false;
                V3 rimA{};
                float rimK = 0.0f;
                if (hasEdge) {
                    float ratio = 1.0f;
                    for (int c = 0; c < 3; ++c) {
                        const float a = std::max(q[c], 1e-12f), b = std::max(edge[c], 1e-12f);
                        float rc;
                        if (a >= b) rc = a / b;
                        else {
                            const float wgt = Bound(p, cl, clip, c);
                            rc = wgt <= 0.0f ? b / a : (wgt >= 1.0f ? 1.0f : std::pow(b / a, 1.0f - wgt));
                        }
                        ratio = std::max(ratio, rc);
                    }
                    const float past = std::min(std::log2(ratio), 64.0f);
                    const float k = Smoothstep(kNearRim, kNearRim + kDepth, past);
                    const float s = edge[0] + edge[1] + edge[2];
                    if (k < 1.0f && s > 1e-6f &&
                        Rebuild(p, q, cl, { edge[0] / s, edge[1] / s, edge[2] / s }, clip, maxLevel, rimA)) {
                        atRim = true;
                        rimK = k;
                    }
                }
                auto pastRim = [&](V3 b) {
                    if (!atRim) return b;
                    for (int c = 0; c < 3; ++c) b[c] = rimA[c] + (b[c] - rimA[c]) * rimK;
                    return b;
                };
                V3 local{}, plain{};
                const bool hasLocal = trust > 0.0f && Rebuild(p, q, cl, borrowed, clip, maxLevel, local);
                const bool hasPlain = trust < 1.0f && Rebuild(p, q, cl, neutral, clip, maxLevel, plain);
                if (hasLocal) local = pastRim(local);
                if (hasPlain) plain = pastRim(plain);
                V3 out;
                if (hasLocal && hasPlain) {
                    for (int c = 0; c < 3; ++c) out[c] = plain[c] + (local[c] - plain[c]) * trust;
                } else if (hasLocal) {
                    out = local;
                } else if (hasPlain) {
                    out = plain;
                } else {
                    const float v = std::max(Max3(q), maxLevel);
                    for (int c = 0; c < 3; ++c) out[c] = white[c] * v;
                }

                /* Fade into the fully clipped neutral on the way from the first channel
                   clipping to the last, measured in stops: how far the rebuilt clipped
                   channels lie above their clip, against how far the lowest unclipped
                   channel still lies below its own. */
                float above = 0.0f, lowest = FLT_MAX;
                for (int c = 0; c < 3; ++c) {
                    if (cl[c]) above = std::max(above, std::log2(std::max(out[c] / std::max(q[c], 1e-9f), 1.0f)));
                    else lowest = std::min(lowest, p[c]);
                }
                const float below = std::max(std::log2(clip / std::max(lowest, 1e-6f)), 0.0f);
                const float way = (above > 0.0f && lowest < clip) ? above / (above + below) : 0.0f;
                /* Brightness goes over in the second half of the way, and so does the
                   color of a strongly colored pixel, or of one whose OWN color is still
                   measured: only one channel clipped and the higher unclipped one well
                   below its clip (white paint with green clipped; a faint rust stain
                   on it is real R:B data). Otherwise a pale pixel's color goes over
                   from the start. "Measured" fades out as that channel nears its clip,
                   so nothing steps when a second channel clips (Winnow, not
                   LightCraft: Lightroom keeps that color, 2026-11-22 ferry, see
                   Documentation 10.1.2). */
                const V3 u = { out[0] / white[0], out[1] / white[1], out[2] / white[2] };
                const float lo = std::min(u[0], std::min(u[1], u[2]));
                const float hi = Max3(u);
                const float saturation = (hi > 0.0f && std::isfinite(hi)) ? 1.0f - std::max(lo, 0.0f) / hi : 1.0f;
                float measured = 0.0f;
                if (int(cl[0]) + int(cl[1]) + int(cl[2]) == 1) {
                    float second = 0.0f;
                    for (int c = 0; c < 3; ++c) if (!cl[c]) second = std::max(second, p[c]);
                    measured = 1.0f - Smoothstep(kFloorLo, 1.0f, second / clip);
                }
                const float start = 0.5f * std::max(Smoothstep(kPale0, kPale1, saturation), measured);
                const float tLevel = Smoothstep(0.5f, 1.0f, way);
                const float tColor = Smoothstep(start, start + 0.5f, way);
                if (tColor > 0.0f) {
                    const float level = (out[0] + out[1] + out[2]) / total;
                    const float full = std::max(Max3(q), maxLevel);
                    const float v = level + (full - level) * tLevel;
                    for (int c = 0; c < 3; ++c) out[c] = out[c] + (white[c] * v - out[c]) * tColor;
                }
                for (int c = 0; c < 3; ++c) rgb[i * 3 + c] = out[c] / wb[c];
            }
    });
    return count;
}
