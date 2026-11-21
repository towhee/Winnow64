#include "FocusStack/fslocalalign.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

/* See fslocalalign.h. LightCraft's flow.rs / refine.rs::match_patch, same constants. */

namespace FSLocalAlign {

namespace {

/* Weakest correlation peak a match may have. */
constexpr float kMinScore = 0.8f;
/* The best peak away from the best must be at least this much weaker. */
constexpr float kUnique = 0.03f;
/* Least standard deviation (0..1 gray) a patch needs to be matched. */
constexpr double kMinStd = 0.01;
/* A gray value at or above this is clipped (LightCraft: 95 % of the clip). */
constexpr int kClipped = 242;
/* Fewest measured tiles a step is made from (LightCraft's refine needs 12 matches). */
constexpr int kMinMeasured = 24;

using V2 = std::optional<cv::Point2f>;

/* The reference patch at (sx, sy) (top-left) found in frame within r pixels. */
V2 MatchPatch(const cv::Mat &ref, const cv::Mat &frame, int sx, int sy, int r)
{
    const cv::Rect pr(sx, sy, kPatch, kPatch);
    const cv::Rect wr(sx - r, sy - r, kPatch + 2 * r, kPatch + 2 * r);
    const cv::Mat a = ref(pr), b = frame(wr);

    double minV, maxV;
    cv::minMaxLoc(a, &minV, &maxV);
    if (maxV >= kClipped) return std::nullopt;
    cv::minMaxLoc(b, &minV, &maxV);
    if (maxV >= kClipped) return std::nullopt;
    cv::Scalar m, sd;
    cv::meanStdDev(a, m, sd);
    if (sd[0] / 255.0 < kMinStd) return std::nullopt;
    cv::meanStdDev(b, m, sd);
    if (sd[0] / 255.0 < kMinStd * 0.5) return std::nullopt;

    cv::Mat score;      // (2r+1)^2, CV_32F
    cv::matchTemplate(b, a, score, cv::TM_CCOEFF_NORMED);
    const int d = 2 * r + 1;
    int bx = -1, by = -1;
    float best = -2.0f;
    for (int y = 0; y < d; ++y)
        for (int x = 0; x < d; ++x) {
            const float v = score.at<float>(y, x);
            if (std::isfinite(v) && v > best) { best = v; bx = x; by = y; }
        }
    if (bx < 0 || best < kMinScore || bx == 0 || by == 0 || bx == d - 1 || by == d - 1)
        return std::nullopt;
    /* a second peak nearly as strong: repeating texture, the match is ambiguous */
    float second = -1.0f;
    for (int y = 0; y < d; ++y)
        for (int x = 0; x < d; ++x)
            if (std::abs(x - bx) > 1 || std::abs(y - by) > 1) {
                const float v = score.at<float>(y, x);
                if (std::isfinite(v)) second = std::max(second, v);
            }
    if (second > best - kUnique) return std::nullopt;
    auto at = [&](int x, int y) { return score.at<float>(y, x); };
    const float n[4] = { at(bx - 1, by), at(bx + 1, by), at(bx, by - 1), at(bx, by + 1) };
    for (float v : n)
        if (!std::isfinite(v)) return std::nullopt;
    auto sub = [](float mi, float c, float p) {
        const float den = mi - 2.0f * c + p;
        return den < -1e-9f ? std::clamp(0.5f * (mi - p) / den, -0.5f, 0.5f) : 0.0f;
    };
    return cv::Point2f(bx - r + sub(n[0], best, n[1]), by - r + sub(n[2], best, n[3]));
}

std::vector<cv::Point2f> Neighbours(const std::vector<V2> &d, int gw, int gh, int t)
{
    const int i = t % gw, j = t / gw;
    std::vector<cv::Point2f> out;
    out.reserve(8);
    for (int b = std::max(j - 1, 0); b <= std::min(j + 1, gh - 1); ++b)
        for (int a = std::max(i - 1, 0); a <= std::min(i + 1, gw - 1); ++a)
            if ((a != i || b != j) && d[static_cast<size_t>(b) * gw + a])
                out.push_back(*d[static_cast<size_t>(b) * gw + a]);
    return out;
}

V2 Mean(const std::vector<cv::Point2f> &n)
{
    if (n.empty()) return std::nullopt;
    cv::Point2f s(0, 0);
    for (const auto &v : n) s += v;
    return s * (1.0f / n.size());
}

float Median(std::vector<float> v)
{
    if (v.empty()) return 0.0f;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

inline float Bilinear(const cv::Mat &m, float fx, float fy)
{
    const int w = m.cols, h = m.rows;
    fx = std::clamp(fx, 0.0f, static_cast<float>(w - 1));
    fy = std::clamp(fy, 0.0f, static_cast<float>(h - 1));
    const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
    const float tx = fx - x0, ty = fy - y0;
    const float a = m.at<float>(y0, x0), b = m.at<float>(y0, x1);
    const float c = m.at<float>(y1, x0), e = m.at<float>(y1, x1);
    const float top = a + (b - a) * tx, bot = c + (e - c) * tx;
    return top + (bot - top) * ty;
}

} // namespace

float Flow::maxShift() const
{
    if (empty()) return 0.0f;
    float m = 0.0f;
    for (int y = 0; y < dx.rows; ++y)
        for (int x = 0; x < dx.cols; ++x)
            m = std::max(m, std::hypot(dx.at<float>(y, x), dy.at<float>(y, x)));
    return m;
}

cv::Point2f Flow::at(float x, float y) const
{
    if (empty() || !std::isfinite(x) || !std::isfinite(y)) return { 0.0f, 0.0f };
    /* tile (i, j)'s value sits at pixel centre (i + 0.5) * step */
    const float fx = x / step - 0.5f, fy = y / step - 0.5f;
    return { Bilinear(dx, fx, fy), Bilinear(dy, fx, fy) };
}

Flow Estimate(const cv::Mat &reference, const cv::Mat &frame, int radius)
{
    Flow out;
    if (reference.empty() || frame.empty() || reference.size() != frame.size() ||
        reference.type() != CV_8UC1 || frame.type() != CV_8UC1)
        return out;
    const int w = reference.cols, h = reference.rows;
    /* an even step, so a patch centred on a tile has a whole-pixel corner */
    const int step = (std::max(std::max(w, h) / kGrid, kPatch) + 1) & ~1;
    const int gw = w / step, gh = h / step;
    if (gw == 0 || gh == 0) return out;
    const int r = std::max(radius, 1);
    const int n = gw * gh;

    /* 1. measure */
    std::vector<V2> raw(n);
    cv::parallel_for_(cv::Range(0, n), [&](const cv::Range &rg) {
        for (int t = rg.start; t < rg.end; ++t) {
            const int i = t % gw, j = t / gw;
            const int sx = i * step + (step - kPatch) / 2, sy = j * step + (step - kPatch) / 2;
            if (sx < r + 1 || sy < r + 1 || sx + kPatch + r + 1 > w || sy + kPatch + r + 1 > h)
                continue;
            raw[t] = MatchPatch(reference, frame, sx, sy, r);
        }
    });
    int measured = 0;
    for (const auto &v : raw) measured += v.has_value();

    if (measured < kMinMeasured) {
        /* too little to go on: no correction for this step (a field filled from a
           handful of tiles would be a guess spread over the whole frame) */
        out.step = step;
        out.dx = cv::Mat::zeros(gh, gw, CV_32F);
        out.dy = cv::Mat::zeros(gh, gw, CV_32F);
        out.measured = measured;
        return out;
    }

    /* 2. outliers against the median of their measured neighbours; a tile with no
          measured neighbour can't be checked, and the fill would spread it: dropped.
          The kept tiles' distance from that median is the measurement noise (a real
          field is smooth, so neighbours agree on it); their distance from zero is
          signal plus noise. */
    std::vector<V2> d(n);
    for (int t = 0; t < n; ++t) {
        if (!raw[t]) continue;
        const auto nb = Neighbours(raw, gw, gh, t);
        if (nb.empty()) continue;
        std::vector<float> xs, ys;
        for (const auto &v : nb) { xs.push_back(v.x); ys.push_back(v.y); }
        const float mx = Median(xs), my = Median(ys);
        if (std::hypot(raw[t]->x - mx, raw[t]->y - my) <= kOutlier) d[t] = raw[t];
    }
    /*    fill outward from measured tiles */
    for (int pass = 0; pass < gw + gh; ++pass) {
        bool all = true, none = true;
        for (const auto &v : d) { all = all && v.has_value(); none = none && !v.has_value(); }
        if (all || none) break;
        const std::vector<V2> prev = d;
        for (int t = 0; t < n; ++t)
            if (!d[t]) d[t] = Mean(Neighbours(prev, gw, gh, t));
    }
    std::vector<cv::Point2f> f(n);
    for (int t = 0; t < n; ++t) f[t] = d[t].value_or(cv::Point2f(0, 0));

    /* 3. smooth (each tile with its neighbours) */
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<V2> prev(n);
        for (int t = 0; t < n; ++t) prev[t] = f[t];
        for (int t = 0; t < n; ++t) {
            auto nb = Neighbours(prev, gw, gh, t);
            nb.push_back(f[t]);
            f[t] = Mean(nb).value_or(f[t]);
        }
    }

    out.step = step;
    out.dx.create(gh, gw, CV_32F);
    out.dy.create(gh, gw, CV_32F);
    for (int t = 0; t < n; ++t) {
        out.dx.at<float>(t / gw, t % gw) = f[t].x;
        out.dy.at<float>(t / gw, t % gw) = f[t].y;
    }
    out.measured = measured;
    return out;
}

Flow Add(const Flow &a, const Flow &b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.step != b.step || a.dx.size() != b.dx.size()) return b;   // different geometry
    Flow out;
    out.step = a.step;
    out.dx = a.dx + b.dx;
    out.dy = a.dy + b.dy;
    out.measured = b.measured;
    return out;
}

static double Rms(const cv::Mat &x, const cv::Mat &y)
{
    return std::sqrt(cv::mean(x.mul(x) + y.mul(y))[0]);
}

bool Consistent(const Flow &stepPrev, const Flow &step, const Flow &direct,
                float *signal, float *disagreement)
{
    if (signal) *signal = 0;
    if (disagreement) *disagreement = 0;
    if (stepPrev.empty() || step.empty() || direct.empty() ||
        stepPrev.dx.size() != step.dx.size() || direct.dx.size() != step.dx.size())
        return false;
    const cv::Mat sx = stepPrev.dx + step.dx, sy = stepPrev.dy + step.dy;
    const double sig = Rms(sx, sy), dis = Rms(direct.dx - sx, direct.dy - sy);
    if (signal) *signal = static_cast<float>(sig);
    if (disagreement) *disagreement = static_cast<float>(dis);
    return sig >= kMinSignal && dis < kAgree * sig;
}

Chain::Decision Chain::Decide(int slice, const Flow &step, bool kept, float sig, float dis)
{
    if (kept) cum = FSLocalAlign::Add(cum, step);     // nothing kept yet: cum stays empty (plain affine)
    Decision d;
    d.slice = slice;
    d.field = cum;
    d.signal = sig;
    d.disagreement = dis;
    d.kept = kept;
    return d;
}

std::vector<Chain::Decision> Chain::Add(const cv::Mat &a)
{
    std::vector<Decision> out;
    const int k = next++;
    if (k == 0) {
        out.push_back(Decide(0, Flow(), false, 0, 0));
        prev1 = a;
        return out;
    }
    const Flow step = Estimate(prev1, a);
    bool ok = false;
    float sig = 0, dis = 0;
    if (k >= 2 && !prev2.empty())
        ok = Consistent(stepPrev, step, Estimate(prev2, a), &sig, &dis);
    /* slice k-1 is decided now: its step stands if its own check or this one confirms it */
    if (k >= 2) out.push_back(Decide(k - 1, stepPrev, okPrev || ok, ok ? sig : sigPrev, ok ? dis : disPrev));
    stepPrev = step;
    okPrev = ok;
    sigPrev = sig;
    disPrev = dis;
    prev2 = prev1;
    prev1 = a;
    return out;
}

std::vector<Chain::Decision> Chain::Finish()
{
    std::vector<Decision> out;
    if (next >= 2) out.push_back(Decide(next - 1, stepPrev, okPrev, sigPrev, disPrev));
    next = 0;
    prev1.release();
    prev2.release();
    return out;
}

void Warp(const cv::Mat &src, const cv::Mat &transform, const Flow &flow, cv::Mat &dst)
{
    if (flow.empty()) {
        /* exactly FSAlign::applyTransform (kept free of fsalign.cpp's dependencies) */
        dst.create(src.rows, src.cols, src.type());
        cv::warpAffine(src, dst, transform, src.size(), cv::INTER_CUBIC, cv::BORDER_REFLECT);
        return;
    }
    /* warpAffine without WARP_INVERSE_MAP samples src at inverse(transform)(p) */
    cv::Mat inv;
    cv::invertAffineTransform(transform, inv);
    inv.convertTo(inv, CV_64F);
    const double a = inv.at<double>(0, 0), b = inv.at<double>(0, 1), c = inv.at<double>(0, 2);
    const double d = inv.at<double>(1, 0), e = inv.at<double>(1, 1), g = inv.at<double>(1, 2);

    dst.create(src.rows, src.cols, src.type());
    constexpr int kBand = 256;      // rows per remap: maps stay ~2 x 8 x width x 256 bytes
    const int bands = (src.rows + kBand - 1) / kBand;
    cv::parallel_for_(cv::Range(0, bands), [&](const cv::Range &rg) {
        cv::Mat mx, my;
        for (int k = rg.start; k < rg.end; ++k) {
            const int y0 = k * kBand, y1 = std::min(src.rows, y0 + kBand);
            mx.create(y1 - y0, src.cols, CV_32F);
            my.create(y1 - y0, src.cols, CV_32F);
            for (int y = y0; y < y1; ++y) {
                float *px = mx.ptr<float>(y - y0), *py = my.ptr<float>(y - y0);
                for (int x = 0; x < src.cols; ++x) {
                    /* warpAffine maps pixel indices (centre at the integer); the field is
                       stored by pixel centre at +0.5 */
                    const cv::Point2f s = flow.at(x + 0.5f, y + 0.5f);
                    const double qx = x + s.x, qy = y + s.y;
                    px[x] = static_cast<float>(a * qx + b * qy + c);
                    py[x] = static_cast<float>(d * qx + e * qy + g);
                }
            }
            cv::Mat band = dst.rowRange(y0, y1);
            cv::remap(src, band, mx, my, cv::INTER_CUBIC, cv::BORDER_REFLECT);
        }
    });
}

} // namespace FSLocalAlign
