#ifndef CAMERAMATRIX_H
#define CAMERAMATRIX_H

#include <QString>

/*
    Per-camera-model colour matrix lookup.

    Returns the CIE XYZ(D65) -> camera-native-RGB matrix for a camera model (the DNG
    ColorMatrix / dcraw-libraw "adobe_coeff" convention). RawColor uses it to build the
    camera-RGB -> linear-sRGB transform and to derive a neutral white balance.

    The table is a full port of libraw's adobe_coeff for the mainstream still-camera makers:
    maker-prefixed model string -> nine integers (matrix * 10000). xyzToCamForModel() does a
    case-insensitive LONGEST-PREFIX match against ImageMetadata::model (e.g. "Sony ILCE-9M2"
    matches the "Sony ILCE-9" key; "NIKON D800E" matches "Nikon D800"), returning false only for
    a model no key prefixes -- the caller then falls back to an identity matrix (approximate
    colour). See cameramatrix.cpp for the scope and how to regenerate from a newer libraw.
*/
bool xyzToCamForModel(const QString &model, float m[3][3]);

#endif // CAMERAMATRIX_H
