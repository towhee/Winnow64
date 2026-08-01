#include "ImageFormats/Fuji/fujicompressed.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

/*
    Fujifilm compressed RAF CFA decoder. Faithful port of LibRaw's fuji_compressed.cpp
    (Copyright 2016-2019 Alexey Danilchenko, adopted to LibRaw by Alex Tutubalin; LGPL 2.1 /
    CDDL 1.0). The only structural change from the upstream is that LibRaw's per-decoder member
    state (the unpacker_data fuji_* fields and the imgdata output) is gathered into a Ctx struct
    threaded through the call chain, so the decoder carries no file-scope mutable state and a
    folder load may decode multiple raws at once. See fujicompressed.h. Algorithm comments are
    kept terse; the upstream file is the reference.
*/

namespace FujiCompressed {
namespace {

typedef uint8_t  uchar;
typedef uint16_t ushort;
typedef int64_t  INT64;

#define _abs(x) (((int)(x) ^ ((int)(x) >> 31)) - ((int)(x) >> 31))
#define _min(a, b) ((a) < (b) ? (a) : (b))
#define _max(a, b) ((a) > (b) ? (a) : (b))

/* Per-call decoder context (replaces LibRaw's unpacker_data + imgdata globals). */
struct Ctx {
    int fuji_total_lines = 0, fuji_total_blocks = 0, fuji_block_width = 0;
    int fuji_bits = 0, fuji_raw_type = 0, fuji_lossless = 0;
    int raw_width = 0, raw_height = 0;
    uint8_t xtrans_abs[6][6] = {{0}};
    int bayer_pat[2][2] = {{0}};
    const uchar *file = nullptr;
    INT64 fileSize = 0;
    ushort *raw = nullptr;                 // output mosaic, raw_width*raw_height
};

static inline int FC(const Ctx &c, int r, int col) { return c.bayer_pat[r & 1][col & 1]; }

static inline unsigned sgetn(int n, const uchar *s) {
    unsigned v = 0; for (int i = 0; i < n; i++) v = (v << 8) | s[i]; return v;
}

struct int_pair { int value1, value2; };
enum _xt_lines { _R0=0,_R1,_R2,_R3,_R4,_G0,_G1,_G2,_G3,_G4,_G5,_G6,_G7,_B0,_B1,_B2,_B3,_B4,_ltotal };

struct fuji_grads { int_pair grads[41]; int_pair lossy_grads[3][5]; };

struct fuji_q_table { int8_t *q_table; int raw_bits; int total_values; int max_grad; int q_grad_mult; int q_base; };
struct fuji_compressed_params {
    fuji_q_table qt[4]; void *buf; int max_bits; int min_value; int max_value; ushort line_width;
};

struct fuji_compressed_block {
    int cur_bit; int cur_pos; INT64 cur_buf_offset; unsigned max_read_size; int cur_buf_size;
    uchar *cur_buf; int fillbytes;
    const uchar *file; INT64 fileSize;     // backing store (LibRaw used a datastream)
    fuji_grads even[3]; fuji_grads odd[3];
    ushort *linealloc; ushort *linebuf[_ltotal];
};

static inline int log2ceil(int val) { int r = 0; if (val--) do ++r; while (val >>= 1); return r; }

static void setup_qlut(int8_t *qt, int *q_point) {
    for (int curVal = -q_point[4]; curVal <= q_point[4]; ++qt, ++curVal) {
        if (curVal <= -q_point[3]) *qt = -4;
        else if (curVal <= -q_point[2]) *qt = -3;
        else if (curVal <= -q_point[1]) *qt = -2;
        else if (curVal <  -q_point[0]) *qt = -1;
        else if (curVal <=  q_point[0]) *qt = 0;
        else if (curVal <   q_point[1]) *qt = 1;
        else if (curVal <   q_point[2]) *qt = 2;
        else if (curVal <   q_point[3]) *qt = 3;
        else *qt = 4;
    }
}

static void init_main_qtable(fuji_compressed_params *params, uchar q_base) {
    fuji_q_table *qt = params->qt; int qp[5];
    int maxVal = params->max_value + 1;
    qp[0] = q_base; qp[1] = 3*q_base+0x12; qp[2] = 5*q_base+0x43; qp[3] = 7*q_base+0x114; qp[4] = params->max_value;
    if (qp[1] >= maxVal || qp[1] < q_base+1) qp[1] = q_base+1;
    if (qp[2] < qp[1] || qp[2] >= maxVal) qp[2] = qp[1];
    if (qp[3] < qp[2] || qp[3] >= maxVal) qp[3] = qp[2];
    setup_qlut(qt->q_table, qp);
    qt->q_base = q_base; qt->max_grad = 0;
    qt->total_values = (qp[4]+2*q_base)/(2*q_base+1)+1;
    qt->raw_bits = log2ceil(qt->total_values);
    qt->q_grad_mult = 9;
    params->max_bits = 4*log2ceil(qp[4]+1);
}

static void init_fuji_compr(const Ctx &c, fuji_compressed_params *params) {
    if ((c.fuji_block_width % 3 && c.fuji_raw_type == 16) ||
        (c.fuji_block_width & 1 && c.fuji_raw_type == 0))
        throw std::runtime_error("Fuji: bad block width");
    size_t q_table_size = (size_t)2 << c.fuji_bits;
    if (c.fuji_lossless) params->buf = calloc(q_table_size, 1);
    else params->buf = calloc(3*q_table_size, 1);

    if (c.fuji_raw_type == 16) params->line_width = (c.fuji_block_width * 2) / 3;
    else params->line_width = c.fuji_block_width >> 1;

    params->min_value = 0x40;
    params->max_value = (1 << c.fuji_bits) - 1;

    if (c.fuji_lossless) {
        memset(params->qt + 1, 0, 3*sizeof(fuji_q_table));
        params->qt[0].q_table = (int8_t *)params->buf;
        params->qt[0].q_base = -1;
        init_main_qtable(params, 0);
    } else {
        memset(params->qt, 0, sizeof(fuji_q_table));
        int qp[5]; qp[0] = 0; qp[4] = params->max_value;
        params->qt[1].q_table = (int8_t *)params->buf; params->qt[1].q_base = 0; params->qt[1].max_grad = 5; params->qt[1].q_grad_mult = 3;
        params->qt[1].total_values = qp[4]+1; params->qt[1].raw_bits = log2ceil(params->qt[1].total_values);
        qp[1] = qp[4]>=0x12?0x12:qp[0]+1; qp[2] = qp[4]>=0x43?0x43:qp[1]; qp[3] = qp[4]>=0x114?0x114:qp[2];
        setup_qlut(params->qt[1].q_table, qp);
        params->qt[2].q_table = params->qt[1].q_table + q_table_size; params->qt[2].q_base = 1; params->qt[2].max_grad = 6; params->qt[2].q_grad_mult = 3;
        params->qt[2].total_values = (qp[4]+2)/3+1; params->qt[2].raw_bits = log2ceil(params->qt[2].total_values);
        qp[0] = params->qt[2].q_base; qp[1] = qp[4]>=0x15?0x15:qp[0]+1; qp[2] = qp[4]>=0x48?0x48:qp[1]; qp[3] = qp[4]>=0x11B?0x11B:qp[2];
        setup_qlut(params->qt[2].q_table, qp);
        params->qt[3].q_table = params->qt[2].q_table + q_table_size; params->qt[3].q_base = 2; params->qt[3].max_grad = 7; params->qt[3].q_grad_mult = 3;
        params->qt[3].total_values = (qp[4]+4)/5+1; params->qt[3].raw_bits = log2ceil(params->qt[3].total_values);
        qp[0] = params->qt[3].q_base; qp[1] = qp[4]>=0x18?0x18:qp[0]+1; qp[2] = qp[4]>=0x4D?0x4D:qp[1]; qp[3] = qp[4]>=0x122?0x122:qp[2];
        setup_qlut(params->qt[3].q_table, qp);
    }
}

#define XTRANS_BUF_SIZE 0x10000

/* Refills the block read buffer from the in-memory file (LibRaw streamed from disk). */
static inline void fuji_fill_buffer(fuji_compressed_block *info) {
    if (info->cur_pos >= info->cur_buf_size) {
        info->cur_pos = 0;
        info->cur_buf_offset += info->cur_buf_size;
        unsigned want = _min(info->max_read_size, (unsigned)XTRANS_BUF_SIZE);
        int got = 0;
        if (info->cur_buf_offset < info->fileSize) {
            INT64 avail = info->fileSize - info->cur_buf_offset;
            got = (int)_min((INT64)want, avail);
            memcpy(info->cur_buf, info->file + info->cur_buf_offset, got);
        }
        info->cur_buf_size = got;
        if (info->cur_buf_size < 1) {
            if (info->fillbytes > 0) {
                int ls = _max(1, _min(info->fillbytes, XTRANS_BUF_SIZE));
                memset(info->cur_buf, 0, ls); info->cur_buf_size = ls; info->fillbytes -= ls;
            } else throw std::runtime_error("Fuji: unexpected EOF");
        }
        info->max_read_size -= info->cur_buf_size;
    }
}

static void init_main_grads(const fuji_compressed_params *params, fuji_compressed_block *info) {
    int max_diff = _max(2, (params->qt->total_values + 0x20) >> 6);
    for (int j = 0; j < 3; j++) for (int i = 0; i < 41; i++) {
        info->even[j].grads[i].value1 = max_diff; info->even[j].grads[i].value2 = 1;
        info->odd[j].grads[i].value1 = max_diff; info->odd[j].grads[i].value2 = 1;
    }
}

static void init_fuji_block(const Ctx &c, fuji_compressed_block *info, const fuji_compressed_params *params,
                            INT64 raw_offset, unsigned dsize) {
    info->linealloc = (ushort *)calloc(sizeof(ushort), (size_t)_ltotal * (params->line_width + 2));
    info->file = c.file; info->fileSize = c.fileSize;
    info->max_read_size = _min((unsigned)(c.fileSize - raw_offset), dsize);
    info->fillbytes = 1;
    info->linebuf[_R0] = info->linealloc;
    for (int i = _R1; i <= _B4; i++) info->linebuf[i] = info->linebuf[i-1] + params->line_width + 2;
    info->cur_buf = (uchar *)calloc(XTRANS_BUF_SIZE, 1);
    info->cur_bit = 0; info->cur_pos = 0; info->cur_buf_offset = raw_offset; info->cur_buf_size = 0;
    fuji_fill_buffer(info);
    if (c.fuji_lossless) init_main_grads(params, info);
    else {
        for (int k = 0; k < 3; ++k) {
            int max_diff = _max(2, ((params->qt[k+1].total_values + 0x20) >> 6));
            for (int j = 0; j < 3; ++j) for (int i = 0; i < 5; ++i) {
                info->even[j].lossy_grads[k][i].value1 = max_diff; info->even[j].lossy_grads[k][i].value2 = 1;
                info->odd[j].lossy_grads[k][i].value1 = max_diff; info->odd[j].lossy_grads[k][i].value2 = 1;
            }
        }
    }
}

static void copy_line_to_xtrans(const Ctx &c, fuji_compressed_block *info, int cur_line, int cur_block, int cur_block_width) {
    ushort *lineBufB[3], *lineBufG[6], *lineBufR[3], *line_buf; unsigned pixel_count; int index;
    int offset = c.fuji_block_width * cur_block + 6 * c.raw_width * cur_line;
    ushort *raw_block_data = c.raw + offset; int row_count = 0;
    for (int i = 0; i < 3; i++) { lineBufR[i] = info->linebuf[_R2+i] + 1; lineBufB[i] = info->linebuf[_B2+i] + 1; }
    for (int i = 0; i < 6; i++) lineBufG[i] = info->linebuf[_G2+i] + 1;
    while (row_count < 6) {
        pixel_count = 0;
        while (pixel_count < (unsigned)cur_block_width) {
            switch (c.xtrans_abs[row_count][pixel_count % 6]) {
                case 0: line_buf = lineBufR[row_count >> 1]; break;
                case 1: default: line_buf = lineBufG[row_count]; break;
                case 2: line_buf = lineBufB[row_count >> 1]; break;
            }
            index = (((pixel_count * 2 / 3) & 0x7FFFFFFE) | ((pixel_count % 3) & 1)) + ((pixel_count % 3) >> 1);
            raw_block_data[pixel_count] = line_buf[index];
            ++pixel_count;
        }
        ++row_count; raw_block_data += c.raw_width;
    }
}

static void copy_line_to_bayer(const Ctx &c, fuji_compressed_block *info, int cur_line, int cur_block, int cur_block_width) {
    ushort *lineBufB[3], *lineBufG[6], *lineBufR[3], *line_buf; unsigned pixel_count;
    int fuji_bayer[2][2]; for (int r = 0; r < 2; r++) for (int col = 0; col < 2; col++) fuji_bayer[r][col] = FC(c, r, col);
    int offset = c.fuji_block_width * cur_block + 6 * c.raw_width * cur_line;
    ushort *raw_block_data = c.raw + offset; int row_count = 0;
    for (int i = 0; i < 3; i++) { lineBufR[i] = info->linebuf[_R2+i] + 1; lineBufB[i] = info->linebuf[_B2+i] + 1; }
    for (int i = 0; i < 6; i++) lineBufG[i] = info->linebuf[_G2+i] + 1;
    while (row_count < 6) {
        pixel_count = 0;
        while (pixel_count < (unsigned)cur_block_width) {
            switch (fuji_bayer[row_count & 1][pixel_count & 1]) {
                case 0: line_buf = lineBufR[row_count >> 1]; break;
                case 1: case 3: default: line_buf = lineBufG[row_count]; break;
                case 2: line_buf = lineBufB[row_count >> 1]; break;
            }
            raw_block_data[pixel_count] = line_buf[pixel_count >> 1];
            ++pixel_count;
        }
        ++row_count; raw_block_data += c.raw_width;
    }
}

#define fuji_quant_gradient(max, q, v1, v2) (q->q_grad_mult * q->q_table[(max) + (v1)] + q->q_table[(max) + (v2)])

static inline void fuji_zerobits(fuji_compressed_block *info, int *count) {
    uchar zero = 0; *count = 0;
    while (zero == 0) {
        zero = (info->cur_buf[info->cur_pos] >> (7 - info->cur_bit)) & 1;
        info->cur_bit++; info->cur_bit &= 7;
        if (!info->cur_bit) { ++info->cur_pos; fuji_fill_buffer(info); }
        if (zero) break;
        ++*count;
    }
}

static inline void fuji_read_code(fuji_compressed_block *info, int *data, int bits_to_read) {
    uchar bits_left = bits_to_read; uchar bits_left_in_byte = 8 - (info->cur_bit & 7); *data = 0;
    if (!bits_to_read) return;
    if (bits_to_read >= bits_left_in_byte) {
        do {
            *data <<= bits_left_in_byte; bits_left -= bits_left_in_byte;
            *data |= info->cur_buf[info->cur_pos] & ((1 << bits_left_in_byte) - 1);
            ++info->cur_pos; fuji_fill_buffer(info); bits_left_in_byte = 8;
        } while (bits_left >= 8);
    }
    if (!bits_left) { info->cur_bit = (8 - (bits_left_in_byte & 7)) & 7; return; }
    *data <<= bits_left; bits_left_in_byte -= bits_left;
    *data |= ((1 << bits_left) - 1) & ((unsigned)info->cur_buf[info->cur_pos] >> bits_left_in_byte);
    info->cur_bit = (8 - (bits_left_in_byte & 7)) & 7;
}

static inline int bitDiff(int value1, int value2) {
    int decBits = 0; if (value2 < value1) while (decBits <= 14 && (value2 << ++decBits) < value1); return decBits;
}

static inline int fuji_decode_sample_even(fuji_compressed_block *info, const fuji_compressed_params *params,
                                          ushort *line_buf, int pos, fuji_grads *grad_params) {
    int interp_val = 0, errcnt = 0, sample = 0, code = 0;
    ushort *line_buf_cur = line_buf + pos;
    int Rb = line_buf_cur[-2 - params->line_width];
    int Rc = line_buf_cur[-3 - params->line_width];
    int Rd = line_buf_cur[-1 - params->line_width];
    int Rf = line_buf_cur[-4 - 2 * params->line_width];
    int grad, gradient, diffRcRb, diffRfRb, diffRdRb;
    diffRcRb = _abs(Rc - Rb); diffRfRb = _abs(Rf - Rb); diffRdRb = _abs(Rd - Rb);
    const fuji_q_table *qt = params->qt; int_pair *grads = grad_params->grads;
    for (int i = 1; params->qt[0].q_base >= i && i < 4; ++i)
        if (diffRfRb + diffRcRb <= params->qt[i].max_grad) { qt = params->qt + i; grads = grad_params->lossy_grads[i-1]; break; }
    grad = fuji_quant_gradient(params->max_value, qt, Rb - Rf, Rc - Rb); gradient = _abs(grad);
    if (diffRcRb > diffRfRb && diffRcRb > diffRdRb) interp_val = Rf + Rd + 2 * Rb;
    else if (diffRdRb > diffRcRb && diffRdRb > diffRfRb) interp_val = Rf + Rc + 2 * Rb;
    else interp_val = Rd + Rc + 2 * Rb;
    fuji_zerobits(info, &sample);
    if (sample < params->max_bits - qt->raw_bits - 1) {
        int decBits = bitDiff(grads[gradient].value1, grads[gradient].value2);
        fuji_read_code(info, &code, decBits); code += sample << decBits;
    } else { fuji_read_code(info, &code, qt->raw_bits); ++code; }
    if (code < 0 || code >= qt->total_values) ++errcnt;
    if (code & 1) code = -1 - code / 2; else code /= 2;
    grads[gradient].value1 += _abs(code);
    if (grads[gradient].value2 == params->min_value) { grads[gradient].value1 >>= 1; grads[gradient].value2 >>= 1; }
    ++grads[gradient].value2;
    if (grad < 0) interp_val = (interp_val >> 2) - code * (2 * qt->q_base + 1);
    else interp_val = (interp_val >> 2) + code * (2 * qt->q_base + 1);
    if (interp_val < -qt->q_base) interp_val += qt->total_values * (2 * qt->q_base + 1);
    else if (interp_val > qt->q_base + params->max_value) interp_val -= qt->total_values * (2 * qt->q_base + 1);
    if (interp_val >= 0) line_buf_cur[0] = _min(interp_val, params->max_value); else line_buf_cur[0] = 0;
    return errcnt;
}

static inline int fuji_decode_sample_odd(fuji_compressed_block *info, const fuji_compressed_params *params,
                                         ushort *line_buf, int pos, fuji_grads *grad_params) {
    int interp_val = 0, errcnt = 0, sample = 0, code = 0;
    ushort *line_buf_cur = line_buf + pos;
    int Ra = line_buf_cur[-1];
    int Rb = line_buf_cur[-2 - params->line_width];
    int Rc = line_buf_cur[-3 - params->line_width];
    int Rd = line_buf_cur[-1 - params->line_width];
    int Rg = line_buf_cur[1];
    int grad, gradient;
    int diffRcRa = _abs(Rc - Ra); int diffRbRc = _abs(Rb - Rc);
    const fuji_q_table *qt = params->qt; int_pair *grads = grad_params->grads;
    for (int i = 1; params->qt[0].q_base >= i && i < 4; ++i)
        if (diffRbRc + diffRcRa <= params->qt[i].max_grad) { qt = params->qt + i; grads = grad_params->lossy_grads[i-1]; break; }
    grad = fuji_quant_gradient(params->max_value, qt, Rb - Rc, Rc - Ra); gradient = _abs(grad);
    if ((Rb > Rc && Rb > Rd) || (Rb < Rc && Rb < Rd)) interp_val = (Rg + Ra + 2 * Rb) >> 2;
    else interp_val = (Ra + Rg) >> 1;
    fuji_zerobits(info, &sample);
    if (sample < params->max_bits - qt->raw_bits - 1) {
        int decBits = bitDiff(grads[gradient].value1, grads[gradient].value2);
        fuji_read_code(info, &code, decBits); code += sample << decBits;
    } else { fuji_read_code(info, &code, qt->raw_bits); ++code; }
    if (code < 0 || code >= qt->total_values) ++errcnt;
    if (code & 1) code = -1 - code / 2; else code /= 2;
    grads[gradient].value1 += _abs(code);
    if (grads[gradient].value2 == params->min_value) { grads[gradient].value1 >>= 1; grads[gradient].value2 >>= 1; }
    ++grads[gradient].value2;
    if (grad < 0) interp_val -= code * (2 * qt->q_base + 1);
    else interp_val += code * (2 * qt->q_base + 1);
    if (interp_val < -qt->q_base) interp_val += qt->total_values * (2 * qt->q_base + 1);
    else if (interp_val > qt->q_base + params->max_value) interp_val -= qt->total_values * (2 * qt->q_base + 1);
    if (interp_val >= 0) line_buf_cur[0] = _min(interp_val, params->max_value); else line_buf_cur[0] = 0;
    return errcnt;
}

static void fuji_decode_interpolation_even(int line_width, ushort *line_buf, int pos) {
    ushort *line_buf_cur = line_buf + pos;
    int Rb = line_buf_cur[-2 - line_width];
    int Rc = line_buf_cur[-3 - line_width];
    int Rd = line_buf_cur[-1 - line_width];
    int Rf = line_buf_cur[-4 - 2 * line_width];
    int diffRcRb = _abs(Rc - Rb), diffRfRb = _abs(Rf - Rb), diffRdRb = _abs(Rd - Rb);
    if (diffRcRb > diffRfRb && diffRcRb > diffRdRb) *line_buf_cur = (Rf + Rd + 2 * Rb) >> 2;
    else if (diffRdRb > diffRcRb && diffRdRb > diffRfRb) *line_buf_cur = (Rf + Rc + 2 * Rb) >> 2;
    else *line_buf_cur = (Rd + Rc + 2 * Rb) >> 2;
}

static void fuji_extend_generic(ushort *linebuf[_ltotal], int line_width, int start, int end) {
    for (int i = start; i <= end; i++) { linebuf[i][0] = linebuf[i-1][1]; linebuf[i][line_width+1] = linebuf[i-1][line_width]; }
}
static void fuji_extend_red(ushort *lb[_ltotal], int lw)   { fuji_extend_generic(lb, lw, _R2, _R4); }
static void fuji_extend_green(ushort *lb[_ltotal], int lw) { fuji_extend_generic(lb, lw, _G2, _G7); }
static void fuji_extend_blue(ushort *lb[_ltotal], int lw)  { fuji_extend_generic(lb, lw, _B2, _B4); }

static void xtrans_decode_block(fuji_compressed_block *info, const fuji_compressed_params *params) {
    int r_even_pos = 0, r_odd_pos = 1, g_even_pos = 0, g_odd_pos = 1, b_even_pos = 0, b_odd_pos = 1, errcnt = 0;
    const int lw = params->line_width;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) { fuji_decode_interpolation_even(lw, info->linebuf[_R2]+1, r_even_pos); r_even_pos += 2;
            errcnt += fuji_decode_sample_even(info, params, info->linebuf[_G2]+1, g_even_pos, &info->even[0]); g_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_R2]+1, r_odd_pos, &info->odd[0]); r_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G2]+1, g_odd_pos, &info->odd[0]); g_odd_pos += 2; }
    }
    fuji_extend_red(info->linebuf, lw); fuji_extend_green(info->linebuf, lw);
    g_even_pos = 0; g_odd_pos = 1;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) { errcnt += fuji_decode_sample_even(info, params, info->linebuf[_G3]+1, g_even_pos, &info->even[1]); g_even_pos += 2;
            fuji_decode_interpolation_even(lw, info->linebuf[_B2]+1, b_even_pos); b_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G3]+1, g_odd_pos, &info->odd[1]); g_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_B2]+1, b_odd_pos, &info->odd[1]); b_odd_pos += 2; }
    }
    fuji_extend_green(info->linebuf, lw); fuji_extend_blue(info->linebuf, lw);
    r_even_pos = 0; r_odd_pos = 1; g_even_pos = 0; g_odd_pos = 1;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) {
            if (r_even_pos & 3) errcnt += fuji_decode_sample_even(info, params, info->linebuf[_R3]+1, r_even_pos, &info->even[2]);
            else fuji_decode_interpolation_even(lw, info->linebuf[_R3]+1, r_even_pos);
            r_even_pos += 2; fuji_decode_interpolation_even(lw, info->linebuf[_G4]+1, g_even_pos); g_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_R3]+1, r_odd_pos, &info->odd[2]); r_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G4]+1, g_odd_pos, &info->odd[2]); g_odd_pos += 2; }
    }
    fuji_extend_red(info->linebuf, lw); fuji_extend_green(info->linebuf, lw);
    g_even_pos = 0; g_odd_pos = 1; b_even_pos = 0; b_odd_pos = 1;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) { errcnt += fuji_decode_sample_even(info, params, info->linebuf[_G5]+1, g_even_pos, &info->even[0]); g_even_pos += 2;
            if ((b_even_pos & 3) == 2) fuji_decode_interpolation_even(lw, info->linebuf[_B3]+1, b_even_pos);
            else errcnt += fuji_decode_sample_even(info, params, info->linebuf[_B3]+1, b_even_pos, &info->even[0]);
            b_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G5]+1, g_odd_pos, &info->odd[0]); g_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_B3]+1, b_odd_pos, &info->odd[0]); b_odd_pos += 2; }
    }
    fuji_extend_green(info->linebuf, lw); fuji_extend_blue(info->linebuf, lw);
    r_even_pos = 0; r_odd_pos = 1; g_even_pos = 0; g_odd_pos = 1;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) {
            if ((r_even_pos & 3) == 2) fuji_decode_interpolation_even(lw, info->linebuf[_R4]+1, r_even_pos);
            else errcnt += fuji_decode_sample_even(info, params, info->linebuf[_R4]+1, r_even_pos, &info->even[1]);
            r_even_pos += 2; errcnt += fuji_decode_sample_even(info, params, info->linebuf[_G6]+1, g_even_pos, &info->even[1]); g_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_R4]+1, r_odd_pos, &info->odd[1]); r_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G6]+1, g_odd_pos, &info->odd[1]); g_odd_pos += 2; }
    }
    fuji_extend_red(info->linebuf, lw); fuji_extend_green(info->linebuf, lw);
    g_even_pos = 0; g_odd_pos = 1; b_even_pos = 0; b_odd_pos = 1;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) { fuji_decode_interpolation_even(lw, info->linebuf[_G7]+1, g_even_pos); g_even_pos += 2;
            if (b_even_pos & 3) errcnt += fuji_decode_sample_even(info, params, info->linebuf[_B4]+1, b_even_pos, &info->even[2]);
            else fuji_decode_interpolation_even(lw, info->linebuf[_B4]+1, b_even_pos);
            b_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G7]+1, g_odd_pos, &info->odd[2]); g_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_B4]+1, b_odd_pos, &info->odd[2]); b_odd_pos += 2; }
    }
    fuji_extend_green(info->linebuf, lw); fuji_extend_blue(info->linebuf, lw);
    if (errcnt) throw std::runtime_error("Fuji: X-Trans decode error");
}

static void fuji_bayer_decode_block(fuji_compressed_block *info, const fuji_compressed_params *params) {
    int r_even_pos = 0, r_odd_pos = 1, g_even_pos = 0, g_odd_pos = 1, b_even_pos = 0, b_odd_pos = 1, errcnt = 0;
    const int lw = params->line_width;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) { errcnt += fuji_decode_sample_even(info, params, info->linebuf[_R2]+1, r_even_pos, &info->even[0]); r_even_pos += 2;
            errcnt += fuji_decode_sample_even(info, params, info->linebuf[_G2]+1, g_even_pos, &info->even[0]); g_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_R2]+1, r_odd_pos, &info->odd[0]); r_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G2]+1, g_odd_pos, &info->odd[0]); g_odd_pos += 2; }
    }
    fuji_extend_red(info->linebuf, lw); fuji_extend_green(info->linebuf, lw);
    g_even_pos = 0; g_odd_pos = 1;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) { errcnt += fuji_decode_sample_even(info, params, info->linebuf[_G3]+1, g_even_pos, &info->even[1]); g_even_pos += 2;
            errcnt += fuji_decode_sample_even(info, params, info->linebuf[_B2]+1, b_even_pos, &info->even[1]); b_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G3]+1, g_odd_pos, &info->odd[1]); g_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_B2]+1, b_odd_pos, &info->odd[1]); b_odd_pos += 2; }
    }
    fuji_extend_green(info->linebuf, lw); fuji_extend_blue(info->linebuf, lw);
    r_even_pos = 0; r_odd_pos = 1; g_even_pos = 0; g_odd_pos = 1;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) { errcnt += fuji_decode_sample_even(info, params, info->linebuf[_R3]+1, r_even_pos, &info->even[2]); r_even_pos += 2;
            errcnt += fuji_decode_sample_even(info, params, info->linebuf[_G4]+1, g_even_pos, &info->even[2]); g_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_R3]+1, r_odd_pos, &info->odd[2]); r_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G4]+1, g_odd_pos, &info->odd[2]); g_odd_pos += 2; }
    }
    fuji_extend_red(info->linebuf, lw); fuji_extend_green(info->linebuf, lw);
    g_even_pos = 0; g_odd_pos = 1; b_even_pos = 0; b_odd_pos = 1;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) { errcnt += fuji_decode_sample_even(info, params, info->linebuf[_G5]+1, g_even_pos, &info->even[0]); g_even_pos += 2;
            errcnt += fuji_decode_sample_even(info, params, info->linebuf[_B3]+1, b_even_pos, &info->even[0]); b_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G5]+1, g_odd_pos, &info->odd[0]); g_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_B3]+1, b_odd_pos, &info->odd[0]); b_odd_pos += 2; }
    }
    fuji_extend_green(info->linebuf, lw); fuji_extend_blue(info->linebuf, lw);
    r_even_pos = 0; r_odd_pos = 1; g_even_pos = 0; g_odd_pos = 1;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) { errcnt += fuji_decode_sample_even(info, params, info->linebuf[_R4]+1, r_even_pos, &info->even[1]); r_even_pos += 2;
            errcnt += fuji_decode_sample_even(info, params, info->linebuf[_G6]+1, g_even_pos, &info->even[1]); g_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_R4]+1, r_odd_pos, &info->odd[1]); r_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G6]+1, g_odd_pos, &info->odd[1]); g_odd_pos += 2; }
    }
    fuji_extend_red(info->linebuf, lw); fuji_extend_green(info->linebuf, lw);
    g_even_pos = 0; g_odd_pos = 1; b_even_pos = 0; b_odd_pos = 1;
    while (g_even_pos < lw || g_odd_pos < lw) {
        if (g_even_pos < lw) { errcnt += fuji_decode_sample_even(info, params, info->linebuf[_G7]+1, g_even_pos, &info->even[2]); g_even_pos += 2;
            errcnt += fuji_decode_sample_even(info, params, info->linebuf[_B4]+1, b_even_pos, &info->even[2]); b_even_pos += 2; }
        if (g_even_pos > 8) { errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_G7]+1, g_odd_pos, &info->odd[2]); g_odd_pos += 2;
            errcnt += fuji_decode_sample_odd(info, params, info->linebuf[_B4]+1, b_odd_pos, &info->odd[2]); b_odd_pos += 2; }
    }
    fuji_extend_green(info->linebuf, lw); fuji_extend_blue(info->linebuf, lw);
    if (errcnt) throw std::runtime_error("Fuji: Bayer decode error");
}

static void fuji_decode_strip(const Ctx &c, fuji_compressed_params *params, int cur_block,
                              INT64 raw_offset, unsigned dsize, uchar *q_bases) {
    fuji_compressed_block info;
    fuji_compressed_params *info_common = params;
    std::vector<uchar> alt;
    if (!c.fuji_lossless) {
        int buf_size = sizeof(fuji_compressed_params) + ((size_t)2 << c.fuji_bits);
        alt.assign(buf_size, 0);
        info_common = (fuji_compressed_params *)alt.data();
        memcpy(info_common, params, sizeof(fuji_compressed_params));
        info_common->qt[0].q_table = (int8_t *)(info_common + 1);
        info_common->qt[0].q_base = -1;
    }
    init_fuji_block(c, &info, info_common, raw_offset, dsize);
    unsigned line_size = sizeof(ushort) * (info_common->line_width + 2);
    int cur_block_width = c.fuji_block_width;
    if (cur_block + 1 == c.fuji_total_blocks) cur_block_width = c.raw_width - (c.fuji_block_width * cur_block);

    struct i_pair { int a, b; };
    const i_pair mtable[6] = {{_R0,_R3},{_R1,_R4},{_G0,_G6},{_G1,_G7},{_B0,_B3},{_B1,_B4}}, ztable[3] = {{_R2,3},{_G2,6},{_B2,3}};
    for (int cur_line = 0; cur_line < c.fuji_total_lines; cur_line++) {
        if (!c.fuji_lossless) {
            int q_base = q_bases ? q_bases[cur_line] : 0;
            if (!cur_line || q_base != info_common->qt[0].q_base) {
                init_main_qtable(info_common, q_bases ? q_bases[cur_line] : 0); init_main_grads(info_common, &info);
            }
        }
        if (c.fuji_raw_type == 16) xtrans_decode_block(&info, info_common);
        else fuji_bayer_decode_block(&info, info_common);
        for (int i = 0; i < 6; i++) memcpy(info.linebuf[mtable[i].a], info.linebuf[mtable[i].b], line_size);
        if (c.fuji_raw_type == 16) copy_line_to_xtrans(c, &info, cur_line, cur_block, cur_block_width);
        else copy_line_to_bayer(c, &info, cur_line, cur_block, cur_block_width);
        for (int i = 0; i < 3; i++) {
            memset(info.linebuf[ztable[i].a], 0, ztable[i].b * line_size);
            info.linebuf[ztable[i].a][0] = info.linebuf[ztable[i].a-1][1];
            info.linebuf[ztable[i].a][info_common->line_width+1] = info.linebuf[ztable[i].a-1][info_common->line_width];
        }
    }
    free(info.linealloc); free(info.cur_buf);
}

} // anonymous namespace

bool isCompressed(const uint8_t *data, int64_t size, int64_t dataOffset)
{
    if (!data || dataOffset < 0 || dataOffset + 16 > size) return false;
    return sgetn(2, data + dataOffset) == 0x4953;       // compressed-header signature
}

bool decode(const uint8_t *data, int64_t size, int64_t dataOffset,
            std::vector<uint16_t> &out, int &rawW, int &rawH,
            const uint8_t xtrans[6][6], const int bayer[2][2], QString &err)
{
    if (!isCompressed(data, size, dataOffset)) { err = "Fuji: not a compressed RAF header."; return false; }

    Ctx c;
    c.file = data; c.fileSize = size;
    const uchar *h = data + dataOffset;
    /* 16-byte big-endian compressed header. */
    c.fuji_lossless    = h[2];
    c.fuji_raw_type    = h[3];
    c.fuji_bits        = h[4];
    c.raw_height       = (int)sgetn(2, h + 5);
    int rounded_width  = (int)sgetn(2, h + 7);
    c.raw_width        = (int)sgetn(2, h + 9);
    c.fuji_block_width = (int)sgetn(2, h + 11);
    c.fuji_total_blocks = h[13];
    c.fuji_total_lines = (int)sgetn(2, h + 14);

    /* Validation mirroring LibRaw::parse_fuji_compressed_header. */
    if (c.fuji_lossless > 1 || c.raw_height < 6 || c.raw_height % 6 ||
        c.fuji_block_width != 0x300 || c.fuji_total_blocks == 0 || c.fuji_total_blocks > 0x10 ||
        rounded_width < c.fuji_block_width || rounded_width % c.fuji_block_width ||
        c.fuji_total_blocks != rounded_width / c.fuji_block_width ||
        c.fuji_total_lines == 0 || c.fuji_total_lines != c.raw_height / 6 ||
        (c.fuji_bits != 12 && c.fuji_bits != 14 && c.fuji_bits != 16) ||
        (c.fuji_raw_type != 16 && c.fuji_raw_type != 0)) {
        err = "Fuji: invalid compressed header."; return false;
    }
    INT64 strip_data = dataOffset + 16;        // block-size table follows the header

    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) c.xtrans_abs[i][j] = xtrans ? xtrans[i][j] : 0;
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) c.bayer_pat[i][j] = bayer ? bayer[i][j] : 0;

    out.assign((size_t)c.raw_width * c.raw_height, 0);
    c.raw = out.data();

    fuji_compressed_params common; memset(&common, 0, sizeof(common));
    try {
        init_fuji_compr(c, &common);

        std::vector<unsigned> block_sizes(c.fuji_total_blocks);
        std::vector<INT64> raw_block_offsets(c.fuji_total_blocks);
        if (strip_data + (INT64)sizeof(unsigned) * c.fuji_total_blocks > size) throw std::runtime_error("Fuji: truncated block table");
        memcpy(block_sizes.data(), data + strip_data, sizeof(unsigned) * c.fuji_total_blocks);

        INT64 raw_offset = ((INT64)(sizeof(unsigned) * c.fuji_total_blocks) + 0xF) & ~0xF;
        uchar *q_bases = nullptr; std::vector<uchar> qb;
        int lineStep = (c.fuji_total_lines + 0xF) & ~0xF;
        if (!c.fuji_lossless) {
            int total_q = c.fuji_total_blocks * lineStep;
            if (strip_data + raw_offset + total_q > size) throw std::runtime_error("Fuji: truncated q_bases");
            qb.assign(total_q, 0);
            memcpy(qb.data(), data + strip_data + raw_offset, total_q);
            q_bases = qb.data();
            raw_offset += total_q;
        }
        raw_offset += strip_data;
        for (int b = 0; b < c.fuji_total_blocks; b++) block_sizes[b] = sgetn(4, (uchar *)&block_sizes[b]);
        raw_block_offsets[0] = raw_offset;
        for (int b = 1; b < c.fuji_total_blocks; b++) raw_block_offsets[b] = raw_block_offsets[b-1] + block_sizes[b-1];

        for (int b = 0; b < c.fuji_total_blocks; b++)
            fuji_decode_strip(c, &common, b, raw_block_offsets[b], block_sizes[b], q_bases ? q_bases + b * lineStep : nullptr);
    } catch (const std::exception &e) {
        free(common.buf); err = QString::fromLatin1(e.what()); return false;
    }
    free(common.buf);
    rawW = c.raw_width; rawH = c.raw_height;
    return true;
}

#undef _abs
#undef _min
#undef _max
#undef fuji_quant_gradient
#undef XTRANS_BUF_SIZE

} // namespace FujiCompressed
