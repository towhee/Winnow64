#ifndef HIGHLIGHTRECON_H
#define HIGHLIGHTRECON_H

/*
    Highlight reconstruction on demosaiced CAMERA RGB, before white balance.

    Where only some channels of a pixel clipped, rebuild them from the unclipped channels
    using the chromaticity of reliable unclipped content nearby; fully clipped pixels
    become neutral at the brightest plausible level; partly clipped ones fade into that
    neutral on their way from the first channel clipping to the last.

    WHY THE CARE. The white-balance gains push clipped channels apart (green clips first,
    red and blue keep rising), so whatever colour the rebuilt pixels carry is hidden at
    default tone only by the roll-off to white. Any darkening (Highlights -, Exposure -,
    Whites -) brings it out. Three things leak colour in, and each has a guard:

      o the colour of whatever unclipped pixel happens to be nearest (dark foliage, purple
        and green fringes along branches, pixels demosaiced from clipped neighbours):
        only BRIGHT pixels (SOURCE_LO..HI of the clip) lend colour, not on strong edges,
        not next to clipped pixels; their colour is diffused into the clipped region
        coarse to fine (a pull/push pyramid), and fades to neutral where there is
        little reliable content within a quarter of the long edge;
      o another surface's colour (bright foliage under a pale sky would turn it teal):
        borrowed colour counts less as both the pixel's own unclipped channels and the
        "rim" (the clipped surface's own colour just below its clip) contradict it;
      o a step where a channel clips: up to NEAR_RIM stops past the rim the pixel takes
        the rim's colour, then goes over to the estimate, so a surface runs into
        clipping without a seam.

    Ported from LightCraft (github.com/storytold/lightcraft, crates/raw/src/highlight.rs,
    reconstruct_with; MIT, Copyright (c) 2026 ArtCraft Team and the LightCraft
    contributors -- see THIRD_PARTY_NOTICES.md). Constants are theirs, tuned against
    their issue #523 (a river photo darkened by Highlights -75).
*/
namespace HighlightRecon {

/*
    rgb: interleaved camera RGB, w x h x 3, a channel at or above clip counts as clipped
    (clip in the same units, ~0.99 of the sensor white). wb: the white-balance multipliers
    applied afterwards. white: the white-balanced camera RGB the colour model renders
    neutral ({1,1,1} for a model that maps the white-balanced neutral to neutral; a white
    that cannot be right falls back to that). Returns the number of pixels with at least
    one clipped channel; every other pixel is left exactly as it was.
*/
int Reconstruct(float *rgb, int w, int h, const float wb[3], float clip, const float white[3]);

} // namespace HighlightRecon

#endif // HIGHLIGHTRECON_H
