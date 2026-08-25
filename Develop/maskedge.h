#ifndef MASKEDGE_H
#define MASKEDGE_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

/*
    GROW or SHRINK a Develop mask boundary by a signed PIXEL distance.

    WHY THIS EXISTS. Feather softens an edge; it does not move one. And feather is not a
    pixel distance for any tool -- it is a fraction of a radius (Radial), of the p1->p2
    length (Linear), of a value band (Luminance/Colour/Depth Range) or a threshold band on
    an alpha (Subject/Sky/Object). So there was no way to say "this cutout lands two
    pixels inside the subject, push it out two pixels", which is the single most common
    fix an AI mask needs. That is Photoshop's Select > Modify > Expand/Contract, and it
    is what the Edge slider does: +n dilates by n px, -n erodes by n px.

    Grayscale dilation by a flat structuring element translates a soft edge RIGIDLY -- the
    feather profile rides along instead of being reshaped -- which is exactly the wanted
    behaviour, and is why this is morphology rather than a re-threshold of a distance
    field.

    WHAT "ONE PIXEL" MEANS. One pixel of the FULL-RESOLUTION image. The caller scales the
    slider value by the render's long edge over the full-res long edge before calling, so
    the proxy, the veil and the settle render all agree. At a typical ~0.3 proxy a 1-unit
    nudge is a 0.3 px radius, so fractional radii have to work properly -- see kMinRadius
    and the lerp in runFrac().

    THE SHAPE IS AN OCTAGON, NOT A SQUARE. A separable square (an L-infinity ball) is the
    cheap decomposition, but it displaces a 45-degree boundary by r*sqrt(2) -- a 41% error
    against a slider whose whole contract is "1 unit = 1 pixel", and it would put the
    analytic tools (Linear Gradient shifts its threshold exactly) and the buffer tools 41%
    apart on the same diagonal edge. The octagon

        square(a) (+) diamond(b)     a = r(sqrt2 - 1),  b = r(2 - sqrt2)

    is exact along BOTH the axes (a + b = r) and the 45-degree diagonals
    (a*sqrt2 + b/sqrt2 = r), and overshoots by at most 8.2% at the eight vertices. The
    diamond is itself the Minkowski sum of the two diagonal segments of half-length b/2
    (because max(|u+v|,|u-v|) == |u|+|v|), so the whole octagon is FOUR separable 1-D
    passes: horizontal, vertical, then the two diagonals.

    COST. Each pass is van Herk / Gil-Werman: a forward prefix and a backward suffix
    extremum over blocks of the window width, then one combine per pixel. That is O(1) per
    pixel REGARDLESS OF RADIUS, so Edge +-100 costs exactly what Edge +-1 costs. The
    diagonal passes stride by w+1 and w-1, which is cache-unfriendly, but they are still
    O(1) per pixel and they only run when the slider is off zero.

    SAMPLES ARE uint16, not float. MaskFalloff::kCutoff is 1/512, so 16 bits sits about
    seven bits below anything the coverage profile itself promises, and it halves both the
    scratch footprint and the memory traffic of four full passes over a 45 MP buffer.

    EVERY PASS IS IN PLACE. Each 1-D line is gathered into a per-thread scratch, processed
    and written back, and lines within a pass are disjoint -- so no image-sized temporary
    is needed at any point, only the one buffer the caller already has.
*/
namespace MaskEdge {

inline constexpr uint16_t kOne = 65535;

/* Below this the octagon's sub-passes round to nothing and the four gather/scatter passes
   would be pure cost for a result indistinguishable from the input. */
inline constexpr double kMinRadius = 0.05;

inline constexpr double kSqrt2 = 1.4142135623730951;

inline uint16_t toU16(float v)
{
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return kOne;
    return uint16_t(v * 65535.0f + 0.5f);
}

inline float toFloat(uint16_t v) { return float(v) * (1.0f / 65535.0f); }

/* Per-thread working store. One instance per parallel band, reused across every line in
   that band, so a whole pass allocates once per thread rather than once per line. */
struct Scratch {
    std::vector<uint16_t> pad, pre, suf, line, copy;
};

/*
    van Herk / Gil-Werman running extremum over the window [i-R, i+R], written back over
    `line` in place.

    The line is copied into `pad` with R neutral samples at each end (0 for max, 65535
    for min), which makes the extremum over a clipped window fall out of the same
    arithmetic as the interior -- a mask touching the frame edge neither grows out of the
    frame nor gets eroded in from it. Output i is then the extremum over pad[i .. i+2R],
    a window exactly one block wide, so it spans at most two blocks and pre[]/suf[] cover
    it exactly.
*/
template <bool DoMax>
inline void run1D(uint16_t *line, int n, int R, Scratch &s)
{
    if (R <= 0 || n <= 0) return;
    const int W = 2 * R + 1;
    const int len = n + 2 * R;
    s.pad.resize(size_t(len));
    s.pre.resize(size_t(len));
    s.suf.resize(size_t(len));
    const uint16_t neutral = DoMax ? uint16_t(0) : kOne;
    for (int i = 0; i < R; ++i)         s.pad[size_t(i)] = neutral;
    for (int i = 0; i < n; ++i)         s.pad[size_t(R + i)] = line[i];
    for (int i = R + n; i < len; ++i)   s.pad[size_t(i)] = neutral;

    for (int i = 0; i < len; ++i) {                  // forward prefix within each block
        const uint16_t v = s.pad[size_t(i)];
        s.pre[size_t(i)] = (i % W == 0) ? v
                         : (DoMax ? std::max(s.pre[size_t(i - 1)], v)
                                  : std::min(s.pre[size_t(i - 1)], v));
    }
    for (int i = len - 1; i >= 0; --i) {             // backward suffix within each block
        const uint16_t v = s.pad[size_t(i)];
        s.suf[size_t(i)] = (i == len - 1 || (i + 1) % W == 0) ? v
                         : (DoMax ? std::max(s.suf[size_t(i + 1)], v)
                                  : std::min(s.suf[size_t(i + 1)], v));
    }
    const int off = 2 * R;
    for (int i = 0; i < n; ++i)
        line[i] = DoMax ? std::max(s.suf[size_t(i)], s.pre[size_t(i + off)])
                        : std::min(s.suf[size_t(i)], s.pre[size_t(i + off)]);
}

/*
    Fractional radius: run the integer radii either side and lerp. Doing it here, inside
    the 1-D primitive, costs one extra line-length temporary rather than a second
    image-sized buffer -- and it is what makes a single arrow-key nudge visible in a 0.3x
    proxy, where a 1 px radius arrives as 0.3. Dilation is monotone in R, so the blend is
    monotone too; on a linear ramp it is exact, since lerping two translates of a ramp is
    a translate by the lerped distance.
*/
template <bool DoMax>
inline void runFrac(uint16_t *line, int n, double Rf, Scratch &s)
{
    if (n <= 0 || Rf <= 0.0) return;
    const int R0 = int(std::floor(Rf));
    const double f = Rf - double(R0);
    if (f < 1e-4) { run1D<DoMax>(line, n, R0, s); return; }
    s.copy.assign(line, line + n);
    run1D<DoMax>(line, n, R0, s);
    run1D<DoMax>(s.copy.data(), n, R0 + 1, s);
    for (int i = 0; i < n; ++i)
        line[i] = uint16_t(std::lround(double(line[i]) * (1.0 - f)
                                     + double(s.copy[size_t(i)]) * f));
}

/* Gather a strided line, run the extremum, scatter it back. stride is always POSITIVE:
   the anti-diagonals are walked with x descending so their step is +(w-1). */
template <bool DoMax>
inline void runLine(uint16_t *buf, ptrdiff_t start, int n, ptrdiff_t stride,
                    double Rf, Scratch &s)
{
    if (n <= 0) return;
    if (stride == 1) { runFrac<DoMax>(buf + start, n, Rf, s); return; }
    s.line.resize(size_t(n));
    for (int i = 0; i < n; ++i) s.line[size_t(i)] = buf[start + ptrdiff_t(i) * stride];
    runFrac<DoMax>(s.line.data(), n, Rf, s);
    for (int i = 0; i < n; ++i) buf[start + ptrdiff_t(i) * stride] = s.line[size_t(i)];
}

/*
    The four octagon passes. `a` is the square's half-width in pixels; `dg` is each
    diagonal's half-length in STEPS, where one step moves (1,1) or (1,-1) -- so the
    diamond those two segments generate has half-width b = 2*dg.
*/
template <bool DoMax, class Par>
inline void passes(uint16_t *buf, int w, int h, double a, double dg, Par &&par)
{
    if (a >= kMinRadius) {
        par(h, [&](int y0, int y1) {                 // horizontal
            Scratch s;
            for (int y = y0; y < y1; ++y)
                runLine<DoMax>(buf, ptrdiff_t(y) * w, w, 1, a, s);
        });
        par(w, [&](int x0, int x1) {                 // vertical
            Scratch s;
            for (int x = x0; x < x1; ++x)
                runLine<DoMax>(buf, x, h, w, a, s);
        });
    }
    if (dg < kMinRadius || w < 2 || h < 2) return;

    /* Diagonals x - y = c, c in [-(h-1), w-1]; index = x*(w+1) - c*w, step +(w+1). */
    par(w + h - 1, [&](int j0, int j1) {
        Scratch s;
        for (int j = j0; j < j1; ++j) {
            const int c = j - (h - 1);
            const int xlo = std::max(0, c), xhi = std::min(w - 1, c + h - 1);
            runLine<DoMax>(buf, ptrdiff_t(xlo) * (w + 1) - ptrdiff_t(c) * w,
                           xhi - xlo + 1, w + 1, dg, s);
        }
    });
    /* Anti-diagonals x + y = d, d in [0, w+h-2]; walked with x DESCENDING from xhi so
       the step is +(w-1), not negative -- an extremum does not care about direction. */
    par(w + h - 1, [&](int j0, int j1) {
        Scratch s;
        for (int d = j0; d < j1; ++d) {
            const int xlo = std::max(0, d - (h - 1)), xhi = std::min(w - 1, d);
            if (xhi < xlo) continue;
            runLine<DoMax>(buf, ptrdiff_t(d - xhi) * w + xhi,
                           xhi - xlo + 1, w - 1, dg, s);
        }
    });
}

/*
    Grow (radiusPx > 0) or shrink (radiusPx < 0) the coverage in `buf` by |radiusPx|
    pixels OF THIS BUFFER. In place; no image-sized allocation.

    `par(count, fn)` must run fn(i0, i1) over disjoint half-open sub-ranges covering
    [0, count) -- the caller supplies it so this header stays free of Qt and of the
    project's thread pool.
*/
template <class Par>
inline void apply(std::vector<uint16_t> &buf, int w, int h, double radiusPx, Par &&par)
{
    if (w <= 0 || h <= 0 || buf.size() != size_t(w) * size_t(h)) return;
    const double r = std::fabs(radiusPx);
    if (r < kMinRadius) return;
    const double a  = r * (kSqrt2 - 1.0);            // square half-width, px
    const double dg = r * (1.0 - 1.0 / kSqrt2);      // diagonal half-length, steps
    if (radiusPx > 0.0) passes<true >(buf.data(), w, h, a, dg, par);
    else                passes<false>(buf.data(), w, h, a, dg, par);
}

} // namespace MaskEdge

#endif // MASKEDGE_H
