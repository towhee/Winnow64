#ifndef AREASHRINK_H
#define AREASHRINK_H

#include <algorithm>
#include <opencv2/imgproc.hpp>

/*
    AreaShrink -- cv::resize(INTER_AREA) for a LARGE shrink, in stages.

    OpenCV's area filter is fast for an exact integer factor and for small ones, but its
    general path costs more the larger the factor: shrinking a 50 MP frame (8672 x 5784,
    float RGB) measured 3.7 ms at x8, 26 ms at x9, 52 ms at x14 and 188 ms at x24
    (Apple M-series, 2026-10-27). Develop does exactly the expensive kind on every image --
    the 128 px StatSample (177 ms), Auto tone's 360 px proxy (188 ms) -- so the whole-frame
    passes were most of an Auto tone solve and the first render's statistics.

    So shrink by ~x4 per stage while the remaining factor is over x8, then make the final
    step directly. Typical sensor sizes divide by 4 exactly (8672 x 5784 -> 2168 x 1446),
    which lands the big first stage on OpenCV's integer fast path. Measured on five
    sensor sizes: 128 px 71-212 ms -> 4-11 ms, 360 px 78-222 ms -> 9-21 ms, 1024 px
    unchanged or faster.

    Area averaging of area averages is not bit-identical to one area average (the partial
    edge pixels weigh slightly differently): max per-pixel difference 0.0002-0.006 of full
    scale on a busy synthetic frame, the largest at 128 px. These buffers feed statistics
    and proxies, never an output pixel.

    dst may be a header over caller-owned storage of the right size and type: the final
    resize writes into it in place.
*/
inline void AreaShrink(const cv::Mat &src, cv::Mat &dst, cv::Size size)
{
    constexpr double kStep = 4.0;
    cv::Mat cur = src;                      // a header, no copy
    while (std::max(double(cur.cols) / size.width, double(cur.rows) / size.height) >
           2.0 * kStep) {
        const cv::Size mid(std::max(size.width,  int(cur.cols / kStep + 0.5)),
                           std::max(size.height, int(cur.rows / kStep + 0.5)));
        cv::Mat next;
        cv::resize(cur, next, mid, 0, 0, cv::INTER_AREA);
        cur = next;
    }
    cv::resize(cur, dst, size, 0, 0, cv::INTER_AREA);
}

#endif // AREASHRINK_H
