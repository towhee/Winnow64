// FSFusionReassign.cpp
#include "fsfusionreassign.h"
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

bool ColorMapBuilder::normalizeColorTo16(const cv::Mat& colorImg, cv::Mat& color16)
{
    if (colorImg.type() == CV_16UC3)
    {
        color16 = colorImg;   // shallow ok
        return true;
    }
    if (colorImg.type() == CV_8UC3)
    {
        // exact 8->16 mapping: 0..255 -> 0..65535
        colorImg.convertTo(color16, CV_16UC3, 257.0);
        return true;
    }
    return false;
}

// StreamPMax pipeline
bool ColorMapBuilder::begin(const cv::Size& size, int fixedCapPerPixel, ColorDepth depth)
{
    m_size = size;
    m_cap = std::max(1, fixedCapPerPixel);
    m_pixels = m_size.width * m_size.height;
    m_depth = depth; // Auto or forced

    m_counts.assign(m_pixels, 0);
    m_grays.assign(m_pixels * m_cap, 0);
    m_colors16.assign(m_pixels * m_cap, cv::Vec3w(0,0,0));
    m_overflow.clear();

    return (m_pixels > 0);
}

// StreamPMax pipeline
bool ColorMapBuilder::addSlice(const cv::Mat& grayImg, const cv::Mat& colorImg)
{
    QString srcFun = "FSFusionReassign::ColorMapBuilder::addSlice";

    if (!isValid()) { qWarning() << "WARNING:" << srcFun << "not initialized"; return false; }
    if (grayImg.empty() || colorImg.empty()) return false;
    if (grayImg.size() != m_size || colorImg.size() != m_size) return false;
    if (grayImg.type() != CV_8U) return false;

    // Decide depth once (or respect forced)
    if (m_depth == ColorDepth::Auto)
        m_depth = (colorImg.depth() == CV_16U) ? ColorDepth::U16 : ColorDepth::U8;

    // We ALWAYS normalize to 16-bit for storage
    cv::Mat color16;
    if (!normalizeColorTo16(colorImg, color16))
    {
        qWarning() << "WARNING:" << srcFun << "expected CV_8UC3 or CV_16UC3";
        return false;
    }

    const int W = m_size.width;
    const int H = m_size.height;

    for (int y = 0; y < H; ++y)
    {
        const uint8_t*  gRow  = grayImg.ptr<uint8_t>(y);
        const cv::Vec3w* cRow = color16.ptr<cv::Vec3w>(y);

        int basePix = y * W;

        for (int x = 0; x < W; ++x)
        {
            const int p = basePix + x;
            const uint8_t g = gRow[x];
            const cv::Vec3w c16 = cRow[x];

            auto it = m_overflow.find(p);
            if (it != m_overflow.end())
            {
                auto& v = it->second;
                bool exists = false;
                for (const auto& e : v) { if (e.gray == g) { exists = true; break; } }
                if (!exists && v.size() < 256) v.emplace_back(g, c16);
                continue;
            }

            uint8_t cnt = m_counts[p];
            const int slotBase = p * m_cap;

            bool exists = false;
            for (int k = 0; k < cnt; ++k)
            {
                if (m_grays[slotBase + k] == g) { exists = true; break; }
            }
            if (exists) continue;

            if (cnt < m_cap)
            {
                m_grays[slotBase + cnt] = g;
                m_colors16[slotBase + cnt] = c16;
                m_counts[p] = uint8_t(cnt + 1);
            }
            else
            {
                std::vector<ColorEntry> v;
                v.reserve(16);
                for (int k = 0; k < m_cap; ++k)
                    v.emplace_back(m_grays[slotBase + k], m_colors16[slotBase + k]);

                v.emplace_back(g, c16);
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
                outColors.emplace_back(m_grays[slotBase + k], m_colors16[slotBase + k]);
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
    m_colors16.clear();
    m_overflow.clear();

    // Optional: free capacity immediately
    m_counts.shrink_to_fit();
    m_grays.shrink_to_fit();
    m_colors16.shrink_to_fit();
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

static inline uint8_t u16_to_u8(uint16_t v)
{
    // round to nearest: (v + 128) / 257
    return uint8_t((uint32_t(v) + 128u) / 257u);
}

bool applyColorMap(const cv::Mat &grayMerged,
                   const std::vector<ColorEntry> &colors,
                   const std::vector<uint8_t> &counts,
                   cv::Mat &colorOut,
                   ColorDepth outDepth)
{
    if (grayMerged.empty() || grayMerged.type() != CV_8U) return false;

    const int width  = grayMerged.cols;
    const int height = grayMerged.rows;

    if (int(counts.size()) != width * height) return false;
    if (colors.empty()) return false;

    if (outDepth == ColorDepth::Auto) outDepth = ColorDepth::U8; // be explicit

    if (outDepth == ColorDepth::U16)
        colorOut.create(height, width, CV_16UC3);
    else
        colorOut.create(height, width, CV_8UC3);

    const ColorEntry *colorsPtr = colors.data();
    const uint8_t *countsPtr    = counts.data();

    for (int y = 0; y < height; ++y)
    {
        const uint8_t *grayRow = grayMerged.ptr<uint8_t>(y);

        if (outDepth == ColorDepth::U16)
        {
            cv::Vec3w *outRow = colorOut.ptr<cv::Vec3w>(y);

            for (int x = 0; x < width; ++x)
            {
                int colorCount = *countsPtr++ + 1;
                const ColorEntry *pos = colorsPtr;
                colorsPtr += colorCount;

                uint8_t gray = grayRow[x];
                const ColorEntry *best = pos;
                int bestErr = std::abs(int(pos->gray) - int(gray));
                ++pos;

                while (bestErr > 0 && pos != colorsPtr)
                {
                    int err = std::abs(int(pos->gray) - int(gray));
                    if (err < bestErr) { bestErr = err; best = pos; }
                    ++pos;
                }

                outRow[x] = best->color16;
            }
        }
        else // U8
        {
            cv::Vec3b *outRow = colorOut.ptr<cv::Vec3b>(y);

            for (int x = 0; x < width; ++x)
            {
                int colorCount = *countsPtr++ + 1;
                const ColorEntry *pos = colorsPtr;
                colorsPtr += colorCount;

                uint8_t gray = grayRow[x];
                const ColorEntry *best = pos;
                int bestErr = std::abs(int(pos->gray) - int(gray));
                ++pos;

                while (bestErr > 0 && pos != colorsPtr)
                {
                    int err = std::abs(int(pos->gray) - int(gray));
                    if (err < bestErr) { bestErr = err; best = pos; }
                    ++pos;
                }

                const cv::Vec3w c16 = best->color16;
                outRow[x] = cv::Vec3b(u16_to_u8(c16[0]),
                                      u16_to_u8(c16[1]),
                                      u16_to_u8(c16[2]));
            }
        }
    }

    return true;
}

} // namespace FSFusionReassign
