#ifndef THUMBSIZESTEPS_H
#define THUMBSIZESTEPS_H

/*
    THE DISCRETE THUMBNAIL SIZES that the [ and ] keys and the status bar's thumbnail
    size slider step through. One step of the slider is one key press, so both have to
    come from here. See "Thumbnail Size Steps" in notes/Documentation.md.

    A WRAPPING view (the grid, or the thumbnails docked left or right) fills each row
    exactly: the cell width is the row width divided by thumbs per row (tpr). Only those
    widths exist, so a step is one tpr. The range depends on the row width:
        tprMin   fewest per row (largest cells) where the cell is <= maxIcon
        tprMax   most per row (smallest cells) where the thumb is still >= minIcon
    Step 0 is the smallest thumbs, the last step the largest. Only a tpr whose cell
    fits exactly tpr times is a step (see wrapTprs), so every step changes the size.

    A NON-WRAPPING filmstrip (thumbnails docked top or bottom) has no row to fill, so its
    steps form a geometric ladder on the thumb's long side: size(k) = minIcon * 1.1^k,
    and the last step is exactly maxIcon. It replaces the old "x 1.1 / x 0.9" pair,
    which did not round-trip (1.1 * 0.9 = 0.99, and ints truncate), so ] then [ drifted.

    Pure integer math with no Qt, so tests/unit/tst_thumbsizesteps.cpp can pin it.
*/

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace ThumbSizeSteps {

// ---- Wrapping views: steps are thumbs per row --------------------------------------

/* Fewest thumbs per row (largest cells): the smallest tpr whose cell is <= maxCell. The
   same ceiling rejustify() has always applied, so a resize and [ ] agree on the top. */
inline int tprMin(int wRow, int maxCell)
{
    if (wRow <= 0) return 1;
    int tpr = 1;
    while (wRow / tpr > maxCell && tpr < wRow) ++tpr;
    return tpr;
}

/* Most thumbs per row (smallest cells): the largest tpr whose thumb (cell - pad2) is
   still >= minThumb. Never below tprMin, so a view too narrow for even one minimum thumb
   still has exactly one step. */
inline int tprMax(int wRow, int pad2, int minThumb, int maxCell)
{
    const int lo = tprMin(wRow, maxCell);
    int tpr = lo;
    while (wRow / (tpr + 1) - pad2 >= minThumb) ++tpr;
    return std::max(lo, tpr);
}

// The tpr a justified row has now: how many of its cells fit in the row.
inline int currentTpr(int wRow, int wCell)
{
    if (wCell <= 0) return 1;
    return std::max(1, wRow / wCell);
}

/* THE WRAPPING STEPS, as tprs in DESCENDING order (step 0 = most per row = smallest).

   Not every tpr in [tprMin, tprMax] is a step. The cell is wRow / tpr (integer), and
   when cells are small that cell can fit MORE than tpr times: wRow 4289, tpr 66 gives
   cell 64, and 67 cells of 64 fit. So 66 and 67 are the same size, and a ] from 67 to
   66 changed nothing -- the old justify() got stuck there. Only a tpr whose cell fits
   exactly tpr times is a step; each is a distinct cell width. */
inline std::vector<int> wrapTprs(int wRow, int pad2, int minThumb, int maxCell)
{
    const int lo = tprMin(wRow, maxCell);
    const int hi = tprMax(wRow, pad2, minThumb, maxCell);
    std::vector<int> tprs;
    for (int tpr = hi; tpr >= lo; --tpr)
        if (currentTpr(wRow, wRow / tpr) == tpr) tprs.push_back(tpr);
    if (tprs.empty()) tprs.push_back(lo);
    return tprs;
}

inline int wrapStepCount(int wRow, int pad2, int minThumb, int maxCell)
{
    return int(wrapTprs(wRow, pad2, minThumb, maxCell).size());
}

/* Step <-> tpr. Step 0 is the smallest thumbs (the most per row). A tpr that is not a
   step reads as the step showing the same number per row, else the nearest. */
inline int wrapStepFromTpr(int tpr, int wRow, int pad2, int minThumb, int maxCell)
{
    const std::vector<int> tprs = wrapTprs(wRow, pad2, minThumb, maxCell);
    int best = 0;
    for (int i = 0; i < int(tprs.size()); ++i)
        if (std::abs(tprs[i] - tpr) < std::abs(tprs[best] - tpr)) best = i;
    return best;
}

inline int tprFromWrapStep(int step, int wRow, int pad2, int minThumb, int maxCell)
{
    const std::vector<int> tprs = wrapTprs(wRow, pad2, minThumb, maxCell);
    return tprs[std::clamp(step, 0, int(tprs.size()) - 1)];
}

// ---- Non-wrapping filmstrip: a geometric ladder on the long side --------------------

constexpr double ladderRatio = 1.1;

/* The last step is maxIcon. When the geometric step below it is within 5% of maxIcon
   (60 -> 256 makes 251), it is dropped, so the top step is not a sliver. */
inline int ladderCount(int minIcon, int maxIcon)
{
    if (maxIcon <= minIcon) return 1;
    const double k = std::log(double(maxIcon) / minIcon) / std::log(ladderRatio);
    int n = int(std::ceil(k - 1e-9)) + 1;
    if (n > 2 && minIcon * std::pow(ladderRatio, n - 2) > maxIcon / 1.05) --n;
    return n;
}

inline int ladderSize(int k, int minIcon, int maxIcon)
{
    const int n = ladderCount(minIcon, maxIcon);
    k = std::clamp(k, 0, n - 1);
    if (k == n - 1) return maxIcon;
    const int s = int(std::lround(minIcon * std::pow(ladderRatio, k)));
    return std::clamp(s, minIcon, maxIcon);
}

// The step whose size is nearest `size` (used to show a dock-dragged size on the slider).
inline int ladderNearest(int size, int minIcon, int maxIcon)
{
    const int n = ladderCount(minIcon, maxIcon);
    int best = 0;
    int bestDiff = std::abs(ladderSize(0, minIcon, maxIcon) - size);
    for (int k = 1; k < n; ++k) {
        const int d = std::abs(ladderSize(k, minIcon, maxIcon) - size);
        if (d < bestDiff) { best = k; bestDiff = d; }
    }
    return best;
}

/* The next step larger / smaller than `size`, which may sit between steps (after a dock
   drag). Returns -1 when there is none. */
inline int ladderUp(int size, int minIcon, int maxIcon)
{
    const int n = ladderCount(minIcon, maxIcon);
    for (int k = 0; k < n; ++k)
        if (ladderSize(k, minIcon, maxIcon) > size) return k;
    return -1;
}

inline int ladderDown(int size, int minIcon, int maxIcon)
{
    const int n = ladderCount(minIcon, maxIcon);
    for (int k = n - 1; k >= 0; --k)
        if (ladderSize(k, minIcon, maxIcon) < size) return k;
    return -1;
}

} // namespace ThumbSizeSteps

#endif // THUMBSIZESTEPS_H
