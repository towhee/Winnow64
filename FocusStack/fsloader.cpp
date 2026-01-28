#include "fsloader.h"
#include "fsphotometric.h"
#include "fsutilities.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>

#include <QDebug>
#include <QString>

#include "Main/global.h"

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

// Build 1xW float row of normalized x and x^2 in [-1..1]
static void buildXRow(int w, cv::Mat& xRow32, cv::Mat& x2Row32)
{
    xRow32.create(1, w, CV_32F);
    x2Row32.create(1, w, CV_32F);

    const float denom = (w > 1) ? float(w - 1) : 1.0f;
    float* x  = xRow32.ptr<float>(0);
    float* x2 = x2Row32.ptr<float>(0);

    for (int ix = 0; ix < w; ++ix)
    {
        float xn = (2.0f * float(ix) / denom) - 1.0f; // [-1..1]
        x[ix]  = xn;
        x2[ix] = xn * xn;
    }
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
    cv::Mat raw = cv::imread(filename, cv::IMREAD_ANYCOLOR | cv::IMREAD_ANYDEPTH);
    if (raw.empty()) {
        qDebug() << srcFun << "2";
        throw std::runtime_error("FSLoader: Could not load: " + filename);
    }

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

    QString msg = "Loaded " + QString::fromStdString(filename);
    if (G::FSLog) G::log(srcFun, msg);

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

// -----------------------------------------------------------------------------
// Apply alignment transform stored in FSAlign::Result
//
// FSAlign::Result::transform is 2x3 CV_32F mapping source -> reference/global.
// We warp the padded gray+color into "align space" using warpAffine.
// -----------------------------------------------------------------------------
static bool warpAlignedToAlignSpace(const FSLoader::Image& in,
                                    const Result& ar,
                                    cv::Mat& grayAlign8,
                                    cv::Mat& colorAlign)
{
    if (in.gray.empty() || in.color.empty())
        return false;

    // Validate transform
    if (ar.transform.empty() ||
        ar.transform.rows != 2 || ar.transform.cols != 3 ||
        ar.transform.type() != CV_32F)
    {
        qWarning().noquote() << "WARNING: warpAlignedToAlignSpace invalid affine:"
                             << "empty=" << ar.transform.empty()
                             << " size=" << ar.transform.cols << "x" << ar.transform.rows
                             << " type=" << ar.transform.type();
        return false;
    }

    // Normalize color to 3 channels (preserve depth)
    cv::Mat colorP = in.color;
    if (colorP.channels() == 1)
        cv::cvtColor(colorP, colorP, cv::COLOR_GRAY2BGR);
    else if (colorP.channels() == 4)
        cv::cvtColor(colorP, colorP, cv::COLOR_BGRA2BGR);

    // Allocate outputs: same size as padded inputs
    grayAlign8.create(in.gray.size(), CV_8U);
    colorAlign.create(colorP.size(), colorP.type());

    // Warp padded -> padded (align space)
    // Note: output size MUST match your align-space expectation;
    // we keep it identical to padded input size (same as your PMax path).
    cv::warpAffine(in.gray, grayAlign8,
                   ar.transform, in.gray.size(),
                   cv::INTER_LINEAR,
                   cv::BORDER_REFLECT);

    cv::warpAffine(colorP, colorAlign,
                   ar.transform, colorP.size(),
                   cv::INTER_LINEAR,
                   cv::BORDER_REFLECT);

    return true;
}

static inline bool rectInside(const cv::Rect& r, const cv::Size& s)
{
    return r.x >= 0 && r.y >= 0 &&
           r.width > 0 && r.height > 0 &&
           r.x + r.width  <= s.width &&
           r.y + r.height <= s.height;
}

bool FSLoader::loadAlignedSliceOrig(int sliceIdx,
                                    const QStringList& inputPaths,
                                    const std::vector<Result>& globals,
                                    cv::Rect roiPadToAlign,
                                    cv::Rect validAreaAlign,
                                    cv::Mat& gray8Orig,
                                    cv::Mat& colorOrig)
{
    QString srcFun = "FSLoader::loadAlignedSliceOrig";
    QString s = QString::number(sliceIdx);
    QString msg = "Slice " + s;
    if (G::FSLog) G::log(srcFun, msg);

    gray8Orig.release();
    colorOrig.release();

    if (sliceIdx < 0 || sliceIdx >= inputPaths.size())
    {
        qWarning().noquote() << "WARNING:" << srcFun << "sliceIdx out of range:" << sliceIdx;
        return false;
    }
    if (sliceIdx < 0 || sliceIdx >= (int)globals.size())
    {
        qWarning().noquote() << "WARNING:" << srcFun << "globals size mismatch for sliceIdx:" << sliceIdx;
        return false;
    }

    // Load padded image (wavelet friendly) + padded gray (8-bit)
    const std::string filename = inputPaths[sliceIdx].toStdString();

    FSLoader::Image im;
    try
    {
        im = FSLoader::load(filename);
    }
    catch (const std::exception& e)
    {
        qWarning().noquote() << "WARNING:" << srcFun << "load failed:" << inputPaths[sliceIdx]
                             << "err:" << e.what();
        return false;
    }

    // Warp padded -> padded (align space)
    cv::Mat grayAlign8, colorAlign;
    if (!warpAlignedToAlignSpace(im, globals[sliceIdx], grayAlign8, colorAlign))
    {
        qWarning().noquote() << "WARNING:" << srcFun << "warpAlignedToAlignSpace failed slice" << sliceIdx;
        return false;
    }

    if (grayAlign8.size() != colorAlign.size())
    {
        qWarning().noquote() << "WARNING:" << srcFun << "grayAlign8.size != colorAlign.size";
        return false;
    }

    // Crop pad -> align
    if (!rectInside(roiPadToAlign, grayAlign8.size()))
    {
        qWarning().noquote() << "WARNING:" << srcFun << "roiPadToAlign out of bounds:"
                             << roiPadToAlign.x << roiPadToAlign.y
                             << roiPadToAlign.width << roiPadToAlign.height
                             << " padSize=" << grayAlign8.cols << "x" << grayAlign8.rows;
        return false;
    }

    cv::Mat grayAlignView  = grayAlign8(roiPadToAlign);
    cv::Mat colorAlignView = colorAlign(roiPadToAlign);

    // Crop align -> orig
    if (!rectInside(validAreaAlign, grayAlignView.size()))
    {
        qWarning().noquote() << "WARNING:" << srcFun << "validAreaAlign out of bounds:"
                             << validAreaAlign.x << validAreaAlign.y
                             << validAreaAlign.width << validAreaAlign.height
                             << " alignSize=" << grayAlignView.cols << "x" << grayAlignView.rows;
        return false;
    }

    gray8Orig = grayAlignView(validAreaAlign).clone();
    colorOrig = colorAlignView(validAreaAlign).clone();

    // Normalize guarantees
    if (gray8Orig.empty() || gray8Orig.type() != CV_8U)
    {
        qWarning().noquote() << "WARNING:" << srcFun << "gray8Orig not CV_8U or empty; type=" << gray8Orig.type();
        return false;
    }

    if (colorOrig.empty())
    {
        qWarning().noquote() << "WARNING:" << srcFun << "colorOrig empty";
        return false;
    }

    if (colorOrig.channels() == 1)
        cv::cvtColor(colorOrig, colorOrig, cv::COLOR_GRAY2BGR);
    else if (colorOrig.channels() == 4)
        cv::cvtColor(colorOrig, colorOrig, cv::COLOR_BGRA2BGR);

    if (gray8Orig.size() != colorOrig.size())
    {
        qWarning().noquote() << "WARNING:" << srcFun << "final size mismatch:"
                             << "gray=" << gray8Orig.cols << "x" << gray8Orig.rows
                             << "color=" << colorOrig.cols << "x" << colorOrig.rows;
        return false;
    }

    // Photometric normalization in orig space (implemented outside FSLoader to avoid coupling)
    {
        const cv::Mat& C  = globals[sliceIdx].contrast;      // 5x1 CV_32F
        const cv::Mat& WB = globals[sliceIdx].whitebalance;  // 6x1 CV_32F

        // Make inputs usable regardless of source format
        if (colorOrig.empty() || gray8Orig.empty())
            return false;

        // Ensure gray is single channel; if not, make a gray
        if (gray8Orig.channels() != 1)
        {
            cv::Mat tmp;
            if (gray8Orig.channels() == 3)
                cv::cvtColor(gray8Orig, tmp, cv::COLOR_BGR2GRAY);
            else if (gray8Orig.channels() == 4)
                cv::cvtColor(gray8Orig, tmp, cv::COLOR_BGRA2GRAY);
            else
                return false;
            gray8Orig = tmp;
        }

        // If gray isn’t 8U/16U, convert to 8U (safe fallback)
        if (!(gray8Orig.depth() == CV_8U || gray8Orig.depth() == CV_16U))
        {
            cv::Mat tmp;
            gray8Orig.convertTo(tmp, CV_8U);
            gray8Orig = tmp;
        }

        // Ensure color is BGR 3ch, keep depth
        if (colorOrig.channels() == 1)
            cv::cvtColor(colorOrig, colorOrig, cv::COLOR_GRAY2BGR);
        else if (colorOrig.channels() == 4)
            cv::cvtColor(colorOrig, colorOrig, cv::COLOR_BGRA2BGR);

        if (colorOrig.channels() != 3 || colorOrig.size() != gray8Orig.size())
            return false;

        // Debug (optional): pre-stats
        if (sliceIdx == 1)
        {
            FSUtilities::dumpMatStats("C", C);
            FSUtilities::dumpMatFirstN("C", C);
            FSUtilities::dumpMatStats("WB", WB);
            FSUtilities::dumpMatFirstN("WB", WB);

            std::vector<cv::Mat> ch;
            cv::split(colorOrig, ch);
            double mn=0,mx=0;
            cv::minMaxLoc(ch[0], &mn, &mx);
            qDebug() << "pre norm B min/max" << mn << mx;
        }

        // Apply photometric; if coefficients are invalid, FSPhotometric will no-op.
        FSPhotometric::applyPhotometricNormalizationOrig(
            gray8Orig,
            colorOrig,
            C,
            WB,
            /*wbOffsetsAre8bitUnits=*/true,
            /*gainLo=*/0.25f,
            /*gainHi=*/4.0f
            );

        // Debug (optional): post-stats
        if (sliceIdx == 1)
        {
            std::vector<cv::Mat> ch;
            cv::split(colorOrig, ch);
            double mn=0,mx=0;
            cv::minMaxLoc(ch[0], &mn, &mx);
            qDebug() << "post norm B min/max" << mn << mx;
        }
    }

    return true;
}

} // namespace FSLoader
