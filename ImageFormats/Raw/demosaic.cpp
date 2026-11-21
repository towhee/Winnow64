#include "ImageFormats/Raw/demosaic.h"
#include <opencv2/core.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

namespace {

/*
    Run rowFn(y) for every row of an H-row image across OpenCV's thread pool. A demosaic
    writes each output row from a read-only mosaic, so rows are independent and the result
    is identical to the serial loop. Single-threaded, a 50 MP bilinear pass took ~550 ms
    and every denoise decode ran two (measured 2026-10-13).

    cv::parallel_for_, not Develop's parallelFor: this runs on ImageCache decoder threads
    and developRenderPool workers, and OpenCV's pool is separate from the QThreadPool
    those may belong to, so a caller waiting on its own pool cannot stall (the same
    choice as lenscorrect.h / skyrefine.h).

    abort is still polled every row; once one stripe sees it the rest stop at their next
    row and false is returned. progress gets the count of rows done so far every 32 rows,
    from whichever worker crosses the boundary (try_lock: a busy reporter skips a tick
    rather than queueing) -- the callers marshal it to the GUI thread themselves.
*/
template <class RowFn>
bool ParallelRows(int H, const QAtomicInt *abort,
                  const std::function<void(int, int)> &progress, RowFn rowFn)
{
    std::atomic<bool> stop{false};
    std::atomic<int> done{0};
    std::mutex progressMutex;
    cv::parallel_for_(cv::Range(0, H), [&](const cv::Range &r) {
        for (int y = r.start; y < r.end; ++y) {
            if (stop.load(std::memory_order_relaxed)) return;
            if (abort && abort->loadAcquire()) {
                stop.store(true, std::memory_order_relaxed);
                return;
            }
            rowFn(y);
            const int d = done.fetch_add(1, std::memory_order_relaxed) + 1;
            if (progress && (d & 31) == 0 && progressMutex.try_lock()) {
                progress(d, H);
                progressMutex.unlock();
            }
        }
    });
    return !stop.load();
}

/*
    Mirror reflection of an index into 0..n-1 without repeating the edge sample
    (-k -> k, n-1+k -> n-1-k). The offset is always even, so Bayer parity -- and with it
    the CFA colour -- is preserved at the border.
*/
inline int Reflect(int i, int n)
{
    if (i >= 0 && i < n) return i;
    if (n == 1) return 0;
    const int period = 2 * (n - 1);
    i %= period;
    if (i < 0) i += period;
    return i >= n ? period - i : i;
}

/* v clamped between a and b, in whichever order they come. */
inline float ClampBetween(float v, float a, float b)
{
    const float lo = std::min(a, b), hi = std::max(a, b);
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Read-only, white-normalised, mirror-reflected view of a Bayer RawImage. */
struct Mosaic
{
    const uint16_t *data;
    int W, H;
    float scale;
    CfaPattern pattern;

    float Get(int x, int y) const {
        return static_cast<float>(data[static_cast<size_t>(Reflect(y, H)) * W + Reflect(x, W)]) * scale;
    }
    int Color(int x, int y) const {
        return Demosaic::BayerColorAt(pattern, Reflect(y, H), Reflect(x, W));
    }
};

Mosaic MakeMosaic(const RawImage &raw)
{
    return { raw.cfa.data(), raw.width, raw.height,
             raw.white > 0 ? 1.0f / static_cast<float>(raw.white) : 1.0f, raw.pattern };
}

} // namespace

bool Demosaic::Run(const RawImage &raw, std::vector<float> &rgb, Algorithm algo,
                   const QAtomicInt *abort, const std::function<void(int, int)> &progress)
{
    if (!raw.isValid()) return false;
    if (raw.pattern == CfaPattern::XTrans)
        return algo == Bilinear ? XTransWindow(raw, rgb, abort, progress)
                                : XTransDirectional(raw, rgb, abort, progress);
    if (raw.pattern == CfaPattern::Unknown) return false;

    /* PPG and AHD read two photosites out; a sliver of a mosaic has nothing to choose. */
    if (raw.width < 4 || raw.height < 4) algo = Bilinear;

    switch (algo) {
    case Bilinear: return Bilinear3x3(raw, rgb, abort, progress);
    case Ppg:      return PpgBayer(raw, rgb, abort, progress);
    case Ahd:      return AhdBayer(raw, rgb, abort, progress);
    }
    return false;
}

bool Demosaic::XTransWindow(const RawImage &raw, std::vector<float> &rgb,
                            const QAtomicInt *abort,
                            const std::function<void(int, int)> &progress)
{
/*
    Simple Fuji X-Trans demosaic. The 6x6 colour map (raw.xtrans) is tiled across the sensor.
    For each pixel the measured channel is exact; the two missing channels are the average of
    the same-colour photosites in a 5x5 window -- which, for X-Trans, always contains at least
    one of every colour. Soft but obviously correct (the X-Trans analogue of the bilinear Bayer
    path); a higher-quality algorithm can replace it later.
*/
    const int W = raw.width;
    const int H = raw.height;
    const float scale = raw.white > 0 ? 1.0f / static_cast<float>(raw.white) : 1.0f;
    const int R = 2;                                  // 5x5 window

    rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3, 0.0f);

    return ParallelRows(H, abort, progress, [&](int y) {
        for (int x = 0; x < W; ++x) {
            float sum[3] = {0, 0, 0};
            int   cnt[3] = {0, 0, 0};
            for (int dy = -R; dy <= R; ++dy) {
                const int yy = y + dy;
                if (yy < 0 || yy >= H) continue;
                for (int dx = -R; dx <= R; ++dx) {
                    const int xx = x + dx;
                    if (xx < 0 || xx >= W) continue;
                    const int c = raw.xtrans[yy % 6][xx % 6];
                    sum[c] += static_cast<float>(raw.cfa[static_cast<size_t>(yy) * W + xx]);
                    cnt[c] += 1;
                }
            }
            const size_t o = (static_cast<size_t>(y) * W + x) * 3;
            for (int c = 0; c < 3; ++c)
                rgb[o + c] = cnt[c] ? (sum[c] / cnt[c]) * scale : 0.0f;
            /* Native channel exact (no self-blur). */
            const int nativeC = raw.xtrans[y % 6][x % 6];
            rgb[o + nativeC] =
                static_cast<float>(raw.cfa[static_cast<size_t>(y) * W + x]) * scale;
        }
    });
}

int Demosaic::BayerColorAt(CfaPattern pattern, int row, int col)
{
    /* Colour code (0=R 1=G 2=B) for each of the 4 positions in the 2x2 cell,
       indexed by ((row & 1) << 1) | (col & 1). */
    static const int RGGB[4] = {0, 1, 1, 2};
    static const int BGGR[4] = {2, 1, 1, 0};
    static const int GRBG[4] = {1, 0, 2, 1};
    static const int GBRG[4] = {1, 2, 0, 1};

    const int i = ((row & 1) << 1) | (col & 1);
    switch (pattern) {
    case CfaPattern::RGGB: return RGGB[i];
    case CfaPattern::BGGR: return BGGR[i];
    case CfaPattern::GRBG: return GRBG[i];
    case CfaPattern::GBRG: return GBRG[i];
    default:               return RGGB[i];
    }
}

bool Demosaic::Bilinear3x3(const RawImage &raw, std::vector<float> &rgb,
                           const QAtomicInt *abort,
                           const std::function<void(int, int)> &progress)
{
/*
    Generic 3x3 bilinear demosaic. For every pixel the native colour is taken from the
    sample itself; the two missing colours are the average of the same-colour samples in
    the surrounding 3x3 neighbourhood (edge-clamped). A Bayer 3x3 window always contains
    at least one sample of each colour, so this handles all positions, including both
    green types, without special cases. Simple and obviously correct — good enough to
    prove the pipeline; AHD/DCB replace it in a later phase.
*/
    const int W = raw.width;
    const int H = raw.height;
    const float scale = raw.white > 0 ? 1.0f / static_cast<float>(raw.white) : 1.0f;

    rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3, 0.0f);

    /* ParallelRows polls abort once per row (cheap relative to a full row of pixels) so a
       long sensor decode bails promptly on shutdown / navigation rather than blocking the
       decoder thread -- and with it the BlockingQueuedConnection in ImageCache::stop. */
    return ParallelRows(H, abort, progress, [&](int y) {
        for (int x = 0; x < W; ++x) {
            float sum[3] = {0, 0, 0};
            int   cnt[3] = {0, 0, 0};

            for (int dy = -1; dy <= 1; ++dy) {
                const int yy = y + dy;
                if (yy < 0 || yy >= H) continue;
                for (int dx = -1; dx <= 1; ++dx) {
                    const int xx = x + dx;
                    if (xx < 0 || xx >= W) continue;
                    const int c = BayerColorAt(raw.pattern, yy, xx);
                    sum[c] += static_cast<float>(raw.cfa[static_cast<size_t>(yy) * W + xx]);
                    cnt[c] += 1;
                }
            }

            const size_t o = (static_cast<size_t>(y) * W + x) * 3;
            for (int c = 0; c < 3; ++c)
                rgb[o + c] = cnt[c] ? (sum[c] / cnt[c]) * scale : 0.0f;

            /* Keep the measured channel exact (no self-blur). */
            const int nativeC = BayerColorAt(raw.pattern, y, x);
            rgb[o + nativeC] =
                static_cast<float>(raw.cfa[static_cast<size_t>(y) * W + x]) * scale;
        }
    });
}


/* ---------------------------------------------------------------------------------------
   PPG and AHD, ported from LightCraft (github.com/storytold/lightcraft,
   crates/raw/src/demosaic/ppg.rs and ahd.rs; MIT, Copyright (c) 2026 ArtCraft Team and the
   LightCraft contributors -- see THIRD_PARTY_NOTICES.md). The maths is theirs, line for
   line; the C++ reads Winnow's uint16 mosaic directly (scaled by 1/white as Bilinear3x3
   does) and runs on OpenCV's pool like the rest of this file.
   --------------------------------------------------------------------------------------- */

bool Demosaic::PpgBayer(const RawImage &raw, std::vector<float> &rgb,
                   const QAtomicInt *abort,
                   const std::function<void(int, int)> &progress)
{
/*
    Patterned Pixel Grouping (after Chuan-kai Lin's published description):

    1. Green at red/blue sites: four directional gradients (same-colour difference x2
       plus the green difference across the site); along the flattest, the nearer green
       weighted 3:1 with the far one plus a same-colour Laplacian correction, clamped to
       those two greens.
    2. Red/blue at green sites: colour-difference (C - G) average of the two same-colour
       neighbours.
    3. Blue at red sites (and red at blue): colour-difference average along the diagonal
       with the smaller gradient (both diagonals when equal).

    Two passes (green plane, then the rest), each half of the progress bar.
*/
    const Mosaic m = MakeMosaic(raw);
    const int W = m.W, H = m.H;

    std::function<void(int, int)> p1, p2;
    if (progress) {
        p1 = [&progress](int d, int t) { progress(d, 2 * t); };
        p2 = [&progress](int d, int t) { progress(t + d, 2 * t); };
    }

    std::vector<float> g(static_cast<size_t>(W) * H);
    const bool ok = ParallelRows(H, abort, p1, [&](int y) {
        float *row = g.data() + static_cast<size_t>(y) * W;
        for (int x = 0; x < W; ++x) {
            const float c = m.Get(x, y);
            if (m.Color(x, y) == 1) { row[x] = c; continue; }
            const float gn = m.Get(x, y - 1), gs = m.Get(x, y + 1);
            const float gw = m.Get(x - 1, y), ge = m.Get(x + 1, y);
            const float cn = m.Get(x, y - 2), cs = m.Get(x, y + 2);
            const float cw = m.Get(x - 2, y), ce = m.Get(x + 2, y);
            const float dv = std::fabs(gn - gs);
            const float dh = std::fabs(gw - ge);
            const float grads[4] = { std::fabs(cn - c) * 2.0f + dv, std::fabs(cs - c) * 2.0f + dv,
                                     std::fabs(cw - c) * 2.0f + dh, std::fabs(ce - c) * 2.0f + dh };
            int best = 0;
            for (int i = 1; i < 4; ++i)
                if (grads[i] < grads[best]) best = i;
            float near_, far_, cfar;
            switch (best) {
            case 0:  near_ = gn; far_ = gs; cfar = cn; break;
            case 1:  near_ = gs; far_ = gn; cfar = cs; break;
            case 2:  near_ = gw; far_ = ge; cfar = cw; break;
            default: near_ = ge; far_ = gw; cfar = ce; break;
            }
            const float est = (near_ * 3.0f + far_ + c - cfar) * 0.25f;
            row[x] = ClampBetween(est, near_, far_);
        }
    });
    if (!ok) return false;

    auto gg = [&](int x, int y) {
        return g[static_cast<size_t>(Reflect(y, H)) * W + Reflect(x, W)];
    };

    rgb.assign(static_cast<size_t>(W) * H * 3, 0.0f);
    return ParallelRows(H, abort, p2, [&](int y) {
        for (int x = 0; x < W; ++x) {
            float *px = rgb.data() + (static_cast<size_t>(y) * W + x) * 3;
            const int own = m.Color(x, y);
            const float gv = gg(x, y);
            px[1] = gv;
            if (own == 1) {
                /* horizontal neighbours carry one colour, vertical the other */
                const int ch = m.Color(x + 1, y);
                const int cv = m.Color(x, y + 1);
                px[ch] = gv + ((m.Get(x - 1, y) - gg(x - 1, y)) + (m.Get(x + 1, y) - gg(x + 1, y))) * 0.5f;
                px[cv] = gv + ((m.Get(x, y - 1) - gg(x, y - 1)) + (m.Get(x, y + 1) - gg(x, y + 1))) * 0.5f;
            } else {
                px[own] = m.Get(x, y);
                const int other = 2 - own;
                auto d = [&](int dx, int dy) { return m.Get(x + dx, y + dy) - gg(x + dx, y + dy); };
                const float ne = std::fabs(m.Get(x + 1, y - 1) - m.Get(x - 1, y + 1))
                               + std::fabs(gg(x + 1, y - 1) - gv) + std::fabs(gg(x - 1, y + 1) - gv);
                const float nw = std::fabs(m.Get(x - 1, y - 1) - m.Get(x + 1, y + 1))
                               + std::fabs(gg(x - 1, y - 1) - gv) + std::fabs(gg(x + 1, y + 1) - gv);
                float diff;
                if (ne < nw)      diff = (d(1, -1) + d(-1, 1)) * 0.5f;
                else if (nw < ne) diff = (d(-1, -1) + d(1, 1)) * 0.5f;
                else              diff = (d(1, -1) + d(-1, 1) + d(-1, -1) + d(1, 1)) * 0.25f;
                px[other] = gv + diff;
            }
        }
    });
}

namespace {

/* AHD tiling: each TILE x TILE block of output is computed from the mosaic MARGIN pixels
   around it -- the choice reads homogeneity 1 px out, which reads Lab 2 px out, the
   candidates read the greens 3 px out, and those read the mosaic 5 px out. */
constexpr int kAhdTile = 128;
constexpr int kAhdMargin = 6;

/* Difference of the two candidates' 3x3 homogeneity sums (each 0..36) at which the more
   homogeneous one is taken alone; closer sums blend linearly. Picking outright let noise
   alone decide in flat areas and drew a maze of short H/V strokes (LightCraft's finding;
   12 keeps the outright pick's PSNR on edges). */
constexpr float kAhdBlend = 12.0f;

/* Linear camera RGB (treated as sRGB/D65 primaries for the homogeneity metric only) ->
   CIELab. */
inline void Lab(const float p[3], float out[3])
{
    const float X = (0.4124f * p[0] + 0.3576f * p[1] + 0.1805f * p[2]) / 0.95047f;
    const float Y =  0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2];
    const float Z = (0.0193f * p[0] + 0.1192f * p[1] + 0.9505f * p[2]) / 1.08883f;
    auto f = [](float t) { return t > 0.008856f ? std::cbrt(t) : 7.787f * t + 16.0f / 116.0f; };
    const float fx = f(X), fy = f(Y), fz = f(Z);
    out[0] = 116.0f * fy - 16.0f;
    out[1] = 500.0f * (fx - fy);
    out[2] = 200.0f * (fy - fz);
}

/* One AHD tile: output pixels (x0..x0+tw, y0..y0+th) written straight into rgb (W wide). */
void AhdTile(const Mosaic &m, int x0, int y0, int tw, int th, float *rgb)
{
    const int M = kAhdMargin;
    const int bw = tw + 2 * M, bh = th + 2 * M;
    const size_t n = static_cast<size_t>(bw) * bh;
    const int ox = x0 - M, oy = y0 - M;

    std::vector<float> mos(n);
    std::vector<uint8_t> col(n);
    for (int y = 0; y < bh; ++y) {
        const int yy = oy + y;
        const bool rowInside = yy >= 0 && yy < m.H && ox >= 0 && ox + bw <= m.W;
        for (int x = 0; x < bw; ++x) {
            const size_t i = static_cast<size_t>(y) * bw + x;
            if (rowInside) {
                mos[i] = static_cast<float>(m.data[static_cast<size_t>(yy) * m.W + ox + x]) * m.scale;
                col[i] = static_cast<uint8_t>(Demosaic::BayerColorAt(m.pattern, yy, ox + x));
            } else {
                mos[i] = m.Get(ox + x, yy);
                col[i] = static_cast<uint8_t>(m.Color(ox + x, yy));
            }
        }
    }

    /* Run fn(i) over the flat indices at least d from the buffer's edge. */
    auto inner = [&](int d, auto fn) {
        for (int y = d; y < bh - d; ++y)
            for (int x = d; x < bw - d; ++x)
                fn(static_cast<size_t>(y) * bw + x);
    };
    const size_t bws = static_cast<size_t>(bw);

    /* 1. Directional greens: 5-tap [-1/4 1/2 1/2 1/2 -1/4], clamped to the two adjacent greens. */
    std::vector<float> gh(n, 0.0f), gv(n, 0.0f);
    inner(M - 3, [&](size_t i) {
        const float c = mos[i];
        if (col[i] == 1) { gh[i] = c; gv[i] = c; return; }
        const float l = mos[i - 1], r = mos[i + 1];
        gh[i] = ClampBetween((l + r) * 0.5f + (2.0f * c - mos[i - 2] - mos[i + 2]) * 0.25f, l, r);
        const float u = mos[i - bws], d = mos[i + bws];
        gv[i] = ClampBetween((u + d) * 0.5f + (2.0f * c - mos[i - 2 * bws] - mos[i + 2 * bws]) * 0.25f, u, d);
    });

    /* 2. Complete each candidate with red/blue by colour difference, and 3. its Lab. */
    std::vector<float> rh(n * 3, 0.0f), rv(n * 3, 0.0f), lh(n * 3, 0.0f), lv(n * 3, 0.0f);
    auto complete = [&](const std::vector<float> &g, std::vector<float> &out, std::vector<float> &lab) {
        inner(M - 2, [&](size_t i) {
            const int own = col[i];
            const float gi = g[i];
            auto d = [&](size_t j) { return mos[j] - g[j]; };
            float *p = &out[i * 3];
            p[0] = 0.0f; p[1] = gi; p[2] = 0.0f;
            if (own == 1) {
                p[col[i + 1]]   = gi + (d(i - 1) + d(i + 1)) * 0.5f;
                p[col[i + bws]] = gi + (d(i - bws) + d(i + bws)) * 0.5f;
            } else {
                p[own] = mos[i];
                p[2 - own] = gi + (d(i - bws - 1) + d(i - bws + 1) + d(i + bws - 1) + d(i + bws + 1)) * 0.25f;
            }
            Lab(p, &lab[i * 3]);
        });
    };
    complete(gh, rh, lh);
    complete(gv, rv, lv);

    /* Homogeneity: how many of the 4 neighbours lie within the adaptive luminance and
       chrominance tolerances (eps_L, eps_C from the paper). */
    std::vector<uint8_t> hh(n, 0), hv(n, 0);
    inner(M - 1, [&](size_t i) {
        const size_t nb[4] = { i - 1, i + 1, i - bws, i + bws };   // left, right, up, down
        const float *a = &lh[i * 3], *b = &lv[i * 3];
        float dlh[4], dch[4], dlv[4], dcv[4];
        for (int k = 0; k < 4; ++k) {
            const float *qa = &lh[nb[k] * 3], *qb = &lv[nb[k] * 3];
            dlh[k] = std::fabs(a[0] - qa[0]);
            dch[k] = (a[1] - qa[1]) * (a[1] - qa[1]) + (a[2] - qa[2]) * (a[2] - qa[2]);
            dlv[k] = std::fabs(b[0] - qb[0]);
            dcv[k] = (b[1] - qb[1]) * (b[1] - qb[1]) + (b[2] - qb[2]) * (b[2] - qb[2]);
        }
        const float epsL = std::min(std::max(dlh[0], dlh[1]), std::max(dlv[2], dlv[3]));
        const float epsC = std::min(std::max(dch[0], dch[1]), std::max(dcv[2], dcv[3]));
        uint8_t sh = 0, sv = 0;
        for (int k = 0; k < 4; ++k) {
            sh += (dlh[k] <= epsL && dch[k] <= epsC) ? 1 : 0;
            sv += (dlv[k] <= epsL && dcv[k] <= epsC) ? 1 : 0;
        }
        hh[i] = sh;
        hv[i] = sv;
    });

    /* 4. Choose: 3x3 homogeneity sums, blended near a tie. */
    for (int y = 0; y < th; ++y) {
        float *dst = rgb + (static_cast<size_t>(y0 + y) * m.W + x0) * 3;
        for (int x = 0; x < tw; ++x) {
            const size_t i = static_cast<size_t>(y + M) * bw + (x + M);
            int sh = 0, sv = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const size_t j = i + dy * bws + dx;
                    sh += hh[j];
                    sv += hv[j];
                }
            const float wh = std::clamp(0.5f + static_cast<float>(sh - sv) / (2.0f * kAhdBlend), 0.0f, 1.0f);
            const float *ph = &rh[i * 3], *pv = &rv[i * 3];
            float *o = dst + static_cast<size_t>(x) * 3;
            if (wh >= 1.0f)      { o[0] = ph[0]; o[1] = ph[1]; o[2] = ph[2]; }
            else if (wh <= 0.0f) { o[0] = pv[0]; o[1] = pv[1]; o[2] = pv[2]; }
            else for (int c = 0; c < 3; ++c) o[c] = ph[c] * wh + pv[c] * (1.0f - wh);
        }
    }
}

} // namespace

bool Demosaic::AhdBayer(const RawImage &raw, std::vector<float> &rgb,
                   const QAtomicInt *abort,
                   const std::function<void(int, int)> &progress)
{
/*
    Adaptive Homogeneity-Directed demosaic (Hirakawa & Parks 2005), in independent
    overlapping tiles (bounded memory, one tile per OpenCV task). Each output pixel
    depends only on the mosaic around it, so the result is the same whatever the thread
    count or tile order. Progress is reported in tiles; abort is polled per tile.
*/
    const Mosaic m = MakeMosaic(raw);
    const int W = m.W, H = m.H;
    const int tx = (W + kAhdTile - 1) / kAhdTile;
    const int ty = (H + kAhdTile - 1) / kAhdTile;
    const int total = tx * ty;

    rgb.assign(static_cast<size_t>(W) * H * 3, 0.0f);

    std::atomic<bool> stop{false};
    std::atomic<int> done{0};
    std::mutex progressMutex;
    cv::parallel_for_(cv::Range(0, total), [&](const cv::Range &r) {
        for (int t = r.start; t < r.end; ++t) {
            if (stop.load(std::memory_order_relaxed)) return;
            if (abort && abort->loadAcquire()) {
                stop.store(true, std::memory_order_relaxed);
                return;
            }
            const int x0 = (t % tx) * kAhdTile, y0 = (t / tx) * kAhdTile;
            AhdTile(m, x0, y0, std::min(kAhdTile, W - x0), std::min(kAhdTile, H - y0), rgb.data());
            const int d = done.fetch_add(1, std::memory_order_relaxed) + 1;
            if (progress && progressMutex.try_lock()) {
                progress(d, total);
                progressMutex.unlock();
            }
        }
    });
    return !stop.load();
}


/* ---------------------------------------------------------------------------------------
   X-Trans directional, ported from LightCraft (crates/raw/src/demosaic/xtrans.rs; MIT,
   see THIRD_PARTY_NOTICES.md). Same maths and summation order; the interior fast path uses
   precomputed offsets per pattern phase, the border reflects like everything else here.
   --------------------------------------------------------------------------------------- */

namespace {

/* Robust-weight floor, in white-normalised units. */
constexpr float kXtEps = 1e-3f;

struct Offset2 { int dx, dy; };

/* Read-only, white-normalised, mirror-reflected view of an X-Trans RawImage. */
struct XtMosaic
{
    const uint16_t *data;
    int W, H;
    float scale;
    const uint8_t (*xt)[6];

    float Get(int x, int y) const {
        return static_cast<float>(data[static_cast<size_t>(Reflect(y, H)) * W + Reflect(x, W)]) * scale;
    }
    int Color(int x, int y) const { return xt[Reflect(y, H) % 6][Reflect(x, W) % 6]; }
};

/* Same-colour offsets for each of the 36 phases, in scan order (dy outer, dx inner) so the
   interior sums run in the same order as the border's plain 2-D loop. */
struct XtOffsets
{
    struct Phase {
        std::vector<Offset2> g1, g2;      // greens at radius 1, then radius 2 (whole 5x5)
        std::vector<Offset2> rb[2];       // 5x5 reds, 5x5 blues
    };
    Phase phase[36];
};

XtOffsets MakeXtOffsets(const uint8_t (*xt)[6])
{
    XtOffsets o;
    auto col = [&](int x, int y) { return xt[((y % 6) + 6) % 6][((x % 6) + 6) % 6]; };
    for (int py = 0; py < 6; ++py)
        for (int px = 0; px < 6; ++px) {
            XtOffsets::Phase &p = o.phase[py * 6 + px];
            for (int r = 1; r <= 2; ++r) {
                auto &ring = (r == 1) ? p.g1 : p.g2;
                for (int dy = -r; dy <= r; ++dy)
                    for (int dx = -r; dx <= r; ++dx)
                        if ((dx || dy) && col(px + dx, py + dy) == 1) ring.push_back({dx, dy});
            }
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx) {
                    const int c = col(px + dx, py + dy);
                    if (c == 0) p.rb[0].push_back({dx, dy});
                    else if (c == 2) p.rb[1].push_back({dx, dy});
                }
        }
    return o;
}

/* Weighted mean of vals around their (upper) median, each weighted 1 / (eps + |v - med|). */
float RobustMean(const float *vals, int n)
{
    float sorted[24];
    std::copy(vals, vals + n, sorted);
    std::sort(sorted, sorted + n);
    const float med = sorted[n / 2];
    float s = 0.0f, ws = 0.0f;
    for (int k = 0; k < n; ++k) {
        const float wt = 1.0f / (kXtEps + std::fabs(vals[k] - med));
        s += wt * vals[k];
        ws += wt;
    }
    return s / ws;
}

} // namespace

bool Demosaic::XTransDirectional(const RawImage &raw, std::vector<float> &rgb,
                                 const QAtomicInt *abort,
                                 const std::function<void(int, int)> &progress)
{
/*
    Edge-weighted colour-difference interpolation for X-Trans (LightCraft's own design):

    1. Green at non-green sites: robust weighted mean of the greens in the 3x3
       neighbourhood (5x5 if none), each weighted 1 / (eps + |g - median|), so samples
       across an edge contribute little.
    2. Red and blue everywhere: G(p) + sum w_q (C(q) - G(q)) / sum w_q over the same-colour
       samples q in the 5x5 neighbourhood, w_q = 1 / (d^2(p,q) * (eps + |G(q) - G(p)|)):
       close samples on the same side of an edge dominate.

    Two passes (green plane, then the rest), each half of the progress bar.
*/
    const XtMosaic m{ raw.cfa.data(), raw.width, raw.height,
                      raw.white > 0 ? 1.0f / static_cast<float>(raw.white) : 1.0f, raw.xtrans };
    const int W = m.W, H = m.H;
    const XtOffsets off = MakeXtOffsets(raw.xtrans);
    auto phaseOf = [](int x, int y) { return (y % 6) * 6 + (x % 6); };
    auto fast = [&](int x, int y) { return y >= 2 && y + 2 < H && x >= 2 && x + 2 < W; };
    auto raw01 = [&](size_t i) { return static_cast<float>(m.data[i]) * m.scale; };

    std::function<void(int, int)> p1, p2;
    if (progress) {
        p1 = [&progress](int d, int t) { progress(d, 2 * t); };
        p2 = [&progress](int d, int t) { progress(t + d, 2 * t); };
    }

    /* 1. Green plane. */
    std::vector<float> g(static_cast<size_t>(W) * H);
    const bool ok = ParallelRows(H, abort, p1, [&](int y) {
        float vals[24];
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y) * W + x;
            if (fast(x, y)) {
                if (raw.xtrans[y % 6][x % 6] == 1) { g[i] = raw01(i); continue; }
                const XtOffsets::Phase &p = off.phase[phaseOf(x, y)];
                const std::vector<Offset2> &ring = p.g1.empty() ? p.g2 : p.g1;
                if (ring.empty()) { g[i] = raw01(i); continue; }
                int n = 0;
                for (const Offset2 &o : ring)
                    vals[n++] = raw01(static_cast<size_t>(static_cast<ptrdiff_t>(i) + static_cast<ptrdiff_t>(o.dy) * W + o.dx));
                g[i] = RobustMean(vals, n);
                continue;
            }
            if (m.Color(x, y) == 1) { g[i] = m.Get(x, y); continue; }
            int n = 0;
            for (int r = 1; r <= 2 && n == 0; ++r)
                for (int dy = -r; dy <= r; ++dy)
                    for (int dx = -r; dx <= r; ++dx)
                        if ((dx || dy) && m.Color(x + dx, y + dy) == 1) vals[n++] = m.Get(x + dx, y + dy);
            g[i] = n ? RobustMean(vals, n) : m.Get(x, y);
        }
    });
    if (!ok) return false;

    auto gg = [&](int x, int y) { return g[static_cast<size_t>(Reflect(y, H)) * W + Reflect(x, W)]; };

    /* 2. Red and blue by weighted colour difference. */
    rgb.assign(static_cast<size_t>(W) * H * 3, 0.0f);
    return ParallelRows(H, abort, p2, [&](int y) {
        for (int x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y) * W + x;
            float *px = rgb.data() + i * 3;
            if (fast(x, y)) {
                const int own = raw.xtrans[y % 6][x % 6];
                const float gp = g[i];
                px[1] = gp;
                const XtOffsets::Phase &p = off.phase[phaseOf(x, y)];
                for (int li = 0; li < 2; ++li) {
                    const int c = li ? 2 : 0;
                    if (c == own) { px[c] = raw01(i); continue; }
                    float s = 0.0f, ws = 0.0f;
                    for (const Offset2 &o : p.rb[li]) {
                        const size_t j = static_cast<size_t>(static_cast<ptrdiff_t>(i) + static_cast<ptrdiff_t>(o.dy) * W + o.dx);
                        const float gq = g[j];
                        const float wt = 1.0f / (static_cast<float>(o.dx * o.dx + o.dy * o.dy) * (kXtEps + std::fabs(gq - gp)));
                        s += wt * (raw01(j) - gq);
                        ws += wt;
                    }
                    px[c] = ws > 0.0f ? gp + s / ws : gp;
                }
                continue;
            }
            const int own = m.Color(x, y);
            const float gp = gg(x, y);
            px[1] = gp;
            for (int c : { 0, 2 }) {
                if (c == own) { px[c] = m.Get(x, y); continue; }
                float s = 0.0f, ws = 0.0f;
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx) {
                        if (m.Color(x + dx, y + dy) != c) continue;
                        const float gq = gg(x + dx, y + dy);
                        const float wt = 1.0f / (static_cast<float>(dx * dx + dy * dy) * (kXtEps + std::fabs(gq - gp)));
                        s += wt * (m.Get(x + dx, y + dy) - gq);
                        ws += wt;
                    }
                px[c] = ws > 0.0f ? gp + s / ws : gp;
            }
        }
    });
}
