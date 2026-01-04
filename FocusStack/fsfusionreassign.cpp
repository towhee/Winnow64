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

bool ColorMapBuilder::normalizeColorTo8(const cv::Mat& colorImg, cv::Mat& color8)
{
    if (colorImg.type() == CV_8UC3)
    {
        color8 = colorImg;                 // shallow
        return true;
    }
    if (colorImg.type() == CV_16UC3)
    {
        colorImg.convertTo(color8, CV_8UC3, 255.0 / 65535.0);
        return true;
    }
    return false;
}

// StreamPMax pipeline
bool ColorMapBuilder::begin(const cv::Size& size, int fixedCapPerPixel)
{
    m_size = size;
    m_cap = std::max(1, fixedCapPerPixel);
    m_pixels = m_size.width * m_size.height;

    m_counts.assign(m_pixels, 0);
    m_grays.assign(m_pixels * m_cap, 0);
    m_colors.assign(m_pixels * m_cap, cv::Vec3b(0,0,0));
    m_overflow.clear();
    return (m_pixels > 0);
}

// StreamPMax pipeline
bool ColorMapBuilder::addSlice(const cv::Mat& grayImg, const cv::Mat& colorImg)
{
    QString srcFun = "FSFusionReassign::ColorMapBuilder::addSlice";

    if (!isValid())
    {
        // Allow lazy init if you want:
        // return begin(grayImg.size());
        QString msg = "builder not initialized (call begin() first)";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    if (grayImg.empty() || colorImg.empty())
    {
        QString msg = "grayImg.empty() || colorImg.empty()";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }
    if (grayImg.size() != m_size || colorImg.size() != m_size)
    {
        QString msg = "slice size mismatch";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }
    if (grayImg.type() != CV_8U)
    {
        QString msg = "grayImg.type() != CV_8U";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    cv::Mat color8;
    if (!normalizeColorTo8(colorImg, color8))
    {
        QString msg = "invalid colorImg.type() (expected CV_8UC3 or CV_16UC3)";
        qWarning() << "WARNING:" << srcFun << msg;
        return false;
    }

    const int W = m_size.width;
    const int H = m_size.height;

    for (int y = 0; y < H; ++y)
    {
        const uint8_t* gRow = grayImg.ptr<uint8_t>(y);
        const cv::Vec3b* cRow = color8.ptr<cv::Vec3b>(y);

        int basePix = y * W;

        for (int x = 0; x < W; ++x)
        {
            const int p = basePix + x;
            const uint8_t g = gRow[x];
            const cv::Vec3b c = cRow[x];

            // If pixel already overflowed, update overflow vector.
            auto it = m_overflow.find(p);
            if (it != m_overflow.end())
            {
                auto& v = it->second;
                bool exists = false;
                for (const auto& e : v)
                {
                    if (e.gray == g) { exists = true; break; }
                }
                if (!exists && v.size() < 256)
                    v.emplace_back(g, c);
                continue;
            }

            // Fixed storage path
            uint8_t cnt = m_counts[p];
            const int slotBase = p * m_cap;

            // Check for existing gray (keep FIRST color, matching your original buildColorMap behavior)
            bool exists = false;
            for (int k = 0; k < cnt; ++k)
            {
                if (m_grays[slotBase + k] == g) { exists = true; break; }
            }
            if (exists)
                continue;

            if (cnt < m_cap)
            {
                m_grays[slotBase + cnt] = g;
                m_colors[slotBase + cnt] = c;
                m_counts[p] = uint8_t(cnt + 1);
            }
            else
            {
                // Promote to overflow for this pixel
                std::vector<ColorEntry> v;
                v.reserve(16);
                for (int k = 0; k < m_cap; ++k)
                    v.emplace_back(m_grays[slotBase + k], m_colors[slotBase + k]);

                // Add new one if still unique
                v.emplace_back(g, c);

                m_overflow.emplace(p, std::move(v));
            }
        }
    }

    return true;
}

// StreamPMax pipeline
void ColorMapBuilder::finish(std::vector<ColorEntry>& outColors,
                             std::vector<uint8_t>& outCountsMinus1) const
{
    outColors.clear();
    outCountsMinus1.assign(m_pixels, 0);

    // Rough reserve: typical ~4 entries/pixel (adjust if you like)
    outColors.reserve(size_t(m_pixels) * size_t(std::max(1, m_cap)));

    for (int p = 0; p < m_pixels; ++p)
    {
        auto it = m_overflow.find(p);
        if (it != m_overflow.end())
        {
            const auto& v = it->second;
            const int cnt = int(v.size());
            outCountsMinus1[p] = uint8_t(std::max(1, cnt) - 1);
            outColors.insert(outColors.end(), v.begin(), v.end());
        }
        else
        {
            const int cnt = int(m_counts[p]);
            const int slotBase = p * m_cap;

            // If cnt==0 (shouldn’t happen if you added at least one slice), keep it safe:
            const int safeCnt = std::max(1, cnt);
            outCountsMinus1[p] = uint8_t(safeCnt - 1);

            for (int k = 0; k < safeCnt; ++k)
                outColors.emplace_back(m_grays[slotBase + k], m_colors[slotBase + k]);
        }
    }

    outColors.shrink_to_fit(); // optional
}

void ColorMapBuilder::reset()
{
    m_size = cv::Size(0, 0);
    m_cap = 0;
    m_pixels = 0;

    m_counts.clear();
    m_grays.clear();
    m_colors.clear();
    m_overflow.clear();

    // Optional: free capacity immediately
    m_counts.shrink_to_fit();
    m_grays.shrink_to_fit();
    m_colors.shrink_to_fit();
    // m_overflow doesn't have shrink_to_fit; reassign if you want:
    // std::unordered_map<int, std::vector<ColorEntry>>().swap(m_overflow);
}

// End StreamPMax pipeline section

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
