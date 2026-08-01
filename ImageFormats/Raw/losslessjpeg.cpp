#include "ImageFormats/Raw/losslessjpeg.h"

/*
    Implementation note: this is a direct port of a reference decoder validated end-to-end
    against a real lossless-JPEG DNG (Leica M10 CFA: SOF3, 14-bit, 2 components, predictor 1) --
    the decoded mosaic demosaiced/colour-converted to a coherent, correctly-coloured image,
    confirming the Huffman tables, bit reader and predictors are correct.
*/

namespace LosslessJpeg {

namespace {

/* Canonical Huffman table: per code length 1..16, the value range, plus the symbol list. */
struct HuffTable {
    int mincode[17];
    int maxcode[17];     // -1 when no codes of that length
    int valptr[17];
    std::vector<uint8_t> vals;
    bool built = false;

    void build(const uint8_t counts[16], const uint8_t *symbols, int nsym)
    {
        vals.assign(symbols, symbols + nsym);
        int code = 0, j = 0;
        for (int l = 1; l <= 16; ++l) {
            if (counts[l - 1]) {
                valptr[l] = j;
                mincode[l] = code;
                code += counts[l - 1];
                maxcode[l] = code - 1;
                j += counts[l - 1];
            } else {
                maxcode[l] = -1;
            }
            code <<= 1;
        }
        built = true;
    }
};

/* MSB-first bit reader over the entropy-coded segment. Handles 0xFF00 byte stuffing; on any
   real marker (or end of data) it stops consuming and feeds zero bits, which is the standard
   way to let a well-formed scan finish decoding its last samples. */
struct BitReader {
    const uint8_t *d;
    size_t size, pos = 0;
    uint32_t buf = 0;
    int cnt = 0;
    bool ended = false;

    uint8_t nextByte()
    {
        if (ended || pos >= size) return 0;
        uint8_t c = d[pos++];
        if (c == 0xFF) {
            const uint8_t n = (pos < size) ? d[pos] : 0;
            if (n == 0) ++pos;                  // stuffed 0xFF00 -> literal 0xFF
            else { ended = true; return 0; }    // marker -> stop, pad with zeros
        }
        return c;
    }
    int bit()
    {
        if (cnt == 0) { buf = nextByte(); cnt = 8; }
        --cnt;
        return (buf >> cnt) & 1;
    }
    uint32_t bits(int k)
    {
        uint32_t v = 0;
        while (k-- > 0) v = (v << 1) | bit();
        return v;
    }
};

inline int DecodeHuff(BitReader &br, const HuffTable &h)
{
    int l = 1, code = br.bit();
    while (l <= 16 && code > h.maxcode[l]) { ++l; code = (code << 1) | br.bit(); }
    if (l > 16) return 0;
    return h.vals[h.valptr[l] + code - h.mincode[l]];
}

/* JPEG "receive + extend": read s bits, sign-extend to the signed difference. */
inline int Receive(BitReader &br, int s)
{
    if (s == 0) return 0;
    int v = static_cast<int>(br.bits(s));
    if (v < (1 << (s - 1))) v += (-1 << s) + 1;
    return v;
}

inline uint16_t rd16(const uint8_t *p) { return uint16_t((p[0] << 8) | p[1]); }

void setErr(QString *err, const char *m) { if (err) *err = QString::fromLatin1(m); }

} // namespace

bool Decode(const uint8_t *data, size_t size, Image &out, QString *err)
{
    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8) { setErr(err, "ljpeg: no SOI"); return false; }

    HuffTable huff[4];          // up to 4 DC tables (Th 0..3)
    int prec = 0, Y = 0, X = 0, Nf = 0;
    int psv = 1, Pt = 0, Ri = 0;
    int scanTd[4] = {0, 0, 0, 0};   // per scan-component Huffman table selector
    int scanNs = 0;

    size_t p = 2;
    bool haveScan = false;
    while (p + 1 < size) {
        if (data[p] != 0xFF) { ++p; continue; }
        const uint8_t marker = data[p + 1];
        p += 2;
        if (marker == 0xD9) break;                       // EOI
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue;  // TEM / RSTn
        if (p + 2 > size) { setErr(err, "ljpeg: truncated"); return false; }
        const int L = rd16(data + p);
        if (L < 2 || p + L > size) { setErr(err, "ljpeg: bad segment length"); return false; }
        const uint8_t *seg = data + p + 2;
        const int segLen = L - 2;

        if (marker == 0xC3) {                            // SOF3 (lossless, Huffman)
            if (segLen < 6) { setErr(err, "ljpeg: short SOF3"); return false; }
            prec = seg[0];
            Y = (seg[1] << 8) | seg[2];
            X = (seg[3] << 8) | seg[4];
            Nf = seg[5];
            if (Nf < 1 || Nf > 4 || prec < 2 || prec > 16) { setErr(err, "ljpeg: unsupported SOF3"); return false; }
            if (segLen < 6 + Nf * 3) { setErr(err, "ljpeg: short SOF3 components"); return false; }
            for (int i = 0; i < Nf; ++i) {
                const uint8_t hv = seg[7 + i * 3];
                if ((hv >> 4) != 1 || (hv & 0xF) != 1) { setErr(err, "ljpeg: subsampling unsupported"); return false; }
            }
        }
        else if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2 || marker == 0xC9 ||
                 marker == 0xCA || marker == 0xCB) {
            setErr(err, "ljpeg: not a lossless (SOF3) frame"); return false;
        }
        else if (marker == 0xC4) {                       // DHT
            int q = 0;
            while (q + 17 <= segLen) {
                const int Th = seg[q] & 0xF;
                const int Tc = seg[q] >> 4;
                ++q;
                if (Tc != 0 || Th > 3) { setErr(err, "ljpeg: bad Huffman class/id"); return false; }
                uint8_t counts[16];
                int nsym = 0;
                for (int i = 0; i < 16; ++i) { counts[i] = seg[q + i]; nsym += counts[i]; }
                q += 16;
                if (q + nsym > segLen) { setErr(err, "ljpeg: bad Huffman table"); return false; }
                huff[Th].build(counts, seg + q, nsym);
                q += nsym;
            }
        }
        else if (marker == 0xDD) {                       // DRI
            if (segLen >= 2) Ri = (seg[0] << 8) | seg[1];
        }
        else if (marker == 0xDA) {                       // SOS
            const int Ns = seg[0];
            if (Ns < 1 || Ns > 4 || segLen < 1 + Ns * 2 + 3) { setErr(err, "ljpeg: bad SOS"); return false; }
            for (int i = 0; i < Ns; ++i) scanTd[i] = seg[2 + i * 2] >> 4;
            psv = seg[1 + Ns * 2];
            Pt  = seg[3 + Ns * 2] & 0xF;
            scanNs = Ns;
            haveScan = true;
            p += L;
            break;                                       // entropy data follows
        }
        p += L;
    }

    if (!haveScan || Nf == 0 || X == 0 || Y == 0) { setErr(err, "ljpeg: missing SOF/SOS"); return false; }
    if (scanNs != Nf) { setErr(err, "ljpeg: scan/frame component mismatch"); return false; }
    if (Ri != 0) { setErr(err, "ljpeg: restart intervals not supported yet"); return false; }
    for (int c = 0; c < Nf; ++c)
        if (!huff[scanTd[c]].built) { setErr(err, "ljpeg: missing Huffman table"); return false; }
    if (psv < 1 || psv > 7) { setErr(err, "ljpeg: bad predictor"); return false; }

    const int clrs = Nf;
    out.width = X;
    out.height = Y;
    out.components = clrs;
    out.precision = prec;
    out.samples.assign(static_cast<size_t>(X) * Y * clrs, 0);

    BitReader br{data + p, size - p};
    const int def = 1 << (prec - Pt - 1);
    const int mask = 0xFFFF;

    std::vector<int> cur(static_cast<size_t>(X) * clrs, 0);
    std::vector<int> above(static_cast<size_t>(X) * clrs, def);

    for (int row = 0; row < Y; ++row) {
        for (int col = 0; col < X; ++col) {
            for (int c = 0; c < clrs; ++c) {
                const int s = DecodeHuff(br, huff[scanTd[c]]);
                const int diff = Receive(br, s);

                int pred;
                if (col == 0) {
                    pred = (row == 0) ? def : above[c];                 // first column: Rb
                } else if (row == 0) {
                    pred = cur[(col - 1) * clrs + c];                   // first row: Ra
                } else {
                    const int Ra = cur[(col - 1) * clrs + c];
                    const int Rb = above[col * clrs + c];
                    const int Rc = above[(col - 1) * clrs + c];
                    switch (psv) {
                    case 1:  pred = Ra; break;
                    case 2:  pred = Rb; break;
                    case 3:  pred = Rc; break;
                    case 4:  pred = Ra + Rb - Rc; break;
                    case 5:  pred = Ra + ((Rb - Rc) >> 1); break;
                    case 6:  pred = Rb + ((Ra - Rc) >> 1); break;
                    default: pred = (Ra + Rb) >> 1; break;              // 7
                    }
                }

                const int val = (pred + diff) & mask;
                cur[col * clrs + c] = val;
                out.samples[(static_cast<size_t>(row) * X + col) * clrs + c] =
                    static_cast<uint16_t>((val << Pt) & mask);
            }
        }
        cur.swap(above);
    }

    return true;
}

} // namespace LosslessJpeg
