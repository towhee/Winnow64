#include "ImageFormats/Raw/demosaic.h"

bool Demosaic::Run(const RawImage &raw, std::vector<float> &rgb, Algorithm algo,
                   const QAtomicInt *abort)
{
    if (!raw.isValid()) return false;
    if (raw.pattern == CfaPattern::XTrans) return XTransWindow(raw, rgb, abort);
    if (raw.pattern == CfaPattern::Unknown) return false;

    switch (algo) {
    case Bilinear: return Bilinear3x3(raw, rgb, abort);
    }
    return false;
}

bool Demosaic::XTransWindow(const RawImage &raw, std::vector<float> &rgb,
                            const QAtomicInt *abort)
{
/*
    Simple Fuji X-Trans demosaic. The 6x6 colour map (raw.xtrans) is tiled across the sensor.
    For each pixel the measured channel is exact; the two missing channels are the average of
    the same-colour photosites in a 5x5 window -- which, for X-Trans, always contains at least
    one of every colour. Soft but obviously correct (the X-Trans analogue of the bilinear Bayer
    path); a higher-quality algorithm can replace it later.
*/
    const int W = raw.width;
    const int H = raw.height;
    const float scale = raw.white > 0 ? 1.0f / static_cast<float>(raw.white) : 1.0f;
    const int R = 2;                                  // 5x5 window

    rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3, 0.0f);

    for (int y = 0; y < H; ++y) {
        if (abort && abort->loadAcquire()) return false;
        for (int x = 0; x < W; ++x) {
            float sum[3] = {0, 0, 0};
            int   cnt[3] = {0, 0, 0};
            for (int dy = -R; dy <= R; ++dy) {
                const int yy = y + dy;
                if (yy < 0 || yy >= H) continue;
                for (int dx = -R; dx <= R; ++dx) {
                    const int xx = x + dx;
                    if (xx < 0 || xx >= W) continue;
                    const int c = raw.xtrans[yy % 6][xx % 6];
                    sum[c] += static_cast<float>(raw.cfa[static_cast<size_t>(yy) * W + xx]);
                    cnt[c] += 1;
                }
            }
            const size_t o = (static_cast<size_t>(y) * W + x) * 3;
            for (int c = 0; c < 3; ++c)
                rgb[o + c] = cnt[c] ? (sum[c] / cnt[c]) * scale : 0.0f;
            /* Native channel exact (no self-blur). */
            const int nativeC = raw.xtrans[y % 6][x % 6];
            rgb[o + nativeC] =
                static_cast<float>(raw.cfa[static_cast<size_t>(y) * W + x]) * scale;
        }
    }
    return true;
}

int Demosaic::BayerColorAt(CfaPattern pattern, int row, int col)
{
    /* Colour code (0=R 1=G 2=B) for each of the 4 positions in the 2x2 cell,
       indexed by ((row & 1) << 1) | (col & 1). */
    static const int RGGB[4] = {0, 1, 1, 2};
    static const int BGGR[4] = {2, 1, 1, 0};
    static const int GRBG[4] = {1, 0, 2, 1};
    static const int GBRG[4] = {1, 2, 0, 1};

    const int i = ((row & 1) << 1) | (col & 1);
    switch (pattern) {
    case CfaPattern::RGGB: return RGGB[i];
    case CfaPattern::BGGR: return BGGR[i];
    case CfaPattern::GRBG: return GRBG[i];
    case CfaPattern::GBRG: return GBRG[i];
    default:               return RGGB[i];
    }
}

bool Demosaic::Bilinear3x3(const RawImage &raw, std::vector<float> &rgb,
                           const QAtomicInt *abort)
{
/*
    Generic 3x3 bilinear demosaic. For every pixel the native colour is taken from the
    sample itself; the two missing colours are the average of the same-colour samples in
    the surrounding 3x3 neighbourhood (edge-clamped). A Bayer 3x3 window always contains
    at least one sample of each colour, so this handles all positions, including both
    green types, without special cases. Simple and obviously correct — good enough to
    prove the pipeline; AHD/DCB replace it in a later phase.
*/
    const int W = raw.width;
    const int H = raw.height;
    const float scale = raw.white > 0 ? 1.0f / static_cast<float>(raw.white) : 1.0f;

    rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3, 0.0f);

    for (int y = 0; y < H; ++y) {
        /* Poll abort once per row (cheap relative to a full row of pixels) so a long
           sensor decode bails promptly on shutdown / navigation rather than blocking the
           decoder thread -- and with it the BlockingQueuedConnection in ImageCache::stop. */
        if (abort && abort->loadAcquire()) return false;
        for (int x = 0; x < W; ++x) {
            float sum[3] = {0, 0, 0};
            int   cnt[3] = {0, 0, 0};

            for (int dy = -1; dy <= 1; ++dy) {
                const int yy = y + dy;
                if (yy < 0 || yy >= H) continue;
                for (int dx = -1; dx <= 1; ++dx) {
                    const int xx = x + dx;
                    if (xx < 0 || xx >= W) continue;
                    const int c = BayerColorAt(raw.pattern, yy, xx);
                    sum[c] += static_cast<float>(raw.cfa[static_cast<size_t>(yy) * W + xx]);
                    cnt[c] += 1;
                }
            }

            const size_t o = (static_cast<size_t>(y) * W + x) * 3;
            for (int c = 0; c < 3; ++c)
                rgb[o + c] = cnt[c] ? (sum[c] / cnt[c]) * scale : 0.0f;

            /* Keep the measured channel exact (no self-blur). */
            const int nativeC = BayerColorAt(raw.pattern, y, x);
            rgb[o + nativeC] =
                static_cast<float>(raw.cfa[static_cast<size_t>(y) * W + x]) * scale;
        }
    }
    return true;
}
