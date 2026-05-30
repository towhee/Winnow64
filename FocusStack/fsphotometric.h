// fsphotometric.h
#ifndef FSPHOTOMETRIC_H
#define FSPHOTOMETRIC_H

#include <opencv2/core.hpp>

namespace FSPhotometric {

// Validate coefficient mats (accept empty as "disabled")
bool isValidContrast5x1(const cv::Mat& contrast);      // 5x1 CV_32F
bool isValidWhiteBalance6x1(const cv::Mat& wb);        // 6x1 CV_32F

// Apply contrast polynomial (spatially varying gain) to gray8 and color.
// Apply white balance affine to color only.
//
// Inputs:
//   gray8Orig  : CV_8U, 1-channel
//   colorOrig  : CV_8U or CV_16U, 3-channel preferred (1/4 accepted; will be converted to BGR)
//   contrast5x1: [C0, Cx, Cx2, Cy, Cy2]^T (CV_32F, 5x1). If empty -> skipped.
//   wb6x1      : FSAlign-style ordering: [B_off, B_gain, G_off, G_gain, R_off, R_gain]^T (CV_32F, 6x1). If empty -> skipped.
//
// Notes:
// - wbOffsetsAre8bitUnits should be true if WB offsets were fit on 8-bit images (your current FSAlign does).
// - If your fitter later produces WB offsets directly in 16-bit units, pass wbOffsetsAre8bitUnits=false.
//
void applyPhotometricNormalizationOrig(cv::Mat& gray8Orig,
                                       cv::Mat& colorOrig,
                                       const cv::Mat& contrast5x1,
                                       const cv::Mat& wb6x1,
                                       bool wbOffsetsAre8bitUnits = true,
                                       float gainLo = 0.25f,
                                       float gainHi = 4.00f);

} // namespace FSPhotometric

#endif // FSPHOTOMETRIC_H
