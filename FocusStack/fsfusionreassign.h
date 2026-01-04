// FSFusionReassign.h
#ifndef FSFUSIONREASSIGN_H
#define FSFUSIONREASSIGN_H

#include <opencv2/core.hpp>
#include <vector>
#include <cstdint>

namespace FSFusionReassign
{
struct ColorEntry
{
    uint8_t gray;
    cv::Vec3b color;

    ColorEntry() = default;
    ColorEntry(uint8_t g, const cv::Vec3b &c)
        : gray(g), color(c[0], c[1], c[2]) {}
};

// StreamPMax pipeline uses ColorMapBuilder instead of buildColorMap
class ColorMapBuilder
{
public:
    // Must call once per stack before addSlice
    bool begin(const cv::Size& size, int fixedCapPerPixel = 4);

    // Call once per slice (streaming)
    bool addSlice(const cv::Mat& grayImg, const cv::Mat& colorImg);

    // Call once after all slices are added
    void finish(std::vector<ColorEntry>& outColors,
                std::vector<uint8_t>& outCountsMinus1) const;

    void reset();

    cv::Size size() const { return m_size; }
    bool isValid() const { return m_size.width > 0 && m_size.height > 0; }

private:
    cv::Size m_size {0,0};
    int m_cap = 0;                 // fixed capacity per pixel
    int m_pixels = 0;

    // For most pixels we store up to m_cap unique gray entries in fixed arrays.
    std::vector<uint8_t>  m_counts;      // number of unique entries stored in fixed arrays (0..m_cap)
    std::vector<uint8_t>  m_grays;       // size = pixels * m_cap
    std::vector<cv::Vec3b> m_colors;     // size = pixels * m_cap

    // Rare overflow: pixelIndex -> vector of entries (includes the fixed ones once promoted)
    std::unordered_map<int, std::vector<ColorEntry>> m_overflow;

private:
    static bool normalizeColorTo8(const cv::Mat& colorImg, cv::Mat& color8);
};



/*
     * Build color reassignment map:
     *  - grayImgs  : N grayscale 8U
     *  - colorImgs : N color 8UC3 or 16UC3
     * Produces per-pixel list of (gray, color) pairs:
     *  - colors : concatenated entries for all pixels
     *  - counts : number of entries per pixel minus one; size = width*height
     */
bool buildColorMap(const std::vector<cv::Mat> &grayImgs,
                   const std::vector<cv::Mat> &colorImgs,
                   std::vector<ColorEntry> &colors,
                   std::vector<uint8_t> &counts);

/*
     * Apply color reassignment given:
     *  - grayMerged  : fused grayscale 8U
     *  - colors      : color entries from buildColorMap
     *  - counts      : pixel entry counts
     *  - colorOut    : fused 8UC3
     */
bool applyColorMap(const cv::Mat &grayMerged,
                   const std::vector<ColorEntry> &colors,
                   const std::vector<uint8_t> &counts,
                   cv::Mat &colorOut);

} // end namespace FSFusionReassign

#endif // FSFUSIONREASSIGN_H
