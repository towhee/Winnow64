#ifndef HOTPIXEL_H
#define HOTPIXEL_H

#include "ImageFormats/Raw/rawimage.h"

/*
    Hot pixels on sensors that don't list their defects.

    A hot pixel is a single photosite that reads far too bright: a leaky or stuck cell,
    most visible at high ISO and in dark areas. Demosaicing turns it into a small
    saturated dot of its color (red, green or blue), and color noise reduction can only
    make that dot fainter, since the photosite is too bright, not just the wrong color.
    Most raws (Sony ARW, Nikon NEF, ...) carry no list of them.

    HotPixel::Suppress finds them in the mosaic, before black subtraction and demosaicing:

    1. NOISE MODEL. On a sparse sample (every 5th row and column) each photosite is
       compared with the median of its same-color neighbours. The spread of that
       difference per brightness gives the noise as sigma^2 = a * level + b (shot noise
       plus read noise), fitted to the image itself.
    2. CANDIDATES. A photosite more than kHot sigma above EVERY same-color neighbour (the
       nearest eight: two photosites away on a Bayer grid, the nearest greens for green).
    3. REAL LIGHT IS KEPT. A star, a spark or a specular point lights more than one
       photosite: its direct neighbours are raised too. A candidate with at least kMinLit
       neighbours more than kSpot sigma above their own surroundings is left alone. Hot
       photosites also come in adjacent pairs, so a single lit neighbour doesn't count.
    4. The rest are replaced by the median of their same-color neighbours.

    If more than maxShare of the photosites would be replaced, the image doesn't fit the
    model (a synthetic pattern, a star field through a pinhole) and nothing is changed.
    Real light kept in step 3 doesn't count: a dense star field still loses its hot pixels.

    Works for any CFA Winnow decodes: the four Bayer phases and X-Trans (6x6). Photosites
    within 4 of the edge are left alone (their neighbourhood is cut).

    Ported from LightCraft (github.com/storytold/lightcraft, crates/raw/src/hotpixel.rs;
    MIT, Copyright (c) 2026 ArtCraft Team and the LightCraft contributors -- see
    THIRD_PARTY_NOTICES.md). Thresholds are theirs, measured on Sony ILCE-7M4 files at
    ISO 3200 and 25600.
*/
namespace HotPixel {

/* More replaced than this share of the photosites and the result is discarded. */
constexpr double kMaxShare = 2e-3;

/* Replace isolated hot photosites in raw.cfa (in place, before SubtractBlack). Returns
   how many were replaced; 0 for an Unknown pattern, an invalid or too-small image, or an
   image that doesn't fit the noise model. */
int Suppress(RawImage &raw, double maxShare = kMaxShare);

} // namespace HotPixel

#endif // HOTPIXEL_H
