#ifndef SCOPEDATA_H
#define SCOPEDATA_H

#include <QtGlobal>
#include <cstring>

/*
    Shared scope accumulation, filled by one strided sample pass over the currently
    displayed image (MW::updateDevelopScopes) and handed to ScopesView. Both the
    histogram and the vectorscope are built from the SAME pass so a single downsample
    feeds both.

        hist[c][v]  256-bin counts, c = 0:R 1:G 2:B 3:luma
        vec[cr][cb] VN x VN Cb/Cr accumulation for the vectorscope (Cb = x, Cr = y),
                    each axis quantised from the 0..255 chroma value to VN bins.

    Counts are quint32: at the ~180k-pixel sample budget no bin can overflow.
*/
struct ScopeData
{
    static constexpr int VN = 128;          // vectorscope bins per axis

    quint32 hist[4][256];
    quint32 vec[VN][VN];

    void clear() {
        std::memset(hist, 0, sizeof(hist));
        std::memset(vec, 0, sizeof(vec));
    }
};

#endif // SCOPEDATA_H
