#ifndef CAMERAPROFILE_H
#define CAMERAPROFILE_H

#include "Develop/colorspace.h"
#include "ImageFormats/Dcp/dcp.h"

/*
    Turning a parsed DNG camera profile into the ONE 3x3 the pipeline needs -- slice 2 of
    Phase 4 (see notes/Documentation.txt, "Camera Profiles (DCP) -- Phase 4 Plan").
    ImageFormats/Dcp reads the file; this interprets it.

    WHAT IT PRODUCES. camToWorking(): camera-native RGB -> the working space, for a chosen
    white balance. That is exactly the slot CameraColor::camToWorking already fills, and it
    folds into PointCoeffs::preMat the same way, so the profile costs NO extra per-pixel
    work -- only a matrix build per render.

    THE CHAIN, and why each step is there:

        camera RGB
          -> inv(AnalogBalance . CameraCalibration)      undo the per-INDIVIDUAL-camera
                                                          correction (usually identity)
          -> diag(1 / referenceNeutral)                   the WHITE BALANCE
          -> ForwardMatrix                                camera -> XYZ, landing on D50
          -> Bradford D50 -> the working space's white    the one adaptation in the pipeline
          -> XYZ -> working primaries
        = working RGB

    THE WHITE BALANCE IS INSIDE THE MATRIX, which is the substantive difference from the
    dcraw-style path Winnow uses today (a fixed matrix with per-channel gains in front of
    it). Under DNG the chosen white picks the interpolation weight AND sets diag(1/ref), so
    the matrix is re-derived per temperature. That is what makes a camera profile track
    mixed lighting, and it is why "the same Kelvin means a different rendering under a
    different profile" -- the coupling is the feature.

    EXPOSURE IS PRESERVED, and this is worth stating because it is easy to assume otherwise.
    The as-shot camera neutral is normalised to GREEN == 1 (CameraColor::asShotMul's own
    convention), and the chain above maps it to exactly (1,1,1) in the working space: with a
    ForwardMatrix because a well-formed one has rows summing to the D50 white, and without
    one because the adaptation is built to carry that white onto D50 exactly. The existing
    rawcolor.cpp path anchors the same point the same way (it row-normalises so camera
    (1,1,1) -> working (1,1,1)). So switching a raw onto a profile re-points its colour
    WITHOUT moving its brightness.

    NOTHING HERE IS PER-PIXEL. The lookup tables (HueSatMap, LookTable) are slice 3 and 4
    and are not touched; a profile's tables are simply not applied yet.
*/
namespace CameraProfile {

/*
    One profile collapsed onto a single temperature: the two calibrations interpolated in
    MIRED (1 / kelvin), which is how the DNG specification weights them and how a
    perceptually even step in temperature is defined.
*/
struct Resolved {
    ColorSpaceMath::Matrix3 color       = ColorSpaceMath::kIdentity3;  // XYZ -> camera
    ColorSpaceMath::Matrix3 forward     = ColorSpaceMath::kIdentity3;  // camera -> XYZ(D50)
    ColorSpaceMath::Matrix3 calibration = ColorSpaceMath::kIdentity3;  // per-individual camera
    bool  haveForward = false;      // false -> camToWorking derives the transform from color
    float weight = 0.0f;            // 0 = entirely the warmer calibration, 1 = the cooler
    float warmKelvin = 0.0f;        // the two calibration temperatures, warm first, so a
    float coolKelvin = 0.0f;        // caller can report what it interpolated between
};

/* Interpolate the profile's calibrations for a colour temperature. False when the profile
   carries no usable ColorMatrix at all. A single-illuminant profile succeeds with
   weight == 0 whatever the temperature -- there is nothing to interpolate towards. */
bool resolve(const Dcp::Profile &p, float kelvin, Resolved &out);

/*
    The camera-native RGB a neutral surface under illuminant (kelvin, tint) produces --
    DNG's "camera neutral", normalised to GREEN == 1 to match CameraColor::asShotMul.
    The illuminant comes from WhiteBalance::illuminantXYZ, so the profile and the Temp
    slider cannot disagree about what a temperature means.
*/
bool neutralCam(const Dcp::Profile &p, float kelvin, float tint, double n[3]);

/* The full camera-native -> working-space matrix for a chosen white balance. False when
   the profile is unusable (no ColorMatrix, or a singular matrix in the chain). */
bool camToWorking(const Dcp::Profile &p, float kelvin, float tint, float out[3][3]);

} // namespace CameraProfile

#endif // CAMERAPROFILE_H
