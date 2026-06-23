#ifndef CAMERAMATRIX_H
#define CAMERAMATRIX_H

#include <QString>

/*
    Per-camera-model colour matrix lookup.

    Returns the CIE XYZ(D65) -> camera-native-RGB matrix for a camera model (the DNG
    ColorMatrix / dcraw-libraw "adobe_coeff" convention). RawColor uses it to build the
    camera-RGB -> linear-sRGB transform and to derive a neutral white balance.

    The table mirrors libraw's adobe_coeff: model string -> nine integers (matrix * 10000).
    It is seeded with a small set of models and grows as formats are added; xyzToCamForModel()
    returns false for an unknown model, and the caller then falls back to an identity matrix
    (approximate colour). Model strings match ImageMetadata::model (e.g. "Sony ILCE-9M2").
*/
bool xyzToCamForModel(const QString &model, float m[3][3]);

#endif // CAMERAMATRIX_H
