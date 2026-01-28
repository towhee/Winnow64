#ifndef FSALIGN_H
#define FSALIGN_H

#include <opencv2/core.hpp>

#include <QString>
#include "fsalign_types.h"
#include "fsloader.h"

namespace FSAlign {

struct Options
{
    bool matchContrast     = true;
    bool matchWhiteBalance = true;
    bool fullResolution    = false;   // true = use full res for final ECC
    int  lowRes            = 256;     // initial rough ECC resolution
    int  maxRes            = 2048;    // default high-res ECC limit
};

Result makeIdentity(const cv::Rect &validArea); // Depth Biased Erosion


// Compute LOCAL alignment: srcGray/srcColor → refGray/refColor.
// refGray/srcGray must be 8-bit single channel (CV_8UC1).
// refColor/srcColor can be 8- or 16-bit, 3-channel.
Result computeLocal(const cv::Mat &refGray,
                    const cv::Mat &refColor,
                    const cv::Mat &srcGray,
                    const cv::Mat &srcColor,
                    const cv::Rect &srcValidArea,
                    const Options &opt);

// Accumulate LOCAL result into GLOBAL (stacked) result, in the same way
// Task_Align does when m_stacked_transform is present.
// - prevGlobal: accumulated transform/coeffs up to previous image (k-1)
// - local:      alignment from k → (k-1)
// - srcValid:   valid area of this source image before transform
// Returns a new Result describing GLOBAL transform/coeffs/validArea for k.
Result accumulate(const Result &prevGlobal,
                  const Result &local,
                  const cv::Rect &srcValid);

// Apply affine transform to src → dst using INTER_CUBIC + BORDER_REFLECT.
// transform must be 2x3 CV_32F (like Result::transform).
void applyTransform(const cv::Mat &src,
                    const cv::Mat &transform,
                    cv::Mat &dst,
                    bool inverse = false);

// Apply contrast + white balance to an image in-place (or on a copy)
void applyContrastWhiteBalance(cv::Mat &img,
                               const Result &r);

// Convenience: identity global result with given validArea.
Result makeIdentity(const cv::Rect &validArea);

using ProgressCallback = std::function<void()>;
using StatusCallback   = std::function<void(const QString &message)>;

class Align
{
public:
    bool alignSlice(
        int                         slice,
        FSLoader::Image            &prevImage,
        FSLoader::Image            &currImage,
        Result                     &prevGlobal,
        Result                     &currGlobal,
        cv::Mat                    &alignedGraySlice,
        cv::Mat                    &alignedColorSlice,
        const Options              &opt,
        std::atomic_bool           *abortFlag,
        StatusCallback              status,
        ProgressCallback            progressCallback
        );
};

} // namespace FSAlign

#endif // FSALIGN_H
