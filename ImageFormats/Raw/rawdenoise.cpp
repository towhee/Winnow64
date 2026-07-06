#include "ImageFormats/Raw/rawdenoise.h"
#include "Develop/workingimage.h"
#include "Utilities/inference/inferencesession.h"
#include "Main/global.h"

#include <QDir>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <vector>

/*
    Post-demosaic RGB denoise (RawRefinery TreeNet). See rawdenoise.h for the model contract.

    Pipeline per call:
      1. For each tile of the WorkingImage: convert scene-linear sRGB -> linear Rec2020, divide by
         the white level and clip to [0,1] (what the model was trained on), feed {1,3,th,tw} plus
         the ISO conditioning {1,1}, and accumulate the denoised tile with a feathered (triangular)
         weight so overlapping tiles stitch seam-free. Tiles run sequentially to keep the per-call
         working set bounded (one decode already runs on a concurrency-capped thread).
      2. Combine per pixel: recover the (unclipped) Rec2020 value, split the denoise correction
         (denoised - clipped-input) into luma + chroma, scale luma by denoiseLuma and chroma by
         max(luma,chroma), ADD it to the original (so highlights above 1.0, where the correction is
         ~0, pass through untouched), convert Rec2020 -> sRGB, and write back.
*/

namespace {

constexpr int   kTile      = 512;
constexpr int   kOverlap   = 64;
const     char *kModelFile = "rawdenoise.onnx";

/* Linear sRGB (D65) <-> linear Rec2020 (D65). */
constexpr float kSrgbToRec2020[9] = {
    0.62740f, 0.32928f, 0.04332f,
    0.06909f, 0.91954f, 0.01138f,
    0.01639f, 0.08801f, 0.89561f,
};
constexpr float kRec2020ToSrgb[9] = {
     1.66049f, -0.58764f, -0.07285f,
    -0.12455f,  1.13293f, -0.00838f,
    -0.01821f, -0.10064f,  1.11885f,
};
/* Rec2020 luma weights, for the luma/chroma split of the correction. */
constexpr float kLumaR = 0.2627f, kLumaG = 0.6780f, kLumaB = 0.0593f;

inline void mat3(const float m[9], float a, float b, float c,
                 float &x, float &y, float &z)
{
    x = m[0]*a + m[1]*b + m[2]*c;
    y = m[3]*a + m[4]*b + m[5]*c;
    z = m[6]*a + m[7]*b + m[8]*c;
}

std::vector<int> tileStarts(int total)
{
    std::vector<int> v;
    if (total <= kTile) { v.push_back(0); return v; }
    const int step = kTile - kOverlap;
    for (int s = 0; s + kTile < total; s += step) v.push_back(s);
    if (v.empty() || v.back() != total - kTile) v.push_back(total - kTile);
    return v;
}

inline float feather(int t, int tw)
{
    const int edge = std::min(t + 1, tw - t);
    return std::min(1.0f, static_cast<float>(edge) / static_cast<float>(kOverlap + 1));
}

} // namespace

namespace RawDenoise {

InferenceSession *SharedSession()
{
    static std::once_flag once;
    static std::unique_ptr<InferenceSession> session;
    std::call_once(once, [] {
        const QString path =
            QDir(QCoreApplication::applicationDirPath()).filePath(kModelFile);
        /* CPU EP: the CoreML EP miscomputes this model -- it fragments the graph into ~44
           partitions and its fp16/mixed CPU-ANE execution produces a green/magenta colour cast and
           a 2px checkerboard (huge relative error on near-zero dark pixels), verified vs the correct
           CPU output. Until a CoreML-friendly re-export exists, run on CPU for correctness. */
        session = std::make_unique<InferenceSession>(path, InferenceDevice::CPU);
        if (!session->IsLoaded())
            qWarning("RawDenoise: %s not found or failed to load at %s (raw denoise disabled)",
                     kModelFile, path.toUtf8().constData());
        else if (G::isLogger)
            G::log("RawDenoise::SharedSession loaded", session->BackendName());
    });
    return session.get();
}

bool Apply(WorkingImage &img, const EditParams &edit, int iso)
{
    if (edit.denoiseLuma <= 0.0f && edit.denoiseChroma <= 0.0f) return false;
    InferenceSession *s = SharedSession();
    if (!s || !s->IsLoaded()) return false;
    return DenoiseRgb(img, edit, iso, *s);
}

bool DenoiseRgb(WorkingImage &img, const EditParams &edit, int iso, InferenceSession &session)
{
    if (!img.isValid() || !session.IsLoaded()) return false;
    if (session.InputNames().size() < 2 || session.OutputNames().empty()) return false;

    const int W = img.width, H = img.height;
    const float white = img.white > 0.0f ? img.white : 1.0f;
    const float invWhite = 1.0f / white;
    const float lum  = std::clamp(edit.denoiseLuma,   0.0f, 1.0f);
    const float chr  = std::clamp(edit.denoiseChroma, 0.0f, 1.0f);
    const float cAmt = std::max(lum, chr);
    const float isoCond = (iso > 0 ? float(iso) : 100.0f) / 6400.0f;

    float *rgb = img.rgb.data();
    const size_t px = static_cast<size_t>(W) * H;

    /* Feathered accumulator for the denoised (Rec2020, normalised) image. */
    std::vector<float> acc(3 * px, 0.0f);
    std::vector<float> wsum(px, 0.0f);

    const std::string inImg  = session.InputNames()[0];   // "input"  [1,3,H,W]
    const std::string inCond = session.InputNames()[1];   // "cond"   [1,1]
    const std::string outNm  = session.OutputNames()[0];

    Tensor cond;
    cond.shape = {1, 1};
    cond.data  = {isoCond};

    const std::vector<int> xs = tileStarts(W);
    const std::vector<int> ys = tileStarts(H);

    for (int ty0 : ys) {
        const int th = std::min(kTile, H - ty0);
        for (int tx0 : xs) {
            const int tw = std::min(kTile, W - tx0);
            const size_t tp = static_cast<size_t>(th) * tw;

            /* Build the model input: sRGB-linear -> Rec2020, /white, clipped to [0,1], planar. */
            Tensor in;
            in.shape = {1, 3, th, tw};
            in.data.resize(3 * tp);
            for (int y = 0; y < th; ++y) {
                for (int x = 0; x < tw; ++x) {
                    const float *s = rgb + (static_cast<size_t>(ty0 + y) * W + (tx0 + x)) * 3;
                    float r2, g2, b2;
                    mat3(kSrgbToRec2020, s[0], s[1], s[2], r2, g2, b2);
                    const size_t o = static_cast<size_t>(y) * tw + x;
                    in.data[0 * tp + o] = std::clamp(r2 * invWhite, 0.0f, 1.0f);
                    in.data[1 * tp + o] = std::clamp(g2 * invWhite, 0.0f, 1.0f);
                    in.data[2 * tp + o] = std::clamp(b2 * invWhite, 0.0f, 1.0f);
                }
            }

            std::vector<Tensor> outs;
            if (!session.Run({inImg, inCond}, {in, cond}, {outNm}, outs) || outs.empty())
                return false;
            const Tensor &out = outs[0];
            if (out.data.size() != 3 * tp) return false;

            for (int y = 0; y < th; ++y) {
                const float wy = feather(y, th);
                for (int x = 0; x < tw; ++x) {
                    const float w = wy * feather(x, tw);
                    const size_t p = static_cast<size_t>(ty0 + y) * W + (tx0 + x);
                    const size_t o = static_cast<size_t>(y) * tw + x;
                    wsum[p] += w;
                    acc[0 * px + p] += w * out.data[0 * tp + o];
                    acc[1 * px + p] += w * out.data[1 * tp + o];
                    acc[2 * px + p] += w * out.data[2 * tp + o];
                }
            }
        }
    }

    /* Combine: highlight-preserving, amount-scaled correction, then Rec2020 -> sRGB. */
    for (size_t p = 0; p < px; ++p) {
        float *s = rgb + p * 3;
        float r2, g2, b2;
        mat3(kSrgbToRec2020, s[0], s[1], s[2], r2, g2, b2);          // scene rec2020
        const float nr = r2 * invWhite, ng = g2 * invWhite, nb = b2 * invWhite;   // normalised (unclipped)
        const float ir = std::clamp(nr, 0.0f, 1.0f);                 // what the model saw
        const float ig = std::clamp(ng, 0.0f, 1.0f);
        const float ib = std::clamp(nb, 0.0f, 1.0f);

        const float ws = wsum[p] > 0.0f ? 1.0f / wsum[p] : 0.0f;
        const float dr = acc[0 * px + p] * ws;                       // denoised (normalised)
        const float dg = acc[1 * px + p] * ws;
        const float db = acc[2 * px + p] * ws;

        const float Yin  = kLumaR*ir + kLumaG*ig + kLumaB*ib;
        const float Yden = kLumaR*dr + kLumaG*dg + kLumaB*db;
        const float dY   = Yden - Yin;                               // luma correction (scalar)

        /* per-channel chroma correction = (den - inC) - dY */
        const float ar = lum*dY + cAmt*((dr - ir) - dY);
        const float ag = lum*dY + cAmt*((dg - ig) - dY);
        const float ab = lum*dY + cAmt*((db - ib) - dY);

        const float or2 = (nr + ar) * white;                         // corrected rec2020
        const float og2 = (ng + ag) * white;
        const float ob2 = (nb + ab) * white;

        float R, Gc, B;
        mat3(kRec2020ToSrgb, or2, og2, ob2, R, Gc, B);
        s[0] = std::max(0.0f, R);
        s[1] = std::max(0.0f, Gc);
        s[2] = std::max(0.0f, B);
    }

    if (G::isLogger)
        G::log("RawDenoise::DenoiseRgb applied",
               QString("%1x%2 iso=%3 luma=%4 chroma=%5 via %6")
                   .arg(W).arg(H).arg(iso).arg(lum).arg(chr).arg(session.BackendName()));
    return true;
}

} // namespace RawDenoise
