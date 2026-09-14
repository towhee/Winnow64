#ifndef CAMERAMODEL_H
#define CAMERAMODEL_H

#include <QString>
#include <QStringList>

/*
    The ONE camera-model string Winnow identifies a body by.

    Two independent tables are keyed on it, and both expect the same shape -- a MAKER-
    PREFIXED, space-normalised name:

        ImageFormats/Raw/cameramatrix.cpp   "Olympus E-M1 Mark III", "Fujifilm GFX 100"
        a DCP's UniqueCameraModel           "Olympus E-M1 Mark III", "Fujifilm GFX 100"

    EXIF does not hand that over. Some makers put the maker in tag 272 already ("NIKON
    D850", "Canon EOS R5") and some do not ("E-M1", "X-T2", "DC-GX9"), and some glue words
    together that both tables space out ("OM-1MarkII", "GFX50S II"). A parser that passes
    tag 272 through unchanged therefore misses BOTH lookups for Olympus, Fujifilm and
    Panasonic -- silently, since an unmatched matrix falls back to identity and an
    unmatched profile simply shows an empty dropdown.

    So every parser runs make/model through canonicalCameraModel() before storing it in
    ImageMetadata::model, and that string is what reaches the model column, the matrix
    table and CameraProfileStore alike.
*/
QString canonicalCameraModel(const QString &make, const QString &model);

/*
    Other maker spellings of the SAME canonical model, for a caller matching against data
    Winnow does not own.

    The canonical form follows libraw, because cameramatrix.cpp is generated from it and
    is the render-critical lookup. Adobe disagrees on exactly one maker: the bodies
    libraw files under "Olympus OM-1" ship profiles named "OM Digital Solutions OM-1".
    Rather than pick a winner, CameraProfileStore retries its lookup over these aliases.

    Returns the alternatives only -- never the model itself, empty when there are none.
*/
QStringList cameraModelAliases(const QString &canonicalModel);

#endif // CAMERAMODEL_H
