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
}

#endif // FSFUSIONREASSIGN_H
