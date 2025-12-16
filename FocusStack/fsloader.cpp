#include "FSLoader.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>

namespace FSLoader {

// ------------------------------------------------------------------------------------
// Compute wavelet-friendly expanded size
// Wavelet code requires dimensions divisible by 2^levels.
// This reproduces Task_Wavelet::levels_for_size behavior closely.
// ------------------------------------------------------------------------------------
static bool divisible(int x, int p) { return (x % p) == 0; }

// We choose max levels such that expanding minimally satisfies divisibility.
int levelsForSize(const cv::Size &orig, cv::Size *expanded)
{
    int w = orig.width;
    int h = orig.height;

    // Max wavelet levels Petteri uses is generally 6–8.
    int maxLevels = 8;

    for (int L = maxLevels; L >= 1; --L)
    {
        int div = 1 << L;
        int ew = (w  % div == 0) ? w  : w  + (div - (w  % div));
        int eh = (h  % div == 0) ? h  : h  + (div - (h  % div));

        if (ew != w || eh != h)
        {
            *expanded = cv::Size(ew, eh);
            return L;
        }
    }

    // No padding needed
    *expanded = orig;
    return 0;
}

// ------------------------------------------------------------------------------------
// Pad via BORDER_REFLECT to expanded size and compute validArea
// ------------------------------------------------------------------------------------
cv::Rect padToWaveletSize(const cv::Mat &src,
                          cv::Mat &dstPadded,
                          int &outExpandX,
                          int &outExpandY)
{
    cv::Size expanded;
    int levels = levelsForSize(src.size(), &expanded);

    int origW = src.cols;
    int origH = src.rows;

    int padW = expanded.width  - origW;
    int padH = expanded.height - origH;

    outExpandX = padW;
    outExpandY = padH;

    if (padW == 0 && padH == 0)
    {
        dstPadded = src.clone();
        return cv::Rect(0, 0, origW, origH);
    }

    int left   = padW / 2;
    int right  = padW - left;
    int top    = padH / 2;
    int bottom = padH - top;

    cv::copyMakeBorder(src,
                       dstPadded,
                       top, bottom,
                       left, right,
                       cv::BORDER_REFLECT);

    return cv::Rect(left, top, origW, origH);
}

// ------------------------------------------------------------------------------------
// Load image from disk and convert to FSLoader::Image
// ------------------------------------------------------------------------------------
Image load(const std::string &filename)
{
    Image out;

    // Load any depth, any color.
    cv::Mat raw = cv::imread(filename, cv::IMREAD_ANYCOLOR | cv::IMREAD_ANYDEPTH);
    if (raw.empty())
        throw std::runtime_error("FSLoader: Could not load: " + filename);

    out.origSize = raw.size();
    out.is16bit  = (raw.depth() == CV_16U);

    // Color: keep as-is (8- or 16-bit)
    cv::Mat paddedColor;
    int ex, ey;
    out.validArea = padToWaveletSize(raw, paddedColor, ex, ey);
    out.paddedSize = paddedColor.size();
    out.color = paddedColor;

    // Build grayscale (8-bit) for alignment
    cv::Mat gray16;

    if (raw.channels() == 1)
    {
        if (raw.depth() == CV_8U)
            gray16 = raw;
        else  // 16-bit → scale down
            raw.convertTo(gray16, CV_8U, 1.0 / 256.0);
    }
    else
    {
        // Use luminance conversion
        if (raw.depth() == CV_8U)
            cv::cvtColor(raw, gray16, cv::COLOR_BGR2GRAY);
        else
        {
            cv::Mat tmp8;
            raw.convertTo(tmp8, CV_8U, 1.0 / 256.0);
            cv::cvtColor(tmp8, gray16, cv::COLOR_BGR2GRAY);
        }
    }

    // Now pad grayscale same way
    cv::Mat paddedGray;
    int ex2, ey2;
    out.validArea = padToWaveletSize(gray16, paddedGray, ex2, ey2);

    out.gray = paddedGray;

    return out;
}

// ------------------------------------------------------------------------------------
// Load from pre-existing cv::Mat
// ------------------------------------------------------------------------------------
Image loadFromMat(const cv::Mat &source)
{
    Image out;
    out.origSize = source.size();
    out.is16bit  = (source.depth() == CV_16U);

    cv::Mat paddedColor;
    int ex, ey;
    out.validArea = padToWaveletSize(source, paddedColor, ex, ey);
    out.paddedSize = paddedColor.size();
    out.color = paddedColor;

    // grayscale
    cv::Mat gray16;
    if (source.channels() == 1)
    {
        if (source.depth() == CV_8U)
            gray16 = source;
        else
            source.convertTo(gray16, CV_8U, 1.0 / 256.0);
    }
    else
    {
        if (source.depth() == CV_8U)
            cv::cvtColor(source, gray16, cv::COLOR_BGR2GRAY);
        else {
            cv::Mat tmp8;
            source.convertTo(tmp8, CV_8U, 1.0 / 256.0);
            cv::cvtColor(tmp8, gray16, cv::COLOR_BGR2GRAY);
        }
    }

    cv::Mat paddedGray;
    int ex2, ey2;
    out.validArea = padToWaveletSize(gray16, paddedGray, ex2, ey2);
    out.gray = paddedGray;

    return out;
}

} // namespace FSLoader
