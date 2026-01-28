#ifndef FSLOADER_H
#define FSLOADER_H

#include "fsalign_types.h"

#include <opencv2/core.hpp>
#include <string>

#include <QStringList>

namespace FSLoader {

// Image loaded for alignment and wavelet stages
struct Image
{
    cv::Mat color;                  // padded color (8 or 16 bit)
    cv::Mat gray;                   // padded grayscale (8-bit)
    cv::Rect validArea;             // original image region inside padded frame
    cv::Size origSize;              // original image dim
    cv::Size paddedSize;            // after wavelet padding
    bool is16bit = false;           // original color depth
};

/*
    Load one image from file:
     - preserves bit depth (8 or 16)
     - converts grayscale to 8-bit
     - pads via BORDER_REFLECT to wavelet-friendly size
*/
Image load(const std::string &filename);

// Load from an existing Mat (for testing):
Image loadFromMat(const cv::Mat &source);

bool loadAlignedSliceOrig(int sliceIdx,
                          const QStringList& inputPaths,
                          const std::vector<Result>& globals,
                          cv::Rect roiPadToAlign,
                          cv::Rect validAreaAlign,
                          cv::Mat& gray8Orig,
                          cv::Mat& colorOrig);

// Pad to wavelet-friendly dimensions via BORDER_REFLECT
cv::Rect padToWaveletSize(const cv::Mat &src,
                          cv::Mat &dstPadded,
                          int &outExpandX,
                          int &outExpandY);

// Compute minimal padding to satisfy Petteri’s 2^L divisibility)
int levelsForSize(const cv::Size &orig, cv::Size *expanded);

} // namespace FSLoader

#endif // FSLOADER_H
