#ifndef DEMOSAIC_H
#define DEMOSAIC_H

#include <vector>
#include <functional>
#include <QAtomicInt>
#include "ImageFormats/Raw/rawimage.h"

/*
    Reconstructs a full-colour image from a single-channel CFA mosaic. The demosaicer
    is deliberately camera-agnostic: it sees only the mosaic and its CFA pattern
    (RawImage), never a vendor or file format. This lets it be unit tested on synthetic
    mosaics and shared by every RawFormat subclass.

    Output is an interleaved float RGB buffer (size width*height*3, range 0..1 relative
    to the white level) which RawColor then converts to a display QImage.
*/
class Demosaic
{
public:
    enum Algorithm {
        Bilinear        // simple, fast; the only algorithm in the scaffold
        // AHD, DCB, XTrans ... added in later phases
    };

    Demosaic() = default;

    /* Demosaic raw.cfa into interleaved RGB floats. Returns false for unsupported
       patterns (e.g. XTrans), an invalid RawImage, or if abort is signalled mid-run.
       progress (when set) is called periodically as (rowsDone, totalRows) for the
       status bar -- the demosaic is a visible slice of a "Denoise raw" decode. */
    bool Run(const RawImage &raw,
             std::vector<float> &rgb,
             Algorithm algo = Bilinear,
             const QAtomicInt *abort = nullptr,
             const std::function<void(int, int)> &progress = {});

private:
    bool Bilinear3x3(const RawImage &raw, std::vector<float> &rgb,
                     const QAtomicInt *abort,
                     const std::function<void(int, int)> &progress);

    /* Fuji X-Trans: per-pixel average of same-colour photosites in a 5x5 window (the 6x6
       pattern guarantees all three colours in that window), native colour kept exact. */
    bool XTransWindow(const RawImage &raw, std::vector<float> &rgb,
                      const QAtomicInt *abort,
                      const std::function<void(int, int)> &progress);

    /* Colour of photosite (row,col) for a Bayer pattern: 0=R, 1=G, 2=B. */
    static int BayerColorAt(CfaPattern pattern, int row, int col);
};

#endif // DEMOSAIC_H
