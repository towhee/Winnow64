#ifndef FSLOCALALIGN_H
#define FSLOCALALIGN_H

#include <opencv2/core.hpp>
#include <vector>

/*
    LOCAL ALIGNMENT for the focus stacker: what one affine per slice leaves.

    FSAlign fits one ECC affine per slice (slice k to slice k-1, accumulated to slice 0).
    An affine is exact only when every part of the frame moved together. A handheld or
    rail stack also has parallax (near and far parts shift differently as the camera
    moves), lens distortion that changes with focus, and focus breathing that is not
    quite a uniform scale. Those leave local shifts of a pixel or two; fusion then picks
    sharp detail from slices that do not line up, and edges double or halo.

    After the global fit, each slice is compared with the previous one, both through
    their global transforms, tile by tile:

      1. MEASURE. On a grid of about kGrid tiles along the long edge, the previous
         slice's kPatch-pixel patch at the tile centre is found in this slice within
         kRadius pixels (normalised cross-correlation, sub-pixel parabola peak; weak,
         ambiguous, textureless and clipped patches dropped).
      2. CLEAN. A shift more than kOutlier px from the median of its measured neighbours
         is dropped (a moving leaf, a mismatch), as is one with no measured neighbour.
         Unmeasured tiles take the mean of their measured neighbours, spreading outward.
      3. SMOOTH with two 3x3 box passes, so the field bends gently. Fewer than 24
         measured tiles: no step (a field filled from a handful would be a guess).
      4. VERIFY. A step is kept only if a second, independent measurement agrees:
         slice k-2 against slice k directly must match step(k-1) + step(k) to within
         half their size (kAgree). Noise does not add up that way; a real shift does.
         A pass confirms BOTH steps, so slice k-1 waits one slice for its verdict
         (Chain). A step nothing confirms adds nothing.
      5. ACCUMULATE. Slice k's field is the sum of the kept steps up to it, so every
         slice lines up with slice 0 (adjacent slices are compared because their focus
         is similar: a patch sharp in one is only a little softer in the next).
      6. WARP. Output pixel p samples the source at inverse(affine)(p + field(p)): ONE
         cubic resample for both corrections, in row bands so the maps stay small.

    Off by default (Preferences > Focus Stack Utility > Local alignment, FS::Options::
    localAlign). With it off nothing here runs and the stack is exactly the affine-only
    one. Self-contained: removing the option means removing this file pair, the
    `o.localAlign` branches in FS::runDMap / FS::runPMax (their warp dispatch), and the
    preference.

    WHY THE VERIFY STEP. Measured on two real OM-1 stacks (20 and 3 slices, 2026-11-21)
    that full-resolution ECC had already aligned: every step was 0.1-0.5 px of NCC noise
    between differently focused patches (the k-2 -> k measurement disagreed with the
    sum of the steps by as much as the steps), and the plain sum random-walked to
    3.5 px by slice 12 -- worse than no local alignment. With VERIFY every step there
    was rejected and the stack is exactly the affine-only one.

    Ported from LightCraft (github.com/storytold/lightcraft, crates/merge/src/flow.rs
    and refine.rs match_patch; MIT, Copyright (c) 2026 ArtCraft Team and the LightCraft
    contributors -- see THIRD_PARTY_NOTICES.md). Differences: frames are 8-bit gray
    already in the same geometry (no homography inside the matcher), so correlation is
    cv::matchTemplate; a patch or search window with any clipped pixel is skipped rather
    than masked; fields are accumulated along the stack, which is why each step must be
    VERIFIED (LightCraft compares every frame with one reference, so never chains).
*/
namespace FSLocalAlign {

/* Tiles along the long edge. */
constexpr int kGrid = 96;
/* Side of a matched patch (pixels). */
constexpr int kPatch = 48;
/* Largest local shift measured per step (pixels). */
constexpr int kRadius = 5;
/* A tile this far (pixels) from the median of its neighbours is an outlier. */
constexpr float kOutlier = 0.75f;
/* VERIFY: the direct k-2 -> k field must lie within this share of the summed steps... */
constexpr float kAgree = 0.5f;
/* ...whose RMS must be at least this (pixels): below it there is nothing to correct. */
constexpr float kMinSignal = 0.05f;

/*
    A smooth field of local shifts: tile (i, j)'s value sits at pixel centre
    ((i + 0.5) * step, (j + 0.5) * step). frame(p + shift(p)) ~ reference(p).
    dx, dy are gw x gh CV_32F. Empty = no shift anywhere.
*/
struct Flow {
    int step = 0;
    cv::Mat dx, dy;
    int measured = 0;           // tiles measured (before filling)
    bool empty() const { return step <= 0 || dx.empty(); }
    float maxShift() const;     // largest |shift| in the field
    /* Shift at pixel (x, y), pixel centres at +0.5, bilinear between tile centres. */
    cv::Point2f at(float x, float y) const;
};

/*
    The local shifts of `frame` against `reference`: both CV_8UC1, same size and
    geometry (both already through their global transforms). Deterministic, whatever
    the thread count.
*/
Flow Estimate(const cv::Mat &reference, const cv::Mat &frame, int radius = kRadius);

/* a + b tile by tile (same grid); an empty side counts as zero. */
Flow Add(const Flow &a, const Flow &b);

/*
    VERIFY: whether stepPrev (k-2 -> k-1) and step (k-1 -> k) are confirmed by direct
    (k-2 -> k). signal / disagreement (RMS, pixels) are returned for the log.
*/
bool Consistent(const Flow &stepPrev, const Flow &step, const Flow &direct,
                float *signal = nullptr, float *disagreement = nullptr);

/*
    The stack's local alignment, slice by slice. Add() takes each slice's gray through its
    global transform, in order, and returns the slices DECIDED by it with the field each
    is to be warped by -- slice k-1 is decided when slice k arrives (its step may be
    confirmed by either check), slice 0 at once (no field). Finish() decides the last.
    Every slice is returned exactly once, in order.
*/
class Chain
{
public:
    struct Decision { int slice; Flow field; float signal = 0, disagreement = 0; bool kept = false; };
    std::vector<Decision> Add(const cv::Mat &alignedGray);
    std::vector<Decision> Finish();

private:
    int next = 0;                   // index of the slice Add() receives next
    cv::Mat prev1, prev2;           // slices k-1 and k-2, through their global transforms
    Flow stepPrev;                  // k-2 -> k-1
    bool okPrev = false;            // stepPrev confirmed by its own check
    float sigPrev = 0, disPrev = 0;
    Flow cum;                       // kept steps summed up to the last decided slice
    Decision Decide(int slice, const Flow &step, bool kept, float sig, float dis);
};

/*
    src resampled to dst (same size) at inverse(transform)(p + flow(p)), INTER_CUBIC,
    BORDER_REFLECT -- applyTransform's warp with the local field folded in. transform is
    FSAlign's 2x3 CV_32F source -> aligned affine. An empty flow is exactly
    FSAlign::applyTransform.
*/
void Warp(const cv::Mat &src, const cv::Mat &transform, const Flow &flow, cv::Mat &dst);

} // namespace FSLocalAlign

#endif // FSLOCALALIGN_H
