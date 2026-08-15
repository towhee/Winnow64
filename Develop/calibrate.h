#ifndef CALIBRATE_H
#define CALIBRATE_H

#include <cmath>

/*
    Pure camera-calibration math, shared by the Develop point-op pipeline
    (Develop::buildPointCoeffs / applyPointOps) and its unit test. No Qt, no OpenCV, so it
    stays header-only and testable in isolation -- the same shape as Develop/colorgrade.h.

    WHAT THIS IS. Lightroom's Calibration panel rotates the three PRIMARIES of the working
    space: each of R, G and B gets a hue shift and a saturation scale, and the resulting
    three vectors become the columns of a 3x3 matrix applied to every pixel. Because the
    matrix is built so its columns sum to the neutral axis, GREY STAYS GREY -- the
    transform re-points the primaries without tinting neutrals.

    WHAT THIS IS NOT. The Color panel's red/green/blue sliders are per-channel linear
    GAINS folded into the white-balance vector (see Develop::buildPointCoeffs): a cruder,
    second white balance. Different maths, different pipeline slot, and they coexist.

    WHERE IT RUNS. Before the tone curve, in linear light -- unlike the Color panel's HSL
    hue rotation, which runs after it. Rotating primaries is a change of colour space and
    belongs upstream of anything perceptual.

    UNITS. hue and sat are both -100..100 (the slider range). hue maps to +/-kMaxHueDeg of
    rotation about the neutral axis; sat maps to a chroma scale of 1 +/- 1 about that
    axis. All zero yields exactly the identity matrix.
*/
namespace Calibrate {

/* Full-scale slider value, and the hue rotation it buys. 30 deg at +/-100 matches the
   useful working range: enough to re-point a primary noticeably, not so much that the
   primary crosses into its neighbour. */
constexpr float kFullScale  = 100.0f;
constexpr float kMaxHueDeg  = 30.0f;

/* Rec.709 luma weights, used to hold a primary's brightness steady while its chroma is
   scaled. */
constexpr float kLumR = 0.2126f;
constexpr float kLumG = 0.7152f;
constexpr float kLumB = 0.0722f;

/* Rodrigues rotation of v about the neutral axis (1,1,1)/sqrt(3) by angle radians, the
   same construction Develop's HSL hue slider uses. Neutral colours lie ON the axis and so
   are fixed points, and the rotation preserves v's component ALONG that axis -- i.e. the
   equal-weight sum r+g+b, NOT the Rec.709 luma (those differ, and assuming otherwise is
   an easy mistake: see tst_calibrate::hueShiftPreservesNeutralComponent). */
inline void rotateAboutNeutral(const float v[3], float angle, float out[3])
{
    const float cs = std::cos(angle);
    const float sn = std::sin(angle);
    const float k  = (1.0f - cs) / 3.0f;        // (1-cos)*nx*ny, axis (1,1,1)/sqrt3
    const float w  = sn * 0.57735027f;              // sin / sqrt(3)
    const float m0 = cs + k, m1 = k - w,    m2 = k + w;
    const float m3 = k + w,  m4 = cs + k,   m5 = k - w;
    const float m6 = k - w,  m7 = k + w,    m8 = cs + k;
    out[0] = m0 * v[0] + m1 * v[1] + m2 * v[2];
    out[1] = m3 * v[0] + m4 * v[1] + m5 * v[2];
    out[2] = m6 * v[0] + m7 * v[1] + m8 * v[2];
}

/* Scale v's chroma about its own Rec.709 luma by `scale` (1 = unchanged, 0 = the neutral
   grey of the same luma, 2 = double chroma). Luma-preserving, so a saturation slider does
   not double as a brightness control for that primary. */
inline void scaleChroma(float v[3], float scale)
{
    const float y = kLumR * v[0] + kLumG * v[1] + kLumB * v[2];
    v[0] = y + (v[0] - y) * scale;
    v[1] = y + (v[1] - y) * scale;
    v[2] = y + (v[2] - y) * scale;
}

/* Build the 3x3 calibration matrix (row-major: out[row*3 + col]) from the six slider
   values, each -100..100. Each primary's unit vector is rotated about the neutral axis by
   its hue and chroma-scaled by its sat; the three results become the matrix COLUMNS, so
   applying the matrix maps (1,0,0) -> the new red, and so on.

   THEN EVERY ROW IS NORMALISED TO SUM TO 1, and that is what keeps grey grey. Note it is
   the ROW sums that matter, not the column sums: (M*(1,1,1))_i is the sum of row i. It is
   an easy slip to "fix" this by making the columns sum to 1 instead -- each primary then
   keeps its own neutral component, which sounds right and is useless, because white is
   still not preserved. Row normalisation makes M*(1,1,1) == (1,1,1) exactly.

   THE SLIDERS THEREFORE INTERACT, and that is correct, not a defect. Turning red toward
   green necessarily gives reds a green response; holding white fixed means green's own
   response must drop to compensate. A real primary change behaves the same way, and
   Lightroom's calibration sliders interact for the same reason. What DOES survive is
   direction: an untouched primary's column stays parallel to its own axis and is only
   rescaled (see tst_calibrate::untouchedPrimaryKeepsItsDirection).

   With every slider at 0 this is exactly the identity, which is what lets the caller skip
   the whole stage (see isIdentity below). */
inline void buildMatrix(float redHue, float redSat,
                        float greenHue, float greenSat,
                        float blueHue, float blueSat,
                        float out[9])
{
    const float hue[3] = {redHue, greenHue, blueHue};
    const float sat[3] = {redSat, greenSat, blueSat};
    for (int c = 0; c < 3; ++c) {
        float v[3] = {0.0f, 0.0f, 0.0f};
        v[c] = 1.0f;                                    // this primary's unit vector
        const float ang = (hue[c] / kFullScale) * kMaxHueDeg * 3.14159265358979323846f
                          / 180.0f;
        if (ang != 0.0f) {
            float r[3];
            rotateAboutNeutral(v, ang, r);
            v[0] = r[0]; v[1] = r[1]; v[2] = r[2];
        }
        if (sat[c] != 0.0f) scaleChroma(v, 1.0f + sat[c] / kFullScale);
        out[0 * 3 + c] = v[0];                          // primary c becomes column c
        out[1 * 3 + c] = v[1];
        out[2 * 3 + c] = v[2];
    }
    /* White-preserving normalisation: make every ROW sum to 1 so M*(1,1,1) == (1,1,1). */
    for (int r = 0; r < 3; ++r) {
        const float s = out[r * 3 + 0] + out[r * 3 + 1] + out[r * 3 + 2];
        if (std::fabs(s) < 1e-6f) continue;             // degenerate; leave the row alone
        out[r * 3 + 0] /= s;
        out[r * 3 + 1] /= s;
        out[r * 3 + 2] /= s;
    }
}

/* True when the six values leave the matrix at identity, so the caller can skip it. */
inline bool isIdentity(float redHue, float redSat, float greenHue, float greenSat,
                       float blueHue, float blueSat)
{
    return redHue == 0.0f && redSat == 0.0f && greenHue == 0.0f &&
           greenSat == 0.0f && blueHue == 0.0f && blueSat == 0.0f;
}

} // namespace Calibrate

#endif // CALIBRATE_H
