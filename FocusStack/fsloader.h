#ifndef FSLOADER_H
#define FSLOADER_H

#include <opencv2/core.hpp>
#include <string>

namespace FSLoader {

// Image loaded for alignment
struct Image
{
    cv::Mat color;      // padded color (8 or 16 bit)
    cv::Mat gray;       // padded grayscale (8-bit)
    cv::Rect validArea; // original image region inside padded frame
    cv::Size origSize;  // original file size
    cv::Size paddedSize;// after wavelet padding

    bool is16bit = false;
};

// Load one image from file:
//  - preserves bit depth (8 or 16)
//  - converts grayscale to 8-bit
//  - pads via BORDER_REFLECT to wavelet-friendly size
Image load(const std::string &filename);

// Load from an existing Mat (for testing):
Image loadFromMat(const cv::Mat &source);

// The padding function (public so you can unit test)
cv::Rect padToWaveletSize(const cv::Mat &src,
                          cv::Mat &dstPadded,
                          int &outExpandX,
                          int &outExpandY);

// Compute wavelet expansion size (copied from Petteri's Task_Wavelet::levels_for_size)
int levelsForSize(const cv::Size &orig, cv::Size *expanded);

} // namespace FSLoader

#endif // FSLOADER_H
