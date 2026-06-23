#include "ImageFormats/Raw/cameramatrix.h"

/*
    Model -> XYZ(D65)->camera matrix, stored as nine integers = matrix * 10000, exactly as in
    libraw/dcraw's adobe_coeff table. To add a camera, copy its nine "trans" values from
    libraw's adobe_coeff (or compute from its DNG ColorMatrix2) and add a row.

    NOTE / VERIFY: the seed values below are the published adobe_coeff figures to the best of
    current knowledge. The A9 II shares the A9 sensor, so they use the same matrix. Confirm a
    new model's colour visually (or against libraw) when adding it -- a wrong matrix shows as a
    hue shift, not a crash.
*/

namespace {

struct Entry {
    const char *model;      // matches ImageMetadata::model
    int c[9];               // XYZ(D65)->cam, row-major, * 10000
};

const Entry kTable[] = {
    /* Sony A9 / A9 II (shared sensor). */
    { "Sony ILCE-9",   { 6389, -1703, -378, -4562, 12809, 2027, -928, 2722, 5861 } },
    { "Sony ILCE-9M2", { 6389, -1703, -378, -4562, 12809, 2027, -928, 2722, 5861 } },
};

} // namespace

bool xyzToCamForModel(const QString &model, float m[3][3])
{
    for (const Entry &e : kTable) {
        if (model == QLatin1String(e.model)) {
            for (int i = 0; i < 9; ++i)
                m[i / 3][i % 3] = e.c[i] / 10000.0f;
            return true;
        }
    }
    return false;
}
