#include "FSLoader.h"
#include <QDebug>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>

#include <QString>

/*
FSLoader is responsible for loading images and preparing them for wavelet-
based focus stacking. Its main job is to pad images to wavelet-friendly
dimensions while preserving enough metadata to later crop results back to
the original image area.

The module produces a small Image struct containing:
    •	padded color image (original bit depth preserved)
    •	padded 8-bit grayscale image (for alignment)
    •	original image size
    •	padded image size
    •	validArea rectangle describing the unpadded region
    •	a flag indicating whether the source was 16-bit
*/

namespace FSLoader {

// ------------------------------------------------------------------------------------
// Compute wavelet-friendly expanded size
// Wavelet code requires dimensions divisible by 2^levels.
// This reproduces Task_Wavelet::levels_for_size behavior closely.
/*
Purpose:
Determine how much an image must be expanded so its dimensions are divisible
by a power of two, which wavelet decomposition requires.

Behavior:
    •	Tries wavelet levels from 8 down to 1.
    •	For each level L, checks divisibility by 2^L.
    •	Computes the minimal expansion needed to satisfy divisibility.
    •	Returns the first level that requires padding.
    •	If no padding is needed, returns level 0 and leaves size unchanged.

Key idea:
Choose the smallest possible padding that still satisfies wavelet constraints.
*/
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
/*
Purpose:
Pad an image to the wavelet-compatible size while keeping track of where the
original image lies inside the padded result.

Behavior:
    •	Calls levelsForSize() to compute the expanded size.
    •	Computes symmetric padding on all sides.
    •	Uses BORDER_REFLECT to avoid introducing hard edges.
    •	Returns a cv::Rect (validArea) describing the original image location.
    •	Outputs how many pixels were added in X and Y.

Key idea:
Padding is reversible and visually smooth, and later stages can crop back to
the original content using validArea.
*/
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
/*
Purpose:
Load an image from disk and convert it into a wavelet-ready FSLoader::Image.

Behavior:
    •	Loads image with IMREAD_ANYCOLOR | IMREAD_ANYDEPTH.
    •	Records original size and whether the image is 16-bit.
    •	Pads the original image (color) to wavelet size.
    •	Builds an 8-bit grayscale version for alignment:
    •	8-bit gray: used directly.
    •	16-bit gray: scaled down to 8-bit.
    •	Color: converted to luminance; 16-bit color is scaled first.
    •	Pads the grayscale image in the same way.
    •	Stores padded color, padded gray, validArea, and sizes.

Key idea:
    Alignment always operates on padded 8-bit grayscale, regardless of input
    format.
*/
Image load(const std::string &filename)
{
    QString srcFun = "FSLoader::load";
    Image out;

    // Load any depth, any color.
    qDebug() << srcFun << "1";
    cv::Mat raw = cv::imread(filename, cv::IMREAD_ANYCOLOR | cv::IMREAD_ANYDEPTH);
    if (raw.empty()) {
        qDebug() << srcFun << "2";
        throw std::runtime_error("FSLoader: Could not load: " + filename);
    }

    out.origSize = raw.size();
    out.is16bit  = (raw.depth() == CV_16U);
    qDebug() << srcFun << "3";

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
/*
Purpose:
Same as load(), but operates on an already-loaded cv::Mat.

Behavior:
    •	Mirrors load(filename) logic exactly.
    •	Useful when images are already in memory or generated by earlier stages.

⸻

Overall intent

FSLoader isolates all wavelet-specific preparation logic:
    •	ensures correct image sizes for wavelet decomposition
    •	preserves original bit depth for later fusion
    •	provides a consistent grayscale alignment input
    •	records enough metadata to undo padding cleanly

This keeps wavelet, alignment, and fusion code simpler and more robust.
*/
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
