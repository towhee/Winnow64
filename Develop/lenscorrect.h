#ifndef LENSCORRECT_H
#define LENSCORRECT_H

/*
    LENS CORRECTIONS: lateral chromatic aberration and edge colour fringing.

    WHY (2026-10-07). After the sky-mask halo work, a thin cyan line stayed along
    Dempster's ridge at -2 EV. The edge pixels were not a mix of sky and ridge -- red too
    low, blue too high -- so no mask could fix them. Two separate lens defects cause
    that kind of edge colour, and this header handles both:

      LATERAL CA -- red and blue imaged at slightly different SIZES than green, so they
      are offset RADIALLY by an amount that grows with distance from the centre. Fixed
      by resampling R and B about the centre (applyCA) by factors estimated from the
      image itself (estimateCA). Measured on Dempster (Nikon D810): R +1.1 px / B -0.4 px
      at the corners; an in-camera-corrected JPEG: ~0, correctly reported as none.

      EDGE FRINGING (axial CA, blooming) -- a coloured rim along high-contrast edges
      ANYWHERE in the frame. It was the bigger part of Dempster's line: the ridge is
      mid-frame, where lateral CA predicts only ~0.25 px. Fixed by defringe().

    DEFRINGE MODEL. An edge pixel's colour should be the mix of its two sides, in
    proportion to where its brightness sits between them -- in LINEAR light, since light
    mixes linearly (a Lab-space mix flagged every vivid edge against shadow). Each pixel
    near a strong edge samples the two sides along the brightness gradient, predicts the
    mixed colour, and its chroma is pulled toward that prediction -- but only where the
    DEVIATION has a fringe hue (purple group: cyan-blue, purple, magenta; green group:
    green), so a real coloured edge is never touched. Lightness is never changed.
    A first version compared chroma with the neighbourhood average instead and FAILED:
    a bluish sky hid the fringe that spills onto the dark side of the edge.

    PROOF (Python prototype, then this port diffed against it): Dempster ridge
    deviation 0.065 -> 0.009 and 0.058 -> 0.008; synthetic clean edges (sky/trees,
    yellow/blue, green leaf/shadow, red/grey, white/navy) changed by <= 0.6 dE -- no
    damage; synthetic axial and lateral fringes reduced 70-90% where their hue is in a
    band.

    Both functions take display-referred-ish RGB in 0..1 (defringe) or any linear RGB
    (CA) as cv::Mat CV_32FC3; Develop/lenscorrect wiring (MW) maps the working image in
    and out. Header-only, OpenCV core + imgproc, no Qt -- tests/unit/tst_lenscorrect.cpp.
*/

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace LensCorrect {

inline float smoothstep(float e0, float e1, float x)
{
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

template <typename F>
inline void forRows(int rows, F fn)
{
    cv::parallel_for_(cv::Range(0, rows), [&](const cv::Range &r) {
        for (int y = r.start; y < r.end; ++y) fn(y);
    });
}

/* ------------------------------------------------------------------------------------
   LATERAL CA
   ------------------------------------------------------------------------------------ */

/* Radial scale factors: a channel's content at radius r from the centre sits at
   r * (1 + k) relative to green. valid = enough edge evidence was found. */
struct CACoeffs {
    float kR = 0.0f, kB = 0.0f;
    bool  valid = false;
    bool  any() const { return valid && (kR != 0.0f || kB != 0.0f); }
};

namespace detail {

/* Central-difference derivative of a single-channel tile along (ux, uy). */
inline cv::Mat radialDeriv(const cv::Mat &t, float ux, float uy)
{
    cv::Mat d(t.size(), CV_32F);
    const int w = t.cols, h = t.rows;
    for (int y = 0; y < h; ++y) {
        const int y0 = std::max(0, y - 1), y1 = std::min(h - 1, y + 1);
        const float sy = (y1 - y0) > 0 ? 1.0f / float(y1 - y0) : 0.0f;
        const float *r0 = t.ptr<float>(y0), *r1 = t.ptr<float>(y1), *rc = t.ptr<float>(y);
        float *dp = d.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            const int x0 = std::max(0, x - 1), x1 = std::min(w - 1, x + 1);
            const float sx = (x1 - x0) > 0 ? 1.0f / float(x1 - x0) : 0.0f;
            dp[x] = (rc[x1] - rc[x0]) * sx * ux + (r1[x] - r0[x]) * sy * uy;
        }
    }
    return d;
}

}   // namespace detail

/*
    Estimate kR, kB from rgb (CV_32FC3, any linear or display RGB -- only the shapes of
    the channels matter), at FULL resolution: the shifts are sub-pixel. Strong-edge tiles away
    from the centre each give the radial shift of R and B against G that best aligns their
    radial derivatives (normalized correlation, coarse grid then parabolic refine); a
    weighted, outlier-rejecting fit then gives shift = k * r. A fitted shift under
    minCornerPx at the corner is treated as no CA (k = 0).
*/
inline CACoeffs estimateCA(const cv::Mat &rgb, float minCornerPx = 0.1f, int tile = 192)
{
    CACoeffs out;
    const int h = rgb.rows, w = rgb.cols;
    if (rgb.type() != CV_32FC3 || w < 2 * tile || h < 2 * tile) return out;
    const double cx = (w - 1) / 2.0, cy = (h - 1) / 2.0;
    cv::Mat ch[3];
    cv::split(rgb, ch);
    cv::Mat gx, gy;
    cv::Sobel(ch[1], gx, CV_32F, 1, 0, 3);
    cv::Sobel(ch[1], gy, CV_32F, 0, 1, 3);

    struct Tile { double e; int tx, ty; float ux, uy; double r; };
    std::vector<Tile> tiles;
    for (int ty = 0; ty + tile <= h; ty += tile)
        for (int tx = 0; tx + tile <= w; tx += tile) {
            const double mx = tx + tile / 2.0 - cx, my = ty + tile / 2.0 - cy;
            const double r = std::hypot(mx, my);
            if (r < 0.15 * std::min(w, h)) continue;
            const float ux = float(mx / r), uy = float(my / r);
            double e = 0;
            for (int y = ty; y < ty + tile; ++y) {
                const float *px = gx.ptr<float>(y), *py = gy.ptr<float>(y);
                for (int x = tx; x < tx + tile; ++x) {
                    const double g = px[x] * ux + py[x] * uy;
                    e += g * g;
                }
            }
            tiles.push_back({e, tx, ty, ux, uy, r});
        }
    std::sort(tiles.begin(), tiles.end(), [](const Tile &a, const Tile &b) { return a.e > b.e; });
    tiles.resize(std::min(tiles.size(), std::max<size_t>(40, tiles.size() / 6)));

    struct Obs { double r, s, wt; };
    std::vector<Obs> obs[2];               // [0] = R, [1] = B
    std::vector<std::vector<Obs>> per(tiles.size() * 2);
    cv::parallel_for_(cv::Range(0, int(tiles.size())), [&](const cv::Range &rg) {
        cv::Mat mapx(tile, tile, CV_32F), mapy(tile, tile, CV_32F), cs;
        for (int ti = rg.start; ti < rg.end; ++ti) {
            const Tile &T = tiles[size_t(ti)];
            const cv::Mat gt = ch[1](cv::Rect(T.tx, T.ty, tile, tile));
            const cv::Mat dG = detail::radialDeriv(gt, T.ux, T.uy);
            const double gg = cv::sum(dG.mul(dG))[0];
            if (gg < 1e-6) continue;
            for (int c = 0; c < 2; ++c) {
                const cv::Mat &C = ch[c == 0 ? 0 : 2];
                auto score = [&](double s) {
                    for (int y = 0; y < tile; ++y) {
                        float *mxp = mapx.ptr<float>(y), *myp = mapy.ptr<float>(y);
                        for (int x = 0; x < tile; ++x) {
                            mxp[x] = float(T.tx + x + s * T.ux);
                            myp[x] = float(T.ty + y + s * T.uy);
                        }
                    }
                    cv::remap(C, cs, mapx, mapy, cv::INTER_CUBIC, cv::BORDER_REFLECT);
                    const cv::Mat dC = detail::radialDeriv(cs, T.ux, T.uy);
                    const double num = cv::sum(dC.mul(dG))[0];
                    const double den = std::sqrt(cv::sum(dC.mul(dC))[0] * gg) + 1e-12;
                    return num / den;
                };
                double coarse[25], sc[25];
                for (int i = 0; i < 25; ++i) { coarse[i] = -3.0 + 0.25 * i; sc[i] = score(coarse[i]); }
                const int i = int(std::max_element(sc, sc + 25) - sc);
                const double lo = coarse[std::max(0, i - 1)], hi = coarse[std::min(24, i + 1)];
                double fine[21], sf[21];
                for (int j = 0; j < 21; ++j) { fine[j] = lo + (hi - lo) * j / 20.0; sf[j] = score(fine[j]); }
                const int j = int(std::max_element(sf, sf + 21) - sf);
                double s = fine[j];
                if (j > 0 && j < 20) {                            // parabolic refine
                    const double y0 = sf[j - 1], y1 = sf[j], y2 = sf[j + 1];
                    const double d = y0 - 2 * y1 + y2;
                    if (std::abs(d) > 1e-12) s += 0.5 * (y0 - y2) / d * (fine[1] - fine[0]);
                }
                if (sf[j] > 0.6) per[size_t(ti) * 2 + size_t(c)].push_back({T.r, s, T.e * sf[j]});
            }
        }
    });
    for (size_t ti = 0; ti < tiles.size(); ++ti)
        for (int c = 0; c < 2; ++c)
            for (const Obs &o : per[ti * 2 + size_t(c)]) obs[c].push_back(o);

    const double rCorner = std::hypot(cx, cy);
    int fitted = 0;
    for (int c = 0; c < 2; ++c) {
        const std::vector<Obs> &a = obs[c];
        if (a.size() < 8) continue;
        std::vector<char> keep(a.size(), 1);
        double k = 0;
        for (int it = 0; it < 5; ++it) {                      // drop > 3 MAD outliers
            double num = 0, den = 0;
            for (size_t i = 0; i < a.size(); ++i)
                if (keep[i]) { num += a[i].wt * a[i].s * a[i].r; den += a[i].wt * a[i].r * a[i].r; }
            if (den <= 0) break;
            k = num / den;
            std::vector<double> res, kept;
            for (size_t i = 0; i < a.size(); ++i) {
                res.push_back(a[i].s - k * a[i].r);
                if (keep[i]) kept.push_back(res.back());
            }
            auto median = [](std::vector<double> v) {
                std::nth_element(v.begin(), v.begin() + long(v.size() / 2), v.end());
                return v[v.size() / 2];
            };
            const double med = median(kept);
            std::vector<double> dev;
            for (double v : kept) dev.push_back(std::abs(v - med));
            const double mad = median(dev) + 1e-6;
            for (size_t i = 0; i < a.size(); ++i) keep[i] = std::abs(res[i]) < 3 * 1.4826 * mad;
        }
        const float kf = std::abs(k * rCorner) < minCornerPx ? 0.0f : float(k);
        (c == 0 ? out.kR : out.kB) = kf;
        ++fitted;
    }
    out.valid = fitted > 0;
    return out;
}

/* Resample R and B radially about the centre by (1 + k): R'(x) = R(c + (x - c)(1 + kR)).
   Resolution independent -- k is a ratio -- so a proxy and a full-res render agree. */
inline void applyCA(cv::Mat &rgb, const CACoeffs &k)
{
    if (!k.any() || rgb.type() != CV_32FC3) return;
    const int h = rgb.rows, w = rgb.cols;
    const float cx = (w - 1) / 2.0f, cy = (h - 1) / 2.0f;
    cv::Mat ch[3];
    cv::split(rgb, ch);
    for (int c : {0, 2}) {
        const float kk = (c == 0) ? k.kR : k.kB;
        if (kk == 0.0f) continue;
        cv::Mat mapx(h, w, CV_32F), mapy(h, w, CV_32F), out;
        forRows(h, [&](int y) {
            float *mx = mapx.ptr<float>(y), *my = mapy.ptr<float>(y);
            for (int x = 0; x < w; ++x) {
                mx[x] = cx + (x - cx) * (1.0f + kk);
                my[x] = cy + (y - cy) * (1.0f + kk);
            }
        });
        cv::remap(ch[c], out, mapx, mapy, cv::INTER_CUBIC, cv::BORDER_REFLECT);
        ch[c] = out;
    }
    cv::merge(ch, 3, rgb);
}

/* ------------------------------------------------------------------------------------
   DEFRINGE
   ------------------------------------------------------------------------------------ */

namespace detail {

struct Band { float centre, halfWidth; };
/* Lab hue (degrees) of the DEVIATION from the predicted mix. */
inline const Band kPurpleBands[] = {{235.0f, 40.0f}, {305.0f, 35.0f}, {350.0f, 25.0f}};
inline const Band kGreenBands[]  = {{140.0f, 35.0f}};

template <size_t N>
inline float hueWeight(float h, const Band (&bands)[N])
{
    float w = 0.0f;
    for (const Band &b : bands) {
        const float d = std::abs(std::fmod(h - b.centre + 540.0f, 360.0f) - 180.0f);
        w = std::max(w, 1.0f - smoothstep(0.6f * b.halfWidth, b.halfWidth, d));
    }
    return w;
}

/* Bilinear sample of a CV_32FC3 image at (x, y), reflect at the borders. */
inline cv::Vec3f sample(const cv::Mat &m, float x, float y)
{
    const int w = m.cols, h = m.rows;
    auto refl = [](int v, int n) { if (v < 0) v = -v - 1; if (v >= n) v = 2 * n - v - 1;
                                   return std::clamp(v, 0, n - 1); };
    const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    const float fx = x - x0, fy = y - y0;
    const cv::Vec3f a = m.at<cv::Vec3f>(refl(y0, h), refl(x0, w));
    const cv::Vec3f b = m.at<cv::Vec3f>(refl(y0, h), refl(x0 + 1, w));
    const cv::Vec3f c = m.at<cv::Vec3f>(refl(y0 + 1, h), refl(x0, w));
    const cv::Vec3f d = m.at<cv::Vec3f>(refl(y0 + 1, h), refl(x0 + 1, w));
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}

}   // namespace detail

/*
    Defringe rgb in place (CV_32FC3, display-referred RGB 0..1). purple / green are the
    amounts (0..1) of the two hue groups; scale = this image's long edge over the full-
    resolution long edge (the fringe reach is ~3 px at full res). Returns the per-pixel
    weight actually applied when `applied` is given (for tests / diagnostics).
*/
inline void defringe(cv::Mat &rgb, float purple, float green, float scale = 1.0f,
                     cv::Mat *applied = nullptr)
{
    using namespace detail;
    if ((purple <= 0.0f && green <= 0.0f) || rgb.type() != CV_32FC3) return;
    const int h = rgb.rows, w = rgb.cols;
    cv::Mat clipped(h, w, CV_32FC3);
    forRows(h, [&](int y) {
        const cv::Vec3f *s = rgb.ptr<cv::Vec3f>(y);
        cv::Vec3f *d = clipped.ptr<cv::Vec3f>(y);
        for (int x = 0; x < w; ++x)
            d[x] = cv::Vec3f(std::clamp(s[x][0], 0.0f, 1.0f), std::clamp(s[x][1], 0.0f, 1.0f),
                             std::clamp(s[x][2], 0.0f, 1.0f));
    });
    cv::Mat lab;
    cv::cvtColor(clipped, lab, cv::COLOR_RGB2Lab);
    cv::Mat lin(h, w, CV_32FC3);
    forRows(h, [&](int y) {
        const cv::Vec3f *s = clipped.ptr<cv::Vec3f>(y);
        cv::Vec3f *d = lin.ptr<cv::Vec3f>(y);
        for (int x = 0; x < w; ++x)
            d[x] = cv::Vec3f(std::pow(s[x][0], 2.2f), std::pow(s[x][1], 2.2f), std::pow(s[x][2], 2.2f));
    });
    cv::Mat L;
    cv::extractChannel(lab, L, 0);

    const int r = std::max(1, int(std::lround(3.0f * scale)));
    cv::Mat Ls, gx, gy;
    cv::GaussianBlur(L, Ls, cv::Size(0, 0), std::max(0.7, r / 2.0));
    cv::Sobel(Ls, gx, CV_32F, 1, 0, 3, 1.0 / 8.0);
    cv::Sobel(Ls, gy, CV_32F, 0, 1, 3, 1.0 / 8.0);
    cv::Mat g, edge(h, w, CV_32F);
    cv::magnitude(gx, gy, g);
    forRows(h, [&](int y) {
        const float *gp = g.ptr<float>(y);
        float *ep = edge.ptr<float>(y);
        for (int x = 0; x < w; ++x) ep[x] = smoothstep(3.0f * scale, 8.0f * scale, gp[x]);
    });
    cv::Mat near;
    cv::dilate(edge, near, cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                     cv::Size(2 * r + 1, 2 * r + 1)));
    /* The gradient direction, carried into the band from the nearest strong edge. */
    const cv::Mat wdir = edge.mul(g);
    cv::Mat nx, ny;
    const double sig = std::max(1.0, double(r));
    cv::GaussianBlur(gx.mul(wdir), nx, cv::Size(0, 0), sig);
    cv::GaussianBlur(gy.mul(wdir), ny, cv::Size(0, 0), sig);

    const float off = float(r) + 1.5f;
    const cv::Vec3f Yw(0.2126f, 0.7152f, 0.0722f);

    /* Pass 1: every pixel near an edge -- its two sides and its predicted LINEAR mix. */
    std::vector<cv::Point> pts;
    for (int y = 0; y < h; ++y) {
        const float *np_ = near.ptr<float>(y);
        for (int x = 0; x < w; ++x) if (np_[x] > 0.05f) pts.emplace_back(x, y);
    }
    const int n = int(pts.size());
    cv::Mat outLab = lab.clone();
    if (applied) *applied = cv::Mat::zeros(h, w, CV_32F);
    if (n == 0) return;
    cv::Mat pred(n, 1, CV_32FC3);
    std::vector<float> dLs(size_t(n), 0.0f);
    cv::parallel_for_(cv::Range(0, n), [&](const cv::Range &rg) {
        for (int i = rg.start; i < rg.end; ++i) {
            const int x = pts[size_t(i)].x, y = pts[size_t(i)].y;
            const float nxv = nx.at<float>(y, x), nyv = ny.at<float>(y, x);
            const float nn = std::hypot(nxv, nyv) + 1e-6f;
            const float ux = nxv / nn, uy = nyv / nn;
            const cv::Vec3f Pa = sample(lab, x + off * ux, y + off * uy);
            const cv::Vec3f Pb = sample(lab, x - off * ux, y - off * uy);
            dLs[size_t(i)] = Pa[0] - Pb[0];
            const cv::Vec3f la = sample(lin, x + off * ux, y + off * uy);
            const cv::Vec3f lb = sample(lin, x - off * ux, y - off * uy);
            const float ya = la.dot(Yw), yb = lb.dot(Yw), yp = lin.at<cv::Vec3f>(y, x).dot(Yw);
            const float dy = ya - yb;
            const float t = std::clamp((yp - yb) / (std::abs(dy) < 1e-5f ? 1e-5f : dy),
                                       0.0f, 1.0f);
            const cv::Vec3f p = lb + t * (la - lb);
            pred.at<cv::Vec3f>(i) = cv::Vec3f(std::pow(std::clamp(p[0], 0.0f, 1.0f), 1 / 2.2f),
                                              std::pow(std::clamp(p[1], 0.0f, 1.0f), 1 / 2.2f),
                                              std::pow(std::clamp(p[2], 0.0f, 1.0f), 1 / 2.2f));
        }
    });
    /* Pass 2: all predictions to Lab in one call. */
    cv::Mat plab;
    cv::cvtColor(pred, plab, cv::COLOR_RGB2Lab);
    /* Pass 3: pull fringe-hued deviations from the prediction back; lightness
       untouched. */
    cv::parallel_for_(cv::Range(0, n), [&](const cv::Range &rg) {
        for (int i = rg.start; i < rg.end; ++i) {
            const int x = pts[size_t(i)].x, y = pts[size_t(i)].y;
            const cv::Vec3f lp = lab.at<cv::Vec3f>(y, x), pl = plab.at<cv::Vec3f>(i);
            const float devA = lp[1] - pl[1], devB = lp[2] - pl[2];
            const float dev = std::hypot(devA, devB);
            const float hdev = std::fmod(float(std::atan2(devB, devA) * 180.0 / CV_PI)
                                         + 360.0f, 360.0f);
            const float gate = near.at<float>(y, x)
                               * smoothstep(6.0f, 15.0f, std::abs(dLs[size_t(i)]))
                               * smoothstep(1.5f, 5.0f, dev);
            const float k = std::clamp(gate * (purple * hueWeight(hdev, kPurpleBands)
                                               + green * hueWeight(hdev, kGreenBands)),
                                       0.0f, 1.0f);
            if (k <= 0.0f) continue;
            cv::Vec3f &o = outLab.at<cv::Vec3f>(y, x);
            o[1] = lp[1] - k * devA;
            o[2] = lp[2] - k * devB;
            if (applied) applied->at<float>(y, x) = k;
        }
    });
    cv::Mat outRgb;
    cv::cvtColor(outLab, outRgb, cv::COLOR_Lab2RGB);
    /* Only pixels that were corrected change, and a clipped highlight is left alone: its
       true colour is unknown, and writing the clamped value back would flatten it. */
    forRows(h, [&](int y) {
        const cv::Vec3f *src = rgb.ptr<cv::Vec3f>(y), *nw = outRgb.ptr<cv::Vec3f>(y),
                        *lb = outLab.ptr<cv::Vec3f>(y), *la = lab.ptr<cv::Vec3f>(y);
        cv::Vec3f *dst = rgb.ptr<cv::Vec3f>(y);
        for (int x = 0; x < w; ++x) {
            if (lb[x][1] == la[x][1] && lb[x][2] == la[x][2]) continue;
            if (src[x][0] >= 1.0f || src[x][1] >= 1.0f || src[x][2] >= 1.0f) continue;
            dst[x] = cv::Vec3f(std::clamp(nw[x][0], 0.0f, 1.0f), std::clamp(nw[x][1], 0.0f, 1.0f),
                               std::clamp(nw[x][2], 0.0f, 1.0f));
        }
    });
}

}   // namespace LensCorrect

#endif // LENSCORRECT_H
