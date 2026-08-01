#ifndef COLORGRADE_H
#define COLORGRADE_H

#include <cmath>

/*
    Pure colour-grading math, shared by the Develop point-op pipeline (Develop::build /
    applyPointOps) and its unit test. No Qt, no OpenCV, so it stays header-only and
    testable in isolation. Two pieces:

      gradeTintVector    hue (0..360 deg) + sat (0..1) -> a ZERO-LUMA RGB push (adds the
                         hue's chroma without shifting Rec.709 luma), by `strength`.
      gradeTonalWeights  a pixel's perceptual lightness L (0..1) -> three smooth weights
                         for shadows / midtones / highlights that partition tone (sum ~1).
*/
namespace ColorGrade {

/* HSV(hueDeg, 1, 1) -> RGB (each 0..1), then subtract Rec.709 luma so the result only
   pushes chroma. Scaled by sat*strength. sat <= 0 yields the zero vector. */
inline void gradeTintVector(float hueDeg, float sat, float strength, float out[3])
{
    out[0] = out[1] = out[2] = 0.0f;
    if (sat <= 0.0f) return;
    const float h  = hueDeg / 60.0f;
    const float ff = h - std::floor(h);
    const int   seg = (static_cast<int>(std::floor(h)) % 6 + 6) % 6;
    float r = 0, g = 0, b = 0;
    switch (seg) {
    case 0:  r = 1;      g = ff;     b = 0;      break;
    case 1:  r = 1 - ff; g = 1;      b = 0;      break;
    case 2:  r = 0;      g = 1;      b = ff;     break;
    case 3:  r = 0;      g = 1 - ff; b = 1;      break;
    case 4:  r = ff;     g = 0;      b = 1;      break;
    default: r = 1;      g = 0;      b = 1 - ff; break;
    }
    const float y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    const float k = sat * strength;
    out[0] = (r - y) * k;
    out[1] = (g - y) * k;
    out[2] = (b - y) * k;
}

/* Smoothstep partition of perceptual lightness L (0..1) into shadow / mid / highlight
   weights. shadowEnd = L where the shadow weight reaches 0; highStart = L where the
   highlight weight begins. Weights are non-negative and sum to 1. */
inline void gradeTonalWeights(float L, float shadowEnd, float highStart,
                              float &wS, float &wM, float &wH)
{
    float ts = L / shadowEnd;
    if (ts < 0.0f) ts = 0.0f; else if (ts > 1.0f) ts = 1.0f;
    wS = 1.0f - ts * ts * (3.0f - 2.0f * ts);
    float th = (L - highStart) / (1.0f - highStart);
    if (th < 0.0f) th = 0.0f; else if (th > 1.0f) th = 1.0f;
    wH = th * th * (3.0f - 2.0f * th);
    wM = 1.0f - wS - wH;
    if (wM < 0.0f) wM = 0.0f;
}

} // namespace ColorGrade

#endif // COLORGRADE_H
