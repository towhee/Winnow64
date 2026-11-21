#ifndef DEMOSAIC_H
#define DEMOSAIC_H

#include <vector>
#include <functional>
#include <QAtomicInt>
#include "ImageFormats/Raw/rawimage.h"

/*
    Reconstructs a full-color image from a single-channel CFA mosaic. The demosaicer
    is deliberately camera-agnostic: it sees only the mosaic and its CFA pattern
    (RawImage), never a vendor or file format. This lets it be unit tested on synthetic
    mosaics and shared by every RawFormat subclass.

    Output is an interleaved float RGB buffer (size width*height*3, range 0..1 relative
    to the white level) which RawColor then converts to a display QImage.
*/
class Demosaic
{
public:
    /*
        Bayer algorithms. A Bayer mosaic smaller than 4x4 always takes Bilinear (PPG / AHD
        read two photosites out). Fuji X-Trans has no H/V structure for PPG or AHD to use:
        Bilinear gives XTransWindow there, and Ppg / Ahd give XTransDirectional.

        Bilinear  3x3 same-color average. Fast and soft; zips color along edges.
        Ppg       Patterned Pixel Grouping (Chuan-kai Lin): green from the flattest of four
                  directions, red/blue by color difference. ~Bilinear's cost, far sharper.
        Ahd       Adaptive Homogeneity-Directed (Hirakawa & Parks, IEEE TIP 2005):
                  horizontal and vertical candidates chosen per pixel by CIELab
                  homogeneity. The quality default.

        PPG and AHD are ported from LightCraft (crates/raw/src/demosaic/{ppg,ahd}.rs, MIT,
        see THIRD_PARTY_NOTICES.md), which implemented them from the published papers;
        XTransDirectional from its xtrans.rs, LightCraft's own design.
        Every algorithm reproduces a constant-color mosaic exactly and keeps each
        photosite's own channel as measured.
    */
    enum Algorithm {
        Bilinear,
        Ppg,
        Ahd
    };

    Demosaic() = default;

    /* Demosaic raw.cfa into interleaved RGB floats. Returns false for unsupported
       patterns (e.g. XTrans), an invalid RawImage, or if abort is signalled mid-run.
       progress (when set) is called periodically as (rowsDone, totalRows) for the
       status bar -- the demosaic is a visible slice of a "Denoise raw" decode. */
    bool Run(const RawImage &raw,
             std::vector<float> &rgb,
             Algorithm algo = Ahd,
             const QAtomicInt *abort = nullptr,
             const std::function<void(int, int)> &progress = {});

    /* Color of photosite (row,col) for a Bayer pattern: 0=R, 1=G, 2=B. */
    static int BayerColorAt(CfaPattern pattern, int row, int col);

private:
    bool PpgBayer(const RawImage &raw, std::vector<float> &rgb,
             const QAtomicInt *abort,
             const std::function<void(int, int)> &progress);

    bool AhdBayer(const RawImage &raw, std::vector<float> &rgb,
             const QAtomicInt *abort,
             const std::function<void(int, int)> &progress);

    bool Bilinear3x3(const RawImage &raw, std::vector<float> &rgb,
                     const QAtomicInt *abort,
                     const std::function<void(int, int)> &progress);

    /* Fuji X-Trans: per-pixel average of same-color photosites in a 5x5 window (the 6x6
       pattern guarantees all three colors in that window), native color kept exact. */
    /* Fuji X-Trans: LightCraft's edge-weighted color-difference interpolation (see the
       definition). The X-Trans quality path. */
    bool XTransDirectional(const RawImage &raw, std::vector<float> &rgb,
                           const QAtomicInt *abort,
                           const std::function<void(int, int)> &progress);

    bool XTransWindow(const RawImage &raw, std::vector<float> &rgb,
                      const QAtomicInt *abort,
                      const std::function<void(int, int)> &progress);

};

#endif // DEMOSAIC_H
