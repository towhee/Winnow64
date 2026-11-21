#include "patchinpaint.h"

#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

/*
    See patchinpaint.h. A line-for-line port of LightCraft's patchmatch.rs / membrane.rs,
    on interleaved float buffers; rayon's par_chunks become cv::parallel_for_ over the
    same fixed bands, so the per-band RNG seeds (and the result) do not depend on the
    thread count.
*/

namespace {

using Pt = std::pair<int, int>;
constexpr float kInf = std::numeric_limits<float>::infinity();
/* Per pixel: the 3 colour channels, then 2 TEXTURE features (Winnow addition, after
   Newson et al., "Video Inpainting of Complex Scenes", SIAM J. Imaging Sci. 2014): the
   local mean of |d/dx| and |d/dy| of the gray, scaled by Params::textureWeight. They
   ride through the pyramid, the membrane, the match distance and the vote like colour,
   so a patch in a textured surround matches textured sources. Colour alone prefers a
   FLAT source for the membrane-smoothed hole interior, and the fill stayed flat. */
constexpr int kColour = 3;
constexpr int kCh = 5;
/* Below this many cells a level runs single-threaded (hand-off costs more than it saves). */
constexpr int kParMin = 128 * 128;

/* SplitMix64: a tiny deterministic RNG. */
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ^ 0x9E3779B97F4A7C15ull) {}
    uint64_t Next()
    {
        s += 0x9E3779B97F4A7C15ull;
        uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    /* Uniform in lo..hi inclusive. */
    int Range(int lo, int hi)
    {
        if (hi <= lo) return lo;
        return lo + int(Next() % uint64_t(hi - lo + 1));
    }
};

struct Level {
    int w = 0, h = 0;
    std::vector<float> img;     // w*h*3
    std::vector<uint8_t> hole;  // w*h
};

Level Downsample(const Level &l)
{
    Level d;
    d.w = (l.w + 1) / 2; d.h = (l.h + 1) / 2;
    d.img.assign(size_t(d.w) * d.h * kCh, 0.0f);
    d.hole.assign(size_t(d.w) * d.h, 0);
    for (int y = 0; y < d.h; ++y)
        for (int x = 0; x < d.w; ++x) {
            float n = 0;
            const size_t o = (size_t(y) * d.w + x) * kCh;
            for (int k = 0; k < 4; ++k) {
                const int sx = x * 2 + (k & 1), sy = y * 2 + (k >> 1);
                if (sx >= l.w || sy >= l.h) continue;
                const size_t si = size_t(sy) * l.w + sx;
                if (l.hole[si]) d.hole[size_t(y) * d.w + x] = 1;
                else {
                    for (int c = 0; c < kCh; ++c) d.img[o + c] += l.img[si * kCh + c];
                    n += 1;
                }
            }
            if (n > 0) for (int c = 0; c < kCh; ++c) d.img[o + c] /= n;
        }
    return d;
}

/* Summed-area table of a 0/1 grid, (w+1) x (h+1). */
std::vector<uint32_t> Integral(int w, int h, const std::vector<uint8_t> &m)
{
    std::vector<uint32_t> s(size_t(w + 1) * (h + 1), 0);
    for (int y = 0; y < h; ++y) {
        uint32_t row = 0;
        for (int x = 0; x < w; ++x) {
            row += m[size_t(y) * w + x] ? 1 : 0;
            s[size_t(y + 1) * (w + 1) + x + 1] = s[size_t(y) * (w + 1) + x + 1] + row;
        }
    }
    return s;
}

uint32_t WindowCount(const std::vector<uint32_t> &s, int gw, int gh, int x0, int y0, int x1, int y1)
{
    x0 = std::max(x0, 0); y0 = std::max(y0, 0);
    x1 = std::min(std::max(x1, 0), gw); y1 = std::min(std::max(y1, 0), gh);
    if (x1 <= x0 || y1 <= y0) return 0;
    const size_t w1 = size_t(gw) + 1;
    return s[y1 * w1 + x1] + s[y0 * w1 + x0] - s[y0 * w1 + x1] - s[y1 * w1 + x0];
}

/* ---------------- membrane (cascadic multigrid + SOR) ---------------- */

constexpr float kTol = 2e-5f;
constexpr int kFineIters = 30;
constexpr float kFineOmega = 1.7f;

void Sor(int w, int h, const std::vector<uint8_t> &unknown, std::vector<float> &v, int maxIter, float omega)
{
    std::vector<std::pair<uint32_t, uint8_t>> cells;
    for (int i = 0; i < w * h; ++i) {
        if (!unknown[i]) continue;
        const int x = i % w, y = i / w;
        const uint8_t m = uint8_t((x > 0) | ((x + 1 < w) << 1) | ((y > 0) << 2) | ((y + 1 < h) << 3));
        cells.push_back({uint32_t(i), m});
    }
    for (int it = 0; it < maxIter; ++it) {
        float maxD = 0;
        for (const auto &[ui, m] : cells) {
            const size_t i = ui;
            float s = 0, n = 0;
            if (m == 0xF) { s = v[i - 1] + v[i + 1] + v[i - w] + v[i + w]; n = 4; }
            else {
                if (m & 1) { s += v[i - 1]; n += 1; }
                if (m & 2) { s += v[i + 1]; n += 1; }
                if (m & 4) { s += v[i - w]; n += 1; }
                if (m & 8) { s += v[i + w]; n += 1; }
                if (n == 0) continue;
            }
            const float d = omega * (s / n - v[i]);
            v[i] += d;
            maxD = std::max(maxD, std::abs(d));
        }
        if (maxD < kTol) break;
    }
}

void SolveRec(int w, int h, const std::vector<uint8_t> &unknown, std::vector<float> &v, int depth)
{
    int nUnknown = 0, nKnown = 0;
    double knownSum = 0;
    for (int i = 0; i < w * h; ++i) {
        if (unknown[i]) ++nUnknown;
        else { ++nKnown; knownSum += v[i]; }
    }
    if (nUnknown == 0 || nKnown == 0) return;
    if (w >= 8 && h >= 8 && nUnknown > 64 && depth < 16) {
        const int cw = (w + 1) / 2, ch = (h + 1) / 2;
        std::vector<uint8_t> cu(size_t(cw) * ch, 1);
        std::vector<float> cv(size_t(cw) * ch, 0.0f);
        for (int cy = 0; cy < ch; ++cy)
            for (int cx = 0; cx < cw; ++cx) {
                float s = 0; int n = 0;
                for (int k = 0; k < 4; ++k) {
                    const int x = cx * 2 + (k & 1), y = cy * 2 + (k >> 1);
                    if (x < w && y < h && !unknown[size_t(y) * w + x]) { s += v[size_t(y) * w + x]; ++n; }
                }
                if (n) { cu[size_t(cy) * cw + cx] = 0; cv[size_t(cy) * cw + cx] = s / n; }
            }
        SolveRec(cw, ch, cu, cv, depth + 1);
        for (int y = 0; y < h; ++y) {
            const float fy = std::clamp((y + 0.5f) / 2.0f - 0.5f, 0.0f, float(ch - 1));
            const int y0 = int(std::floor(fy)), y1 = std::min(y0 + 1, ch - 1);
            const float ty = fy - y0;
            for (int x = 0; x < w; ++x) {
                if (!unknown[size_t(y) * w + x]) continue;
                const float fx = std::clamp((x + 0.5f) / 2.0f - 0.5f, 0.0f, float(cw - 1));
                const int x0 = int(std::floor(fx)), x1 = std::min(x0 + 1, cw - 1);
                const float tx = fx - x0;
                const float a = cv[size_t(y0) * cw + x0] + (cv[size_t(y0) * cw + x1] - cv[size_t(y0) * cw + x0]) * tx;
                const float b = cv[size_t(y1) * cw + x0] + (cv[size_t(y1) * cw + x1] - cv[size_t(y1) * cw + x0]) * tx;
                v[size_t(y) * w + x] = a + (b - a) * ty;
            }
        }
        Sor(w, h, unknown, v, kFineIters, kFineOmega);
    } else {
        const float mean = float(knownSum / nKnown);
        for (int i = 0; i < w * h; ++i) if (unknown[i]) v[i] = mean;
        Sor(w, h, unknown, v, 2000, 1.85f);
    }
}

/* Membrane on an interleaved 3-channel buffer, channel by channel. */
void MembraneFill(int w, int h, std::vector<float> &img, const std::vector<uint8_t> &hole)
{
    std::vector<float> v(size_t(w) * h);
    for (int c = 0; c < kCh; ++c) {
        for (size_t i = 0; i < v.size(); ++i) v[i] = img[i * kCh + c];
        SolveRec(w, h, hole, v, 0);
        for (size_t i = 0; i < v.size(); ++i) img[i * kCh + c] = v[i];
    }
}

/* ---------------- PatchMatch / EM solver for one level ---------------- */

struct Solver {
    int w, h, r;
    std::vector<float> &img;
    const std::vector<uint8_t> &hole;
    std::vector<uint8_t> valid;     // whole patch inside the grid and hole-free
    std::vector<Pt> validList;
    std::vector<uint8_t> targets;   // patch overlaps the hole

    bool IsValid(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < w && y < h && valid[size_t(y) * w + x];
    }

    /* Mean SSD between the (clipped) target patch at t and the source patch at s. */
    float Dist(Pt t, Pt s, float cutoff) const
    {
        const float full = float((2 * r + 1) * (2 * r + 1)) * kCh;
        float sum = 0;
        int n = 0;
        for (int dy = -r; dy <= r; ++dy) {
            const int ty = t.second + dy;
            if (ty < 0 || ty >= h) continue;
            const int sy = s.second + dy;
            for (int dx = -r; dx <= r; ++dx) {
                const int tx = t.first + dx;
                if (tx < 0 || tx >= w) continue;
                const float *a = &img[(size_t(ty) * w + tx) * kCh];
                const float *b = &img[(size_t(sy) * w + s.first + dx) * kCh];
                for (int c = 0; c < kCh; ++c) { const float d = a[c] - b[c]; sum += d * d; }
                n += kCh;
            }
            if (sum > cutoff * full) return kInf;    // the mean can only be >= sum / full
        }
        return n ? sum / n : kInf;
    }

    void PatchMatch(std::vector<Pt> &nnf, std::vector<float> &cost, int pass, uint64_t seed, int searchRadius) const
    {
        constexpr int kBand = 16;
        const bool reverse = pass % 2 == 1;
        const int step = reverse ? -1 : 1;
        const int nBands = (h + kBand - 1) / kBand;
        auto band = [&](int bi) {
            Rng rng(seed ^ (uint64_t(bi) * 0x2545F4914F6CDD1Dull) ^ (uint64_t(pass) << 40));
            const int y0 = bi * kBand, rows = std::min(kBand, h - y0);
            for (int ry = 0; ry < rows; ++ry) {
                const int ly = reverse ? rows - 1 - ry : ry;
                const int y = y0 + ly;
                for (int rx = 0; rx < w; ++rx) {
                    const int x = reverse ? w - 1 - rx : rx;
                    const size_t i = size_t(y) * w + x;
                    if (!targets[i]) continue;
                    const Pt t(x, y);
                    Pt best = nnf[i];
                    float bc = cost[i];
                    /* Propagation from the already-visited neighbours, within this band. */
                    const int nx = x - step;
                    if (nx >= 0 && nx < w) {
                        const Pt c = nnf[size_t(y) * w + nx];
                        const Pt cand(c.first + step, c.second);
                        if (IsValid(cand.first, cand.second) && cand != best) {
                            const float d = Dist(t, cand, bc);
                            if (d < bc) { best = cand; bc = d; }
                        }
                    }
                    const int nly = ly - step;
                    if (nly >= 0 && nly < rows) {
                        const Pt c = nnf[size_t(y0 + nly) * w + x];
                        const Pt cand(c.first, c.second + step);
                        if (IsValid(cand.first, cand.second) && cand != best) {
                            const float d = Dist(t, cand, bc);
                            if (d < bc) { best = cand; bc = d; }
                        }
                    }
                    /* Random search in exponentially shrinking windows. */
                    for (int rad = searchRadius; rad >= 1; rad /= 2) {
                        const Pt cand(best.first + rng.Range(-rad, rad), best.second + rng.Range(-rad, rad));
                        if (IsValid(cand.first, cand.second) && cand != best) {
                            const float d = Dist(t, cand, bc);
                            if (d < bc) { best = cand; bc = d; }
                        }
                    }
                    nnf[i] = best;
                    cost[i] = bc;
                }
            }
        };
        if (w * h >= kParMin)
            cv::parallel_for_(cv::Range(0, nBands), [&](const cv::Range &rg) {
                for (int bi = rg.start; bi < rg.end; ++bi) band(bi);
            });
        else
            for (int bi = 0; bi < nBands; ++bi) band(bi);
    }

    /* Each hole pixel = similarity-weighted vote of every overlapping patch match. */
    void Vote(const std::vector<Pt> &nnf, const std::vector<float> &cost)
    {
        /* sigma^2 from the 75th percentile of match costs (a robust scale, after Wexler et al.). */
        std::vector<float> cs;
        for (size_t i = 0; i < cost.size(); ++i)
            if (targets[i] && std::isfinite(cost[i])) cs.push_back(cost[i]);
        float sigma2 = 1.0f;
        if (!cs.empty()) {
            const size_t k = std::min(cs.size() * 3 / 4, cs.size() - 1);
            std::nth_element(cs.begin(), cs.begin() + k, cs.end());
            sigma2 = std::max(cs[k], 1e-6f);
        }
        std::vector<float> out(img.size());
        std::vector<uint8_t> set(size_t(w) * h, 0);
        auto row = [&](int y) {
            for (int x = 0; x < w; ++x) {
                if (!hole[size_t(y) * w + x]) continue;
                float acc[kCh] = {};
                float wsum = 0;
                for (int dy = -r; dy <= r; ++dy) {
                    const int ty = y + dy;
                    if (ty < 0 || ty >= h) continue;
                    for (int dx = -r; dx <= r; ++dx) {
                        const int tx = x + dx;
                        if (tx < 0 || tx >= w) continue;
                        const size_t ti = size_t(ty) * w + tx;
                        if (!targets[ti] || !std::isfinite(cost[ti])) continue;
                        const Pt s = nnf[ti];
                        const int sx = s.first - dx, sy = s.second - dy;
                        const float wt = std::max(std::exp(-cost[ti] / (2.0f * sigma2)), 1e-8f);
                        const float *p = &img[(size_t(sy) * w + sx) * kCh];
                        for (int c = 0; c < kCh; ++c) acc[c] += p[c] * wt;
                        wsum += wt;
                    }
                }
                if (wsum > 0) {
                    const size_t o = (size_t(y) * w + x) * kCh;
                    for (int c = 0; c < kCh; ++c) out[o + c] = acc[c] / wsum;
                    set[size_t(y) * w + x] = 1;
                }
            }
        };
        if (w * h >= kParMin)
            cv::parallel_for_(cv::Range(0, h), [&](const cv::Range &rg) {
                for (int y = rg.start; y < rg.end; ++y) row(y);
            });
        else
            for (int y = 0; y < h; ++y) row(y);
        for (size_t i = 0; i < set.size(); ++i)
            if (set[i]) for (int c = 0; c < kCh; ++c) img[i * kCh + c] = out[i * kCh + c];
    }
};

/* img + hole into a level buffer; textureWeight > 0 adds the texture features (from the
   gray with the hole membrane-filled first, so the painted-over content leaves no edge
   in them), 0 leaves them zero. */
bool ToBuffers(const cv::Mat &img, const cv::Mat &hole, Level &l, float textureWeight)
{
    if (img.empty() || img.type() != CV_32FC3 || hole.type() != CV_8UC1 || img.size() != hole.size())
        return false;
    l.w = img.cols; l.h = img.rows;
    l.img.assign(size_t(l.w) * l.h * kCh, 0.0f);
    l.hole.resize(size_t(l.w) * l.h);
    for (int y = 0; y < l.h; ++y) {
        const float *s = img.ptr<float>(y);
        for (int x = 0; x < l.w; ++x)
            for (int c = 0; c < kColour; ++c)
                l.img[(size_t(y) * l.w + x) * kCh + c] = s[x * kColour + c];
        const uint8_t *m = hole.ptr<uint8_t>(y);
        for (int x = 0; x < l.w; ++x) l.hole[size_t(y) * l.w + x] = m[x] ? 1 : 0;
    }
    if (textureWeight > 0.0f && l.w >= 3 && l.h >= 3) {
        std::vector<float> filled = l.img;
        MembraneFill(l.w, l.h, filled, l.hole);
        cv::Mat gray(l.h, l.w, CV_32F);
        for (int y = 0; y < l.h; ++y)
            for (int x = 0; x < l.w; ++x) {
                const float *q = &filled[(size_t(y) * l.w + x) * kCh];
                gray.at<float>(y, x) = (q[0] + q[1] + q[2]) / 3.0f;
            }
        cv::Mat gx, gy;
        cv::Sobel(gray, gx, CV_32F, 1, 0, 3, 0.25, 0, cv::BORDER_REFLECT);
        cv::Sobel(gray, gy, CV_32F, 0, 1, 3, 0.25, 0, cv::BORDER_REFLECT);
        gx = cv::abs(gx); gy = cv::abs(gy);
        cv::blur(gx, gx, cv::Size(5, 5), cv::Point(-1, -1), cv::BORDER_REFLECT);
        cv::blur(gy, gy, cv::Size(5, 5), cv::Point(-1, -1), cv::BORDER_REFLECT);
        for (int y = 0; y < l.h; ++y)
            for (int x = 0; x < l.w; ++x) {
                float *q = &l.img[(size_t(y) * l.w + x) * kCh];
                q[3] = textureWeight * gx.at<float>(y, x);
                q[4] = textureWeight * gy.at<float>(y, x);
            }
    }
    return true;
}

/* Write the hole pixels of buf back into img. */
void FromBuffer(const Level &l, const std::vector<float> &buf, cv::Mat &img)
{
    for (int y = 0; y < l.h; ++y) {
        float *d = img.ptr<float>(y);
        for (int x = 0; x < l.w; ++x)
            if (l.hole[size_t(y) * l.w + x])
                for (int c = 0; c < kColour; ++c) d[x * kColour + c] = buf[(size_t(y) * l.w + x) * kCh + c];
    }
}

} // namespace

namespace PatchInpaint {

void Membrane(cv::Mat &img, const cv::Mat &hole)
{
    Level l;
    if (!ToBuffers(img, hole, l, 0.0f)) return;
    MembraneFill(l.w, l.h, l.img, l.hole);
    FromBuffer(l, l.img, img);
}

bool Complete(cv::Mat &img, const cv::Mat &hole, const Params &p)
{
    Level base;
    if (!ToBuffers(img, hole, base, p.textureWeight)) return false;
    int bx0 = base.w, by0 = base.h, bx1 = -1, by1 = -1;
    for (int y = 0; y < base.h; ++y)
        for (int x = 0; x < base.w; ++x)
            if (base.hole[size_t(y) * base.w + x]) {
                bx0 = std::min(bx0, x); by0 = std::min(by0, y);
                bx1 = std::max(bx1, x + 1); by1 = std::max(by1, y + 1);
            }
    if (bx1 < 0) return true;                       // no hole: nothing to do

    const int r = std::max(p.patchRadius, 1);
    const int psz = 2 * r + 1;
    /* Pyramid: shrink until the hole is a few patches across. */
    int ext = std::max(bx1 - bx0, by1 - by0);
    if (p.byThickness) {
        /* Thickness = twice the largest inscribed radius. A 22 px stroke 300 px long is a
           22 px hole to fill: by its bbox the pyramid shrank it to a pixel, and the fine
           levels, still "wider than kRefineExtent", only refined -- a smeared band. */
        cv::Mat dt;
        cv::distanceTransform(hole != 0, dt, cv::DIST_L2, 3);
        double maxR = 0;
        cv::minMaxLoc(dt, nullptr, &maxR);
        ext = std::min(ext, int(std::ceil(2.0 * maxR)) + 1);
    }
    std::vector<Level> levels;
    levels.push_back(std::move(base));
    std::vector<int> exts{ext};
    while (true) {
        const Level &l = levels.back();
        if (ext <= 2 * psz || l.w / 2 < 3 * psz || l.h / 2 < 3 * psz) break;
        levels.push_back(Downsample(l));
        ext = (ext + 1) / 2;
        exts.push_back(ext);
    }
    const int nlev = int(levels.size());
    std::vector<Pt> nnf;
    int pw = 0, ph = 0;
    std::vector<float> prev;                        // the coarser level's result
    for (int li = nlev - 1; li >= 0; --li) {
        const int lw = levels[li].w, lh = levels[li].h;
        std::vector<float> limg = levels[li].img;
        const std::vector<uint8_t> &lhole = levels[li].hole;
        /* Initialise the hole: membrane at the coarsest level, the upsampled result elsewhere. */
        if (prev.empty()) MembraneFill(lw, lh, limg, lhole);
        else
            for (int y = 0; y < lh; ++y)
                for (int x = 0; x < lw; ++x)
                    if (lhole[size_t(y) * lw + x]) {
                        const int sx = std::min(x / 2, pw - 1), sy = std::min(y / 2, ph - 1);
                        for (int c = 0; c < kCh; ++c)
                            limg[(size_t(y) * lw + x) * kCh + c] = prev[(size_t(sy) * pw + sx) * kCh + c];
                    }
        const std::vector<uint32_t> hs = Integral(lw, lh, lhole);
        Solver s{lw, lh, r, limg, lhole, {}, {}, {}};
        s.valid.assign(size_t(lw) * lh, 0);
        s.targets.assign(size_t(lw) * lh, 0);
        for (int y = 0; y < lh; ++y)
            for (int x = 0; x < lw; ++x) {
                const uint32_t cnt = WindowCount(hs, lw, lh, x - r, y - r, x + r + 1, y + r + 1);
                const bool inside = x >= r && y >= r && x + r < lw && y + r < lh;
                const size_t i = size_t(y) * lw + x;
                if (inside && cnt == 0) { s.valid[i] = 1; s.validList.push_back({x, y}); }
                s.targets[i] = cnt > 0;
            }
        if (s.validList.empty()) {
            if (li == 0) return false;
            prev = std::move(limg); pw = lw; ph = lh;
            nnf.clear();
            continue;
        }
        /* NNF init: the coarser level's match where valid, random otherwise. */
        Rng rng(p.seed ^ (uint64_t(li) << 20));
        std::vector<Pt> fresh(size_t(lw) * lh, Pt(0, 0));
        for (int y = 0; y < lh; ++y)
            for (int x = 0; x < lw; ++x) {
                const size_t i = size_t(y) * lw + x;
                if (!s.targets[i]) continue;
                bool got = false;
                if (!nnf.empty() && pw > 0) {
                    const size_t pi = size_t(y / 2) * pw + x / 2;
                    if (pi < nnf.size()) {
                        const Pt c(nnf[pi].first * 2 + (x % 2), nnf[pi].second * 2 + (y % 2));
                        if (s.IsValid(c.first, c.second)) { fresh[i] = c; got = true; }
                    }
                }
                if (!got) fresh[i] = s.validList[rng.Next() % s.validList.size()];
            }
        nnf = std::move(fresh);
        const bool refineOnly = li + 1 < nlev && exts[li] > p.refineExtent;
        const int em = li + 1 == nlev ? p.emIters
                     : refineOnly    ? 1
                     : std::max(p.emIters >> (nlev - 1 - li), p.minEmIters);
        const int pmIters = refineOnly ? 1 : p.pmIters;
        const int fullSearch = std::max(lw, lh);
        std::vector<float> cost(size_t(lw) * lh, kInf);
        for (int it = 0; it < em; ++it) {
            /* Costs change as hole pixels are re-estimated: refresh before each PatchMatch. */
            for (size_t i = 0; i < cost.size(); ++i)
                if (s.targets[i]) cost[i] = s.Dist(Pt(int(i % lw), int(i / lw)), nnf[i], kInf);
            /* Wide random search on the coarsest level / first iteration, local refinement after. */
            const int radius = refineOnly ? 2
                             : (li + 1 == nlev || it == 0) ? fullSearch
                             : std::min(4 * psz, fullSearch);
            for (int pass = 0; pass < pmIters; ++pass)
                s.PatchMatch(nnf, cost, pass + it * pmIters, p.seed + uint64_t(li * 1000 + it), radius);
            /* The vote averages ~(2r+1)^2 matches per pixel, which smooths texture away;
               at full resolution the last iteration COPIES each hole pixel from the
               centre of its own best match, so the fill keeps the image's grain. */
            if (p.finalCopy && li == 0 && it == em - 1) {
                std::vector<float> out = limg;
                for (size_t i = 0; i < cost.size(); ++i)
                    if (lhole[i] && std::isfinite(cost[i])) {
                        const Pt src = nnf[i];
                        const size_t si = size_t(src.second) * lw + src.first;
                        for (int c = 0; c < kCh; ++c) out[i * kCh + c] = limg[si * kCh + c];
                    }
                limg = std::move(out);
            } else
                s.Vote(nnf, cost);
        }
        prev = std::move(limg); pw = lw; ph = lh;
    }
    FromBuffer(levels[0], prev, img);
    return true;
}

} // namespace PatchInpaint
