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

/*
    Per-model black pedestal and sensor saturation from the same libraw adobe_coeff table
    (t_black / t_maximum). Returns false for an unknown model. A returned value of 0 means
    "unset" (libraw has no override); a negative black means "use only when the file carries no
    black". Decoders use this as a fallback where they would otherwise guess -- e.g. Canon, whose
    saturation (maximum) is below the bit-depth maximum. White balance and CFA stay per-file.
*/
bool cameraLevelsForModel(const QString &model, int &black, int &maximum);

#endif // CAMERAMATRIX_H
