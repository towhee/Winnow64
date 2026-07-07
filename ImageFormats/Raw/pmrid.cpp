#include "ImageFormats/Raw/pmrid.h"
#include "ImageFormats/Raw/rawimage.h"
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
    See pmrid.h for the model contract. This ports tools/oracle_pmrid.py 1:1:
      - map the CFA to RGGB phase, pack to 4-ch [R,G1,G2,B] normalised by the white level,
      - KSigma linear noise-normalization to the OPPO anchor ISO (the level the net
        trained on),
      - x256 -> net (residual) -> /256 -> inverse KSigma, tiled with a feathered blend,
      - unpack back into the mosaic.
*/

namespace {

constexpr int   kTile      = 512;     // packed-grid tile (px); 512 is a multiple of 32
constexpr int   kOverlap   = 32;
constexpr int   kPadMult   = 32;      // net downsamples 4x2 -> H,W must be multiples of 32
constexpr float kV         = 959.0f;  // OPPO signal scale the KSigma operates in
constexpr float kInpScale  = 256.0f;  // net input scaling
constexpr double kAnchor   = 1600.0;  // OPPO anchor ISO (KSigma target)
const     char *kModelFile = "pmrid.onnx";

/* OPPO calibration polynomials from PMRID run_benchmark.py (numpy poly1d order: highest first). */
constexpr double kK[2] = {0.0005995267, 0.00868861};                 // K(iso)   = kK0*iso + kK1
constexpr double kB[3] = {7.11772e-7, 6.514934e-4, 0.11492713};      // Sigma(iso)= ...*iso^2 + ...

/* Single-frame Sony noise estimate (var = k*x + b, [0,1] domain), from the ISO-6400 spike.
   STOPGAP until per-model K(ISO)/B(ISO) come from the calibration capture. */
constexpr double kSonyK = 1.052e-3;
constexpr double kSonyB = 1.197e-6;

/* KSigma affine coefficients: map the measured (k,b) noise onto the OPPO anchor's statistics. */
struct Cvt { float k; float b; };

Cvt cvtCoeffs(double k, double b)
{
    const double k_s   = kV * k;                 // express on the OPPO signal scale
    const double sig_s = double(kV) * kV * b;
    const double k_a   = kK[0] * kAnchor + kK[1];
    const double sig_a = kB[0] * kAnchor * kAnchor + kB[1] * kAnchor + kB[2];
    const double cvt_k = k_a / k_s;
    const double cvt_b = (sig_s / (k_s * k_s) - sig_a / (k_a * k_a)) * k_a;
    return { float(cvt_k), float(cvt_b) };
}

/* Origin offset (dy,dx) that makes the 2x2 tiling start on an R site (RGGB phase). */
bool rggbOrigin(CfaPattern p, int &dy, int &dx)
{
    switch (p) {
    case CfaPattern::RGGB: dy = 0; dx = 0; return true;
    case CfaPattern::GRBG: dy = 0; dx = 1; return true;
    case CfaPattern::GBRG: dy = 1; dx = 0; return true;
    case CfaPattern::BGGR: dy = 1; dx = 1; return true;
    default:               return false;   // XTrans / Unknown: not a Bayer phase we handle
    }
}

inline float feather(int t, int len)
{
    const int edge = std::min(t + 1, len - t);
    return std::min(1.0f, static_cast<float>(edge) / static_cast<float>(kOverlap + 1));
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

} // namespace

namespace PMRID {

static InferenceSession *SharedSession()
{
    static std::once_flag once;
    static std::unique_ptr<InferenceSession> session;
    std::call_once(once, [] {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(kModelFile);
        /* CoreML/ANE is safe for PMRID (the graph partitions cleanly -- unlike TreeNet), so let
           the backend pick the best device. */
        session = std::make_unique<InferenceSession>(path, InferenceDevice::Auto);
        if (!session->IsLoaded())
            qWarning("PMRID: %s not found or failed to load at %s (raw denoise disabled)",
                     kModelFile, path.toUtf8().constData());
        else if (G::isLogger)
            G::log("PMRID::SharedSession loaded", session->BackendName());
    });
    return session.get();
}

bool Apply(RawImage &raw, int iso)
{
    Q_UNUSED(iso)   // reserved for the future ISO-dependent coefficient table
    if (!raw.isValid()) return false;

    int dy, dx;
    if (!rggbOrigin(raw.pattern, dy, dx)) return false;   // non-Bayer: leave untouched

    InferenceSession *s = SharedSession();
    if (!s || !s->IsLoaded()) return false;
    if (s->InputNames().empty() || s->OutputNames().empty()) return false;

    const int W = raw.width, H = raw.height;
    const int Wpk = (W - dx) / 2, Hpk = (H - dy) / 2;      // packed (half-res) dims
    if (Wpk <= 0 || Hpk <= 0) return false;
    const float white = raw.white > 0 ? float(raw.white) : 1.0f;
    const float invWhite = 1.0f / white;
    const Cvt cvt = cvtCoeffs(kSonyK, kSonyB);

    const std::string inName  = s->InputNames()[0];
    const std::string outName = s->OutputNames()[0];

    /* Pack cfa -> planar RGGB [4][Hpk*Wpk], normalised to [0,1]. */
    const size_t pk = static_cast<size_t>(Hpk) * Wpk;
    std::vector<float> rggb(4 * pk);
    auto at = [&](int y, int x) -> uint16_t {
        return raw.cfa[static_cast<size_t>(y) * W + x];
    };
    for (int y = 0; y < Hpk; ++y) {
        for (int x = 0; x < Wpk; ++x) {
            const int sy = dy + 2 * y, sx = dx + 2 * x;
            const size_t o = static_cast<size_t>(y) * Wpk + x;
            rggb[0 * pk + o] = at(sy,     sx)     * invWhite;   // R
            rggb[1 * pk + o] = at(sy,     sx + 1) * invWhite;   // G1
            rggb[2 * pk + o] = at(sy + 1, sx)     * invWhite;   // G2
            rggb[3 * pk + o] = at(sy + 1, sx + 1) * invWhite;   // B
        }
    }

    /* TEMP (raw-denoise test signal -- remove after testing): green-channel mean/std BEFORE. */
    double dbgBSum = 0.0, dbgBSumSq = 0.0;
    for (int c = 1; c <= 2; ++c)
        for (size_t p = 0; p < pk; ++p) {
            const float v = rggb[c * pk + p];
            dbgBSum += v; dbgBSumSq += double(v) * v;
        }
    const double dbgN = 2.0 * double(pk);
    const double dbgBMean = dbgBSum / dbgN;
    const double dbgBStd = std::sqrt(std::max(0.0, dbgBSumSq / dbgN - dbgBMean * dbgBMean));

    /* Feathered accumulator for the denoised packed image. */
    std::vector<float> acc(4 * pk, 0.0f);
    std::vector<float> wsum(pk, 0.0f);

    const std::vector<int> xs = tileStarts(Wpk);
    const std::vector<int> ys = tileStarts(Hpk);

    for (int ty0 : ys) {
        const int th = std::min(kTile, Hpk - ty0);
        const int thp = ((th + kPadMult - 1) / kPadMult) * kPadMult;   // pad to mult of 32
        for (int tx0 : xs) {
            const int tw = std::min(kTile, Wpk - tx0);
            const int twp = ((tw + kPadMult - 1) / kPadMult) * kPadMult;
            const size_t tpp = static_cast<size_t>(thp) * twp;

            /* Build padded, VST-transformed, x256 input tile (edge-replicated padding). */
            Tensor in;
            in.shape = {1, 4, thp, twp};
            in.data.assign(4 * tpp, 0.0f);
            for (int c = 0; c < 4; ++c) {
                const float *src = rggb.data() + c * pk;
                for (int y = 0; y < thp; ++y) {
                    const int sy = std::min(y, th - 1);
                    for (int x = 0; x < twp; ++x) {
                        const int sx = std::min(x, tw - 1);
                        const float p = src[static_cast<size_t>(ty0 + sy) * Wpk + (tx0 + sx)];
                        const float v = (p * kV * cvt.k + cvt.b) / kV;      // KSigma forward
                        in.data[c * tpp + static_cast<size_t>(y) * twp + x] = v * kInpScale;
                    }
                }
            }

            std::vector<Tensor> outs;
            if (!s->Run({inName}, {in}, {outName}, outs) || outs.empty()) return false;
            const Tensor &out = outs[0];
            if (out.data.size() != 4 * tpp) return false;

            for (int y = 0; y < th; ++y) {
                const float wy = feather(y, th);
                for (int x = 0; x < tw; ++x) {
                    const float w = wy * feather(x, tw);
                    const size_t p = static_cast<size_t>(ty0 + y) * Wpk + (tx0 + x);
                    wsum[p] += w;
                    for (int c = 0; c < 4; ++c) {
                        const float o = out.data[c * tpp + static_cast<size_t>(y) * twp + x] / kInpScale;
                        const float d = (o * kV - cvt.b) / cvt.k / kV;      // inverse KSigma
                        acc[c * pk + p] += w * d;
                    }
                }
            }
        }
    }

    /* Unpack the denoised packed image back into the mosaic (RGGB-phase region). */
    for (int y = 0; y < Hpk; ++y) {
        for (int x = 0; x < Wpk; ++x) {
            const int sy = dy + 2 * y, sx = dx + 2 * x;
            const size_t o = static_cast<size_t>(y) * Wpk + x;
            const float ws = wsum[o] > 0.0f ? 1.0f / wsum[o] : 0.0f;
            auto put = [&](int py, int px, float norm) {
                const float v = std::clamp(norm * ws * white, 0.0f, white);
                raw.cfa[static_cast<size_t>(py) * W + px] = static_cast<uint16_t>(std::lround(v));
            };
            put(sy,     sx,     acc[0 * pk + o]);
            put(sy,     sx + 1, acc[1 * pk + o]);
            put(sy + 1, sx,     acc[2 * pk + o]);
            put(sy + 1, sx + 1, acc[3 * pk + o]);
        }
    }

    /* TEMP (raw-denoise test signal -- remove after testing): green-channel mean/std AFTER +
       backend. std drops with noise (also with any detail smoothing), so the % is a rough
       noise-reduction indicator, not exact. */
    double dbgASum = 0.0, dbgASumSq = 0.0;
    for (size_t p = 0; p < pk; ++p) {
        const float ws = wsum[p] > 0.0f ? 1.0f / wsum[p] : 0.0f;
        for (int c = 1; c <= 2; ++c) {
            const float v = acc[c * pk + p] * ws;
            dbgASum += v; dbgASumSq += double(v) * v;
        }
    }
    const double dbgAMean = dbgASum / dbgN;
    const double dbgAStd = std::sqrt(std::max(0.0, dbgASumSq / dbgN - dbgAMean * dbgAMean));
    qDebug("PMRID::Apply [TEST] %dx%d iso=%d via %s  green mean %.5f->%.5f  std %.5f->%.5f (~%.0f%% less)",
           W, H, iso, s->BackendName().toUtf8().constData(),
           dbgBMean, dbgAMean, dbgBStd, dbgAStd,
           100.0 * (1.0 - (dbgBStd > 0.0 ? dbgAStd / dbgBStd : 0.0)));

    if (G::isLogger)
        G::log("PMRID::Apply", QString("%1x%2 iso=%3 via %4")
                                   .arg(W).arg(H).arg(iso).arg(s->BackendName()));
    return true;
}

} // namespace PMRID
