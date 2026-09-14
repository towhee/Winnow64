#ifndef DCP_H
#define DCP_H

#include <QFile>
#include <QString>
#include <vector>

#include "ImageFormats/Raw/tiffwalk.h"

/*
    DCP (DNG Camera Profile) reader -- slice 1 of Phase 4 (see notes/Documentation.txt,
    "Camera Profiles (DCP) -- Phase 4 Plan").

    A .dcp is a TIFF file built out of DNG tag numbers: the same header as any TIFF but with
    magic 0x4352 instead of 42, one IFD, and no image data. The identical tags can also sit in
    IFD0 of a DNG, which is why the IFD-level entry point is public -- a DNG carries its own
    profile and must not need a second file.

    THIS FILE ONLY READS. It does not interpolate the two calibrations, build a rendering
    matrix, or apply either lookup table; that is slice 2 and slice 3. Everything here is a
    faithful report of what the file contains, including which optional pieces are ABSENT --
    a profile with no ForwardMatrix is legal and common, and the renderer has to derive one
    from the inverse of ColorMatrix instead, so "missing" has to survive parsing as a fact
    rather than as a silently-identity matrix.

    THE TWO MATRIX FAMILIES ARE NOT INTERCHANGEABLE, and swapping them is the classic DCP
    bug: ColorMatrix maps XYZ -> camera and exists to SOLVE for the illuminant from the
    as-shot neutral; ForwardMatrix maps camera -> XYZ(D50) and exists to RENDER. They are
    near-inverses but not inverses, because ForwardMatrix is built so that a white-balanced
    camera neutral lands exactly on D50.

    NOT HANDLED, deliberately: the DNG 1.6 THIRD illuminant (tags 52529 / 52531 / 52532 /
    52537) and its spectral IlluminantData. Its presence is reported (hasThirdIlluminant) so
    the renderer can say so rather than quietly using two thirds of a profile. Also not
    handled: ExtraCameraProfiles (50933), a pointer array to further profile IFDs inside a
    DNG -- parseIfd() is all a caller needs to walk those when it wants them.
*/
namespace Dcp {

/* Row-major 3x3, identity by default -- the same shape and convention as
   CameraColor::xyzToCam so a parsed matrix can be handed straight across. */
struct Matrix3 {
    float m[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
};

/*
    A HueSatMap or a LookTable. Both are the SAME structure -- a 3-D table indexed by hue,
    saturation and value, each entry a (hue shift in DEGREES, saturation SCALE, value SCALE)
    triple -- and differ only in where they run in the pipeline: the HueSatMap is the
    profile's colorimetric correction, the LookTable is the creative grade on top.

    Hue wraps (the first and last hue divisions are adjacent); saturation and value do not.
    valDivs == 1 is the common case and means the table is effectively 2-D.
*/
struct Table3D {
    int hueDivs = 0;
    int satDivs = 0;
    int valDivs = 0;
    int encoding = 0;               // ProfileHueSatMap/LookTableEncoding: 0 = linear, 1 = sRGB
    std::vector<float> v;           // hueDivs * satDivs * valDivs * 3

    bool isEmpty() const { return v.empty(); }
    int  entries() const { return hueDivs * satDivs * valDivs; }
};

/* One illuminant's worth of characterisation. A profile carries two of these (or one). */
struct Calibration {
    int illuminant = 0;             // EXIF LightSource code; 0 = absent / unknown
    bool haveColor = false;
    bool haveForward = false;
    bool haveCalibration = false;
    Matrix3 color;                  // ColorMatrix1/2      XYZ -> camera
    Matrix3 forward;                // ForwardMatrix1/2    camera -> XYZ(D50)
    Matrix3 calibration;            // CameraCalibration1/2  per-INDIVIDUAL-camera, usually identity
    Table3D hueSatMap;              // ProfileHueSatMapData1/2
};

struct Profile {
    bool valid = false;
    QString uniqueCameraModel;      // 50708 -- the model this profile is FOR
    QString name;                   // 50936 -- "Adobe Standard", "Camera Vivid", ...
    QString copyright;              // 50942
    QString calibrationSignature;   // 50932 -- e.g. "com.adobe"
    int embedPolicy = 0;            // 50941: 0 allow copying, 1 embed if used, 2 never embed, 3 no restrictions

    Calibration cal[2];             // [0] = illuminant 1, [1] = illuminant 2
    float analogBalance[3] = {1.0f, 1.0f, 1.0f};    // 50727

    Table3D lookTable;              // 50981 / 50982 / 51108
    std::vector<float> toneCurve;   // 50940 -- x0,y0,x1,y1,... in 0..1, x ascending
    float baselineExposure = 0.0f;          // 50730, EV
    float baselineExposureOffset = 0.0f;    // 51109, EV -- the offset the LOOK assumes
    bool defaultBlackRender = false;        // 51110: false = Auto, true = None
    bool hasThirdIlluminant = false;        // DNG 1.6 tri-illuminant, not read here

    /* True when both calibrations are usable AND they are for different illuminants, i.e.
       when interpolation is meaningful. A single-illuminant profile is legal; the renderer
       just uses cal[0] whatever the temperature. */
    bool dual() const;
};

/*
    The correlated colour temperature an EXIF LightSource code stands for, in kelvin, or 0
    when the code names no definite temperature. These are the CIE / EXIF standard values,
    and they are what the illuminant interpolation is keyed on.
*/
float illuminantKelvin(int lightSourceCode);

/* Read a standalone .dcp. Returns false (and sets err, if given) on anything malformed;
   out.valid mirrors the return. */
bool parseFile(const QString &path, Profile &out, QString *err = nullptr);

/* Read profile tags out of an already-walked IFD -- IFD0 of a DNG, or the single IFD of a
   .dcp. r must be positioned on the same file the IFD came from and must stay open, since
   TiffWalk re-reads out-of-line values on demand. */
bool parseIfd(TiffWalk::Reader &r, const TiffWalk::Ifd &tags, Profile &out,
              QString *err = nullptr);

} // namespace Dcp

#endif // DCP_H
