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

/* Per-camera noise calibration: var = k*iso-dependent*x + b in the white-normalised [0,1] domain
   (green plane), measured by a photon-transfer sweep (tools/calibrate_pmrid.py -> _pmrid_out/
   calib_<model>.json). k = shot-noise gain (~doubles per ISO stop), b = read-noise floor. */
struct IsoKB { int iso; double k; double b; };

constexpr IsoKB kA7R5[] = {   // Sony ILCE-7RM5
    {   100, 2.679249e-05, 1.209511e-09 }, {   200, 5.324481e-05, 2.094189e-08 },
    {   400, 1.070231e-04, 2.446033e-08 }, {   800, 2.140609e-04, 9.039847e-08 },
    {  1600, 4.277943e-04, 3.610540e-07 }, {  3200, 8.555372e-04, 1.386220e-06 },
    {  6400, 1.710885e-03, 5.153719e-06 }, { 12800, 3.379437e-03, 2.121978e-05 },
    { 25600, 4.760316e-03, 3.789355e-05 },
};
constexpr IsoKB kA1M2[] = {   // Sony ILCE-1M2 (A1 II)
    {   100, 2.262302e-05, 5.375863e-09 }, {   200, 4.564027e-05, 3.518424e-08 },
    {   400, 9.117619e-05, 1.471515e-07 }, {   800, 1.807107e-04, 3.878459e-08 },
    {  1600, 3.619991e-04, 1.659519e-07 }, {  3200, 7.263821e-04, 6.005952e-07 },
    {  6400, 1.442054e-03, 2.510492e-06 }, { 12800, 2.881715e-03, 1.002150e-05 },
    { 25600, 3.973885e-03, 2.307054e-05 },
};

/* Interpolate (k,b) for iso from a table, log-log (k ~ ISO, geometric ISO grid), clamped. */
void lookupKB(const IsoKB *t, int n, int iso, double &k, double &b)
{
    if (iso <= t[0].iso)     { k = t[0].k;     b = t[0].b;     return; }
    if (iso >= t[n - 1].iso) { k = t[n - 1].k; b = t[n - 1].b; return; }
    for (int i = 1; i < n; ++i) {
        if (iso <= t[i].iso) {
            const double f = (std::log(double(iso))     - std::log(double(t[i - 1].iso)))
                           / (std::log(double(t[i].iso)) - std::log(double(t[i - 1].iso)));
            auto lerpLog = [f](double a, double c) {
                return std::exp(std::log(std::max(a, 1e-12)) * (1.0 - f) +
                                std::log(std::max(c, 1e-12)) * f);
            };
            k = lerpLog(t[i - 1].k, t[i].k);
            b = lerpLog(t[i - 1].b, t[i].b);
            return;
        }
    }
}

/* Pick the calibration table by camera model (m.model is "Sony ILCE-..."); default to the A7R5
   (a generic full-frame Sony) for uncalibrated bodies -- better than no denoise, and any camera
   can be added by capturing its frames (tools/pmrid_calibration_capture.md). */
void modelKB(const QString &model, int iso, double &k, double &b)
{
    if (model.contains("ILCE-1M2")) lookupKB(kA1M2, int(sizeof(kA1M2) / sizeof(kA1M2[0])), iso, k, b);
    else                            lookupKB(kA7R5, int(sizeof(kA7R5) / sizeof(kA7R5[0])), iso, k, b);
}

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

bool Apply(RawImage &raw, int iso, const QString &model, const ProgressFn &progress)
{
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
    double k, b;
    modelKB(model, iso > 0 ? iso : 100, k, b);            // per-camera, per-ISO noise model
    const Cvt cvt = cvtCoeffs(k, b);

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

    /* Feathered accumulator for the denoised packed image. */
    std::vector<float> acc(4 * pk, 0.0f);
    std::vector<float> wsum(pk, 0.0f);

    const std::vector<int> xs = tileStarts(Wpk);
    const std::vector<int> ys = tileStarts(Hpk);
    const int tileTotal = int(xs.size() * ys.size());
    int tileDone = 0;
    if (progress) progress(0, tileTotal);

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
            if (progress) progress(++tileDone, tileTotal);
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


    if (G::isLogger)
        G::log("PMRID::Apply", QString("%1x%2 iso=%3 via %4")
                                   .arg(W).arg(H).arg(iso).arg(s->BackendName()));
    return true;
}

} // namespace PMRID
