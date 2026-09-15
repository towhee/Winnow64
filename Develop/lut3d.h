#ifndef LUT3D_H
#define LUT3D_H

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

/*
    A creative LOOK: an RGB-domain colour lookup table, the open-format answer to what
    Adobe ships as a camera-matching "look". Pure math, no Qt and no file reader, so it
    unit-tests in isolation -- the same shape as Develop/huesatmap.h, Develop/calibrate.h
    and Develop/colorgrade.h. Develop/lutparse.h reads .cube and HaldCLUT files into this
    struct; Develop/lutstore finds them; OutputTransform applies one.

    NOT THE SAME THING AS A CAMERA PROFILE, and the distinction is the whole design.
    A DCP characterises the SENSOR and replaces stage 0, so it runs at the very front of
    the pipeline in linear ProPhoto. A look is a GRADE: it is authored against
    display-referred sRGB and is applied last, after the output transfer, because that is
    the encoding its author saw. See notes/Documentation.txt, "Film-Look LUTs".

    STORAGE ORDER, which is the thing to get wrong silently:

        index = ((b * size + g) * size + r) * 3

    RED VARIES FASTEST. Both formats agree on this -- a .cube's data lines and a
    HaldCLUT's raster order are the same sequence -- so one indexer serves both, and a
    transposition in either parser shows up as a red/blue swap. tst_lutparse proves it two
    ways rather than trusting the spec: an eight-corner 2x2x2 cube, and a cross-format
    agreement test that a transposition in one parser alone cannot pass.

    VALUES ARE NOT CLAMPED ON THE WAY IN. A look may legitimately emit above 1.0 or below
    0, and clamping here would quietly destroy an HDR-authored table; the caller clamps at
    quantisation, which is the only place that has to.
*/
namespace Lut3d {

/*
    128^3 * 3 floats is 24 MB, which is already far past any sane look. The .cube spec
    allows 256, but a table that large is a denial-of-service dressed as a file, and
    these are untrusted user content.
*/
constexpr int kMaxSize = 128;

struct Table {
    /* 3-D lattice: size^3 entries. Zero when this is a 1-D table. */
    int size = 0;
    std::vector<float> v;

    /*
        The 1-D case (.cube's LUT_1D_SIZE) -- a per-channel curve, a genuinely different
        animal that shares this struct rather than a parallel one. Carried because 1-D
        cubes are common enough in free LUT packs that rejecting them would read as a bug
        on half a downloaded folder.
    */
    int size1D = 0;
    std::vector<float> v1D;             // size1D * 3

    /* DOMAIN_MIN / DOMAIN_MAX: the input range the table is indexed over. Almost always
       0..1; a log-authored table is the usual reason it is not. */
    float domainMin[3] = {0.0f, 0.0f, 0.0f};
    float domainMax[3] = {1.0f, 1.0f, 1.0f};

    /* TITLE, or the file's name when it carries none. Display only -- NEVER an identity:
       titles are missing or duplicated across a pack often enough that the store keys on
       the file's path instead. */
    std::string title;

    bool is3D() const {
        return size >= 2 && v.size() == size_t(size) * size_t(size) * size_t(size) * 3;
    }
    bool is1D() const {
        return size1D >= 2 && v1D.size() == size_t(size1D) * 3;
    }
    bool isValid() const { return is3D() || is1D(); }
    bool isEmpty()  const { return !isValid(); }
};

namespace detail {

/* Domain map then clamp. A value outside the domain takes the boundary entry rather than
   extrapolating: a look has no defined behaviour past its own range, and extrapolating a
   3-D table is how a highlight turns a colour nobody chose. */
inline float norm(const Table &t, int c, float x)
{
    const float lo = t.domainMin[c], hi = t.domainMax[c];
    const float d = hi - lo;
    /* Negated compares so a NaN lands on the 0 branch instead of falling through. */
    if (!(d > 0.0f)) return 0.0f;
    const float u = (x - lo) / d;
    if (!(u > 0.0f)) return 0.0f;
    return u < 1.0f ? u : 1.0f;
}

/* Lattice coordinate -> base index + fraction, with the top cell folded back one so
   i + 1 is always addressable. */
inline void axis(float u, int n, int &i, float &f)
{
    const float p = u * float(n - 1);
    int k = int(p);
    if (k < 0) k = 0;
    if (k > n - 2) k = n - 2;
    i = k;
    f = p - float(k);
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
}

inline const float *at(const Table &t, int r, int g, int b)
{
    return &t.v[(size_t((b * t.size + g) * t.size + r)) * 3];
}

} // namespace detail

/*
    TRILINEAR -- the REFERENCE implementation, kept for tst_lut3d and not used in the
    pipeline. It is the obvious reading of a 3-D table and it agrees with the tetrahedral
    sampler exactly at every lattice point, which is what makes it a useful oracle.

    It is not what renders, because it blends all eight surrounding corners: on the grey
    axis six of those are off-axis, so every neutral picks up a small tint. See Sample().
*/
inline void SampleTrilinear(const Table &t, float &r, float &g, float &b)
{
    if (!t.is3D()) return;
    int ir, ig, ib;
    float fr, fg, fb;
    detail::axis(detail::norm(t, 0, r), t.size, ir, fr);
    detail::axis(detail::norm(t, 1, g), t.size, ig, fg);
    detail::axis(detail::norm(t, 2, b), t.size, ib, fb);

    float out[3] = {0.0f, 0.0f, 0.0f};
    for (int db = 0; db < 2; ++db) {
        const float wb = db ? fb : 1.0f - fb;
        for (int dg = 0; dg < 2; ++dg) {
            const float wg = dg ? fg : 1.0f - fg;
            for (int dr = 0; dr < 2; ++dr) {
                const float w = wb * wg * (dr ? fr : 1.0f - fr);
                if (w == 0.0f) continue;
                const float *c = detail::at(t, ir + dr, ig + dg, ib + db);
                out[0] += w * c[0];
                out[1] += w * c[1];
                out[2] += w * c[2];
            }
        }
    }
    r = out[0]; g = out[1]; b = out[2];
}

/*
    TETRAHEDRAL -- what actually renders.

    WHY, in this pipeline's own terms. The unit cell is split into six tetrahedra and the
    result is a weighted sum of FOUR corners instead of eight. Two consequences:

    NEUTRALS STAY NEUTRAL. When r == g == b the point lies on the cell's main diagonal,
    which is an EDGE shared by every one of the six tetrahedra, so the result is a plain
    lerp between the two grey lattice entries. A look that keeps its grey axis neutral
    therefore keeps it exactly neutral. Trilinear cannot: six of its eight corners are off
    the diagonal, so it tints every grey by a little, and that error compounds with
    everything downstream. This is the same property AgxInsetNormalised() normalises a
    matrix to protect (see outputtransform.cpp), and it is asserted both ways in
    tst_lut3d -- tetrahedral must hold the axis, trilinear must measurably fail to, so
    swapping the sampler back cannot pass silently.

    IT IS CHEAPER. Four lattice fetches and three lerps against eight and seven; this
    loop is bound by the scattered loads, so the fetch count is what matters.

    IT IS WHAT THE FORMAT EXPECTS. Resolve, ACES and the .cube ecosystem interpolate
    tetrahedrally, so a look renders as its author saw it.
*/
inline void Sample(const Table &t, float &r, float &g, float &b)
{
    if (!t.is3D()) return;
    int ir, ig, ib;
    float fr, fg, fb;
    detail::axis(detail::norm(t, 0, r), t.size, ir, fr);
    detail::axis(detail::norm(t, 1, g), t.size, ig, fg);
    detail::axis(detail::norm(t, 2, b), t.size, ib, fb);

    const float *c000 = detail::at(t, ir,     ig,     ib);
    const float *c111 = detail::at(t, ir + 1, ig + 1, ib + 1);

    /*
        The six cases are the six orderings of (fr, fg, fb). Each walks c000 -> c111 by
        stepping ONE axis at a time in descending-weight order, which is exactly what
        keeps the path along the edges of a single tetrahedron. `step` names the two
        intermediate corners as axis offsets; the weights are the fractions in that same
        descending order.

        Read a row as: take the largest fraction's axis first, then the next.
    */
    struct Case { int o1[3]; int o2[3]; float w1, w2, w3; };
    const Case c =
        fr > fg
            ? (fg > fb ? Case{{1,0,0}, {1,1,0}, fr, fg, fb}    // fr > fg > fb
             : fr > fb ? Case{{1,0,0}, {1,0,1}, fr, fb, fg}    // fr > fb > fg
                       : Case{{0,0,1}, {1,0,1}, fb, fr, fg})   // fb > fr > fg
            : (fb > fg ? Case{{0,0,1}, {0,1,1}, fb, fg, fr}    // fb > fg > fr
             : fb > fr ? Case{{0,1,0}, {0,1,1}, fg, fb, fr}    // fg > fb > fr
                       : Case{{0,1,0}, {1,1,0}, fg, fr, fb});  // fg > fr > fb

    const float *p1 = detail::at(t, ir + c.o1[0], ig + c.o1[1], ib + c.o1[2]);
    const float *p2 = detail::at(t, ir + c.o2[0], ig + c.o2[1], ib + c.o2[2]);
    const float w1 = c.w1, w2 = c.w2, w3 = c.w3;

    float out[3];
    for (int k = 0; k < 3; ++k) {
        out[k] = c000[k] + w1 * (p1[k] - c000[k])
                         + w2 * (p2[k] - p1[k])
                         + w3 * (c111[k] - p2[k]);
    }
    r = out[0]; g = out[1]; b = out[2];
}

/* The 1-D case: an independent curve per channel, linearly interpolated. */
inline void Sample1D(const Table &t, float &r, float &g, float &b)
{
    if (!t.is1D()) return;
    float *ch[3] = {&r, &g, &b};
    for (int c = 0; c < 3; ++c) {
        int i;
        float f;
        detail::axis(detail::norm(t, c, *ch[c]), t.size1D, i, f);
        const float a = t.v1D[size_t(i) * 3 + c];
        const float z = t.v1D[size_t(i + 1) * 3 + c];
        *ch[c] = a + f * (z - a);
    }
}

/*
    Apply whichever kind of table this is. The ONE entry point the render path calls, so
    that neither OutputTransform nor any future caller has to know that .cube has two
    shapes. An invalid table is a no-op rather than an error: a look that failed to parse
    was already refused at load, and a render is not the place to discover it.
*/
inline void Apply(const Table &t, float &r, float &g, float &b)
{
    if (t.is3D()) Sample(t, r, g, b);
    else if (t.is1D()) Sample1D(t, r, g, b);
}

} // namespace Lut3d

#endif // LUT3D_H
