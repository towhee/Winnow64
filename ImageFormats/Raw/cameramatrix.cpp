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
    { "Sony ILCE-9",    { 6389, -1703, -378, -4562, 12265, 2587, -670, 1489, 6550 } },
    { "Sony ILCE-9M2",  { 6389, -1703, -378, -4562, 12265, 2587, -670, 1489, 6550 } },
    { "Sony ILCE-7RM2", { 6629, -1900, -483, -4618, 12349, 2550, -622, 1381, 6514 } },
    { "Sony ILCE-7RM4", { 7662, -2686, -660, -5240, 12965, 2530, -796, 1508, 6167 } },
    /* Canon. */
    { "Canon EOS 7D Mark II", { 7268, -1082, -969, -4186, 11839, 2663, -825, 2029, 5839 } },
    /* Nikon (model string as stored in TIFF tag 272, e.g. "NIKON D810"). */
    { "NIKON D810",  { 9369, -3195, -791, -4488, 12430, 2301, -893, 1796, 6872 } },
    { "NIKON D7200", { 8322, -3112, -1047, -6367, 14342, 2179, -988, 1638, 6394 } },
    { "NIKON D850",  { 10405, -3755, -1270, -5461, 13787, 1793, -1040, 2015, 6785 } },
    { "NIKON D2H",   { 5733, -911, -629, -7967, 15987, 2055, -3050, 4013, 7048 } },
    { "NIKON D100",  { 5902, -933, -782, -8983, 16719, 2354, -1402, 1455, 6464 } },
    { "NIKON D800E", { 7866, -2108, -555, -4869, 12483, 2681, -1176, 2069, 7501 } },
    /* Olympus (model string is "Olympus " + TIFF tag 272, e.g. "Olympus E-M1"). */
    { "Olympus E-M1", { 7687, -1984, -606, -4327, 11928, 2721, -1381, 2339, 6452 } },
    /* Panasonic (model string is "Panasonic " + TIFF tag 272, e.g. "Panasonic DC-GX9"). */
    { "Panasonic DC-GX9", { 7564, -2263, -606, -3148, 11239, 2177, -540, 1435, 4853 } },
    /* Fujifilm (model string is "Fujifilm " + the RAF header camera id, e.g. "Fujifilm X-T50"). */
    { "Fujifilm X-T50", { 11809, -5358, -1141, -4248, 12164, 2343, -514, 1097, 5848 } },
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
