#ifndef SKYREFINE_H
#define SKYREFINE_H

/*
    SKY MATTE: fuse two sky models and matte the result against the photo's colours.

    The Select Sky mask runs TWO models (Utilities/skypredictor) and this header turns
    their outputs into one full-resolution 0..1 alpha. Header-only (all inline), OpenCV
    core + imgproc only, no Qt -- so tests/unit/tst_skyrefine.cpp can drive it directly.

    WHY TWO MODELS. Measured on the user's library (offline, 2026-10-07):
      - skyseg (U^2-Net, sigmoid output) draws CRISP edges and finds sunsets, grey, hazy
        and night skies well, but calls smooth bokeh and calm water "sky" at full
        confidence -- a wildlife library is full of both.
      - SegFormer-B4 (ADE20K, sky = class 2) is semantically better on those, but its
        output is 128x128, so its edges are blobs.
    They fail on DIFFERENT pictures, so agreement removes most false sky while skyseg
    keeps the edges.

    THE STEPS (each fixes a failure that was seen, not imagined):
      1. CALIBRATED PROBABILITY. skyseg's d0 is a true sigmoid. The old SegRefine path
         min-max stretched it, which turned a no-sky picture peaking at 0.005 into a full
         false sky. Probabilities are used as they come.
      2. REGION GATE. Each connected skyseg sky region is kept or dropped as a WHOLE by
         SegFormer's MEAN agreement over it. A per-pixel gate was tried first and failed:
         SegFormer's blob ate the sky between branches.
      3. SMALL GAPS. Regions under 1% of the frame (sky between branches) are below
         SegFormer's resolution, so its mean says nothing; they are kept when a kept large
         region is near, and colour decides them pixel by pixel.
      4. DISPUTED PIXELS (skyseg yes, SegFormer firmly no) fall back to the lower model
         ONLY where the pixel also looks unlike the nearby sky. Without that test a
         uniform bokeh background was punched full of blotches -- worse than leaving it.
      5. COLOUR MATTE in the uncertain band: local sky colour S and local foreground
         colour F are filled in from the confident pixels (push-pull pyramid), and alpha
         is the pixel's projection on the F->S line. That is what removes the halo --
         leaf gaps and branch tips get real fractional alpha instead of a smeared ramp.
         Trusted only where S and F are well separated AND the pixel lies near the line.
      6. RECOVERY. Where SegFormer sees sky that skyseg missed, next to kept sky, strong
         colour evidence may PROMOTE pixels to sky (snowy branches, canopy). Never demote.

    THE HALO PASS (2026-10-07, from the user's ridgeline halos -- Joffrey Lake, Dempster):
    sky just above a ridge is usually darker, paler or hazier than the sky further up,
    and step 5's single sky colour read it as "part ridge", leaving a band of half-
    darkened sky hugging the skyline. Colour alone cannot separate that sky from a real
    ridge/sky mix -- DISTANCE can: a real mix only exists within the edge's own width.
      5a. SPREAD-AWARE colours: the local sky (and foreground) is a RANGE along the F->S
          axis, measured from the confident pixels; inside the sky's spread is sky.
      5b. Snap the colour alpha to 0/1 beyond its own edge width (from its steepness).
      7.  Snap the FINAL alpha beyond the PHOTO's edge width (contrast / steepest local
          gradient), everywhere -- tightens the soft fallback too, but a hazy ridge (low
          gradient, wide width) stays soft and a defocused branch keeps its soft edge.
    Measured at 4096 px against the matte without it: halo pixels -49% / -34% / -29%
    (Joffrey Lake / Dempster / Manning Park), over-masking on Dempster -75%, canopy
    error vs colour ground truth 0.177 -> 0.135, the false-sky set unchanged.

    SECOND PASS (same day, from the user's in-app -2 EV screenshots: a 1-2 px bright
    line ON the edge). A per-pixel profile across the ridge showed sky-coloured pixels
    1-4 px above it still at alpha 0.77-0.94 -- the snap allowed the WHOLE ramp width on
    each side of the 0.5 contour, where a mixed pixel can only be within HALF of it --
    and, where the rock is about as bright as the darkened sky, properly mixed pixels
    ending brighter than both sides. Fixes: the half-width snap, and step 8's
    decontaminated alpha. Together: halo pixels -79% / -77% / -65% (Joffrey / Dempster /
    Manning) against the first matte, over-masking on Dempster -82%, bright-line
    overshoot on a fixed edge set -37% / -48% (Dempster / Joffrey), canopy error 0.107.

    All radii scale with the long edge L, so the result is resolution independent. The
    prototype this ports is recorded in notes/Documentation.txt "SELECT SKY MASK".
*/

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace SkyRefine {

inline float smoothstep(float e0, float e1, float x)
{
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* Run fn(y) for every row, in parallel. */
template <typename F>
inline void forRows(int rows, F fn)
{
    cv::parallel_for_(cv::Range(0, rows), [&](const cv::Range &r) {
        for (int y = r.start; y < r.end; ++y) fn(y);
    });
}

/* Clamp a CV_32F matrix to 0..1 in place. */
inline void clamp01(cv::Mat &m)
{
    forRows(m.rows, [&](int y) {
        float *p = m.ptr<float>(y);
        for (int x = 0; x < m.cols; ++x) p[x] = std::clamp(p[x], 0.0f, 1.0f);
    });
}

inline cv::Mat box(const cv::Mat &x, int r)
{
    cv::Mat o;
    cv::boxFilter(x, o, -1, cv::Size(2 * r + 1, 2 * r + 1), cv::Point(-1, -1), true,
                  cv::BORDER_REFLECT);
    return o;
}

inline cv::Mat ellipse(int r)
{
    return cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2 * r + 1, 2 * r + 1));
}

/*
    Colour guided filter (He et al.), "fast" variant: the per-window linear coefficients
    are solved at 1/s resolution and applied at full resolution. I is CV_32FC3, p CV_32F.
    The colour guide is what the luma-only SegRefine filter lacked -- blue sky and green
    leaves of equal luma have no edge in luma.
*/
inline cv::Mat guidedFilterRgbFast(const cv::Mat &I, const cv::Mat &p, int r, float eps, int s)
{
    const int w = p.cols, h = p.rows;
    const cv::Size ss(std::max(1, w / s), std::max(1, h / s));
    cv::Mat Is, ps;
    cv::resize(I, Is, ss, 0, 0, cv::INTER_AREA);
    cv::resize(p, ps, ss, 0, 0, cv::INTER_AREA);
    const int rs = std::max(1, r / s);

    cv::Mat Ic[3];
    cv::split(Is, Ic);
    cv::Mat m[3], cIp[3];
    const cv::Mat mp = box(ps, rs);
    for (int c = 0; c < 3; ++c) {
        m[c] = box(Ic[c], rs);
        cIp[c] = box(Ic[c].mul(ps), rs) - m[c].mul(mp);
    }
    cv::Mat v[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = i; j < 3; ++j) {
            v[i][j] = box(Ic[i].mul(Ic[j]), rs) - m[i].mul(m[j]);
            if (j != i) v[j][i] = v[i][j];
        }

    cv::Mat a[3], b(ss, CV_32F);
    for (int c = 0; c < 3; ++c) a[c].create(ss, CV_32F);
    forRows(ss.height, [&](int y) {
        for (int x = 0; x < ss.width; ++x) {
            /* Solve (Sigma + eps*I) a = cov by the symmetric 3x3 inverse. */
            const double s00 = v[0][0].at<float>(y, x) + eps, s01 = v[0][1].at<float>(y, x),
                         s02 = v[0][2].at<float>(y, x), s11 = v[1][1].at<float>(y, x) + eps,
                         s12 = v[1][2].at<float>(y, x), s22 = v[2][2].at<float>(y, x) + eps;
            const double i00 = s11 * s22 - s12 * s12, i01 = s02 * s12 - s01 * s22,
                         i02 = s01 * s12 - s02 * s11, i11 = s00 * s22 - s02 * s02,
                         i12 = s01 * s02 - s00 * s12, i22 = s00 * s11 - s01 * s01;
            const double det = s00 * i00 + s01 * i01 + s02 * i02;
            const double c0 = cIp[0].at<float>(y, x), c1 = cIp[1].at<float>(y, x),
                         c2 = cIp[2].at<float>(y, x);
            const double inv = (std::abs(det) > 1e-20) ? 1.0 / det : 0.0;
            const double a0 = (i00 * c0 + i01 * c1 + i02 * c2) * inv;
            const double a1 = (i01 * c0 + i11 * c1 + i12 * c2) * inv;
            const double a2 = (i02 * c0 + i12 * c1 + i22 * c2) * inv;
            a[0].at<float>(y, x) = float(a0);
            a[1].at<float>(y, x) = float(a1);
            a[2].at<float>(y, x) = float(a2);
            b.at<float>(y, x) = float(mp.at<float>(y, x) - (a0 * m[0].at<float>(y, x)
                                + a1 * m[1].at<float>(y, x) + a2 * m[2].at<float>(y, x)));
        }
    });

    cv::Mat A[3], B;
    for (int c = 0; c < 3; ++c) cv::resize(box(a[c], rs), A[c], p.size(), 0, 0, cv::INTER_LINEAR);
    cv::resize(box(b, rs), B, p.size(), 0, 0, cv::INTER_LINEAR);
    cv::Mat q(p.size(), CV_32F);
    forRows(h, [&](int y) {
        const cv::Vec3f *ip = I.ptr<cv::Vec3f>(y);
        float *qp = q.ptr<float>(y);
        const float *a0 = A[0].ptr<float>(y), *a1 = A[1].ptr<float>(y), *a2 = A[2].ptr<float>(y);
        const float *bp = B.ptr<float>(y);
        for (int x = 0; x < w; ++x)
            qp[x] = a0[x] * ip[x][0] + a1[x] * ip[x][1] + a2[x] * ip[x][2] + bp[x];
    });
    return q;
}

/*
    Push-pull fill: estimate a smooth colour field everywhere from the pixels where wt is
    1 -- each level trusts its own data where its weight is solid and the coarser level
    elsewhere. val CV_32FC3, wt CV_32F (0/1). Sky is low-frequency, which is why a local
    average is a good sky colour; for the foreground it is a local mean, which is enough
    for the projection to point the right way.
*/
inline cv::Mat pushPull(const cv::Mat &val, const cv::Mat &wt)
{
    std::vector<cv::Mat> V, W;
    {
        cv::Mat wt3, pv;
        cv::merge(std::vector<cv::Mat>{wt, wt, wt}, wt3);
        pv = val.mul(wt3);
        V.push_back(pv);
        W.push_back(wt.clone());
    }
    while (std::min(W.back().rows, W.back().cols) > 2) {
        cv::Mat v2, w2;
        cv::pyrDown(V.back(), v2);
        cv::pyrDown(W.back(), w2);
        V.push_back(v2);
        W.push_back(w2);
    }
    auto normalize = [](const cv::Mat &v, const cv::Mat &w) {
        cv::Mat o(v.size(), CV_32FC3);
        for (int y = 0; y < v.rows; ++y) {
            const cv::Vec3f *vp = v.ptr<cv::Vec3f>(y);
            const float *wp = w.ptr<float>(y);
            cv::Vec3f *op = o.ptr<cv::Vec3f>(y);
            for (int x = 0; x < v.cols; ++x) op[x] = vp[x] / std::max(wp[x], 1e-6f);
        }
        return o;
    };
    cv::Mat est = normalize(V.back(), W.back());
    for (int i = int(V.size()) - 2; i >= 0; --i) {
        cv::Mat up;
        cv::resize(est, up, W[i].size(), 0, 0, cv::INTER_LINEAR);
        const cv::Mat here = normalize(V[i], W[i]);
        for (int y = 0; y < up.rows; ++y) {
            cv::Vec3f *u = up.ptr<cv::Vec3f>(y);
            const cv::Vec3f *hp = here.ptr<cv::Vec3f>(y);
            const float *wp = W[i].ptr<float>(y);
            for (int x = 0; x < up.cols; ++x) {
                const float k = std::clamp(wp[x] / 0.25f, 0.0f, 1.0f);
                u[x] = hp[x] * k + u[x] * (1.0f - k);
            }
        }
        est = up;
    }
    return est;
}

/* Each pixel's distance (px) to the 0.5 contour of a 0/1 map: sky pixels measure to the
   nearest non-sky pixel and vice versa. */
inline cv::Mat distToEdge(const cv::Mat &B)
{
    cv::Mat inv, dIn, dOut;
    cv::bitwise_not(B, inv);
    cv::threshold(inv, inv, 254, 1, cv::THRESH_BINARY);
    cv::distanceTransform(B, dIn, cv::DIST_L2, 3);
    cv::distanceTransform(inv, dOut, cv::DIST_L2, 3);
    cv::Mat d(B.size(), CV_32F);
    forRows(B.rows, [&](int y) {
        const uchar *bp = B.ptr<uchar>(y);
        const float *ip = dIn.ptr<float>(y), *op = dOut.ptr<float>(y);
        float *dp = d.ptr<float>(y);
        for (int x = 0; x < B.cols; ++x) dp[x] = bp[x] ? ip[x] : op[x];
    });
    return d;
}

/* Gradient magnitude by central differences (one-sided at the border), summed over the
   channels of a CV_32F / CV_32FC3 image. */
inline cv::Mat gradMag(const cv::Mat &m)
{
    const int w = m.cols, h = m.rows, cn = m.channels();
    cv::Mat g(m.size(), CV_32F);
    forRows(h, [&](int y) {
        const int y0 = std::max(0, y - 1), y1 = std::min(h - 1, y + 1);
        const float sy = (y1 - y0) > 0 ? 1.0f / float(y1 - y0) : 0.0f;
        const float *r0 = m.ptr<float>(y0), *r1 = m.ptr<float>(y1), *rc = m.ptr<float>(y);
        float *gp = g.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            const int x0 = std::max(0, x - 1), x1 = std::min(w - 1, x + 1);
            const float sx = (x1 - x0) > 0 ? 1.0f / float(x1 - x0) : 0.0f;
            float acc = 0.0f;
            for (int c = 0; c < cn; ++c) {
                const float gx = (rc[x1 * cn + c] - rc[x0 * cn + c]) * sx;
                const float gy = (r1[x * cn + c] - r0[x * cn + c]) * sy;
                acc += gx * gx + gy * gy;
            }
            gp[x] = std::sqrt(acc);
        }
    });
    return g;
}

/*
    EDGE-WIDTH SNAP. A genuinely mixed pixel can only sit within the edge's own blur
    width of the 0.5 contour; further out, a value between 0 and 1 is a darker / paler sky
    or a lighter foreground, not a mix, so it is snapped to 0/1. width (px) is per pixel.
    This is what removed the ridgeline halo: sky just above a ridge is usually darker or
    hazier than the sky further up, and a colour model read it as "part ridge".
*/
inline void snapBeyondEdgeWidth(cv::Mat &a, const cv::Mat &width)
{
    cv::Mat B;
    cv::threshold(a, B, 0.5, 255, cv::THRESH_BINARY);
    B.convertTo(B, CV_8U);
    const cv::Mat dEdge = distToEdge(B);
    forRows(a.rows, [&](int y) {
        float *ap = a.ptr<float>(y);
        const float *dp = dEdge.ptr<float>(y), *wp = width.ptr<float>(y);
        const uchar *bp = B.ptr<uchar>(y);
        for (int x = 0; x < a.cols; ++x) {
            /* width is the WHOLE ramp, so a mixed pixel lies within half of it of the
               0.5 contour; allowing the full width on each side left 1-4 px of
               half-masked sky on a sharp ridge -- the thin bright line. */
            const float k = smoothstep(0.0f, 1.0f, (dp[x] - 0.5f * wp[x])
                                                       / std::max(0.25f * wp[x], 0.5f));
            ap[x] = ap[x] * (1.0f - k) + (bp[x] ? 1.0f : 0.0f) * k;
        }
    });
}

/* The PHOTO's local edge width (px) for snapBeyondEdgeWidth: the colour contrast across
   the edge (sep, the local F->S separation) over the steepest local colour gradient. A
   crisp ridge gives ~1-2 px, a hazy ridge or a defocused branch a wide one. */
inline cv::Mat photoEdgeWidth(const cv::Mat &rgb, const cv::Mat &sep, int L)
{
    cv::Mat sm;
    cv::GaussianBlur(rgb, sm, cv::Size(0, 0), 0.8);
    cv::Mat gmax;
    cv::dilate(gradMag(sm), gmax, ellipse(std::max(2, L / 400)));
    cv::Mat wI(rgb.size(), CV_32F);
    forRows(rgb.rows, [&](int y) {
        const float *gp = gmax.ptr<float>(y), *sq = sep.ptr<float>(y);
        float *wp = wI.ptr<float>(y);
        for (int x = 0; x < rgb.cols; ++x)
            wp[x] = std::clamp(std::max(sq[x], 0.02f) / std::max(gp[x], 1e-4f), 1.0f, L / 100.0f);
    });
    return wI;
}

/*
    DECONTAMINATED ALPHA. An edge pixel is p = (1-a)F + aS in linear light. Blending an
    exposure change by a scales ALL of p, so its sky part is under-changed and its
    foreground part over-changed: on a foreground about as bright as the adjusted sky that
    is a line brighter than both sides. a' = a * S / p puts the change on the "in" part
    alone -- exact for exposure (darken or brighten), close for other adjustments, and
    <= 1 by construction. S is the luminance of the nearest PURE pixel (a > 0.98, a 2 px
    Gaussian reach). Only fractional pixels change. rgb is display-referred 0..1.
*/
inline void decontaminate(cv::Mat &a, const cv::Mat &rgb)
{
    const int w = a.cols, h = a.rows;
    cv::Mat Y(a.size(), CV_32F), pure(a.size(), CV_32F);
    forRows(h, [&](int y) {
        const cv::Vec3f *ip = rgb.ptr<cv::Vec3f>(y);
        const float *op = a.ptr<float>(y);
        float *yp = Y.ptr<float>(y), *pp = pure.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            yp[x] = 0.2126f * std::pow(ip[x][0], 2.2f) + 0.7152f * std::pow(ip[x][1], 2.2f)
                    + 0.0722f * std::pow(ip[x][2], 2.2f);
            pp[x] = op[x] > 0.98f ? 1.0f : 0.0f;
        }
    });
    cv::Mat num, den;
    cv::GaussianBlur(Y.mul(pure), num, cv::Size(0, 0), 2.0);
    cv::GaussianBlur(pure, den, cv::Size(0, 0), 2.0);
    forRows(h, [&](int y) {
        const float *yp = Y.ptr<float>(y), *np = num.ptr<float>(y), *dp = den.ptr<float>(y);
        float *op = a.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            if (op[x] <= 0.02f || op[x] >= 0.98f || dp[x] <= 0.05f) continue;
            const float sNear = np[x] / std::max(dp[x], 1e-6f);
            const float ad = std::clamp(op[x] * sNear / std::max(yp[x], 1e-4f), 0.0f, 1.0f);
            op[x] = std::max(op[x], ad);
        }
    });
}

/*
    The whole sky matte. rawSky: skyseg d0 (CV_32F, any size, 0..1). rawSeg: SegFormer's
    sky softmax (CV_32F, any size, 0..1). rgb: the developed photo as CV_32FC3, RGB
    order, 0..1, at the resolution the matte should have. Returns CV_32F alpha, rgb-sized.
*/
inline cv::Mat refine(const cv::Mat &rawSky, const cv::Mat &rawSeg, const cv::Mat &rgb)
{
    const int w = rgb.cols, h = rgb.rows, L = std::max(w, h);
    const cv::Size full(w, h);
    cv::Mat pS, pF;
    cv::resize(rawSky, pS, full, 0, 0, cv::INTER_CUBIC);
    cv::resize(rawSeg, pF, full, 0, 0, cv::INTER_LINEAR);
    clamp01(pS);
    clamp01(pF);

    const int rg = std::max(3, L / 64);
    const int rd = std::max(2, rg / 4);
    const int blurS = std::max(1, L / 512);

    /* Step 2-3: region gate, at quarter resolution. */
    const cv::Size qs(std::max(1, w / 4), std::max(1, h / 4));
    cv::Mat pSs, pFs;
    cv::resize(pS, pSs, qs, 0, 0, cv::INTER_AREA);
    cv::resize(pF, pFs, qs, 0, 0, cv::INTER_AREA);
    cv::Mat lab;
    const int n = cv::connectedComponents(pSs > 0.5f, lab, 8, CV_32S);
    cv::Mat gsmall = cv::Mat::zeros(qs, CV_32F);
    cv::Mat smallKept = cv::Mat::zeros(qs, CV_8U);
    if (n > 1) {
        std::vector<double> sums(n, 0.0);
        std::vector<long long> cnts(n, 0);
        for (int y = 0; y < qs.height; ++y) {
            const int *lp = lab.ptr<int>(y);
            const float *fp = pFs.ptr<float>(y);
            for (int x = 0; x < qs.width; ++x) { sums[lp[x]] += fp[x]; ++cnts[lp[x]]; }
        }
        std::vector<float> keep(n);
        std::vector<char> small(n);
        const double total = double(qs.area());
        for (int k = 0; k < n; ++k) {
            keep[k] = smoothstep(0.2f, 0.3f, float(sums[k] / std::max<long long>(cnts[k], 1)));
            small[k] = cnts[k] < 0.01 * total;
        }
        keep[0] = 0.0f;
        small[0] = 0;
        cv::Mat bigKept(qs, CV_8U);
        for (int y = 0; y < qs.height; ++y) {
            const int *lp = lab.ptr<int>(y);
            uchar *bp = bigKept.ptr<uchar>(y);
            for (int x = 0; x < qs.width; ++x)
                bp[x] = (!small[lp[x]] && keep[lp[x]] > 0.5f) ? 1 : 0;
        }
        cv::Mat near;
        cv::dilate(bigKept, near, ellipse(qs.width / 10));
        std::vector<char> nearHit(n, 0);
        for (int y = 0; y < qs.height; ++y) {
            const int *lp = lab.ptr<int>(y);
            const uchar *np = near.ptr<uchar>(y);
            for (int x = 0; x < qs.width; ++x) if (np[x]) nearHit[lp[x]] = 1;
        }
        for (int k = 1; k < n; ++k) if (small[k] && nearHit[k]) keep[k] = 1.0f;
        for (int y = 0; y < qs.height; ++y) {
            const int *lp = lab.ptr<int>(y);
            float *gp = gsmall.ptr<float>(y);
            uchar *sp = smallKept.ptr<uchar>(y);
            for (int x = 0; x < qs.width; ++x) {
                gp[x] = keep[lp[x]];
                sp[x] = (small[lp[x]] && keep[lp[x]] > 0.5f) ? 1 : 0;
            }
        }
    }
    /* Spread each region's weight over its own soft (<0.5) fringe. */
    cv::dilate(gsmall, gsmall, ellipse(rd));
    cv::Mat gate;
    cv::resize(gsmall, gate, full, 0, 0, cv::INTER_LINEAR);

    const cv::Mat p = pS.mul(gate);

    /* SegFormer-only sky is trusted only near KEPT skyseg sky. A smooth proximity field,
       so it is built at quarter resolution: the full-res elliptical dilation (radius
       L/32, non-separable) cost ~2 s at 4096 px for an answer a blur then erases. */
    cv::Mat gateS;
    {
        cv::Mat gq;
        cv::resize(gate, gq, qs, 0, 0, cv::INTER_AREA);
        gq = pSs.mul(gq);
        cv::dilate(gq, gq, ellipse(std::max(1, 2 * rg / 4)));
        cv::GaussianBlur(gq, gq, cv::Size(0, 0), std::max(1.0, rg / 4.0));
        cv::resize(gq, gateS, full, 0, 0, cv::INTER_LINEAR);
    }

    cv::Mat smallUp;
    cv::dilate(smallKept, smallUp, ellipse(rd));
    cv::resize(smallUp, smallUp, full, 0, 0, cv::INTER_NEAREST);

    /* Disputed, the trimap and the uncertain band, in one pass. */
    cv::Mat disputed(full, CV_32F), dBigRaw(full, CV_32F), skyC(full, CV_32F),
            fgC(full, CV_32F), band(full, CV_8U), recover(full, CV_32F);
    forRows(h, [&](int y) {
        const float *pp = p.ptr<float>(y), *fp = pF.ptr<float>(y), *sp = pS.ptr<float>(y),
                    *gs = gateS.ptr<float>(y);
        const uchar *su = smallUp.ptr<uchar>(y);
        float *dp = disputed.ptr<float>(y), *db = dBigRaw.ptr<float>(y),
              *sc = skyC.ptr<float>(y), *fc = fgC.ptr<float>(y), *rp = recover.ptr<float>(y);
        uchar *bp = band.ptr<uchar>(y);
        for (int x = 0; x < w; ++x) {
            const bool disp = pp[x] > 0.5f && fp[x] < 0.2f;
            dp[x] = disp ? 1.0f : 0.0f;
            db[x] = (disp && !su[x]) ? 1.0f : 0.0f;
            sc[x] = (pp[x] > 0.9f && !disp) ? 1.0f : 0.0f;
            fc[x] = (pp[x] < 0.1f && fp[x] < 0.3f) ? 1.0f : 0.0f;
            bp[x] = ((pp[x] >= 0.1f && pp[x] <= 0.9f) || disp) ? 1 : 0;
            rp[x] = std::clamp(fp[x] - sp[x], 0.0f, 1.0f) * smoothstep(0.15f, 0.5f, gs[x]);
        }
    });
    cv::Mat dBig;
    cv::GaussianBlur(dBigRaw, dBig, cv::Size(0, 0), blurS);
    cv::dilate(band, band, ellipse(std::max(2, L / 400)));

    const int guideR = std::max(4, L / 256);
    if (cv::countNonZero(skyC) < 50 || cv::countNonZero(fgC) < 50) {
        /* No sky, or nothing but sky: no colour model to fit. */
        cv::Mat a = guidedFilterRgbFast(rgb, p, guideR, 1e-4f, 4);
        clamp01(a);
        return a;
    }

    /* Step 5: local colour models at quarter resolution, from eroded confident sets so
       mixed edge pixels cannot pollute them. */
    cv::Mat small, ws, wf;
    cv::resize(rgb, small, qs, 0, 0, cv::INTER_AREA);
    cv::resize(skyC, ws, qs, 0, 0, cv::INTER_AREA);
    cv::resize(fgC, wf, qs, 0, 0, cv::INTER_AREA);
    cv::erode(ws, ws, ellipse(2));
    cv::erode(wf, wf, ellipse(2));
    cv::Mat wsB, wfB;
    cv::threshold(ws, wsB, 0.99, 1.0, cv::THRESH_BINARY);
    cv::threshold(wf, wfB, 0.99, 1.0, cv::THRESH_BINARY);
    cv::Mat Sc, Fc;
    cv::resize(pushPull(small, wsB), Sc, full, 0, 0, cv::INTER_LINEAR);
    cv::resize(pushPull(small, wfB), Fc, full, 0, 0, cv::INTER_LINEAR);

    /* Step 4: a disputed pixel falls to the lower model only where it looks unlike the
       nearby sky. */
    cv::Mat distinct(full, CV_32F);
    forRows(h, [&](int y) {
        const cv::Vec3f *ip = rgb.ptr<cv::Vec3f>(y), *sp = Sc.ptr<cv::Vec3f>(y);
        float *op = distinct.ptr<float>(y);
        for (int x = 0; x < w; ++x)
            op[x] = smoothstep(0.06f, 0.14f, float(cv::norm(ip[x] - sp[x])));
    });
    cv::GaussianBlur(distinct, distinct, cv::Size(0, 0), blurS);
    cv::Mat dsoft;
    cv::GaussianBlur(disputed, dsoft, cv::Size(0, 0), blurS);
    cv::Mat pBase(full, CV_32F);
    forRows(h, [&](int y) {
        const float *pp = p.ptr<float>(y), *fp = pF.ptr<float>(y),
                    *ds = dsoft.ptr<float>(y), *dt = distinct.ptr<float>(y);
        float *op = pBase.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            const float k = ds[x] * dt[x];
            op[x] = pp[x] * (1.0f - k) + std::min(pp[x], fp[x]) * k;
        }
    });
    cv::Mat base = guidedFilterRgbFast(rgb, pBase, guideR, 1e-4f, 4);

    /* Step 5: the colour alpha -- the pixel's projection t on the local F->S line --
       with the line's separation and the pixel's distance off it. */
    cv::Mat alphaC(full, CV_32F), sep(full, CV_32F), resid(full, CV_32F), tRaw(full, CV_32F);
    forRows(h, [&](int y) {
        const cv::Vec3f *ip = rgb.ptr<cv::Vec3f>(y), *sp = Sc.ptr<cv::Vec3f>(y),
                        *fp = Fc.ptr<cv::Vec3f>(y);
        float *ap = alphaC.ptr<float>(y), *sq = sep.ptr<float>(y), *rp = resid.ptr<float>(y),
              *tp = tRaw.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            const cv::Vec3f d = sp[x] - fp[x];
            const float dd = d.dot(d);
            const float t = (ip[x] - fp[x]).dot(d) / std::max(dd, 1e-6f);
            const float a = std::clamp(t, 0.0f, 1.0f);
            tp[x] = t;
            ap[x] = a;
            sq[x] = std::sqrt(dd);
            rp[x] = float(cv::norm(ip[x] - (fp[x] + a * d)));
        }
    });

    /* Step 5a, SPREAD-AWARE: the sky is a range of colours, not a point. Measure locally
       how far the confident sky spreads along the F->S axis (t) and off it (r), and the
       same for the foreground; a pixel inside the sky's own spread is sky. */
    {
        cv::Mat dq, Fq;
        cv::Mat dFull = Sc - Fc;
        cv::resize(dFull, dq, qs, 0, 0, cv::INTER_AREA);
        cv::resize(Fc, Fq, qs, 0, 0, cv::INTER_AREA);
        cv::Mat tq(qs, CV_32F), rq(qs, CV_32F);
        for (int y = 0; y < qs.height; ++y) {
            const cv::Vec3f *ip = small.ptr<cv::Vec3f>(y), *dp = dq.ptr<cv::Vec3f>(y),
                            *fp = Fq.ptr<cv::Vec3f>(y);
            float *tp = tq.ptr<float>(y), *rp = rq.ptr<float>(y);
            for (int x = 0; x < qs.width; ++x) {
                const float dd = std::max(dp[x].dot(dp[x]), 1e-6f);
                const float t = (ip[x] - fp[x]).dot(dp[x]) / dd;
                tp[x] = t;
                rp[x] = float(cv::norm(ip[x] - (fp[x] + t * dp[x])));
            }
        }
        /* Local mean and std of v over the pixels where m is 1, at full resolution. */
        auto localStat = [&](const cv::Mat &v, const cv::Mat &m, cv::Mat &mu, cv::Mat &sd) {
            cv::Mat v3(qs, CV_32FC3);
            for (int y = 0; y < qs.height; ++y) {
                const float *vp = v.ptr<float>(y);
                cv::Vec3f *o = v3.ptr<cv::Vec3f>(y);
                for (int x = 0; x < qs.width; ++x) o[x] = cv::Vec3f(vp[x], vp[x] * vp[x], vp[x]);
            }
            const cv::Mat f = pushPull(v3, m);
            cv::Mat muq(qs, CV_32F), sdq(qs, CV_32F);
            for (int y = 0; y < qs.height; ++y) {
                const cv::Vec3f *fp = f.ptr<cv::Vec3f>(y);
                float *mp = muq.ptr<float>(y), *sp = sdq.ptr<float>(y);
                for (int x = 0; x < qs.width; ++x) {
                    mp[x] = fp[x][0];
                    sp[x] = std::sqrt(std::max(fp[x][1] - fp[x][0] * fp[x][0], 0.0f));
                }
            }
            cv::resize(muq, mu, full, 0, 0, cv::INTER_LINEAR);
            cv::resize(sdq, sd, full, 0, 0, cv::INTER_LINEAR);
        };
        cv::Mat muS, sdS, muF, sdF, rS, unused;
        localStat(tq, wsB, muS, sdS);
        localStat(tq, wfB, muF, sdF);
        localStat(rq, wsB, rS, unused);
        forRows(h, [&](int y) {
            const float *tp = tRaw.ptr<float>(y), *ms = muS.ptr<float>(y),
                        *ss = sdS.ptr<float>(y), *mf = muF.ptr<float>(y),
                        *sf = sdF.ptr<float>(y), *rs = rS.ptr<float>(y);
            float *ap = alphaC.ptr<float>(y), *rp = resid.ptr<float>(y);
            for (int x = 0; x < w; ++x) {
                const float lo = mf[x] + 2.0f * sf[x], hi = ms[x] - 2.0f * ss[x];
                if (hi - lo > 0.15f)
                    ap[x] = std::clamp((tp[x] - lo) / std::max(hi - lo, 1e-3f), 0.0f, 1.0f);
                rp[x] = std::max(rp[x] - rs[x], 0.0f);   // off-axis spread the sky itself has
            }
        });
    }

    /* Step 5b: snap the colour alpha beyond ITS OWN edge width (from its steepness), so
       only genuinely mixed pixels stay fractional. */
    {
        cv::Mat sm;
        cv::GaussianBlur(alphaC, sm, cv::Size(0, 0), 0.8);
        cv::Mat gmax;
        cv::dilate(gradMag(sm), gmax, ellipse(std::max(2, L / 400)));
        cv::Mat wE(full, CV_32F);
        forRows(h, [&](int y) {
            const float *gp = gmax.ptr<float>(y);
            float *wp = wE.ptr<float>(y);
            for (int x = 0; x < w; ++x)
                wp[x] = std::clamp(1.0f / std::max(gp[x], 1e-3f), 1.0f, L / 100.0f);
        });
        snapBeyondEdgeWidth(alphaC, wE);
    }

    /* Steps 5-6: colour alpha in the band; promote-only recovery. */
    cv::Mat out(full, CV_32F);
    forRows(h, [&](int y) {
        const float *ap = alphaC.ptr<float>(y), *sq = sep.ptr<float>(y),
                    *rp = resid.ptr<float>(y);
        const float *bs = base.ptr<float>(y), *db = dBig.ptr<float>(y),
                    *rc = recover.ptr<float>(y);
        const uchar *bp = band.ptr<uchar>(y);
        float *op = out.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            const float conf = smoothstep(0.06f, 0.18f, sq[x])
                               * (1.0f - smoothstep(0.05f, 0.15f, rp[x]));
            const float wb = (bp[x] ? 1.0f : 0.0f) * conf
                             * (1.0f - std::clamp(db[x] * 2.0f, 0.0f, 1.0f));
            const float b = std::clamp(bs[x], 0.0f, 1.0f);
            float o = b * (1.0f - wb) + ap[x] * wb;
            const float wr = rc[x] * smoothstep(0.12f, 0.25f, sq[x])
                             * (1.0f - smoothstep(0.04f, 0.10f, rp[x]));
            o = std::max(o, ap[x] * smoothstep(0.6f, 0.9f, ap[x]) * wr);
            op[x] = std::clamp(o, 0.0f, 1.0f);
        }
    });

    /* Step 7: snap the FINAL alpha beyond the PHOTO's edge width (contrast across the
       edge / steepest local gradient), everywhere. This also tightens the soft fallback
       where the colour test backed off, but leaves a genuinely hazy ridge soft. */
    snapBeyondEdgeWidth(out, photoEdgeWidth(rgb, sep, L));
    clamp01(out);

    /* Step 8: decontaminated alpha (see decontaminate()). */
    decontaminate(out, rgb);
    return out;
}

} // namespace SkyRefine

#endif // SKYREFINE_H
