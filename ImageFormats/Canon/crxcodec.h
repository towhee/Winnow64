#ifndef CRXCODEC_H
#define CRXCODEC_H

#include <cstdint>
#include <vector>

/*
    Canon CRX bitstream decoder -- the WAVELET (lossy "C-RAW") path, ported from LibRaw.
    See crxcodec.cpp for provenance, the list of changes from upstream, and validation.

    Decode() takes the CR3 already resident in memory plus the three things the container
    walk in crx.cpp has already found -- the CMP1 payload, and the CRX bitstream's offset
    and length -- and returns the FULL sensor mosaic (no crop, no black subtraction): one
    uint16 per photosite, `outW` x `outH`, CFA phase per CMP1's cfaLayout. The caller owns
    everything after that: the IAD1 active-area crop, black calibration, white level, white
    balance and the colour matrix.

    Handles level-0 (lossless RAW) as well as levels 1-3 (C-RAW), so it is a superset of
    crx.cpp's clean-room decoder; crx.cpp nonetheless keeps that decoder for level 0, which
    it already renders byte-exactly, and routes only levels > 0 here.

    Returns false on a bitstream this port does not handle or cannot parse, leaving `out`
    unspecified and pointing *errMsg at a static reason string; the caller then falls back
    to the embedded preview.
*/
namespace CrxCodec {

bool Decode(const uint8_t *file, int64_t fileSize,
            const uint8_t *cmp1, int64_t cmp1Size,
            int64_t mdatOffset, int64_t mdatSize,
            std::vector<uint16_t> &out, int &outW, int &outH,
            int &outBits, const char **errMsg);

} // namespace CrxCodec

#endif // CRXCODEC_H
