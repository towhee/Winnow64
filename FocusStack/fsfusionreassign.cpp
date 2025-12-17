// FSFusionReassign.cpp
#include "FSFusionReassign.h"
#include "Main/global.h"

#include <QString>

#include <opencv2/imgproc.hpp>
#include <cassert>
#include <cmath>

namespace
{
constexpr int REASSIGN_MAX_BATCH = 32;
}

namespace FSFusionReassign
{

bool buildColorMap(const std::vector<cv::Mat> &grayImgs,
                   const std::vector<cv::Mat> &colorImgs,
                   std::vector<ColorEntry> &colors,
                   std::vector<uint8_t> &counts)
{
    QString srcFun = "FSFusionReassign::buildColorMap";

    const int N = static_cast<int>(grayImgs.size());
    if (N == 0 || N != static_cast<int>(colorImgs.size()))
        return false;

    const cv::Size size = grayImgs[0].size();
    const int W = size.width;
    const int H = size.height;

    // Validate types
    for (int i = 0; i < N; ++i)
    {
        if (grayImgs[i].empty() || colorImgs[i].empty()) {
            QString msg = "grayImgs[i].empty() || colorImgs[i].empty()";
            qWarning() << "WARNING:" << srcFun << msg;
            return false;
        }

        if (grayImgs[i].size() != size || colorImgs[i].size() != size) {
            QString msg = "grayImgs[i].size() != size || colorImgs[i].size() != size";
            qWarning() << "WARNING:" << srcFun << msg;
            return false;
        }

        if (grayImgs[i].type() != CV_8U) {
            QString msg = "grayImgs[i].type() != CV_8U";
            qWarning() << "WARNING:" << srcFun << msg;
            return false;
        }
    }

    // Normalize color to 8-bit
    std::vector<cv::Mat> color8(N);
    for (int i = 0; i < N; ++i)
    {
        if (colorImgs[i].type() == CV_8UC3)
        {
            color8[i] = colorImgs[i];
        }
        else if (colorImgs[i].type() == CV_16UC3)
        {
            colorImgs[i].convertTo(color8[i], CV_8UC3, 255.0 / 65535.0);
        }
        else
        {
            QString msg = "invalid colorImgs[i].type()";
            qWarning() << "WARNING:" << srcFun << msg;
            return false;
        }
    }

    // Preallocate but not huge (80% smaller than before)
    colors.clear();
    counts.clear();
    colors.reserve(W * H * 4);  // typical case << full bound
    counts.resize(W * H);

    // Duplicate-elimination via stamping
    uint16_t seenStamp[256];
    std::fill(std::begin(seenStamp), std::end(seenStamp), 0);
    uint16_t stampID = 1;

    size_t colorWritePos = 0;

    for (int y = 0; y < H; ++y)
    {
        const uint8_t* grayRowPtrs[REASSIGN_MAX_BATCH];
        const cv::Vec3b* colorRowPtrs[REASSIGN_MAX_BATCH];

        for (int i = 0; i < N; ++i)
        {
            grayRowPtrs[i]  = grayImgs[i].ptr<uint8_t>(y);
            colorRowPtrs[i] = color8[i].ptr<cv::Vec3b>(y);
        }

        for (int x = 0; x < W; ++x)
        {
            stampID++;
            if (stampID == 0) {
                // Overflow: reset table
                std::fill(std::begin(seenStamp), std::end(seenStamp), 0);
                stampID = 1;
            }

            int count = 0;
            for (int i = 0; i < N; ++i)
            {
                uint8_t g = grayRowPtrs[i][x];
                if (seenStamp[g] != stampID)
                {
                    seenStamp[g] = stampID;
                    count++;
                    colors.emplace_back(g, colorRowPtrs[i][x]);
                }
            }

            counts[y * W + x] = uint8_t(count - 1);
        }
    }

    colors.shrink_to_fit();
    return true;
}
bool applyColorMap(const cv::Mat &grayMerged,
                   const std::vector<ColorEntry> &colors,
                   const std::vector<uint8_t> &counts,
                   cv::Mat &colorOut)
{
    if (grayMerged.empty())
        return false;
    if (grayMerged.type() != CV_8U)
        return false;

    int width  = grayMerged.cols;
    int height = grayMerged.rows;

    if (static_cast<int>(counts.size()) != width * height)
        return false;
    if (colors.empty())
        return false;

    colorOut.create(height, width, CV_8UC3);

    const ColorEntry *colorsPtr = colors.data();
    const uint8_t *countsPtr    = counts.data();

    for (int y = 0; y < height; ++y)
    {
        const uint8_t *grayRow = grayMerged.ptr<uint8_t>(y);
        cv::Vec3b *outRow      = colorOut.ptr<cv::Vec3b>(y);

        for (int x = 0; x < width; ++x)
        {
            int colorCount = *countsPtr++ + 1;
            const ColorEntry *pos = colorsPtr;
            colorsPtr += colorCount;

            uint8_t gray = grayRow[x];
            ColorEntry closest = *pos++;
            int error = std::abs(int(closest.gray) - int(gray));

            while (error > 0 && pos != colorsPtr)
            {
                ColorEntry candidate = *pos++;
                int distance = std::abs(int(candidate.gray) - int(gray));
                if (distance < error)
                {
                    error = distance;
                    closest = candidate;
                }
            }

            outRow[x] = closest.color;
        }
    }

    return true;
}

} // namespace FSFusionReassign
