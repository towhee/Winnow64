#include "segmenter.h"
#include "Main/global.h"
#include "Utilities/segrefine.h"

#include "opencv2/core.hpp"
#include "opencv2/dnn.hpp"
#include "opencv2/imgproc.hpp"

#include <QDeadlineTimer>
#include <QJsonDocument>
#include <QObject>
#include <QPainter>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>

/*
    SAM 2 automatic mask generation for the Segmenter tool. The pipeline and the reasons
    for it are in the header; notes/Documentation.txt "SEGMENTER" has the measurements.
*/

namespace Segmenter {

namespace {

/* ---- Automatic mask generation settings ----
   SAM 2's AutomaticMaskGenerator defaults are 32 points per side, IoU 0.8, stability
   0.95. A 16 x 16 grid is a quarter of the decodes (the decoder is the cost: ~110 ms per
   prompt per core) and still finds every object a mask is likely to be wanted for; the
   gap-fill rounds pick up what it misses. The thresholds are a notch looser because the
   grid is sparser: on the six test photos 0.8/0.95 left a third of a car photo
   unsegmented, 0.7/0.88 found the same objects plus the foliage around them. */
constexpr int    kPointsPerSide = 16;
constexpr int    kGridPoints    = kPointsPerSide * kPointsPerSide;
constexpr float  kIouThresh     = 0.70f;
constexpr float  kStabThresh    = 0.88f;
constexpr float  kStabOffset    = 1.0f;    // logit offset for the stability score
constexpr float  kNmsIou        = 0.70f;   // mask IoU above which two candidates are one
/* Gap fill: relaxed, because a region nothing claimed is by definition one SAM is unsure
   of -- but a candidate must lie mostly IN the gap, or it is just a confident object
   next to it found again. */
constexpr int    kGapRounds     = 3;
constexpr int    kGapPerRound   = 16;
constexpr float  kGapIouThresh  = 0.50f;
constexpr float  kGapStabThresh = 0.75f;
constexpr float  kGapInside     = 0.50f;
constexpr double kGapMinArea    = 0.002;   // an uncovered region worth a prompt
/* Areas as fractions of the frame. Holes and specks below kMinArea are cleaned out of a
   mask (SAM's remove_small_regions); a segment below it is dropped. */
constexpr double kMinArea       = 0.0005;
constexpr double kMaxArea       = 0.95;    // "the whole frame" is not an object
constexpr double kRemainderMinArea = 0.003;
/* Hierarchy: B is A's parent if it holds kContain of A and is meaningfully larger. */
constexpr double kContain       = 0.80;
constexpr double kParentGrowth  = 1.10;
/* Outline simplification, in label-grid pixels. */
constexpr double kRingEpsilon   = 0.75;

/* A decode under evaluation, at the decoder's kMaskRes. */
struct Cand {
    Candidate c;
    float stab = 0.0f;
    cv::Mat bin;            // CV_8U 0/255, cleaned
    int area = 0;
    cv::Rect box;
    int depth = 0;
};

cv::Mat logitMat(const Candidate &c)
{
    return cv::Mat(kMaskRes, kMaskRes, CV_32F, const_cast<float *>(c.logits.data()));
}

bool cancelled(const Progress *p) { return p && p->cancel.load(); }

void report(Progress *p, int pct)
{
    if (p && pct > p->pct.load()) p->pct.store(std::min(pct, 100));
}

/*
    SAM's remove_small_regions, both modes, in one pass over each labelling: holes
    smaller than minA are filled and islands smaller than minA dropped -- except the
    island under the prompt point, which IS the object however small. A background
    region touching the frame edge is open background, not a hole. Labels are judged
    first and applied through a lookup, so a mask with hundreds of specks still costs two
    passes rather than one per speck.
*/
void cleanBinary(cv::Mat &bin, int minA, cv::Point seed)
{
    cv::Mat lab, st, cen;
    {
        const cv::Mat inv = (bin == 0);
        const int n = cv::connectedComponentsWithStats(inv, lab, st, cen, 4, CV_32S);
        std::vector<uint8_t> fill(size_t(n), 0);
        bool any = false;
        for (int i = 1; i < n; ++i) {
            if (st.at<int>(i, cv::CC_STAT_AREA) >= minA) continue;
            const int x = st.at<int>(i, cv::CC_STAT_LEFT), y = st.at<int>(i, cv::CC_STAT_TOP);
            const int w = st.at<int>(i, cv::CC_STAT_WIDTH), h = st.at<int>(i, cv::CC_STAT_HEIGHT);
            if (x == 0 || y == 0 || x + w == bin.cols || y + h == bin.rows) continue;
            fill[size_t(i)] = 1;
            any = true;
        }
        if (any)
            for (int y = 0; y < bin.rows; ++y) {
                const int *l = lab.ptr<int>(y);
                uchar *b = bin.ptr<uchar>(y);
                for (int x = 0; x < bin.cols; ++x) if (fill[size_t(l[x])]) b[x] = 255;
            }
    }
    const int n = cv::connectedComponentsWithStats(bin, lab, st, cen, 8, CV_32S);
    const bool seedIn = seed.x >= 0 && seed.y >= 0 && seed.x < bin.cols && seed.y < bin.rows;
    const int keep = seedIn ? lab.at<int>(seed) : -1;
    std::vector<uint8_t> drop(size_t(n), 0);
    bool any = false;
    for (int i = 1; i < n; ++i)
        if (st.at<int>(i, cv::CC_STAT_AREA) < minA && i != keep) { drop[size_t(i)] = 1; any = true; }
    if (any)
        for (int y = 0; y < bin.rows; ++y) {
            const int *l = lab.ptr<int>(y);
            uchar *b = bin.ptr<uchar>(y);
            for (int x = 0; x < bin.cols; ++x) if (drop[size_t(l[x])]) b[x] = 0;
        }
}

/* Confident, stable, cleaned, and a sensible size. Fills c.stab/bin/area/box. */
bool qualifies(Cand &c, float iouT, float stabT)
{
    if (c.c.iou < iouT || c.c.logits.size() != size_t(kMaskRes) * kMaskRes) return false;
    const cv::Mat L = logitMat(c.c);
    const int hi = cv::countNonZero(L > kStabOffset);
    const int lo = cv::countNonZero(L > -kStabOffset);
    c.stab = lo > 0 ? float(hi) / float(lo) : 0.0f;
    if (c.stab < stabT) return false;
    const int minA = std::max(1, int(kMinArea * kMaskRes * kMaskRes));
    c.bin = (L > 0.0f);
    cleanBinary(c.bin, minA, cv::Point(int(c.c.px * kMaskRes), int(c.c.py * kMaskRes)));
    c.area = cv::countNonZero(c.bin);
    if (c.area < minA || c.area > kMaxArea * kMaskRes * kMaskRes) return false;
    c.box = cv::boundingRect(c.bin);
    return true;
}

int overlap(const cv::Mat &a, const cv::Rect &ab, const cv::Mat &b, const cv::Rect &bb)
{
    const cv::Rect r = ab & bb;
    if (r.area() <= 0) return 0;
    return cv::countNonZero(a(r) & b(r));
}

float maskIou(const Cand &a, const Cand &b)
{
    const int inter = overlap(a.bin, a.box, b.bin, b.box);
    return inter > 0 ? float(inter) / float(a.area + b.area - inter) : 0.0f;
}

/* Greedy non-maximum suppression into `kept`, best (IoU x stability) first. */
void suppressInto(std::vector<Cand> &kept, std::vector<Cand> cands)
{
    std::stable_sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
        return a.c.iou * a.stab > b.c.iou * b.stab;
    });
    for (Cand &c : cands) {
        bool dup = false;
        for (const Cand &k : kept) if (maskIou(c, k) > kNmsIou) { dup = true; break; }
        if (!dup) kept.push_back(std::move(c));
    }
}

/*
    Up to kGapPerRound prompt points inside the UNCOVERED part of the frame: interior
    pixels (at least 3 cells from anything covered), not within 6 cells of a point that
    was already tried, spread out by a greedy farthest-point pick. Uncovered specks below
    kGapMinArea are not worth a prompt. The distance transform runs on a padded copy so
    the frame edge does not count as covered -- unpadded, a gap touching the edge put its
    "deepest" point in the corner.
*/
std::vector<QPointF> gapPoints(const cv::Mat &uncovered, const std::vector<QPointF> &tried)
{
    const int R = kMaskRes;
    cv::Mat lab, st, cen;
    const int n = cv::connectedComponentsWithStats(uncovered, lab, st, cen, 8, CV_32S);
    cv::Mat big(R, R, CV_8U, cv::Scalar(0));
    for (int i = 1; i < n; ++i)
        if (st.at<int>(i, cv::CC_STAT_AREA) >= kGapMinArea * R * R) big.setTo(255, lab == i);
    if (cv::countNonZero(big) == 0) return {};

    cv::Mat pad, dt;
    cv::copyMakeBorder(big, pad, 1, 1, 1, 1, cv::BORDER_CONSTANT, cv::Scalar(255));
    cv::distanceTransform(pad, dt, cv::DIST_L2, 3);
    dt = dt(cv::Rect(1, 1, R, R)).clone();

    cv::Mat blocked(R, R, CV_8U, cv::Scalar(0));
    for (const QPointF &q : tried)
        cv::circle(blocked, cv::Point(int(q.x() * R), int(q.y() * R)), 6, cv::Scalar(255), cv::FILLED);

    std::vector<QPointF> picks;
    for (int k = 0; k < kGapPerRound; ++k) {
        double best = 0.0;
        cv::Point bp(-1, -1);
        for (int y = 0; y < R; y += 2) {
            const float *d = dt.ptr<float>(y);
            const uchar *bl = blocked.ptr<uchar>(y);
            for (int x = 0; x < R; x += 2) {
                if (d[x] < 3.0f || bl[x]) continue;
                double nearPick = 48.0;
                for (const QPointF &q : picks)
                    nearPick = std::min(nearPick, std::hypot(q.x() * R - x, q.y() * R - y));
                if (nearPick < 16.0) continue;
                const double s = std::min<double>(d[x], 12.0) * nearPick;
                if (s > best) { best = s; bp = cv::Point(x, y); }
            }
        }
        if (bp.x < 0) break;
        picks.push_back(QPointF((bp.x + 0.5) / R, (bp.y + 0.5) / R));
    }
    return picks;
}

/* A kMaskRes candidate's mask on the label grid: SAM's logits upsampled (the soft
   boundary interpolates to sub-cell precision) and thresholded at the zero crossing,
   then cleaned at the finer scale. */
cv::Mat gridMask(const Cand &c, int gw, int gh)
{
    cv::Mat up, bin;
    cv::resize(logitMat(c.c), up, cv::Size(gw, gh), 0, 0, cv::INTER_LINEAR);
    bin = (up > 0.0f);
    const int minA = std::max(1, int(kMinArea * gw * gh));
    cleanBinary(bin, minA, cv::Point(int(c.c.px * gw), int(c.c.py * gh)));
    return bin;
}

/* Trace a grid mask into mask-normalized rings (outer boundaries and holes alike; the
   path's even-odd rule tells them apart). */
QVector<QPolygonF> traceRings(const cv::Mat &bin, int gw, int gh)
{
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hier;
    cv::findContours(bin.clone(), contours, hier, cv::RETR_CCOMP, cv::CHAIN_APPROX_NONE);
    QVector<QPolygonF> rings;
    for (const auto &c : contours) {
        std::vector<cv::Point> a;
        cv::approxPolyDP(c, a, kRingEpsilon, true);
        if (a.size() < 3 || std::abs(cv::contourArea(a)) < 4.0) continue;
        QPolygonF r;
        r.reserve(int(a.size()));
        for (const cv::Point &p : a) r << QPointF((p.x + 0.5) / gw, (p.y + 0.5) / gh);
        rings << r;
    }
    return rings;
}

QPainterPath ringsPath(const QVector<QPolygonF> &rings)
{
    QPainterPath p;
    p.setFillRule(Qt::OddEvenFill);
    for (const QPolygonF &r : rings) { p.addPolygon(r); p.closeSubpath(); }
    return p;
}

/* SAM 2 encoder preprocessing -- the same as ObjectMaskPredictor's: RGB, resized to
   S^2 without keeping the aspect (SAM 2's own transform), ImageNet mean/std, NCHW. */
cv::Mat preprocess(const QImage &image, int S)
{
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888)
                            .scaled(S, S, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    static const float mean[3] = {0.485f, 0.456f, 0.406f};
    static const float istd[3] = {1.0f / 0.229f, 1.0f / 0.224f, 1.0f / 0.225f};
    const int dims[4] = {1, 3, S, S};
    cv::Mat blob(4, dims, CV_32F);
    float *p = blob.ptr<float>();
    const size_t plane = size_t(S) * S;
    for (int y = 0; y < S; ++y) {
        const uchar *line = rgb.constScanLine(y);
        for (int x = 0; x < S; ++x) {
            const size_t idx = size_t(y) * S + x;
            for (int c = 0; c < 3; ++c)
                p[c * plane + idx] = (line[x * 3 + c] / 255.0f - mean[c]) * istd[c];
        }
    }
    return blob;
}

/* Round a coordinate for the sidecar: 1e-5 of the frame is a twentieth of a pixel at
   4096 px, and keeps the JSON at five decimals instead of seventeen digits. */
double round5(double v) { return std::round(v * 1e5) / 1e5; }

}   // namespace

/* ------------------------------------------------------------------------------------ */

QString levelName(int level)
{
    switch (level) {
    case Objects: return QObject::tr("Objects");
    case Parts:   return QObject::tr("Parts");
    default:      return QObject::tr("Details");
    }
}

int Segmentation::at(QPointF n, int level) const
{
    if (level < 0 || level >= kLevels || gw <= 0 || gh <= 0) return -1;
    if (n.x() < 0.0 || n.y() < 0.0 || n.x() >= 1.0 || n.y() >= 1.0) return -1;
    const std::vector<int16_t> &L = label[level];
    const size_t i = size_t(int(n.y() * gh)) * size_t(gw) + size_t(int(n.x() * gw));
    return i < L.size() ? int(L[i]) : -1;
}

int Segmentation::count(int level) const
{
    int n = 0;
    for (const Segment &s : segs) if (s.depth <= level) ++n;
    return n;
}

std::shared_ptr<const Segmentation> assemble(const DecodeFn &decode, int gw, int gh,
                                             Progress *progress, int pctFrom, int pctTo)
{
    if (!decode || gw <= 0 || gh <= 0) return nullptr;
    const int R = kMaskRes;
    auto stage = [&](double f) { report(progress, pctFrom + int(f * (pctTo - pctFrom))); };

    /* ---- 1. the grid ---- */
    std::vector<QPointF> grid;
    grid.reserve(kGridPoints);
    for (int j = 0; j < kPointsPerSide; ++j)
        for (int i = 0; i < kPointsPerSide; ++i)
            grid.push_back(QPointF((i + 0.5) / kPointsPerSide, (j + 0.5) / kPointsPerSide));
    std::vector<Candidate> raw = decode(grid);
    if (cancelled(progress)) return nullptr;

    std::vector<Cand> passing;
    for (Candidate &c : raw) {
        Cand k;
        k.c = std::move(c);
        if (qualifies(k, kIouThresh, kStabThresh)) passing.push_back(std::move(k));
    }
    raw.clear();
    std::vector<Cand> kept;
    suppressInto(kept, std::move(passing));
    stage(0.3);

    /* ---- 2. gap fill ---- */
    std::vector<QPointF> tried = grid;
    for (int round = 0; round < kGapRounds; ++round) {
        if (cancelled(progress)) return nullptr;
        cv::Mat cov(R, R, CV_8U, cv::Scalar(0));
        for (const Cand &k : kept) cov |= k.bin;
        cv::Mat unc = (cov == 0);
        cv::morphologyEx(unc, unc, cv::MORPH_OPEN,
                         cv::getStructuringElement(cv::MORPH_ELLIPSE, {5, 5}));
        const std::vector<QPointF> pts = gapPoints(unc, tried);
        if (pts.empty()) break;
        tried.insert(tried.end(), pts.begin(), pts.end());

        std::vector<Candidate> g = decode(pts);
        if (cancelled(progress)) return nullptr;
        /* One segment per gap point at most: the candidate that is confident AND lies
           mostly in the gap. */
        std::vector<Cand> found;
        for (size_t p = 0; p < pts.size(); ++p) {
            int best = -1;
            float bestScore = -1.0f;
            std::vector<Cand> three(3);
            for (size_t k = 0; k < 3 && p * 3 + k < g.size(); ++k) {
                three[k].c = std::move(g[p * 3 + k]);
                if (!qualifies(three[k], kGapIouThresh, kGapStabThresh)) continue;
                const int inGap = overlap(three[k].bin, three[k].box, unc, three[k].box);
                const float inside = float(inGap) / float(three[k].area);
                const float score = three[k].c.iou * three[k].stab * inside;
                if (inside > kGapInside && score > bestScore) { bestScore = score; best = k; }
            }
            if (best >= 0) found.push_back(std::move(three[best]));
        }
        const size_t before = kept.size();
        suppressInto(kept, std::move(found));
        stage(0.3 + 0.2 * (round + 1) / kGapRounds);
        if (kept.size() == before) break;
    }
    if (cancelled(progress)) return nullptr;

    /* ---- 3. hierarchy, at the decoder's resolution ---- */
    std::stable_sort(kept.begin(), kept.end(), [](const Cand &a, const Cand &b) {
        return a.area > b.area;
    });
    for (size_t i = 0; i < kept.size(); ++i) {
        /* Walk back from the smallest larger segment, so the first holder found is the
           tightest one: the shirt's parent is the person, not the whole picture. */
        for (int j = int(i) - 1; j >= 0; --j) {
            if (kept[size_t(j)].area < kParentGrowth * kept[i].area) continue;
            const int inter = overlap(kept[i].bin, kept[i].box,
                                      kept[size_t(j)].bin, kept[size_t(j)].box);
            if (inter >= kContain * kept[i].area) {
                kept[i].depth = kept[size_t(j)].depth + 1;
                break;
            }
        }
    }
    stage(0.6);

    /* ---- 4. onto the label grid: outlines + the remainder ---- */
    auto seg = std::make_shared<Segmentation>();
    seg->gw = gw;
    seg->gh = gh;
    std::vector<cv::Mat> bins;
    cv::Mat covered(gh, gw, CV_8U, cv::Scalar(0));
    const double frame = double(gw) * double(gh);
    for (const Cand &k : kept) {
        if (cancelled(progress)) return nullptr;
        cv::Mat b = gridMask(k, gw, gh);
        const int a = cv::countNonZero(b);
        if (a < kMinArea * frame) continue;
        Segment s;
        s.rings = traceRings(b, gw, gh);
        if (s.rings.isEmpty()) continue;
        s.path = ringsPath(s.rings);
        const cv::Rect bb = cv::boundingRect(b);
        s.box = QRectF(double(bb.x) / gw, double(bb.y) / gh,
                       double(bb.width) / gw, double(bb.height) / gh);
        s.area = a / frame;
        s.depth = k.depth;
        covered |= b;
        seg->segs << s;
        bins.push_back(b);
    }
    /* Whatever no prompt claimed, so every sizeable region of the frame can be clicked.
       Opened first so the hairline gaps between neighbouring masks do not count. */
    {
        cv::Mat unc = (covered == 0);
        const int k = std::max(3, (std::max(gw, gh) / 128) | 1);
        cv::morphologyEx(unc, unc, cv::MORPH_OPEN,
                         cv::getStructuringElement(cv::MORPH_ELLIPSE, {k, k}));
        cv::Mat lab, st, cen;
        const int n = cv::connectedComponentsWithStats(unc, lab, st, cen, 8, CV_32S);
        for (int i = 1; i < n; ++i) {
            const int a = st.at<int>(i, cv::CC_STAT_AREA);
            if (a < kRemainderMinArea * frame) continue;
            cv::Mat b = (lab == i);
            Segment s;
            s.rings = traceRings(b, gw, gh);
            if (s.rings.isEmpty()) continue;
            s.path = ringsPath(s.rings);
            s.box = QRectF(double(st.at<int>(i, cv::CC_STAT_LEFT)) / gw,
                           double(st.at<int>(i, cv::CC_STAT_TOP)) / gh,
                           double(st.at<int>(i, cv::CC_STAT_WIDTH)) / gw,
                           double(st.at<int>(i, cv::CC_STAT_HEIGHT)) / gh);
            s.area = a / frame;
            s.depth = 0;
            s.remainder = true;
            seg->segs << s;
            bins.push_back(b);
        }
    }
    stage(0.85);

    /* ---- 5. area order + the per-level label maps ---- */
    std::vector<int> order(size_t(seg->segs.size()));
    for (size_t i = 0; i < order.size(); ++i) order[i] = int(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return seg->segs[a].area > seg->segs[b].area;
    });
    QVector<Segment> sorted;
    std::vector<cv::Mat> sortedBins;
    for (int i : order) { sorted << seg->segs[i]; sortedBins.push_back(bins[size_t(i)]); }
    seg->segs = sorted;
    /* int16 labels: more than 32767 segments would need a 16x16 grid to find 128 per
       prompt, which SAM does not do. */
    for (int L = 0; L < kLevels; ++L) {
        cv::Mat lab(gh, gw, CV_16S, cv::Scalar(-1));
        for (int i = 0; i < seg->segs.size() && i < 32767; ++i) {
            if (seg->segs[i].depth > L) continue;
            const cv::Rect bb = cv::boundingRect(sortedBins[size_t(i)]);
            lab(bb).setTo(cv::Scalar(i), sortedBins[size_t(i)](bb));
        }
        seg->label[L].assign(lab.ptr<int16_t>(), lab.ptr<int16_t>() + size_t(gw) * gh);
    }
    report(progress, pctTo);
    return seg;
}

std::shared_ptr<const Segmentation> segment(const QImage &guide, const QString &encoderPath,
                                            const QString &decoderPath, int threads,
                                            Progress *progress, QString *error)
{
    if (G::isLogger) G::log("Segmenter::segment");
    auto fail = [&](const QString &e) -> std::shared_ptr<const Segmentation> {
        if (error) *error = e;
        return nullptr;
    };
    if (guide.isNull() || guide.width() <= 0 || guide.height() <= 0)
        return fail(QObject::tr("There is no image to segment."));
    const int S = 1024;                     // the encoder's square input
    threads = std::max(1, threads);

    /* ---- encode, on a net freed before the decoders load: the encoder's activations
       peak at ~2.4 GB, and they are not needed once the embedding exists. ---- */
    cv::Mat embed, feats0, feats1;
    {
        cv::dnn::Net enc;
        try {
            enc = cv::dnn::readNetFromONNX(encoderPath.toStdString());
        } catch (const cv::Exception &e) {
            return fail(QObject::tr("The SAM 2 encoder failed to load: %1").arg(e.what()));
        }
        if (enc.empty()) return fail(QObject::tr("The SAM 2 encoder failed to load."));
        report(progress, 2);
        std::vector<cv::String> names = enc.getUnconnectedOutLayersNames();
        std::vector<cv::Mat> outs;
        try {
            enc.setInput(preprocess(guide, S), "image");
            enc.forward(outs, names);
        } catch (const cv::Exception &e) {
            return fail(QObject::tr("SAM 2 could not encode the image: %1").arg(e.what()));
        }
        for (size_t i = 0; i < names.size() && i < outs.size(); ++i) {
            const std::string n(names[i]);
            if      (n == "image_embed")      embed  = outs[i].clone();
            else if (n == "high_res_feats_0") feats0 = outs[i].clone();
            else if (n == "high_res_feats_1") feats1 = outs[i].clone();
        }
    }
    if (embed.empty() || feats0.empty() || feats1.empty())
        return fail(QObject::tr("SAM 2's encoder did not produce the expected outputs."));
    if (cancelled(progress)) return nullptr;
    report(progress, 20);

    /* ---- the decoder pool: one net per worker, loaded by the worker on its first
       prompt and kept across the gap rounds. ---- */
    std::vector<cv::dnn::Net> nets(static_cast<size_t>(threads));
    /* The ONNX import is serialized: concurrent imports are not documented as safe, and
       the first workers are already decoding while the later ones wait their turn. */
    std::mutex loadMutex;
    std::atomic<bool> loadFailed{false};
    std::atomic<int> decoded{0};
    /* The grid is ~85% of the decodes; the gap rounds add up to 48 more. */
    const int expected = kGridPoints + kGapRounds * kGapPerRound / 2;

    DecodeFn decode = [&](const std::vector<QPointF> &pts) {
        std::vector<Candidate> out(pts.size() * 3);
        std::atomic<int> next{0};
        auto work = [&](int t) {
            cv::dnn::Net &d = nets[size_t(t)];
            if (d.empty()) {
                std::lock_guard<std::mutex> lock(loadMutex);
                try { d = cv::dnn::readNetFromONNX(decoderPath.toStdString()); }
                catch (const cv::Exception &) {}
                if (d.empty()) { loadFailed = true; return; }
            }
            const std::vector<cv::String> outNames = d.getUnconnectedOutLayersNames();
            const int pcDims[3] = {1, 2, 2}, plDims[2] = {1, 2};
            const int miDims[4] = {1, 1, kMaskRes, kMaskRes}, hmDims[1] = {1};
            cv::Mat pc(3, pcDims, CV_32F), pl(2, plDims, CV_32F);
            cv::Mat mi(4, miDims, CV_32F, cv::Scalar(0)), hm(1, hmDims, CV_32F, cv::Scalar(0));
            /* One positive point; the decoder is exported with two point slots, so the
               second is the padding point SAM expects (label -1). No dense prompt. */
            pl.ptr<float>()[0] = 1.0f;
            pl.ptr<float>()[1] = -1.0f;
            std::vector<cv::Mat> o;
            for (;;) {
                if (cancelled(progress) || loadFailed) return;
                const int k = next++;
                if (k >= int(pts.size())) return;
                float *c = pc.ptr<float>();
                c[0] = float(pts[size_t(k)].x() * S);
                c[1] = float(pts[size_t(k)].y() * S);
                c[2] = 0.0f;
                c[3] = 0.0f;
                try {
                    d.setInput(embed,  "image_embed");
                    d.setInput(feats0, "high_res_feats_0");
                    d.setInput(feats1, "high_res_feats_1");
                    d.setInput(pc, "point_coords");
                    d.setInput(pl, "point_labels");
                    d.setInput(mi, "mask_input");
                    d.setInput(hm, "has_mask_input");
                    d.forward(o, outNames);
                } catch (const cv::Exception &) {
                    continue;                   // an empty candidate never qualifies
                }
                cv::Mat masks, iou;
                for (const cv::Mat &m : o) { if (m.dims == 4) masks = m; else iou = m; }
                if (masks.empty() || iou.empty() || masks.size[1] < 3) continue;
                for (int j = 0; j < 3; ++j) {
                    Candidate &cd = out[size_t(k) * 3 + size_t(j)];
                    cd.px = float(pts[size_t(k)].x());
                    cd.py = float(pts[size_t(k)].y());
                    cd.iou = iou.ptr<float>()[j];
                    const float *m = masks.ptr<float>(0, j);
                    cd.logits.assign(m, m + size_t(kMaskRes) * kMaskRes);
                }
                const int done = ++decoded;
                report(progress, 20 + std::min(70, 70 * done / expected));
            }
        };
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t) pool.emplace_back(work, t);
        for (std::thread &th : pool) th.join();
        return out;
    };

    /* The label grid: the guide's aspect, long edge kGrid. */
    const double ls = double(kGrid) / std::max(guide.width(), guide.height());
    const int gw = std::max(1, int(std::lround(guide.width()  * ls)));
    const int gh = std::max(1, int(std::lround(guide.height() * ls)));
    auto seg = assemble(decode, gw, gh, progress, 90, 100);
    if (loadFailed) return fail(QObject::tr("The SAM 2 decoder failed to load."));
    return seg;
}

/* ---- paramsJson ------------------------------------------------------------------- */

QJsonObject segmentJson(const Segment &s)
{
    QJsonArray rings;
    for (const QPolygonF &r : s.rings) {
        QJsonArray flat;
        for (const QPointF &p : r) { flat.append(round5(p.x())); flat.append(round5(p.y())); }
        rings.append(flat);
    }
    QJsonObject o;
    o["rings"] = rings;
    return o;
}

QPainterPath pathFromJson(const QJsonObject &segment)
{
    QPainterPath p;
    p.setFillRule(Qt::OddEvenFill);
    for (const QJsonValue &rv : segment.value("rings").toArray()) {
        const QJsonArray flat = rv.toArray();
        if (flat.size() < 6) continue;
        QPolygonF r;
        r.reserve(flat.size() / 2);
        for (int i = 0; i + 1 < flat.size(); i += 2)
            r << QPointF(flat.at(i).toDouble(), flat.at(i + 1).toDouble());
        p.addPolygon(r);
        p.closeSubpath();
    }
    return p;
}

QJsonArray selection(const QString &paramsJson)
{
    if (paramsJson.isEmpty()) return {};
    return QJsonDocument::fromJson(paramsJson.toUtf8()).object().value("segments").toArray();
}

QString paramsWith(const QJsonArray &segments)
{
    QJsonObject o;
    o["segments"] = segments;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool hasSelection(const QString &paramsJson)
{
    return !selection(paramsJson).isEmpty();
}

bool rasterize(const QString &paramsJson, int W, int H, std::vector<float> &alpha)
{
    alpha.clear();
    const QJsonArray segs = selection(paramsJson);
    if (segs.isEmpty() || W <= 0 || H <= 0) return false;
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.scale(W, H);                      // mask-normalized -> pixels
        for (const QJsonValue &v : segs) p.drawPath(pathFromJson(v.toObject()));
    }
    alpha.resize(size_t(W) * size_t(H));
    for (int y = 0; y < H; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        float *a = alpha.data() + size_t(y) * W;
        for (int x = 0; x < W; ++x) a[x] = qAlpha(line[x]) / 255.0f;
    }
    return true;
}

bool matte(const QString &paramsJson, const QImage &guide, std::vector<float> &cov,
           int &w, int &h)
{
    if (guide.isNull()) return false;
    std::vector<float> alpha;
    if (!rasterize(paramsJson, guide.width(), guide.height(), alpha)) return false;
    const cv::Mat a(guide.height(), guide.width(), CV_32F, alpha.data());
    return SegRefine::refine(a, guide, cov, w, h, SegRefine::Mode::Alpha);
}

/* ---- Job -------------------------------------------------------------------------- */

namespace {
/* The GUI-thread object the finished run is posted to. If the Job is gone, so is this,
   and Qt drops the queued call instead of running it on a dead Job. */
struct JobContext : QObject {};
}

Job::~Job()
{
    shutdown();
}

bool Job::start(const QString &key, const QImage &guide, const QString &encoderPath,
                const QString &decoderPath, int threads, Done done)
{
    if (thread) return false;
    if (!ctx) ctx = std::make_unique<JobContext>();
    prog = std::make_shared<Progress>();
    runningKey = key;
    const int gen = ++generation;
    std::shared_ptr<Progress> p = prog;
    QObject *context = ctx.get();
    thread = QThread::create([this, p, key, guide, encoderPath, decoderPath, threads,
                              done, gen, context] {
        QString err;
        std::shared_ptr<const Segmentation> result =
            segment(guide, encoderPath, decoderPath, threads, p.get(), &err);
        QMetaObject::invokeMethod(context, [this, key, result, err, done, gen] {
            if (gen != generation) return;          // shut down, or superseded
            if (thread) { thread->wait(); delete thread; thread = nullptr; }
            runningKey.clear();
            if (done) done(key, result, err);
        }, Qt::QueuedConnection);
    });
    thread->start();
    return true;
}

void Job::cancel()
{
    if (prog) prog->cancel = true;
}

void Job::shutdown(int maxWaitMs)
{
    if (!thread) return;
    ++generation;                           // a result still in the queue is dropped
    cancel();
    if (thread->wait(QDeadlineTimer(maxWaitMs))) delete thread;
    /* Not finished in time: quitting anyway, so the thread object is left to the OS
       rather than deleted while it runs. */
    thread = nullptr;
    runningKey.clear();
}

}   // namespace Segmenter
