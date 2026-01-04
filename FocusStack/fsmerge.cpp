// FSMerge.cpp
#include "FSMerge.h"
#include "Main/global.h"

#include <opencv2/core.hpp>
#include <cassert>

/*
FSMerge.cpp merges a stack of wavelet transforms into a single “best of stack”
wavelet image, while also producing a per-pixel depth index that records which
input slice “won” at each pixel. It implements a PMax-style selection in
wavelet space, with optional consistency cleanup passes.

KEY IDEA
For each pixel (x,y), each input slice provides a complex wavelet coefficient
(CV_32FC2). The algorithm computes |v|^2 = real^2 + imag^2 for each slice at
that pixel, picks the slice with the maximum value, copies that complex value
into the merged output, and stores the winning slice index into depthIndex16.

FILE STRUCTURE
    •	Anonymous namespace:
    •	levelsForSize(): chooses wavelet “levels” based on image size, ensuring
        dimensions are divisible by 2^levels (used by subband denoise).
    •	getSqAbsval(): computes per-pixel squared magnitude |v|^2 for a complex
        wavelet image (CV_32FC2 -> CV_32F).
    •	denoiseSubbands(): optional “two-out-of-three” voting in subbands to fix
        inconsistent depth choices across wavelet subbands (consistency >= 1).
    •	denoiseNeighbours(): optional neighborhood smoothing; fixes isolated local
        extrema in depthIndex by averaging the 4-neighbors and then re-sampling the
        merged wavelet coefficient from the newly chosen slice (consistency >= 2).

PUBLIC API (namespace FSMerge)
merge(wavelets, consistency, abortFlag, mergedOut, depthIndex16)
    1.	Validation

        •	Ensures wavelets is non-empty.
        •	Ensures each wavelet:
        •	is not empty
        •	has type CV_32FC2
        •	matches the size of wavelets[0]
        •	Aborts early if abortFlag is set.

    2.	PMax merge (core behavior)

        •	Allocates:
            •	maxAbs (CV_32F), initialized to -1
            •	mergedOut (CV_32FC2)
            •	depthIndex16 (CV_16U), initialized to 0
        •	For each slice i:
            •	absval = |wavelets[i]|^2 (CV_32F)
            •	mask = absval > maxAbs
            •	update maxAbs where mask is true
            •	copy wavelets[i] into mergedOut where mask is true
            •	set depthIndex16 to i where mask is true

        This produces:
            •	mergedOut: complex wavelet coefficients from the “sharpest” slice
                per pixel
            •	depthIndex16: index of the slice that contributed that coefficient

    3.	Consistency cleanup (optional)

        •	If consistency >= 1:
        •	denoiseSubbands() applies subband voting on depthIndex to reduce
            cross-subband disagreement, operating level-by-level on wavelet subregions.
        •	If consistency >= 2:
        •	denoiseNeighbours() fixes pixels that are strict local maxima/minima in the
            depth index relative to 4-neighbors by averaging neighbors and then
            re-sampling mergedOut at that pixel from the new winning slice.

OUTPUTS AND THEIR USE
    •	mergedOut (CV_32FC2):
        Intended to be passed into the inverse wavelet transform to reconstruct a
        fused grayscale result.
    •	depthIndex16 (CV_16U):
        A per-pixel “which slice won” map. It can be used for:
        •	debug/visualization of depth ordering
        •	driving simple color picking or later refinement stages

NOTES / BEHAVIORAL DETAILS
    •	The “PMax” selection uses squared magnitude (no sqrt) for speed.
    •	denoiseNeighbours changes both depthIndex16 and mergedOut so they stay
        consistent (if you change the index, the coefficient must match that slice).
    •	denoiseSubbands only adjusts depthIndex16; it does not rewrite mergedOut for
        those subband corrections (it assumes index consistency is the main goal
        there).
    •	The abortFlag is checked during validation and during the per-slice merge
        loop, and also inside denoiseNeighbours via the outer merge caller checks.
*/

// We reuse the levels logic from FSFusionWavelet

namespace FSMerge
{

namespace
{

int levelsForSize(cv::Size size)
{
    const int minLevels = 5;
    const int maxLevels = 10;

    int dimension = std::max(size.width, size.height);

    int levels = minLevels;
    while ((dimension >> levels) > 8 && levels < maxLevels)
    {
        levels++;
    }

    int divider = (1 << levels);
    cv::Size expanded = size;

    if (expanded.width % divider != 0)
    {
        expanded.width += divider - expanded.width % divider;
    }
    if (expanded.height % divider != 0)
    {
        expanded.height += divider - expanded.height % divider;
    }

    if (levels < maxLevels && expanded != size)
    {
        levels = levelsForSize(expanded);
    }

    return levels;
}

inline void getSqAbsval(const cv::Mat &complexMat, cv::Mat &absval)
{
    CV_Assert(complexMat.type() == CV_32FC2);
    absval.create(complexMat.rows, complexMat.cols, CV_32F);

    for (int y = 0; y < complexMat.rows; ++y)
    {
        const cv::Vec2f *row = complexMat.ptr<cv::Vec2f>(y);
        float *outRow = absval.ptr<float>(y);
        for (int x = 0; x < complexMat.cols; ++x)
        {
            const cv::Vec2f &v = row[x];
            outRow[x] = v[0] * v[0] + v[1] * v[1];
        }
    }
}

// Two-out-of-three subband voting (adapted from Task_Merge::denoise_subbands)
void denoiseSubbands(cv::Mat &mergedWavelet, cv::Mat &depthIndex)
{
    CV_Assert(mergedWavelet.type() == CV_32FC2);
    CV_Assert(depthIndex.type() == CV_16U);
    CV_Assert(mergedWavelet.size() == depthIndex.size());

    int levels = levelsForSize(mergedWavelet.size());
    for (int level = 0; level < levels; ++level)
    {
        int w  = mergedWavelet.cols >> level;
        int h  = mergedWavelet.rows >> level;
        int w2 = w / 2;
        int h2 = h / 2;

        cv::Mat sub1 = depthIndex(cv::Rect(w2,     0,   w2, h2));
        cv::Mat sub2 = depthIndex(cv::Rect(w2,    h2,   w2, h2));
        cv::Mat sub3 = depthIndex(cv::Rect(0,     h2,   w2, h2));

        for (int y = 0; y < h2; ++y)
        {
            for (int x = 0; x < w2; ++x)
            {
                uint16_t v1 = sub1.at<uint16_t>(y, x);
                uint16_t v2 = sub2.at<uint16_t>(y, x);
                uint16_t v3 = sub3.at<uint16_t>(y, x);

                if (v1 == v2 && v2 == v3)
                {
                    // already consistent
                    continue;
                }

                if (v2 == v3)
                {
                    // Fix sub1
                    sub1.at<uint16_t>(y, x) = v2;
                }
                else if (v1 == v3)
                {
                    // Fix sub2
                    sub2.at<uint16_t>(y, x) = v1;
                }
                else if (v1 == v2)
                {
                    // Fix sub3
                    sub3.at<uint16_t>(y, x) = v1;
                }
            }
        }
    }
}

// Neighbourhood denoise
void denoiseNeighbours(cv::Mat &mergedWavelet,
                       cv::Mat &depthIndex,
                       const std::vector<cv::Mat> &wavelets)
{
    CV_Assert(mergedWavelet.type() == CV_32FC2);
    CV_Assert(depthIndex.type() == CV_16U);

    for (int y = 1; y < depthIndex.rows - 1; ++y)
    {
        for (int x = 1; x < depthIndex.cols - 1; ++x)
        {
            uint16_t left   = depthIndex.at<uint16_t>(y, x - 1);
            uint16_t right  = depthIndex.at<uint16_t>(y, x + 1);
            uint16_t top    = depthIndex.at<uint16_t>(y - 1, x);
            uint16_t bottom = depthIndex.at<uint16_t>(y + 1, x);
            uint16_t center = depthIndex.at<uint16_t>(y, x);

            bool gtAll = (center > top && center > bottom && center > left && center > right);
            bool ltAll = (center < top && center < bottom && center < left && center < right);

            if (!(gtAll || ltAll))
                continue;

            int avg = (top + bottom + left + right + 2) / 4;
            depthIndex.at<uint16_t>(y, x) = static_cast<uint16_t>(avg);

            // Re-sample wavelet from the winning slice
            CV_Assert(avg >= 0 && avg < static_cast<int>(wavelets.size()));
            const cv::Mat &srcWavelet = wavelets[avg];
            mergedWavelet.at<cv::Vec2f>(y, x) =
                srcWavelet.at<cv::Vec2f>(y, x);
        }
    }
}

// StreamPMax pipeline
static bool ensureInit(StreamState &state,
                       const cv::Size &size,
                       int consistency,
                       cv::Mat &mergedOut)
{
    state.size = size;
    state.sliceIndex = 0;

    const int rows = size.height;
    const int cols = size.width;

    state.maxAbs.create(rows, cols, CV_32F);
    state.maxAbs.setTo(-1.0f);

    state.depthIndex16.create(rows, cols, CV_16U);
    state.depthIndex16.setTo(uint16_t(0));

    mergedOut.create(rows, cols, CV_32FC2);
    mergedOut.setTo(cv::Scalar(0, 0));

    state.wavelets.clear();
    if (consistency >= 2)
        state.wavelets.reserve(64); // optional

    return true;
}

} // anonymous namespace


bool merge(const std::vector<cv::Mat> &wavelets,
           int consistency,
           std::atomic_bool *abortFlag,
           cv::Mat &mergedOut,
           cv::Mat &depthIndex16)
{
    QString srcFun = "FSMerge::merge";

    if (wavelets.empty()) {
        QString msg = "WARNING: FSMerge: wavelets are empty.";
        qWarning() << msg;
        return false;
    }

    if (G::FSLog) G::log(srcFun, "Validating wavelets");

    const int N = static_cast<int>(wavelets.size());
    const cv::Size size = wavelets[0].size();

    // Validate wavelets
    for (int i = 0; i < N; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

        if (G::FSLog) G::log(srcFun, "Slice " + QString::number(i));

        if (wavelets[i].empty() ||
            wavelets[i].type() != CV_32FC2 ||
            wavelets[i].size() != size)
        {
            QString idx = QString::number(i);
            QString msg = "WARNING: FSMerge: wavelets " + idx;
            if (wavelets[i].empty()) msg += " is empty.";
            if (wavelets[i].type() != CV_32FC2) msg += " type != CV_32FC2.";
            if (wavelets[i].size() != size) msg += " != wavelets[0].size.";
            qWarning() << msg;
            return false;
        }
    }

    int rows = size.height;
    int cols = size.width;

    qDebug() << srcFun << "W =" << cols << "H =" << rows;

    cv::Mat maxAbs(rows, cols, CV_32F);
    mergedOut.create(rows, cols, CV_32FC2);
    depthIndex16.create(rows, cols, CV_16U);

    maxAbs = -1.0f;
    depthIndex16 = 0;

    // PMax: for each pixel, choose wavelet with max |v|^2
    if (G::FSLog) G::log(srcFun, "PMax: for each pixel, choose wavelet with max |v|^2");
    for (int i = 0; i < N; ++i)
    {
        if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;
        if (G::FSLog) G::log(srcFun, "Slice " + QString::number(i));

        cv::Mat absval(rows, cols, CV_32F);
        getSqAbsval(wavelets[i], absval);

        cv::Mat mask = (absval > maxAbs);
        absval.copyTo(maxAbs, mask);
        wavelets[i].copyTo(mergedOut, mask);
        depthIndex16.setTo(static_cast<uint16_t>(i), mask);
    }

    // if (*abortFlag) return false;

    if (consistency >= 1)
    {
        denoiseSubbands(mergedOut, depthIndex16);
    }

    if (consistency >= 2)
    {
        denoiseNeighbours(mergedOut, depthIndex16, wavelets);
    }

    if (G::FSLog) G::log(srcFun, "Done.");

    return true;
}

// StreamPMax pipeline
bool mergeSlice(StreamState &state,
                const cv::Mat &wavelet,
                const cv::Size &sizeIn,
                int consistency,
                std::atomic_bool *abortFlag,
                cv::Mat &mergedOut)
{
    QString srcFun = "FSMerge::mergeSlice";

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    if (wavelet.empty())
    {
        qWarning() << "WARNING:" << srcFun << "wavelet is empty.";
        return false;
    }
    if (wavelet.type() != CV_32FC2)
    {
        qWarning() << "WARNING:" << srcFun << "wavelet.type() != CV_32FC2";
        return false;
    }

    cv::Size size = sizeIn;
    if (size.width <= 0 || size.height <= 0)
        size = wavelet.size();

    if (wavelet.size() != size)
    {
        qWarning() << "WARNING:" << srcFun << "wavelet.size() != size";
        return false;
    }

    // Init on first call or if size changed or output is not aligned with state
    const bool needInit =
        !state.initialized() ||
        state.size != size ||
        mergedOut.empty() ||
        mergedOut.type() != CV_32FC2 ||
        mergedOut.size() != size;

    if (needInit)
    {
        if (G::FSLog) G::log(srcFun, "Initializing streaming merge state");
        ensureInit(state, size, consistency, mergedOut);
    }

    // Cache wavelet for neighbour denoise (consistency==2)
    // Must clone because caller reuses wavelet buffer each slice.
    if (consistency >= 2)
        state.wavelets.emplace_back(wavelet.clone());

    // Compute |v|^2
    cv::Mat absval(size.height, size.width, CV_32F);
    getSqAbsval(wavelet, absval);

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    // PMax: update where this slice is stronger than current max
    cv::Mat mask = (absval > state.maxAbs);

    absval.copyTo(state.maxAbs, mask);
    wavelet.copyTo(mergedOut, mask);
    state.depthIndex16.setTo(state.sliceIndex, mask);

    ++state.sliceIndex;
    return true;
}

// StreamPMax pipeline
bool mergeSliceFinish(StreamState &state,
                      int consistency,
                      std::atomic_bool *abortFlag,
                      cv::Mat &mergedOut,
                      cv::Mat *depthIndex16Out)
{
    QString srcFun = "FSMerge::mergeSliceFinish";

    if (abortFlag && abortFlag->load(std::memory_order_relaxed)) return false;

    if (!state.initialized() || mergedOut.empty() || mergedOut.type() != CV_32FC2)
    {
        qWarning() << "WARNING:" << srcFun << "called with empty merge state.";
        return false;
    }

    if (consistency >= 1)
    {
        if (G::FSLog) G::log(srcFun, "denoiseSubbands");
        denoiseSubbands(mergedOut, state.depthIndex16);
    }

    if (consistency >= 2)
    {
        if (G::FSLog) G::log(srcFun, "denoiseNeighbours");
        denoiseNeighbours(mergedOut, state.depthIndex16, state.wavelets);
    }

    if (depthIndex16Out)
        *depthIndex16Out = state.depthIndex16; // shallow copy OK

    if (G::FSLog) G::log(srcFun, "Done.");
    return true;
}

} // namespace FSMerge
