#include "Utilities/inference/lamafill.h"
#include "Utilities/inference/inferencesession.h"
#include "Develop/fillspot.h"
#include "Main/global.h"

#include <QDir>
#include <QCoreApplication>
#include <QDebug>                     // TEMP: A/B mode qDebug tracing
#include <QHash>
#include <QPoint>
#include <climits>
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>          // cv::seamlessClone (Poisson)
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/*
    See lamafill.h for the contract. LaMa packing (verified against the ONNX by probe):
      inputs  image: 1x3x512x512 RGB in [0,1];  mask: 1x1x512x512, 1 = region to inpaint
      output       : 1x3x512x512 RGB in [0,255]
    The hole (spot) is dilated a few px for the model (hard edges seam), while the
    ORIGINAL soft brush alpha composites the patch back, so the seam is feathered by the
    brush's feather -- identical strategy to MiganFill. Runs on the GPU
    (InferenceDevice::GPU) because the ANE cannot service LaMa's FFC (project_lama_coreml_gpu).
*/

namespace {

const char *kModelFile = "lama.onnx";
constexpr int kNet = 512;              // LaMa fixed square input

/*
    Engine-derived composite feather (image px). The brush paints a HARD area
    (fillspot.h rasterizes exactly what was brushed); the transition band that makes a
    heal blend seamlessly is the ENGINE's concern, sized from the stroke THICKNESS (max
    inscribed radius of the hole), NOT its bbox -- a thin diagonal stroke has a huge
    bbox, and a bbox-sized feather smeared content far outside the brushed area
    (lab-measured on the branch test). hole8 = hard hole mask, 8U, nonzero inside.
*/
double featherFromHole(const cv::Mat &hole8)
{
    cv::Mat dt;
    cv::distanceTransform(hole8, dt, cv::DIST_L2, 3);
    double maxR = 0.0;
    cv::minMaxLoc(dt, nullptr, &maxR);
    /* The 32px cap matters for WIDE fills: a 12px ramp on a 150px-wide fill left a
       visible coverage edge (stalk test); thin strokes stay tight via the 0.5x. */
    return std::clamp(0.5 * maxR, 2.0, 32.0);
}

/*
    A large/long painted region heals PIECEWISE: one source offset for a whole stroke
    leaves a stroke-shaped ghost when the background structure doesn't repeat at a
    single shift, so the coverage is split into brush-scale chunks and each chunk
    clones from its own LOCAL source. A ChunkWin windows the chunk's share of the
    coverage: weight 1 on the core cell, a linear ramp to 0 over `ramp` px beyond it.
    Cores tile the spot bbox, so every covered pixel is at full weight in exactly one
    chunk and the ramps blend neighbouring chunks' (different) sources into each other.
*/
struct ChunkWin {
    int cx0 = 0, cy0 = 0, cx1 = -1, cy1 = -1;   // core cell, full-res image px
    double ramp = 0.0;
    bool active = false;
};

double winWeight(const ChunkWin &w, int x, int y)
{
    if (!w.active || w.ramp <= 0.0) return 1.0;
    const double wx = std::clamp(std::min((x - (w.cx0 - w.ramp)) / w.ramp,
                                          ((w.cx1 + w.ramp) - x) / w.ramp), 0.0, 1.0);
    const double wy = std::clamp(std::min((y - (w.cy0 - w.ramp)) / w.ramp,
                                          ((w.cy1 + w.ramp) - y) / w.ramp), 0.0, 1.0);
    return wx * wy;
}

/*
    Pinned source offsets. The template match runs on DEVELOPED pixels, so re-matching
    every render could flip the chosen source mid-slider-drag (heal-content shimmer) --
    the first successful match per (image, render size, spot, chunk) is pinned and
    reused: the blend still re-runs on the current pixels, so exposure/colour edits
    track, but the heal GEOMETRY is stable. Also removes the per-render match cost.
    "No source found" is pinned too (kNoSource) so failing chunks don't re-search every
    render. Mutex-guarded: full-res renders run off the GUI thread.
*/
std::mutex gOffsetMutex;
QHash<QString, QPoint> gOffsetCache;
const QPoint kNoSource(INT_MIN, INT_MIN);

bool lookupPinnedOffset(const QString &key, cv::Point &d, bool &noSource)
{
    if (key.isEmpty()) return false;
    std::lock_guard<std::mutex> lock(gOffsetMutex);
    const auto it = gOffsetCache.constFind(key);
    if (it == gOffsetCache.constEnd()) return false;
    noSource = (*it == kNoSource);
    if (!noSource) d = cv::Point(it->x(), it->y());
    return true;
}

void pinOffset(const QString &key, const QPoint &d)
{
    if (key.isEmpty()) return;
    std::lock_guard<std::mutex> lock(gOffsetMutex);
    if (gOffsetCache.size() > 4096) gOffsetCache.clear();   // simple bound
    gOffsetCache.insert(key, d);
}

InferenceSession *SharedSession()
{
    static std::once_flag once;
    static std::unique_ptr<InferenceSession> session;
    std::call_once(once, [] {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(kModelFile);
        session = std::make_unique<InferenceSession>(path, InferenceDevice::GPU);
        if (!session->IsLoaded())
            qWarning("LamaFill: %s not found or failed to load at %s (LaMa spot fill disabled)",
                     kModelFile, path.toUtf8().constData());
        else if (G::isLogger)
            G::log("LamaFill::SharedSession loaded", session->BackendName());
    });
    return session.get();
}

/*
    Reduce the low-frequency luminance/colour mismatch a large fill leaves against a
    smooth surround (the visible "patch" artifact). All mats are net-res: `model` and
    `orig` are 32FC3 in [0,255]; `hard` is 32F, 1.0 inside the hole. Returns the corrected
    fill to use INSIDE the hole. mode 0 = none (raw model); 1 = Poisson (cv::seamlessClone
    matches boundary gradients); 2 = low-frequency transfer (INERT for this lama.onnx:
    the export composites the input over the keep region in-graph, so orig == model
    outside the hole and the membrane of their difference is identically zero); 3 = pure
    harmonic (push-pull surround only, discard the model -- seamless on smooth surfaces).
    These corrections apply only on the MODEL path -- Object-kind spots and clone
    fallbacks (see apply). The exemplar clone heal (cloneHealOne) bypasses the model and
    needs none of them. See G::spotFillCorrectMode (B cycles it, A/B debug only).
*/
/*
    Multiscale "push-pull" (Gortler et al.): smoothly extrapolate `img` (3ch 32F) into the
    region where weight `w` (1ch 32F) is ~0, using a Gaussian pyramid so even a LARGE hole
    is filled by a smooth harmonic-like membrane (a single Gaussian can't reach a big
    hole's centre). Returns a dense 3ch field == orig where w==1, smooth in the hole.
*/
cv::Mat pushPull(const cv::Mat &img, const cv::Mat &w)
{
    std::vector<cv::Mat> S, W;
    {
        std::vector<cv::Mat> ch; cv::split(img, ch);
        for (auto &c : ch) c = c.mul(w);                 // premultiply by weight
        cv::Mat s0; cv::merge(ch, s0);
        S.push_back(s0);
        W.push_back(w.clone());
    }
    while (S.back().rows > 2 && S.back().cols > 2) {      // pull: build the pyramid
        cv::Mat sd, wd;
        cv::pyrDown(S.back(), sd);
        cv::pyrDown(W.back(), wd);
        S.push_back(sd);
        W.push_back(wd);
    }
    cv::Mat up;                                          // push: coarse -> fine
    for (int i = int(S.size()) - 1; i >= 0; --i) {
        std::vector<cv::Mat> sc; cv::split(S[i], sc);
        cv::Mat denom = W[i] + 1e-6f;
        for (auto &c : sc) cv::divide(c, denom, c);       // normalized value this level
        cv::Mat norm; cv::merge(sc, norm);
        cv::Mat conf; cv::min(W[i], 1.0, conf);           // 0..1 confidence
        if (up.empty()) { up = norm; continue; }
        cv::Mat up2; cv::resize(up, up2, norm.size());
        cv::Mat inv = 1.0f - conf;
        std::vector<cv::Mat> nc, uc, oc(3);
        cv::split(norm, nc); cv::split(up2, uc);
        for (int k = 0; k < 3; ++k)            // norm if confident, else coarse
            oc[k] = nc[k].mul(conf) + uc[k].mul(inv);
        cv::merge(oc, up);
    }
    return up;
}

cv::Mat applyFillCorrection(const cv::Mat &model, const cv::Mat &orig, const cv::Mat &hard,
                            int mode)
{
    if (mode == 1) {                                     // Poisson / seamless clone
        cv::Mat src8, dst8, m8;
        model.convertTo(src8, CV_8UC3);
        orig.convertTo(dst8, CV_8UC3);
        hard.convertTo(m8, CV_8U, 255.0);
        cv::Mat mask; cv::threshold(m8, mask, 127, 255, cv::THRESH_BINARY);
        const cv::Rect bb = cv::boundingRect(mask);
        /* seamlessClone needs a non-empty mask strictly inside the frame; bail to the raw
           model (no-op) if the hole is empty or touches an edge (Poisson unstable). */
        if (bb.area() <= 0 || bb.x <= 1 || bb.y <= 1 ||
            bb.br().x >= model.cols - 1 || bb.br().y >= model.rows - 1) {
            qDebug() << "[A/B] mode1 Poisson -> RAW fallback (empty/edge)";  // TEMP
            return model;
        }
        const cv::Point center(bb.x + bb.width / 2, bb.y + bb.height / 2);
        cv::Mat out8;
        try { cv::seamlessClone(src8, dst8, mask, center, out8, cv::NORMAL_CLONE); }
        catch (const cv::Exception &e) {
            qDebug() << "[A/B] mode1 Poisson -> RAW fallback (exc)" << e.what();  // TEMP
            return model;
        }
        qDebug() << "[A/B] mode1 Poisson seamlessClone EXECUTED";  // TEMP
        cv::Mat out32; out8.convertTo(out32, CV_32FC3);
        return out32;
    }
    if (mode == 2 || mode == 3) {
        /* Reconstruct the smooth surround across the (possibly large) hole with push-pull
           -- a true harmonic membrane, unlike the single-Gaussian normalized convolution
           that collapsed to a flat average deep in a big hole (the residual blob). */
        cv::Mat keep = 1.0f - hard;                      // 32F, 1 outside the hole
        if (mode == 3) {                                 // PURE harmonic fill (no model)
            qDebug() << "[A/B] mode3 harmonic (push-pull, no model) EXECUTED";  // TEMP
            return pushPull(orig, keep);                 // seamless on a smooth surface
        }
        /* mode 2: keep the model's texture, take the surround's level -- as a DOUBLE
           membrane: corrected = model - pushPull(model) + pushPull(orig), folded into
           one call via linearity: model + pushPull(orig - model). The earlier Gaussian
           highpass (sigma net/8) let LaMa's mid-frequency dome through (measured +5
           luma warm bump at the heal centre); the model's own membrane is boundary-
           aware, so everything the membrane can represent comes from the surround and
           only boundary-consistent model texture survives, per channel. */
        cv::Mat out32 = model + pushPull(orig - model, keep);
        qDebug() << "[A/B] mode2 membrane transfer (push-pull) EXECUTED";  // TEMP
        return out32;
    }
    qDebug() << "[A/B] mode0 none (raw model)";  // TEMP
    return model;                                        // mode 0: raw model
}

/*
    Match the healed region's grain to the surround. Sensor noise makes a smooth fill read
    as a plastic patch even at a perfect luminance match. Synthetic RNG noise measured
    WRONG against the real grain (white, lag-1 autocorrelation ~0.05 vs the real ~0.38;
    channel-independent, cross-channel corr ~0.04 vs the real ~0.92 -> chroma confetti
    that also attenuates more than real grain under display resampling), so the grain is
    QUILTED from the tile itself: random high-frequency residual blocks from the kept
    surround are overlap-blended across the hole -- copied real grain carries sigma,
    spatial correlation, channel correlation and fine mottle for free. The amplitude tops
    the fill's existing grain up to the surround's in quadrature, per channel. Falls back
    to channel- and spatially-correlated synthetic noise when the surround has too few
    clean source blocks. Deterministic seed so proxy/full-res re-renders don't shimmer.
    Modifies `fill` (32FC3, tile res) in place; the alpha composite confines the hole.
*/
void applyGrainMatch(cv::Mat &fill, const cv::Mat &orig, const cv::Mat &hard)
{
    const int t = fill.rows;                             // square tile
    cv::Mat keepMask, holeMask, covMask;
    cv::compare(hard, 0.05, keepMask, cv::CMP_LT);       // clean surround (grain source)
    cv::compare(hard, 0.5,  holeMask, cv::CMP_GT);       // hole core (stats target)
    cv::compare(hard, 0.01, covMask,  cv::CMP_GT);       // full brush (grain coverage)
    if (cv::countNonZero(keepMask) < 64 || cv::countNonZero(holeMask) < 1) return;

    /* High-frequency residuals (grain + fine mottle) of the surround and the fill. */
    const double sigmaCut = 4.0;
    cv::Mat origLow, fillLow;
    cv::GaussianBlur(orig, origLow, cv::Size(), sigmaCut);
    cv::GaussianBlur(fill, fillLow, cv::Size(), sigmaCut);
    const cv::Mat res = orig - origLow;                  // quilting source
    const cv::Mat fres = fill - fillLow;

    /* Per-channel amplitude: top the fill's grain up to the surround's in quadrature. */
    std::vector<cv::Mat> rc, fc;
    cv::split(res, rc);
    cv::split(fres, fc);
    double amp[3];
    bool need = false;
    for (int c = 0; c < 3; ++c) {
        cv::Scalar m, sSur, sFill;
        cv::meanStdDev(rc[c], m, sSur,  keepMask);
        cv::meanStdDev(fc[c], m, sFill, holeMask);
        amp[c] = std::sqrt(std::max(0.0, sSur[0]*sSur[0] - sFill[0]*sFill[0]));
        if (amp[c] > 1e-3) need = true;
    }
    if (!need) return;

    cv::RNG rng(0x5EED5EED);                             // fixed -> stable re-renders

    /* Source candidates: BxB residual blocks fully inside the kept surround (O(1) block
       test via an integral image), screened to near-median energy so real structure
       (edges, objects in the tile) is never copied into the hole as grain. */
    cv::Mat keep01, integ;
    keepMask.convertTo(keep01, CV_8U, 1.0 / 255.0);
    cv::integral(keep01, integ, CV_32S);
    auto blockKept = [&](int x, int y, int B) {
        const int sum = integ.at<int>(y + B, x + B) - integ.at<int>(y, x + B)
                      - integ.at<int>(y + B, x) + integ.at<int>(y, x);
        return sum == B * B;
    };
    const cv::Mat resGray = (rc[0] + rc[1] + rc[2]) / 3.0f;

    int B = 0;
    std::vector<cv::Point> cand;
    for (int tryB : {64, 32, 16}) {
        if (tryB * 2 > t) continue;
        cand.clear();
        for (int y = 0; y + tryB <= t; y += tryB / 2)
            for (int x = 0; x + tryB <= t; x += tryB / 2)
                if (blockKept(x, y, tryB)) cand.emplace_back(x, y);
        if (int(cand.size()) >= 8) { B = tryB; break; }
    }
    if (B > 0) {
        std::vector<double> rms(cand.size());
        for (size_t i = 0; i < cand.size(); ++i) {
            cv::Scalar m, s;
            cv::meanStdDev(resGray(cv::Rect(cand[i].x, cand[i].y, B, B)), m, s);
            rms[i] = s[0];
        }
        std::vector<double> sorted = rms;
        std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
        const double med = sorted[sorted.size() / 2];
        std::vector<cv::Point> screened;
        for (size_t i = 0; i < cand.size(); ++i)
            if (rms[i] > 0.4 * med && rms[i] < 2.5 * med) screened.push_back(cand[i]);
        if (int(screened.size()) >= 4) cand = screened;  // else keep the unscreened set
    }

    cv::Mat grainCh[3];
    if (B > 0) {
        /* Overlap-add quilting over the brush's bounding box: each grid cell takes a
           random source block (random flips break visible repetition), windowed by a
           linear ramp over the overlap margin; accumulate value*w and w, then divide. */
        cv::Rect bb = cv::boundingRect(covMask);
        const int O = B / 4, stride = B - O;
        bb.x = std::max(0, bb.x - O);
        bb.y = std::max(0, bb.y - O);
        bb.width  = std::min(t - bb.x, bb.width  + 2 * O);
        bb.height = std::min(t - bb.y, bb.height + 2 * O);

        cv::Mat win(B, B, CV_32F);
        for (int y = 0; y < B; ++y) {
            const float wy = std::min({1.0f, (y + 1) / float(O + 1), (B - y) / float(O + 1)});
            float *w = win.ptr<float>(y);
            for (int x = 0; x < B; ++x) {
                const float wx = std::min({1.0f, (x + 1) / float(O + 1), (B - x) / float(O + 1)});
                w[x] = wx * wy;
            }
        }
        for (int c = 0; c < 3; ++c) grainCh[c] = cv::Mat::zeros(t, t, CV_32F);
        cv::Mat wsum = cv::Mat::zeros(t, t, CV_32F);
        for (int y = bb.y; y < bb.y + bb.height; y += stride) {
            for (int x = bb.x; x < bb.x + bb.width; x += stride) {
                const int bw = std::min(B, t - x), bh = std::min(B, t - y);
                if (bw <= 0 || bh <= 0) continue;
                const cv::Point &s = cand[rng.uniform(0, int(cand.size()))];
                cv::Mat blk = res(cv::Rect(s.x, s.y, B, B)).clone();
                const int f = rng.uniform(0, 4);         // 0 none, 1 x, 2 y, 3 both
                if (f) cv::flip(blk, blk, f == 1 ? 1 : (f == 2 ? 0 : -1));
                std::vector<cv::Mat> bc;
                cv::split(blk, bc);
                const cv::Rect dstR(x, y, bw, bh), srcR(0, 0, bw, bh);
                const cv::Mat winR = win(srcR);
                for (int c = 0; c < 3; ++c)
                    grainCh[c](dstR) += bc[c](srcR).mul(winR);
                wsum(dstR) += winR;
            }
        }
        const cv::Mat denom = wsum + 1e-6f;
        for (int c = 0; c < 3; ++c) cv::divide(grainCh[c], denom, grainCh[c]);
        qDebug() << "[A/B] grain QUILT B=" << B << "cand=" << int(cand.size());  // TEMP
    } else {
        /* Too little clean surround to quilt (tiny tile or busy keep region): synthetic
           noise shaped like the measured grain -- one shared luma field (channel corr
           ~0.95) blurred to approximate the ~0.4 lag-1 autocorrelation, plus a small
           independent per-channel component, each normalized to unit sigma. */
        auto field = [&](cv::Mat &m) {
            m.create(t, t, CV_32F);
            rng.fill(m, cv::RNG::NORMAL, 0.0, 1.0);
            cv::GaussianBlur(m, m, cv::Size(), 0.7);
            cv::Scalar mu, sd;
            cv::meanStdDev(m, mu, sd);
            m = (m - mu[0]) / std::max(sd[0], 1e-6);
        };
        cv::Mat luma;
        field(luma);
        const float rho = 0.95f;
        for (int c = 0; c < 3; ++c) {
            cv::Mat ind;
            field(ind);
            grainCh[c] = luma * rho + ind * std::sqrt(1.0f - rho * rho);
        }
        qDebug() << "[A/B] grain match SYNTH fallback";  // TEMP
    }

    /* Normalize each channel's grain to the quadrature top-up amplitude (measured over
       the hole core, mean-subtracted so the level correction is untouched) and add. */
    std::vector<cv::Mat> out;
    cv::split(fill, out);
    for (int c = 0; c < 3; ++c) {
        if (amp[c] < 1e-3) continue;
        cv::Scalar m, s;
        cv::meanStdDev(grainCh[c], m, s, holeMask);
        if (s[0] < 1e-6) continue;
        out[c] += (grainCh[c] - m[0]) * float(amp[c] / s[0]);
    }
    cv::merge(out, fill);
}

/*
    Exemplar "clone heal" (Lightroom heal-brush style): find the best-matching SOURCE
    patch near the spot with a masked template match on the context ring (all blemish
    coverage is excluded from the match), Poisson-blend it over the hole and feather it
    in with the thickness-based engine ramp. Copied pixels carry the surround's true
    texture at EVERY frequency -- grain, mid-freq mottle, paint texture -- which no
    512-net generative fill upscaled to the tile can; measured as the remaining tell
    after the level + grain fixes. Runs at full image resolution, no model inference.
    Returns false when no clean source exists (hole at a frame edge, no room to search,
    or every candidate would copy part of the blemish) -- the caller then falls back to
    the model path. Coarse-to-fine: match at reduced resolution (hole ~<=48px), refine
    +-q px at full res.
*/
bool cloneHealOne(cv::Mat &full, const std::vector<float> &cov,
                  int bx0, int by0, int bx1, int by1, const ChunkWin &win = {},
                  const QString &offsetKey = QString())
{
    const int W = full.cols, H = full.rows;
    const int bw = bx1 - bx0 + 1, bh = by1 - by0 + 1;

    /* Pinned source from an earlier render of this exact spot/chunk/size? */
    cv::Point pinned;
    bool havePinned = false, pinnedNoSource = false;
    if (lookupPinnedOffset(offsetKey, pinned, pinnedNoSource)) {
        if (pinnedNoSource) return false;               // known failing chunk
        havePinned = true;
    }

    /* Context rect: hole bbox + a matching ring the template match aligns on. */
    const int m = std::max(16, std::max(bw, bh) / 2);
    cv::Rect C(bx0 - m, by0 - m, bw + 2 * m, bh + 2 * m);
    C &= cv::Rect(0, 0, W, H);

    /* Composite alpha = this chunk's windowed share of the coverage; the hard hole
       (clone extent + source-validity shape) derives from it. */
    cv::Mat alpha(C.height, C.width, CV_32F);
    for (int y = 0; y < C.height; ++y) {
        float *a = alpha.ptr<float>(y);
        const size_t row = size_t(C.y + y) * W + C.x;
        for (int x = 0; x < C.width; ++x)
            a[x] = cov[row + x] * float(winWeight(win, C.x + x, C.y + y));
    }
    cv::Mat hard;
    cv::threshold(alpha, hard, 0.05, 1.0, cv::THRESH_BINARY);

    cv::Mat hole8;
    hard.convertTo(hole8, CV_8U, 255.0);
    if (cv::countNonZero(hole8) < 1) return false;
    const double feather = featherFromHole(hole8);       // thickness-based
    const int fk = 2 * int(std::ceil(feather)) + 1;

    /* The clone extent is dilated by the feather band so the alpha ramp (below) blends
       cloned content over its whole width. */
    cv::dilate(hard, hard, cv::getStructuringElement(cv::MORPH_ELLIPSE, {fk, fk}));
    cv::Mat hard8;
    hard.convertTo(hard8, CV_8U, 255.0);
    const cv::Rect mb = cv::boundingRect(hard8);
    if (mb.area() <= 0) return false;
    /* seamlessClone needs the mask strictly inside the patch; a hole reaching a frame
       edge is handled by REFLECT-padding the tile at blend time (below), so edge
       strokes still clone rather than falling back to the model. */
    const bool needPad = mb.x < 2 || mb.y < 2 ||
                         mb.br().x > C.width - 2 || mb.br().y > C.height - 2;
    /* Feather the composite alpha: the brushed area is hard (preview == exactly what
       was painted); the engine's ramp makes the transition seamless. */
    cv::GaussianBlur(alpha, alpha, cv::Size(), feather / 2.0);

    cv::Point d;                                         // best source offset, full-res
    if (havePinned) {
        d = pinned;
    } else {
    const cv::Mat tmpl = full(C);
    /* The match ring must ignore the WHOLE blemish (unwindowed coverage), not just this
       chunk's hole -- else it seeks sources that LOOK like the remaining blemish. */
    cv::Mat tmplMask(C.height, C.width, CV_8U);
    for (int y = 0; y < C.height; ++y) {
        uchar *tm = tmplMask.ptr<uchar>(y);
        const size_t row = size_t(C.y + y) * W + C.x;
        for (int x = 0; x < C.width; ++x) tm[x] = cov[row + x] > 0.02f ? 0 : 255;
    }
    cv::erode(tmplMask, tmplMask, cv::getStructuringElement(cv::MORPH_ELLIPSE, {9, 9}));

    /* Search window around the context; nearby sources keep the lighting consistent. */
    const int R = 3 * std::max(bw, bh);
    cv::Rect S(C.x - R, C.y - R, C.width + 2 * R, C.height + 2 * R);
    S &= cv::Rect(0, 0, W, H);
    if (S.width < C.width + 8 && S.height < C.height + 8) {
        pinOffset(offsetKey, kNoSource);
        return false;
    }

    /* Source validity: the pixels a candidate would copy (this chunk's hole SHAPE,
       padded a little beyond the feather dilation) must not overlap ANY of the blemish
       coverage -- its own hole or the still-unhealed remainder of a long stroke. The
       overlap for EVERY offset comes from one cross-correlation of the coverage with
       the hole shape. (A bounding-RECT test rejected nearly every candidate for a
       diagonal stroke, whose bbox is a huge mostly-empty square -- chunks then all fell
       back to the model, which was the visible smudge; lab-verified.) */
    cv::Mat covS(S.height, S.width, CV_32F);
    for (int y = 0; y < S.height; ++y) {
        float *cm = covS.ptr<float>(y);
        const size_t row = size_t(S.y + y) * W + S.x;
        for (int x = 0; x < S.width; ++x) cm[x] = cov[row + x] > 0.02f ? 1.0f : 0.0f;
    }
    cv::Mat holeP;
    cv::dilate(hard, holeP, cv::getStructuringElement(cv::MORPH_ELLIPSE, {9, 9}));
    cv::Mat overlap;                                     // indexed by C's position in S
    cv::matchTemplate(covS, holeP, overlap, cv::TM_CCORR);
    auto sourceHitsCoverage = [&](int dx, int dy) {
        const int px = C.x - S.x + dx, py = C.y - S.y + dy;
        if (px < 0 || py < 0 || px >= overlap.cols || py >= overlap.rows) return true;
        return overlap.at<float>(py, px) > 0.5f;
    };

    const int q = std::clamp((std::max(bw, bh) + 47) / 48, 1, 8);
    {
        cv::Mat sQ, tQ, mQ;
        const cv::Size sSz(std::max(1, S.width / q), std::max(1, S.height / q));
        const cv::Size tSz(std::max(1, C.width / q), std::max(1, C.height / q));
        cv::resize(full(S), sQ, sSz, 0, 0, cv::INTER_AREA);
        cv::resize(tmpl, tQ, tSz, 0, 0, cv::INTER_AREA);
        cv::resize(tmplMask, mQ, tSz, 0, 0, cv::INTER_NEAREST);
        if (tQ.cols > sQ.cols || tQ.rows > sQ.rows) {
            pinOffset(offsetKey, kNoSource);
            return false;
        }
        cv::Mat score;
        cv::matchTemplate(sQ, tQ, score, cv::TM_SQDIFF, mQ);
        const int pCx = (C.x - S.x) / q, pCy = (C.y - S.y) / q;
        for (int y = 0; y < score.rows; ++y) {
            float *r = score.ptr<float>(y);
            const int dy = (y - pCy) * q;
            for (int x = 0; x < score.cols; ++x) {
                const int dx = (x - pCx) * q;
                if (!std::isfinite(r[x]) || sourceHitsCoverage(dx, dy)) {
                    r[x] = FLT_MAX;
                } else {
                    /* Distance penalty: a far source must be ~2x better (at the search
                       edge) to beat a near one -- keeps the heal inside the local
                       colour/lighting zone (stalk test cloned from 2000px away into a
                       different colour gradient). Multiplicative, so scale-free. */
                    r[x] *= 1.0f + float((double(dx) * dx + double(dy) * dy)
                                         / (double(R) * R));
                }
            }
        }
        double mn, mx;
        cv::Point mnLoc, mxLoc;
        cv::minMaxLoc(score, &mn, &mx, &mnLoc, &mxLoc);
        if (mn >= double(FLT_MAX)) {
            pinOffset(offsetKey, kNoSource);
            return false;
        }
        d = cv::Point((mnLoc.x - pCx) * q, (mnLoc.y - pCy) * q);
    }
    if (q > 1) {                                         // full-res refinement, +-(q+2)
        const int rr = q + 2;
        cv::Rect Sr(C.x + d.x - rr, C.y + d.y - rr, C.width + 2 * rr, C.height + 2 * rr);
        Sr &= cv::Rect(0, 0, W, H);
        if (Sr.width >= C.width && Sr.height >= C.height) {
            cv::Mat score;
            cv::matchTemplate(full(Sr), tmpl, score, cv::TM_SQDIFF, tmplMask);
            double mn, mx;
            cv::Point mnLoc, mxLoc;
            cv::minMaxLoc(score, &mn, &mx, &mnLoc, &mxLoc);
            const cv::Point d2(Sr.x + mnLoc.x - C.x, Sr.y + mnLoc.y - C.y);
            if (!sourceHitsCoverage(d2.x, d2.y)) d = d2;
        }
    }
    /* Clamp so the source rect stays in-frame, then re-check the coverage rule. */
    d.x = std::clamp(d.x, -C.x, W - C.width - C.x);
    d.y = std::clamp(d.y, -C.y, H - C.height - C.y);
    if (sourceHitsCoverage(d.x, d.y)) {
        pinOffset(offsetKey, kNoSource);
        return false;
    }
    pinOffset(offsetKey, QPoint(d.x, d.y));
    }                                                    // end !havePinned search

    /* Poisson-blend (cv::seamlessClone NORMAL_CLONE): texture AND gradients from the
       source, level solved from the destination boundary. Ground-truth benchmarked
       (heal strokes over CLEAN background, compare to the original) as the best blend
       on every content type -- trunk RMSE 5.8 vs 7.5 membrane-split / 5.9 shiftmap,
       bokeh 5.4 vs 10.3 / 8.7. The key: harmonic membranes obey the maximum principle
       and can NEVER reconstruct a local brightness maximum, so any membrane-based
       low-freq dims the bokeh blobs a stroke crosses -- the visible "dark sausage";
       Poisson keeps the source's own blobs and only tilts them to fit the boundary. */
    cv::Mat src = full(cv::Rect(C.x + d.x, C.y + d.y, C.width, C.height)).clone();
    cv::Mat dst = full(C).clone();
    cv::Mat mask = hard8;
    cv::Point center(mb.x + mb.width / 2, mb.y + mb.height / 2);
    constexpr int kPad = 8;
    if (needPad) {                       // frame-edge hole: give Poisson a border
        cv::copyMakeBorder(src, src, kPad, kPad, kPad, kPad, cv::BORDER_REFLECT_101);
        cv::copyMakeBorder(dst, dst, kPad, kPad, kPad, kPad, cv::BORDER_REFLECT_101);
        cv::copyMakeBorder(hard8, mask, kPad, kPad, kPad, kPad,
                           cv::BORDER_CONSTANT, cv::Scalar(0));
        center += cv::Point(kPad, kPad);
    }
    cv::Mat blend;
    try { cv::seamlessClone(src, dst, mask, center, blend, cv::NORMAL_CLONE); }
    catch (const cv::Exception &e) {
        qDebug() << "[A/B] CLONE -> model fallback (exc)" << e.what();  // TEMP
        return false;
    }
    if (needPad) blend = blend(cv::Rect(kPad, kPad, C.width, C.height)).clone();
    cv::Mat out = full(C);
    for (int y = 0; y < C.height; ++y) {
        cv::Vec3b *o = out.ptr<cv::Vec3b>(y);
        const cv::Vec3b *b = blend.ptr<cv::Vec3b>(y);
        const float *a = alpha.ptr<float>(y);
        for (int x = 0; x < C.width; ++x) {
            const float aa = std::clamp(a[x], 0.0f, 1.0f);
            for (int c = 0; c < 3; ++c)
                o[x][c] = cv::saturate_cast<uchar>(o[x][c] * (1.0f - aa) + b[x][c] * aa);
        }
    }
    qDebug() << "[A/B] mode4 CLONE heal EXECUTED offset" << d.x << d.y;  // TEMP
    return true;
}

/* Heal one spot's (or one chunk's windowed share of a) coverage `cov` (row-major W*H,
   0..1) into RGB888 `full`, in place, with the model. */
void healOne(cv::Mat &full, const std::vector<float> &cov, int bx0, int by0, int bx1, int by1,
             InferenceSession *s, const std::string &imgName, const std::string &maskName,
             const std::string &outName, const ChunkWin &win = {})
{
    const int W = full.cols, H = full.rows;
    const int cx = (bx0 + bx1) / 2, cy = (by0 + by1) / 2;
    const int half = std::max(bx1 - bx0, by1 - by0) / 2;

    /* Square context tile ~= 4x the spot, capped to the image, clamped in-bounds. */
    int t = std::min({W, H, std::max(16, half * 4)});
    int tx0 = std::clamp(cx - t / 2, 0, W - t);
    int ty0 = std::clamp(cy - t / 2, 0, H - t);
    const cv::Rect roi(tx0, ty0, t, t);

    cv::Mat tileImg = full(roi).clone();                 // RGB 8u
    cv::Mat hole(t, t, CV_32F);
    for (int y = 0; y < t; ++y) {
        float *d = hole.ptr<float>(y);
        for (int x = 0; x < t; ++x)
            d[x] = cov[size_t(ty0 + y) * W + (tx0 + x)]
                   * float(winWeight(win, tx0 + x, ty0 + y));
    }

    cv::Mat imgN, holeN;
    cv::resize(tileImg, imgN,  cv::Size(kNet, kNet), 0, 0, cv::INTER_AREA);
    cv::resize(hole,    holeN, cv::Size(kNet, kNet), 0, 0, cv::INTER_LINEAR);

    /* Hard, slightly dilated hole for the model; soft holeN alpha for compositing. */
    cv::Mat hard;
    cv::threshold(holeN, hard, 0.05, 1.0, cv::THRESH_BINARY);
    cv::dilate(hard, hard, cv::getStructuringElement(cv::MORPH_ELLIPSE, {7, 7}));

    const int P = kNet * kNet;
    /* LaMa: image RGB in [0,1]; separate mask where 1 = region to inpaint. */
    Tensor inImg;  inImg.shape  = {1, 3, kNet, kNet}; inImg.data.resize(size_t(3) * P);
    Tensor inMask; inMask.shape = {1, 1, kNet, kNet}; inMask.data.resize(size_t(P));
    for (int y = 0; y < kNet; ++y) {
        const cv::Vec3b *row = imgN.ptr<cv::Vec3b>(y);
        const float *hr = hard.ptr<float>(y);
        for (int x = 0; x < kNet; ++x) {
            const int o = y * kNet + x;
            inImg.data[0 * P + o] = row[x][0] / 255.0f;  // R
            inImg.data[1 * P + o] = row[x][1] / 255.0f;  // G
            inImg.data[2 * P + o] = row[x][2] / 255.0f;  // B
            inMask.data[o] = hr[x];                      // 1 = hole to fill
        }
    }

    std::vector<Tensor> outs;
    if (!s->Run({imgName, maskName}, {inImg, inMask}, {outName}, outs) || outs.empty()) return;
    const Tensor &out = outs[0];
    if (int(out.data.size()) != 3 * P) return;

    /* Raw model fill (0..255) and the original tile as 32FC3, then optional correction
       (A/B via G::spotFillCorrectMode) to kill the large-fill luminance patch. */
    cv::Mat model(kNet, kNet, CV_32FC3), orig;
    imgN.convertTo(orig, CV_32FC3);
    for (int y = 0; y < kNet; ++y) {
        cv::Vec3f *mp = model.ptr<cv::Vec3f>(y);
        for (int x = 0; x < kNet; ++x) {
            const int o = y * kNet + x;
            mp[x][0] = out.data[0 * P + o];
            mp[x][1] = out.data[1 * P + o];
            mp[x][2] = out.data[2 * P + o];
        }
    }
    cv::Mat fill = applyFillCorrection(model, orig, hard, G::spotFillCorrectMode);

    /* net-res healed patch = original in keep, corrected fill in hole. */
    cv::Mat healN(kNet, kNet, CV_8UC3);
    for (int y = 0; y < kNet; ++y) {
        cv::Vec3b *hp = healN.ptr<cv::Vec3b>(y);
        const cv::Vec3b *row = imgN.ptr<cv::Vec3b>(y);
        const float *hr = hard.ptr<float>(y);
        const cv::Vec3f *fp = fill.ptr<cv::Vec3f>(y);
        for (int x = 0; x < kNet; ++x) {
            const float keep = 1.0f - hr[x];
            for (int c = 0; c < 3; ++c)
                hp[x][c] = cv::saturate_cast<uchar>(row[x][c] * keep + fp[x][c] * (1.0f - keep));
        }
    }

    cv::Mat heal, alpha;
    cv::resize(healN, heal,  cv::Size(t, t), 0, 0, cv::INTER_CUBIC);
    cv::resize(holeN, alpha, cv::Size(t, t), 0, 0, cv::INTER_LINEAR);
    /* Feather the composite alpha: the brushed area is hard; the engine's ramp makes
       the transition seamless (thickness-based, see featherFromHole). */
    cv::Mat holeHard8;
    cv::threshold(hole, holeHard8, 0.05, 255.0, cv::THRESH_BINARY);
    holeHard8.convertTo(holeHard8, CV_8U);
    if (cv::countNonZero(holeHard8) >= 1)
        cv::GaussianBlur(alpha, alpha, cv::Size(), featherFromHole(holeHard8) / 2.0);

    /* Grain match at FULL tile resolution (not net-res) so the quilted grain is
       per-pixel fine grain, not the coarse blobs a large net->tile upscale would smear it
       into. alpha (0..1) doubles as the hole indicator; the grain is sourced from the
       full-res original tile's kept surround. */
    if (G::spotFillGrain) {
        cv::Mat healF, origF;
        heal.convertTo(healF, CV_32FC3);
        tileImg.convertTo(origF, CV_32FC3);
        applyGrainMatch(healF, origF, alpha);
        healF.convertTo(heal, CV_8UC3);
    }

    cv::Mat dst = full(roi);
    for (int y = 0; y < t; ++y) {
        cv::Vec3b *d = dst.ptr<cv::Vec3b>(y);
        const cv::Vec3b *hp = heal.ptr<cv::Vec3b>(y);
        const float *a = alpha.ptr<float>(y);
        for (int x = 0; x < t; ++x) {
            const float aa = std::clamp(a[x], 0.0f, 1.0f);
            for (int c = 0; c < 3; ++c)
                d[x][c] = cv::saturate_cast<uchar>(d[x][c] * (1.0f - aa) + hp[x][c] * aa);
        }
    }
}

} // namespace

namespace LamaFill {

bool isAvailable()
{
    InferenceSession *s = SharedSession();
    return s && s->IsLoaded();
}

bool apply(QImage &img, const QVector<FillSpot> &spots, const QString &cachePath)
{
    if (img.isNull()) return false;
    /* Clone heals (Spot/Fill kinds) need no model; the session is only required for the
       model path (Object fills + clone fallbacks), which no-ops warn-if-absent style. */
    InferenceSession *s = SharedSession();
    const bool modelOk = s && s->IsLoaded();

    bool any = false;
    for (const FillSpot &sp : spots)
        if (sp.enabled && !sp.paramsJson.isEmpty()) { any = true; break; }
    if (!any) return false;

    const int W = img.width(), H = img.height();
    QImage work = img.convertToFormat(QImage::Format_RGB888);
    work.detach();                                       // writable, unshared
    cv::Mat full(H, W, CV_8UC3, work.bits(), work.bytesPerLine());

    const auto ins = modelOk ? s->InputNames() : std::vector<std::string>();
    const std::string imgName  = ins.size() > 0 ? ins[0] : "image";
    const std::string maskName = ins.size() > 1 ? ins[1] : "mask";
    const std::string outName  = (!modelOk || s->OutputNames().empty())
                                 ? "output" : s->OutputNames()[0];

    bool applied = false;
    int healed = 0;
    std::vector<float> cov;
    for (const FillSpot &sp : spots) {
        if (!sp.enabled || sp.paramsJson.isEmpty()) continue;
        const FillSpotGeom::Parsed p = FillSpotGeom::parse(sp.paramsJson);
        if (!p.valid()) continue;
        int bx0, by0, bx1, by1;
        FillSpotGeom::rasterize(p, W, H, cov, bx0, by0, bx1, by1);
        if (bx1 < bx0 || by1 < by0) continue;            // empty coverage
        /* The spot's kind picks the engine (Replace panel mode): Spot/Fill heal by
           exemplar CLONE (real neighbouring texture), falling back to the model when no
           clean source region exists; Object is a regenerative model fill (the removed
           object leaves no clean source to clone). A large/long painted region heals
           in brush-scale CHUNKS, each cloning from its own local source (see ChunkWin);
           chunks run in scan order, so later chunks blend over earlier heals. */
        const bool wantClone = p.kind != FillSpotGeom::Object;
        bool done = false;
        /* Pinned-offset key: engine version + image path + render size + this spot's
           geometry (see lookupPinnedOffset -- keeps heal geometry stable across
           re-renders; the version tag retires pins from older chunking/matching). */
        const QString spotKey = cachePath.isEmpty() ? QString()
            : QString("v2|%1|%2x%3|%4").arg(cachePath).arg(W).arg(H)
                  .arg(qHash(sp.paramsJson));
        if (wantClone) {
            /* Chunk size from the painted THICKNESS (max inscribed radius), like the
               feather -- NOT the brush diameter: a big brush previously made 640px
               chunks whose interiors sit far from any boundary, and nothing matches a
               smoothly varying background at that scale (bad colour drift, 2000px
               offsets; stalk test). 3x thickness keeps every chunk source-matchable;
               thin strokes stay at the 96px floor. */
            cv::Mat covB(by1 - by0 + 3, bx1 - bx0 + 3, CV_8U, cv::Scalar(0));
            for (int y = by0; y <= by1; ++y) {
                uchar *dm = covB.ptr<uchar>(y - by0 + 1) + 1;
                const float *c = cov.data() + size_t(y) * W;
                for (int x = bx0; x <= bx1; ++x) dm[x - bx0] = c[x] > 0.05f ? 255 : 0;
            }
            cv::Mat dtS;
            cv::distanceTransform(covB, dtS, cv::DIST_L2, 3);
            double maxR = 0.0;
            cv::minMaxLoc(dtS, nullptr, &maxR);
            const int core = std::clamp(int(6.0 * maxR), 96, 384);
            if (std::max(bx1 - bx0 + 1, by1 - by0 + 1) <= core * 3 / 2) {
                done = cloneHealOne(full, cov, bx0, by0, bx1, by1, {},
                                    spotKey.isEmpty() ? QString() : spotKey + "|s");
                if (!done)
                    qDebug() << "[A/B] CLONE -> model fallback (no source)";  // TEMP
            } else {
                const int ramp = std::max(8, core / 4);
                int nClone = 0, nModel = 0;
                for (int gy = by0; gy <= by1; gy += core) {
                    for (int gx = bx0; gx <= bx1; gx += core) {
                        ChunkWin win;
                        win.active = true;
                        win.ramp = ramp;
                        win.cx0 = gx; win.cy0 = gy;
                        win.cx1 = gx + core - 1; win.cy1 = gy + core - 1;
                        /* This chunk's coverage bbox within the window's reach. */
                        const int ex0 = std::max(0, gx - ramp);
                        const int ey0 = std::max(0, gy - ramp);
                        const int ex1 = std::min(W - 1, gx + core - 1 + ramp);
                        const int ey1 = std::min(H - 1, gy + core - 1 + ramp);
                        int cx0 = ex1 + 1, cy0 = ey1 + 1, cx1 = ex0 - 1, cy1 = ey0 - 1;
                        for (int y = ey0; y <= ey1; ++y) {
                            const float *c = cov.data() + size_t(y) * W;
                            for (int x = ex0; x <= ex1; ++x) {
                                if (c[x] > 0.0f) {
                                    cx0 = std::min(cx0, x); cy0 = std::min(cy0, y);
                                    cx1 = std::max(cx1, x); cy1 = std::max(cy1, y);
                                }
                            }
                        }
                        if (cx1 < cx0) continue;         // no coverage in this chunk
                        const QString chunkKey = spotKey.isEmpty() ? QString()
                            : spotKey + '|' + QString::number(gx) + ','
                              + QString::number(gy);
                        if (cloneHealOne(full, cov, cx0, cy0, cx1, cy1, win, chunkKey)) {
                            ++nClone;
                            done = true;
                        } else if (modelOk) {
                            healOne(full, cov, cx0, cy0, cx1, cy1,
                                    s, imgName, maskName, outName, win);
                            ++nModel;
                            done = true;
                        }
                    }
                }
                qDebug() << "[A/B] CLONE chunked: core=" << core << nClone
                         << "clone +" << nModel << "model chunks";  // TEMP
            }
        }
        if (done) {
            applied = true;
            ++healed;
            continue;
        }
        if (!modelOk) continue;             // no model: this spot stays unhealed
        healOne(full, cov, bx0, by0, bx1, by1, s, imgName, maskName, outName);
        applied = true;
        ++healed;
    }

    if (applied) img = work.convertToFormat(img.format());
    if (G::isLogger || G::isFlowLogger) {
        static const char *kMode[] = {"none", "Poisson", "low-freq", "harmonic"};
        const int mi = qBound(0, G::spotFillCorrectMode, 3);
        G::log("LamaFill::apply", QString("healed %1 spot(s) via %2, correct=%3 grain=%4")
               .arg(healed).arg(modelOk ? s->BackendName() : QString("clone-only"))
               .arg(kMode[mi]).arg(G::spotFillGrain ? "on" : "off"));
    }
    return applied;
}

} // namespace LamaFill
