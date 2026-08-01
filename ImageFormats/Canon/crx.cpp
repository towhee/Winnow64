/*
    crx.cpp  --  Canon CR3 (CRX) full-sensor decode: the RawFormat override for .cr3.

    Clean-room reimplementation of Canon's CRX lossless codec (level 0), re-derived from the
    documented format (T.87 JPEG-LS run/MED core + the canon_cr3 reverse-engineering) and
    validated byte-for-byte against the libraw/rawpy oracle on EOS R5 and R6 II samples
    (all four planes, zero-diff). See notes/Documentation.txt "RAW DECODING / CR3".

    Pipeline for CR3:
      1. Walk the ISO-BMFF box tree, pick the full-resolution CRAW track, read its CMP1
         geometry, IAD1 active-area crop, and the CRX bitstream location (stbl co64 + stsz).
      2. Decode the CRX entropy stream: the tile's 4 Bayer planes share ONE continuous
         bitstream (do NOT re-seek per plane) -- adaptive Golomb-Rice + MED prediction + T.87
         run mode, with the non-top-line K adapted from a look-ahead-adjusted code.
      3. Interleave the 4 planes row-major into the RGGB mosaic, crop to the active area.
      4. Self-calibrate per-channel black from the masked border; white/matrix/WB from the
         bit depth, the per-model table, and the CMT3 makernote ColorData.

    Only level-0 (no wavelet) single-tile CRAW is handled -- true for current full-frame
    bodies. Anything else returns false and the caller falls back to the embedded preview.
*/

#include "ImageFormats/Canon/canon.h"
#include "ImageFormats/Raw/cameramatrix.h"
#include "ImageFormats/Raw/tiffwalk.h"

#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>
#include <algorithm>

namespace {

/* ---- little BMFF helpers (big-endian) ------------------------------------------------- */
using Bytes = std::vector<uint8_t>;
inline uint16_t u16(const uint8_t *b) { return (uint16_t(b[0]) << 8) | b[1]; }
inline uint32_t u32(const uint8_t *b) {
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
}
inline uint64_t u64(const uint8_t *b) { return (uint64_t(u32(b)) << 32) | u32(b + 4); }
inline bool tagEq(const uint8_t *b, const char *t) { return std::memcmp(b, t, 4) == 0; }

struct Box { const char *type; size_t body, end; };

/* Iterate direct child boxes in [begin,end). */
void forEachBox(const Bytes &d, size_t begin, size_t end, const std::function<void(size_t, const char *, size_t, size_t)> &fn) {
    size_t o = begin;
    while (o + 8 <= end) {
        uint64_t size = u32(&d[o]);
        size_t hdr = 8;
        if (size == 1) { if (o + 16 > end) break; size = u64(&d[o + 8]); hdr = 16; }
        else if (size == 0) size = end - o;
        if (size < hdr || o + size > end) break;
        fn(o, reinterpret_cast<const char *>(&d[o + 4]), o + hdr, o + size);
        o += size;
    }
}

/* ---- CRX entropy decoder (level 0) ---------------------------------------------------- */
constexpr uint32_t CRX_ESC = 41, CRX_ESCBITS = 21, CRX_KMAX = 15;

struct BitReader {
    const uint8_t *p; size_t len, pos = 0;
    uint64_t cache = 0; int nbits = 0; bool eof = false;
    BitReader(const uint8_t *d, size_t n) : p(d), len(n) {}
    void fill() { while (nbits <= 56 && pos < len) { cache = (cache << 8) | p[pos++]; nbits += 8; } }
    uint32_t getBit() { if (nbits < 1) fill(); if (nbits < 1) { eof = true; return 0; } nbits--; return (cache >> nbits) & 1u; }
    uint32_t getBits(int n) {
        if (n == 0) return 0;
        if (nbits < n) fill();
        if (nbits < n) { uint32_t v = (uint32_t)(cache & ((1ull << nbits) - 1)); nbits = 0; return v; }
        nbits -= n; return (uint32_t)((cache >> nbits) & ((1ull << n) - 1));
    }
    uint32_t zeros() { uint32_t z = 0; while (getBit() == 0) { if (eof) return CRX_ESC; z++; } return z; }
};

/* Adaptive Golomb-Rice K predictor (identical for top-line adapt and the non-top update). */
inline uint32_t predictK(uint32_t prevK, uint32_t value, uint32_t kMax) {
    uint32_t nk = prevK;
    if ((value >> prevK) > 2) nk++;
    if ((value >> prevK) > 5) nk++;
    if (value < ((1u << prevK) >> 1)) nk--;   // unsigned-safe: threshold is 0 at prevK==0
    return kMax > 0 ? std::min(nk, kMax) : nk;
}

const uint32_t J[32]      = {0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,5,5,6,6,7,7,8,9,10,11,12,13,14,15};
const uint32_t JSHIFT[32] = {1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,8,16,16,32,32,64,64,128,128,256,512,1024,2048,4096,8192,16384,32768};

inline int32_t errCodeSigned(uint32_t c) { return -((int32_t)(c & 1)) ^ (int32_t)(c >> 1); }
inline int32_t med(int32_t a, int32_t b, int32_t c) {
    if (c >= std::max(a, b)) return std::min(a, b);
    if (c <= std::min(a, b)) return std::max(a, b);
    return a + b - c;
}

/*
    One CRX band decoder. line_buf[0]=previous line, line_buf[1]=current, with a guard
    sample on each end (index 0 and w+1). The bit reader is shared across all planes of the
    tile (continuous stream); only k and s_param reset at each plane's first line.
*/
struct Band {
    BitReader br;
    int w; uint32_t k = 0, s_param = 0;
    std::vector<int32_t> buf0, buf1; int pos = 1;
    Band(const uint8_t *d, size_t n, int width) : br(d, n), w(width),
        buf0(width + 2, 0), buf1(width + 2, 0) {}

    int32_t a() const { return buf1[pos - 1]; }
    int32_t b() const { return buf0[pos]; }
    int32_t c() const { return buf0[pos - 1]; }
    int32_t d() const { return buf0[pos + 1]; }
    uint32_t rice(bool adapt) {
        uint32_t prefix = br.zeros(), v;
        if (prefix >= CRX_ESC) v = br.getBits(CRX_ESCBITS);
        else if (k > 0)        v = (prefix << k) | br.getBits((int)k);
        else                   v = prefix;
        if (adapt) k = predictK(k, v, CRX_KMAX);
        return v;
    }
    uint32_t runCount(uint32_t remaining) {
        uint32_t n = 1;
        while (n != remaining && br.getBit() == 1) {
            n += JSHIFT[s_param];
            if (n > remaining) { n = remaining; break; }
            s_param = std::min<uint32_t>(s_param + 1, 31);
        }
        if (n < remaining) {
            if (J[s_param] > 0) n += br.getBits((int)J[s_param]);
            if (s_param > 0) s_param--;
        }
        return n;
    }
    void topLine() {
        pos = 1; int64_t rem = w; buf1[0] = 0;
        while (rem > 1) {
            if (a() != 0) buf1[pos] = a();
            else {
                if (br.getBit() == 1) {
                    uint32_t n = runCount((uint32_t)rem); rem -= n;
                    for (uint32_t i = 0; i < n; ++i) { buf1[pos] = a(); pos++; }
                    if (rem == 0) break;
                }
                buf1[pos] = 0;
            }
            buf1[pos] += errCodeSigned(rice(true));
            pos++; rem--;
        }
        if (rem == 1) { int32_t x = a(); buf1[pos] = x + errCodeSigned(rice(true)); pos++; }
        buf1[pos] = a() + 1;
    }
    void nonTopLine() {
        pos = 1; int64_t rem = w; buf1[pos - 1] = b();          // init coeff a = b
        while (rem > 1) {
            int32_t x = 0;
            if (a() == b() && a() == d()) {
                if (br.getBit() == 1) {
                    uint32_t n = runCount((uint32_t)rem); rem -= n;
                    for (uint32_t i = 0; i < n; ++i) { buf1[pos] = a(); pos++; }
                }
                if (rem > 0) x = b();
            } else {
                x = med(a(), b(), c());
            }
            if (rem > 0) {
                uint32_t bitCode = rice(false);                 // decode WITHOUT adapting
                buf1[pos] = x + errCodeSigned(bitCode);
                if (rem > 1) {                                  // look-ahead K estimate
                    int32_t delta = (d() - b()) << 1;
                    bitCode = (bitCode + (uint32_t)(delta < 0 ? -delta : delta)) >> 1;
                }
                k = predictK(k, bitCode, CRX_KMAX);
                pos++;
            }
            rem--;
        }
        if (rem == 1) { int32_t x = med(a(), b(), c()); buf1[pos] = x + errCodeSigned(rice(true)); pos++; }
        buf1[pos] = a() + 1;
    }
};

/* ---- CR3 container: locate the full-raw CRAW track ------------------------------------ */
struct CrxLoc {
    bool ok = false;
    uint32_t fW = 0, fH = 0, tileW = 0, tileH = 0;
    uint8_t nBits = 0, nPlanes = 0, cfaLayout = 0;
    uint64_t mdatOff = 0, streamLen = 0;
    int cropL = 0, cropT = 0, cropR = 0, cropB = 0;   // right/bottom EXCLUSIVE; 0 => none
};

/* Scan a CRAW sample-entry range for CMP1 (geometry) and the full-raw IAD1 (crop). */
void parseCraw(const Bytes &d, size_t seBody, size_t seEnd, CrxLoc &c) {
    for (size_t o = seBody; o + 8 <= seEnd; ++o) {
        if (tagEq(&d[o], "CMP1")) {
            const size_t p = o + 4;                    // payload after fourCC
            c.fW = u32(&d[p + 8]);  c.fH = u32(&d[p + 12]);
            c.tileW = u32(&d[p + 16]); c.tileH = u32(&d[p + 20]);
            c.nBits = d[p + 24]; c.nPlanes = d[p + 25] >> 4; c.cfaLayout = d[p + 25] & 0x0f;
        } else if (tagEq(&d[o], "IAD1")) {
            const size_t p = o + 4;                    // u16[2,3]=fullWH, u16[8..11]=crop l,t,rExcl,bExcl
            const int fw = u16(&d[p + 4]), fh = u16(&d[p + 6]);
            const int l = u16(&d[p + 16]), t = u16(&d[p + 18]);
            const int r = u16(&d[p + 20]), bo = u16(&d[p + 22]);
            /* Keep only the full-raw IAD1 (its full dims match CMP1), and a sane crop. */
            if ((uint32_t)fw == c.tileW && (uint32_t)fh == c.tileH &&
                l >= 0 && t >= 0 && r > l && bo > t && r <= fw && bo <= fh) {
                c.cropL = l; c.cropT = t; c.cropR = r; c.cropB = bo;
            }
        }
    }
}

/* Parse one trak; if it is the (largest) full-raw CRAW track, fill loc. */
void parseTrak(const Bytes &d, size_t body, size_t end, CrxLoc &loc) {
    CrxLoc t;
    bool isCraw = false; uint64_t mdat = 0, slen = 0;
    forEachBox(d, body, end, [&](size_t, const char *ty, size_t mb, size_t me) {
        if (std::memcmp(ty, "mdia", 4)) return;
        forEachBox(d, mb, me, [&](size_t, const char *ty2, size_t mib, size_t mie) {
            if (std::memcmp(ty2, "minf", 4)) return;
            forEachBox(d, mib, mie, [&](size_t, const char *ty3, size_t sb, size_t se) {
                if (std::memcmp(ty3, "stbl", 4)) return;
                forEachBox(d, sb, se, [&](size_t, const char *ty4, size_t xb, size_t xe) {
                    if (!std::memcmp(ty4, "stsd", 4)) {
                        forEachBox(d, xb + 8, xe, [&](size_t bo, const char *sety, size_t seb, size_t see) {
                            if (!std::memcmp(sety, "CRAW", 4)) { isCraw = true; parseCraw(d, seb, see, t); (void)bo; }
                        });
                    } else if (!std::memcmp(ty4, "co64", 4)) { if (u32(&d[xb + 4]) >= 1) mdat = u64(&d[xb + 8]); }
                    else if (!std::memcmp(ty4, "stco", 4))   { if (u32(&d[xb + 4]) >= 1) mdat = u32(&d[xb + 8]); }
                    else if (!std::memcmp(ty4, "stsz", 4))   { uint32_t ss = u32(&d[xb + 4]);
                        slen = ss ? ss : (u32(&d[xb + 8]) >= 1 ? u32(&d[xb + 12]) : 0); }
                });
            });
        });
    });
    if (isCraw && t.fW && t.tileW == t.fW && t.tileH == t.fH) {   // single-tile full-raw only
        if (!loc.ok || uint64_t(t.fW) * t.fH > uint64_t(loc.fW) * loc.fH) {
            t.mdatOff = mdat; t.streamLen = slen; t.ok = true; loc = t;
        }
    }
}

/* CRX header bytes (TLV markers 0xff01/02/03), and validate level 0 (1 subband/plane). */
bool crxHeaderInfo(const Bytes &d, uint64_t off, uint64_t len, size_t &headerBytes, int &nPlanes) {
    size_t o = (size_t)off, end = o + (size_t)len;
    int planes = 0, sb = 0;
    while (o + 4 <= end) {
        uint16_t t = u16(&d[o]);
        if (t < 0xff01 || t > 0xff04) break;
        uint16_t l = u16(&d[o + 2]);
        if (t == 0xff02) planes++;
        else if (t == 0xff03) sb++;
        o += 4 + l;
    }
    headerBytes = o - (size_t)off;
    nPlanes = planes;
    return planes > 0 && sb == planes;    // 1 subband/plane => levels == 0
}

} // namespace

/* ------------------------------------------------------------------------------------------
   CanonCR3Raw::UnpackCfa  --  CR3 CRX sensor unpack (container -> planes -> RGGB mosaic)
   ------------------------------------------------------------------------------------------ */
bool CanonCR3Raw::UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw)
{
    if (!file.isOpen() && !file.open(QIODevice::ReadOnly)) { errMsg = "CR3: cannot open file."; return false; }
    if (!file.seek(0)) { errMsg = "CR3: seek failed."; return false; }
    const QByteArray all = file.readAll();
    if (all.size() < 16) { errMsg = "CR3: file too small."; return false; }
    Bytes d(reinterpret_cast<const uint8_t *>(all.constData()),
            reinterpret_cast<const uint8_t *>(all.constData()) + all.size());

    if (!tagEq(&d[4], "ftyp")) { errMsg = "CR3: not an ISO-BMFF file."; return false; }

    CrxLoc loc;
    forEachBox(d, 0, d.size(), [&](size_t, const char *ty, size_t body, size_t end) {
        if (std::memcmp(ty, "moov", 4)) return;
        forEachBox(d, body, end, [&](size_t, const char *ty2, size_t tb, size_t te) {
            if (!std::memcmp(ty2, "trak", 4)) parseTrak(d, tb, te, loc);
        });
    });
    if (!loc.ok) { errMsg = "CR3: no single-tile full-raw CRAW track."; return false; }
    if (loc.nBits == 0 || loc.nPlanes != 4) { errMsg = "CR3: unexpected CMP1 (planes/bits)."; return false; }
    if (loc.mdatOff == 0 || loc.streamLen == 0 || loc.mdatOff + loc.streamLen > d.size()) {
        errMsg = "CR3: bad CRX bitstream location."; return false;
    }

    size_t headerBytes = 0; int nPlanes = 0;
    if (!crxHeaderInfo(d, loc.mdatOff, loc.streamLen, headerBytes, nPlanes) || nPlanes != 4) {
        errMsg = "CR3: unsupported CRX (levels>0 or plane count)."; return false;
    }

    /* Continuous entropy stream: all planes decode from one reader starting after the header. */
    const int pw = int(loc.tileW / 2), ph = int(loc.tileH / 2);
    const int fW = int(loc.tileW), fH = int(loc.tileH);
    if (pw <= 0 || ph <= 0) { errMsg = "CR3: bad plane geometry."; return false; }
    const uint8_t *stream = &d[loc.mdatOff + headerBytes];
    const size_t streamBytes = (size_t)loc.streamLen - headerBytes;
    const int32_t median = 1 << (loc.nBits - 1);
    const int32_t maxv = (1 << loc.nBits) - 1;

    /* Decode 4 planes and interleave row-major into the full RGGB mosaic. */
    std::vector<uint16_t> full((size_t)fW * fH, 0);
    Band band(stream, streamBytes, pw);
    for (int pl = 0; pl < 4; ++pl) {
        std::fill(band.buf0.begin(), band.buf0.end(), 0);
        std::fill(band.buf1.begin(), band.buf1.end(), 0);
        band.k = 0; band.s_param = 0;
        const int dy = pl >> 1, dx = pl & 1;                  // plane 0/1/2/3 -> quad (0,0)(0,1)(1,0)(1,1)
        for (int y = 0; y < ph; ++y) {
            if (y == 0) band.topLine(); else { std::swap(band.buf0, band.buf1); band.nonTopLine(); }
            uint16_t *row = &full[(size_t)(2 * y + dy) * fW + dx];
            for (int x = 0; x < pw; ++x) {
                int32_t v = median + band.buf1[x + 1];
                row[(size_t)2 * x] = (uint16_t)std::max(0, std::min(maxv, v));
            }
        }
    }

    /* Active-area crop (IAD1). Fall back to the full sensor if absent/implausible. Keep the
       origin even so the RGGB phase is preserved. */
    int left = loc.cropL, top = loc.cropT;
    int cw = loc.cropR - loc.cropL, ch = loc.cropB - loc.cropT;
    if (cw <= 0 || ch <= 0 || left + cw > fW || top + ch > fH) { left = top = 0; cw = fW; ch = fH; }
    if (left & 1) { ++left; --cw; }
    if (top & 1)  { ++top;  --ch; }

    /* Per-channel black from the masked columns to the left of the active area (self-
       calibrating; no per-model black table). Indexed by ((row&1)<<1)|(col&1). */
    uint16_t black[4] = {0, 0, 0, 0};
    if (left > 4) {
        double sum[4] = {0, 0, 0, 0}; size_t cnt[4] = {0, 0, 0, 0};
        const int mb = left - 2;                              // skip the transition columns
        for (int y = 0; y < fH; ++y)
            for (int x = 2; x < mb; ++x) {
                const int idx = ((y & 1) << 1) | (x & 1);
                sum[idx] += full[(size_t)y * fW + x]; ++cnt[idx];
            }
        for (int i = 0; i < 4; ++i) if (cnt[i]) black[i] = uint16_t(sum[i] / double(cnt[i]) + 0.5);
    }

    raw.width = cw; raw.height = ch;
    raw.cfa.assign((size_t)cw * ch, 0);
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x)
            raw.cfa[(size_t)y * cw + x] = full[(size_t)(top + y) * fW + (left + x)];

    raw.pattern = CfaPattern::RGGB;                           // Canon full-frame CRAW is RGGB
    raw.white = uint16_t(maxv);
    for (int i = 0; i < 4; ++i) raw.black[i] = black[i];

    /* As-shot white balance from the CMT3 (Canon makernote) ColorData tag 0x4001. */
    {
        int t = all.indexOf(QByteArray("CMT3", 4));
        if (t >= 4) {
            using namespace TiffWalk;
            Reader r;
            if (r.init(&file, quint32(t + 4))) {              // TIFF embedded at the box payload
                Ifd mn; QList<quint32> subs; quint32 next = 0;
                if (r.readIfd(r.firstIfd(), mn, subs, next) && mn.contains(0x4001)) {
                    const QVector<quint32> cd = r.u32s(mn[0x4001]);
                    const int n = cd.size();
                    const int o = n == 582 ? 25 : n == 653 ? 24 : n == 5120 ? 142 : 63;
                    if (o + 3 < n && cd[o] > 0 && cd[o + 1] > 0 && cd[o + 3] > 0) {
                        raw.camMul[0] = cd[o];       raw.camMul[1] = cd[o + 1];   // R, G1
                        raw.camMul[2] = cd[o + 3];   raw.camMul[3] = cd[o + 2];   // B, G2
                    }
                }
            }
        }
    }

    xyzToCamForModel(m.model, raw.xyzToCam);                  // identity fallback if unknown

    /* Refine the clip level from the per-model saturation table when it has one. */
    { int tb = 0, tmax = 0; if (cameraLevelsForModel(m.model, tb, tmax) && tmax > 0) raw.white = uint16_t(tmax); }

    return true;
}
