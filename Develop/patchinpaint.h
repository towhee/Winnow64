#ifndef PATCHINPAINT_H
#define PATCHINPAINT_H

#include <opencv2/core.hpp>
#include <cstdint>

/*
    CONTENT-AWARE FILL for the Replace tool's Content-Aware mode (FillSpotGeom::Patch):
    a hole is rebuilt from patches of its own surroundings. No model, so it runs on every
    platform and in builds without ONNX Runtime; the pixels it writes are real pixels from
    the image, so they carry the image's grain.

    The method is the multi-scale completion of Wexler, Shechtman and Irani ("Space-Time
    Completion of Video", PAMI 2007) with the nearest-neighbour field found by PatchMatch
    (Barnes, Shechtman, Finkelstein and Goldman, SIGGRAPH 2009):

      1. PYRAMID. Halve the tile until the hole is a few patches across. A hole pixel
         makes its coarse pixel a hole pixel too; a known coarse pixel is the mean of its
         known children.
      2. SEED the coarsest hole with a membrane (harmonic) fill from its boundary.
      3. EM, coarse to fine. For every patch overlapping the hole, PatchMatch finds the
         most similar patch lying wholly outside it (random start or the coarser level's
         match, propagation from neighbours, exponentially shrinking random search). Each
         hole pixel is then re-estimated as the similarity-weighted vote of every matched
         patch covering it. The result, upsampled, seeds the next finer level.
      4. Fine levels where the hole is still wider than kRefineExtent only REFINE (one
         local pass, one vote): structure is decided at the coarse scale, fine levels add
         detail, so a large hole stays tractable.

    Winnow's changes to LightCraft's method (each measured on real photos, 2026-11-21,
    ground-truth holes painted over clean areas of five JPEGs):

      - TEXTURE FEATURES in the match (Newson et al., SIAM J. Imaging Sci. 2014): the local
        mean |gradient| rides along as two extra channels (textureWeight). Colour alone
        matched the membrane-smoothed hole interior to the flattest source nearby -- skin
        beside a patterned fabric, a smooth gap between tree trunks -- and the fill came
        out a smooth band. With them, texture carries through.
      - Pyramid sized by the hole's THICKNESS, not its bbox: a 22 px stroke 300 px long was
        shrunk to a pixel and its fine levels only refined.
      - The last full-resolution iteration COPIES each hole pixel from its best match
        instead of voting: the vote averages ~49 matches and smooths the grain away.

    Deterministic: a fixed-seed hash RNG per row band, so the result is the same whatever
    the thread count. Re-renders do not shimmer.

    Ported from LightCraft (github.com/storytold/lightcraft, crates/inpaint/src/
    patchmatch.rs and membrane.rs; MIT, Copyright (c) 2026 ArtCraft Team and the
    LightCraft contributors -- see THIRD_PARTY_NOTICES.md).
*/
namespace PatchInpaint {

struct Params {
    int patchRadius = 3;        // patches are 2r+1 square
    int emIters = 8;            // EM iterations at the coarsest level
    int minEmIters = 2;         // finer levels halve down to this
    int pmIters = 2;            // PatchMatch passes per EM iteration
    int refineExtent = 96;      // a level whose hole is wider than this only refines
    uint64_t seed = 1;
    /* Winnow additions (see Complete): size the pyramid by the hole's THICKNESS rather
       than its bounding box, and finish with a pixel copy instead of a vote. */
    bool byThickness = true;
    bool finalCopy = true;
    /* Weight of the texture features in the match distance (patchinpaint.cpp, kCh);
       0 = colour only, as LightCraft. */
    float textureWeight = 4.0f;
};

/*
    Fill the hole in place. img: CV_32FC3 (any scale), hole: CV_8UC1 same size, nonzero =
    rebuild. Pixels outside the hole are untouched. Returns false (img unchanged) when
    there is no whole patch outside the hole to copy from, or on a size / type mismatch.
*/
bool Complete(cv::Mat &img, const cv::Mat &hole, const Params &p = Params());

/*
    Membrane fill: the hole becomes the harmonic interpolant of its boundary (the seed of
    Complete's coarsest level, and the caller's fallback when Complete returns false).
    img CV_32FC3, hole CV_8UC1. No known pixel: unchanged.
*/
void Membrane(cv::Mat &img, const cv::Mat &hole);

} // namespace PatchInpaint

#endif // PATCHINPAINT_H
