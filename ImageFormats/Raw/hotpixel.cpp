#include "ImageFormats/Raw/hotpixel.h"
#include "ImageFormats/Raw/demosaic.h"
#include <opencv2/core.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

/*
    See hotpixel.h. A line-for-line port of LightCraft's hotpixel.rs (MIT); the C++ reads
    Winnow's RawImage (uint16 mosaic, per-2x2 black, single white) and runs its row passes
    on OpenCV's pool (cv::parallel_for_, as Demosaic does: this runs on ImageCache decoder
    threads, whose QThreadPool it must not wait on). Each row's output is collected into
    its own slot and joined in row order, so the result never depends on the thread count.
*/

namespace {

/* A candidate stands more than this many sigma above the brightest of its same-color
   neighbours (pure noise hardly reaches 4.5 sigma above the brightest of eight). */
constexpr float kHot = 5.0f;
/* A direct neighbour this many sigma above its own surroundings means the light is real... */
constexpr float kSpot = 4.0f;
/* ...and it takes this many lit neighbours (a hot pair has one). */
constexpr int kMinLit = 2;
/* Every this many rows and columns are sampled for the noise model. */
constexpr int kSampleStep = 5;
/* Brightness bins of the noise model (on a square-root scale), and the samples a bin needs. */
constexpr int kBins = 32;
constexpr int kMinBinSamples = 64;
/* Photosites this close to the edge are left alone. */
constexpr int kMargin = 4;

struct Offset { int dx, dy; };

/* For one position in the CFA period: the nearest same-color photosites (at most 8,
   within two photosites). */
struct Phase {
    Offset same[8];
    int nSame = 0;
};

constexpr Offset kAround[8] = { {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                                {1, 0},   {-1, 1}, {0, 1}, {1, 1} };

struct Noise {
    float a = 0, b = 0;
    float Sigma(float level) const {
        return std::sqrt(std::max(a * std::max(level, 0.0f) + b, 1.0f));
    }
};

/* Median of n (<= 8 here, or a bin's deviations) values; reorders v. */
float Median(float *v, int n)
{
    if (n <= 0) return 0.0f;
    std::sort(v, v + n);
    return (n % 2) ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

struct View {
    const uint16_t *d;
    int w;
    int pw, ph;                     // CFA period
    const std::vector<Phase> *phases;

    float At(int x, int y) const { return static_cast<float>(d[static_cast<size_t>(y) * w + x]); }
    const Phase &PhaseAt(int x, int y) const { return (*phases)[(y % ph) * pw + (x % pw)]; }

    /* Same-color neighbours of (x,y) into buf (returns their count); max gets the largest. */
    int Neighbours(int x, int y, float *buf, float &max) const {
        const Phase &p = PhaseAt(x, y);
        max = -1e30f;
        for (int k = 0; k < p.nSame; ++k) {
            const float v = At(x + p.same[k].dx, y + p.same[k].dy);
            max = std::max(max, v);
            buf[k] = v;
        }
        return p.nSame;
    }
};

int ColorAt(const RawImage &raw, int x, int y)
{
    if (raw.pattern == CfaPattern::XTrans) return raw.xtrans[y % 6][x % 6];
    return Demosaic::BayerColorAt(raw.pattern, y, x);
}

std::vector<Phase> MakePhases(const RawImage &raw, int pw, int ph)
{
    std::vector<Phase> out(static_cast<size_t>(pw) * ph);
    for (int j = 0; j < ph; ++j)
        for (int i = 0; i < pw; ++i) {
            /* anchored far enough from 0 that i + dx stays positive */
            const int x0 = i + 2 * pw, y0 = j + 2 * ph;
            const int c = ColorAt(raw, x0, y0);
            std::vector<Offset> same;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                    if ((dx || dy) && ColorAt(raw, x0 + dx, y0 + dy) == c) same.push_back({dx, dy});
            std::stable_sort(same.begin(), same.end(), [](const Offset &a, const Offset &b) {
                return a.dx * a.dx + a.dy * a.dy < b.dx * b.dx + b.dy * b.dy;
            });
            Phase &p = out[static_cast<size_t>(j) * pw + i];
            p.nSame = std::min<int>(8, static_cast<int>(same.size()));
            for (int k = 0; k < p.nSame; ++k) p.same[k] = same[k];
        }
    return out;
}

/* Fit sigma^2 = a * level + b to the spread of (photosite - median of its same-color
   neighbours) on a sparse sample. False when no bin has enough samples. */
bool NoiseModel(const View &v, int x0, int x1, int y0, int y1, float black, float range, Noise &out)
{
    auto binOf = [&](float level) {
        return std::min(kBins - 1, static_cast<int>(std::sqrt(std::max(level, 0.0f) / range) * kBins));
    };
    struct Sample { int bin; float level, dev; };
    std::vector<int> rows;
    for (int y = y0; y < y1; y += kSampleStep) rows.push_back(y);
    std::vector<std::vector<Sample>> perRow(rows.size());
    cv::parallel_for_(cv::Range(0, static_cast<int>(rows.size())), [&](const cv::Range &r) {
        float buf[8];
        for (int i = r.start; i < r.end; ++i) {
            const int y = rows[i];
            auto &dst = perRow[i];
            for (int x = x0; x < x1; x += kSampleStep) {
                float mx;
                const int n = v.Neighbours(x, y, buf, mx);
                const float m = Median(buf, n);
                const float level = m - black;
                dst.push_back({binOf(level), level, std::fabs(v.At(x, y) - m)});
            }
        }
    });

    std::vector<std::vector<float>> dev(kBins);
    std::vector<double> sum(kBins, 0.0);
    std::vector<size_t> cnt(kBins, 0);
    for (const auto &row : perRow)
        for (const Sample &s : row) {
            dev[s.bin].push_back(s.dev);
            sum[s.bin] += s.level;
            ++cnt[s.bin];
        }

    /* per bin: mean level and sigma from the median absolute deviation (robust to the
       hot pixels themselves) */
    struct Pt { double level, var, n; };
    std::vector<Pt> pts;
    for (int b = 0; b < kBins; ++b) {
        if (cnt[b] < static_cast<size_t>(kMinBinSamples) || dev[b].empty()) continue;
        const double s = 1.4826 * Median(dev[b].data(), static_cast<int>(dev[b].size()));
        pts.push_back({sum[b] / cnt[b], s * s, static_cast<double>(cnt[b])});
    }
    if (pts.empty()) return false;

    /* weighted least squares for sigma^2 = a*L + b, both kept >= 0 */
    double sw = 0, sl = 0, sv = 0;
    for (const Pt &p : pts) { sw += p.n; sl += p.n * p.level; sv += p.n * p.var; }
    const double ml = sl / sw, mv = sv / sw;
    double cov = 0, varl = 0;
    for (const Pt &p : pts) {
        cov  += p.n * (p.level - ml) * (p.var - mv);
        varl += p.n * (p.level - ml) * (p.level - ml);
    }
    const double a = varl > 0 ? std::max(cov / varl, 0.0) : 0.0;
    const double b = std::max(mv - a * ml, 0.0);
    if (!std::isfinite(a) || !std::isfinite(b)) return false;
    out = { static_cast<float>(a), static_cast<float>(b) };
    return true;
}

struct Fix { int x, y; uint16_t value; };

/* Photosites far above every same-color neighbour, with the value to replace them by. */
std::vector<Fix> Candidates(const View &v, int x0, int x1, int y0, int y1, float black, const Noise &noise)
{
    std::vector<std::vector<Fix>> perRow(y1 - y0);
    cv::parallel_for_(cv::Range(y0, y1), [&](const cv::Range &r) {
        float buf[8];
        for (int y = r.start; y < r.end; ++y) {
            auto &dst = perRow[y - y0];
            for (int x = x0; x < x1; ++x) {
                const float s = v.At(x, y);
                float mx;
                const int n = v.Neighbours(x, y, buf, mx);
                if (s <= mx) continue;
                /* sigma rises with level and the median is at least the dimmest
                   neighbour, so sigma there bounds the threshold from below: most
                   photosites fail it without sorting their neighbours */
                const float mn = *std::min_element(buf, buf + n);
                if (s - mx <= kHot * noise.Sigma(mn - black)) continue;
                const float m = Median(buf, n);
                if (s - mx > kHot * noise.Sigma(m - black))
                    dst.push_back({x, y, static_cast<uint16_t>(std::clamp(std::round(m), 0.0f, 65535.0f))});
            }
        }
    });
    std::vector<Fix> out;
    for (auto &row : perRow) out.insert(out.end(), row.begin(), row.end());
    return out;
}

/* Are at least kMinLit direct neighbours of (x,y) clearly above their own same-color
   surroundings (real light)? A single one is not enough: hot photosites come in pairs. */
bool LitAround(const View &v, int x, int y, float black, const Noise &noise)
{
    int lit = 0;
    float buf[8];
    for (const Offset &o : kAround) {
        const int nx = x + o.dx, ny = y + o.dy;
        const float s = v.At(nx, ny);
        float mx;
        const int n = v.Neighbours(nx, ny, buf, mx);
        const float m = Median(buf, n);
        if (s - m > kSpot * noise.Sigma(m - black)) ++lit;
    }
    return lit >= kMinLit;
}

} // namespace

int HotPixel::Suppress(RawImage &raw, double maxShare)
{
    if (!raw.isValid()) return 0;
    int pw, ph;
    switch (raw.pattern) {
    case CfaPattern::RGGB: case CfaPattern::BGGR:
    case CfaPattern::GRBG: case CfaPattern::GBRG: pw = ph = 2; break;
    case CfaPattern::XTrans:                      pw = ph = 6; break;
    default: return 0;
    }
    const int w = raw.width, h = raw.height;
    if (w < 4 * kMargin || h < 4 * kMargin) return 0;

    const std::vector<Phase> phases = MakePhases(raw, pw, ph);
    for (const Phase &p : phases)
        if (p.nSame < 4) return 0;

    const float black = (raw.black[0] + raw.black[1] + raw.black[2] + raw.black[3]) / 4.0f;
    const float range = std::max(static_cast<float>(raw.white) - black, 1.0f);
    const int x0 = kMargin, x1 = w - kMargin, y0 = kMargin, y1 = h - kMargin;

    const View v{ raw.cfa.data(), w, pw, ph, &phases };
    Noise noise;
    if (!NoiseModel(v, x0, x1, y0, y1, black, range, noise)) return 0;

    std::vector<Fix> fixes;
    for (const Fix &f : Candidates(v, x0, x1, y0, y1, black, noise))
        if (!LitAround(v, f.x, f.y, black, noise)) fixes.push_back(f);

    /* (real light kept above doesn't count against the limit) */
    const double limit = maxShare * static_cast<double>(x1 - x0) * (y1 - y0);
    if (std::isnan(limit) || static_cast<double>(fixes.size()) > limit) return 0;

    for (const Fix &f : fixes) raw.cfa[static_cast<size_t>(f.y) * w + f.x] = f.value;
    return static_cast<int>(fixes.size());
}
