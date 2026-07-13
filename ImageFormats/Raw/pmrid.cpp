#include "ImageFormats/Raw/pmrid.h"
#include "ImageFormats/Raw/pmrid_calib_tables.h"
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

/* Per-camera noise tables (var = k*x + b, white-normalised [0,1], green plane) live in
   pmrid_calib_tables.h; bring the names into this translation unit. */
using PMRID::IsoKB;
using PMRID::CalibTable;
using PMRID::kA7R5;
using PMRID::kCalibTables;

/* Interpolate (k,b) for iso from a table, log-log (k ~ ISO, geometric grid), clamped. */
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

/* Look up (k,b) for a KNOWN camera model (m.model is "Sony ILCE-...") by scanning the
   pmrid_calib_tables.h registry for the first substring match. Returns false and leaves
   k,b untouched for an unrecognised body, so resolveKB() can fall through to blind
   estimation rather than borrowing a mismatched sensor's curve. Add a body via a MEASURED
   sweep (pmrid_calibration_capture.md) or a DERIVED table (gen_pmrid_tables.py). */
bool modelKB(const QString &model, int iso, double &k, double &b)
{
    for (const CalibTable &t : kCalibTables) {
        if (model.contains(QLatin1String(t.modelMatch))) {
            lookupKB(t.rows, t.n, iso, k, b);
            return true;
        }
    }
    return false;
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

/* Blind Poisson-Gaussian noise estimation from the green CFA plane (tier 3 of resolveKB):
   fit var = k*x + b in the white-normalised [0,1] domain from flat image regions, so an
   uncalibrated body still denoises with its own noise model. Method: per block, measure
   (mean, variance) of the green sites; bin by intensity and keep the low variance
   percentile in each bin (texture only raises variance, so that floor is ~pure noise);
   least-squares fit the per-bin floors. Returns false if the frame is too flat/textured
   to fit reliably. One-shot pre-inference sweep of ~half the pixels -- negligible beside
   the net. */
bool estimateKB(const RawImage &raw, double &k, double &b)
{
    int dy, dx;
    if (!rggbOrigin(raw.pattern, dy, dx)) return false;

    const int W = raw.width;
    const float white = raw.white > 0 ? float(raw.white) : 1.0f;
    const float invWhite = 1.0f / white;

    constexpr int kBlk  = 16;    // packed-grid block (16 -> 32x32 mosaic)
    constexpr int kBins = 32;
    const int Wpk = (W - dx) / 2, Hpk = (raw.height - dy) / 2;
    if (Wpk < kBlk || Hpk < kBlk) return false;

    auto at = [&](int y, int x) -> uint16_t { return raw.cfa[size_t(y) * W + x]; };

    std::vector<std::vector<float>> bins(kBins);   // block variances per intensity bin
    for (int by = 0; by + kBlk <= Hpk; by += kBlk) {
        for (int bx = 0; bx + kBlk <= Wpk; bx += kBlk) {
            /* Mean/variance of G1 and G2 separately, then average, so the fixed G1/G2
               pedestal difference does not masquerade as noise. */
            double s1 = 0, s2 = 0, ss1 = 0, ss2 = 0;
            for (int y = 0; y < kBlk; ++y) {
                const int sy = dy + 2 * (by + y);
                for (int x = 0; x < kBlk; ++x) {
                    const int sx = dx + 2 * (bx + x);
                    const float g1 = at(sy,     sx + 1) * invWhite;   // G1
                    const float g2 = at(sy + 1, sx)     * invWhite;   // G2
                    s1 += g1; ss1 += double(g1) * g1;
                    s2 += g2; ss2 += double(g2) * g2;
                }
            }
            const int n = kBlk * kBlk;
            const double m1 = s1 / n, m2 = s2 / n;
            const double v1 = std::max(0.0, ss1 / n - m1 * m1);
            const double v2 = std::max(0.0, ss2 / n - m2 * m2);
            const double mean = 0.5 * (m1 + m2);
            if (mean <= 0.0 || mean >= 1.0) continue;   // skip black / clipped blocks
            int bi = int(mean * kBins);
            if (bi >= kBins) bi = kBins - 1;
            bins[bi].push_back(float(0.5 * (v1 + v2)));
        }
    }

    /* Per-bin noise floor = low percentile of block variances (the flat regions). */
    std::vector<double> xs, ys;
    for (int i = 0; i < kBins; ++i) {
        std::vector<float> &vv = bins[i];
        if (vv.size() < 8) continue;                // too few flat samples to trust
        std::sort(vv.begin(), vv.end());
        xs.push_back((i + 0.5) / kBins);
        ys.push_back(vv[size_t(vv.size() * 0.10)]); // 10th percentile
    }
    if (xs.size() < 4) return false;                // not enough intensity coverage

    /* Least-squares var = k*x + b over the per-bin floors. */
    const int n = int(xs.size());
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < n; ++i) {
        sx += xs[i]; sy += ys[i]; sxx += xs[i] * xs[i]; sxy += xs[i] * ys[i];
    }
    const double den = n * sxx - sx * sx;
    if (std::abs(den) < 1e-12) return false;
    const double kk = (n * sxy - sx * sy) / den;
    if (kk <= 0.0) return false;                    // shot noise must rise with signal
    k = kk;
    b = std::max(0.0, (sy - kk * sx) / n);
    return true;
}

/* Which tier of resolveKB() supplied the (k,b) noise model -- logged for diagnostics. */
enum class KBSource { DngNoiseProfile, ModelTable, BlindEstimate, GenericFallback };

const char *kbSourceName(KBSource s)
{
    switch (s) {
    case KBSource::DngNoiseProfile: return "DNG NoiseProfile";
    case KBSource::ModelTable:      return "model table";
    case KBSource::BlindEstimate:   return "blind estimate";
    case KBSource::GenericFallback: return "generic fallback";
    }
    return "?";
}

/* Resolve the (k,b) noise model for this frame via the priority chain:
   1) a DNG NoiseProfile baked into the file, 2) a measured per-model table, 3) blind
   per-image estimation from the CFA, 4) a generic full-frame table as a last-ditch guard
   (so we never denoise with nonsense coefficients when 1-3 all decline). */
KBSource resolveKB(const RawImage &raw, const QString &model, int iso, double &k, double &b)
{
    if (raw.hasNoiseProfile && raw.npScale > 0.0) {
        k = raw.npScale; b = raw.npOffset;
        return KBSource::DngNoiseProfile;
    }
    if (modelKB(model, iso, k, b))   return KBSource::ModelTable;
    if (estimateKB(raw, k, b))       return KBSource::BlindEstimate;
    lookupKB(kA7R5, int(sizeof(kA7R5) / sizeof(kA7R5[0])), iso, k, b);
    return KBSource::GenericFallback;
}

} // namespace

namespace PMRID {

/* Last resolution snapshot for the Develop diagnostics (see pmrid.h). */
static std::mutex g_resMutex;
static Resolution g_lastRes;

Resolution LastResolution()
{
    std::lock_guard<std::mutex> lock(g_resMutex);
    return g_lastRes;
}

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
    const KBSource kbSrc = resolveKB(raw, model, iso > 0 ? iso : 100, k, b);
    const Cvt cvt = cvtCoeffs(k, b);
    {
        std::lock_guard<std::mutex> lock(g_resMutex);
        g_lastRes = { true, model, iso, QString::fromLatin1(kbSourceName(kbSrc)),
                      k, b, raw.hasNoiseProfile };
    }

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
        G::log("PMRID::Apply", QString("%1x%2 iso=%3 via %4 kb=[%5,%6] (%7)")
                                   .arg(W).arg(H).arg(iso).arg(s->BackendName())
                                   .arg(k, 0, 'g', 4).arg(b, 0, 'g', 4)
                                   .arg(kbSourceName(kbSrc)));
    return true;
}

} // namespace PMRID
