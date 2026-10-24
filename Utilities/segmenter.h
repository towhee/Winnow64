#ifndef SEGMENTER_H
#define SEGMENTER_H

#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class QThread;

/*
    SEGMENTER -- the engine behind the Develop "Segmenter" mask tool.

    The tool cuts the photo into the objects it is made of, highlights each one as the
    pointer passes over it, and toggles it in or out of the submask on a click. The
    submask is the union of the objects selected.

    SEGMENT EVERYTHING. SAM 2 is a PROMPTABLE segmenter: given a point it returns three
    candidate masks for the object under that point (roughly sub-part, part and whole)
    plus a predicted IoU for each. "Automatic mask generation" prompts a grid of points
    and keeps the candidates that are both confident and STABLE (the mask barely moves
    when its logit threshold is nudged), then drops near-duplicates. That is SAM's own
    recipe (segment_anything / sam2 AutomaticMaskGenerator), run here through the SAME
    sam2_encoder/sam2_decoder pair the Object Mask uses -- no new model, no new runtime.

    WHAT RUNS:
      1. encode the frame once (~1.3 s CPU)
      2. a 16 x 16 point grid, decoded in parallel on N private decoder nets
         (~110 ms per prompt on one core; ~2.2 s for the grid on 16 threads)
      3. filter (predicted IoU, stability), clean (fill small holes, drop specks), and
         de-duplicate by MASK IoU (a whole object and its part overlap too little to be
         merged by a box test, so mask IoU keeps both)
      4. GAP FILL: the grid misses amorphous "stuff" -- dark foliage, a wall -- whose
         points come back unconfident. Up to three rounds prompt the uncovered regions
         again, spaced by a greedy farthest-point pick, with relaxed thresholds, keeping
         a candidate only if it mostly lies in the gap. Measured on six test photos:
         the unsegmented area on the dark forest behind a car fell from 36% to 11%.
      5. what is STILL uncovered becomes "remainder" segments, so every sizeable region
         of the frame can be clicked
      6. a containment HIERARCHY: a segment's parent is the smallest larger segment that
         holds >= 80% of it. depth 0 = a whole object, 1 = a part of one, 2+ = finer.
         The Detail setting offers depth 0 (Objects), <= 1 (Parts) or all (Details).
      7. outlines: each segment's logits are upsampled to the label grid (long edge
         kGrid), thresholded at SAM's zero crossing and traced into polygon rings.

    WHY OUTLINES ARE WHAT IS SAVED. The submask's paramsJson records the selected
    segments' RINGS, not the prompts that produced them. The render then needs no model
    at all -- a cold open, an export, or a machine without the SAM download all
    rasterize the same polygons -- and a later model swap (AI masks carry no backward
    compatibility, see notes/Documentation.txt) cannot silently move a saved mask. The
    alternative, persisting (point, candidate) and re-decoding, would tie every sidecar
    to these exact weights and to the exact developed pixels the encoder saw.

    COORDINATES are MASK-normalized: 0..1 of the oriented, UNCROPPED frame -- the space
    every other mask tool stores (see ImageView::maskNormToItem).

    THREADING. segment() runs on a worker (Job below): it loads its OWN encoder and
    decoder nets and frees them before returning, so nothing is shared with the GUI
    thread's ObjectMaskPredictor. The rest of this header is pure and thread-agnostic.
*/
namespace Segmenter {

/* The Detail setting: which depths of the hierarchy are offered on the canvas. */
enum Level { Objects = 0, Parts = 1, Details = 2 };
constexpr int kLevels = 3;
QString levelName(int level);

/* Long edge of the label grid the outlines are traced on. The decoder's own masks are
   256^2 over a squashed frame, so this is already ~4x their linear resolution; the
   render snaps the edge to the photo anyway (SegRefine). */
constexpr int kGrid = 1024;

struct Segment {
    QVector<QPolygonF> rings;   // outline, mask-normalized; even-odd (holes are rings)
    QPainterPath path;          // the rings as one OddEvenFill path, mask-normalized
    QRectF box;                 // mask-normalized bounding box
    double area = 0.0;          // fraction of the frame
    int depth = 0;              // 0 = a whole object, 1 = a part of one, 2+ finer
    bool remainder = false;     // a region no prompt claimed (between objects)
};

struct Segmentation {
    int gw = 0, gh = 0;                         // label grid (aspect-correct)
    QVector<Segment> segs;                      // area-descending
    /* Per Detail level, the segment offered under each grid cell, -1 = none. Painted
       largest first, so the SMALLEST segment at that level wins a cell. */
    std::vector<int16_t> label[kLevels];
    /* Segment under mask-normalized n at a Detail level -- or, where that level offers
       nothing, the next finer level that does -- -1 if none. */
    int at(QPointF n, int level) const;
    /* How many segments a Detail level offers. */
    int count(int level) const;
};

/* ---- One SAM 2 decode, as the post-processing sees it. ----
   logits: kMaskRes^2 row-major, the decoder's low-res mask over the SQUASHED frame
   (SAM 2 resizes the image to 1024^2 without keeping the aspect, so these cells are
   not square on the photo). px/py: the prompt point, mask-normalized. */
constexpr int kMaskRes = 256;
struct Candidate {
    float px = 0.0f, py = 0.0f;
    float iou = 0.0f;
    std::vector<float> logits;
};

/* Decode a batch of point prompts: THREE candidates per point, in point order. */
using DecodeFn = std::function<std::vector<Candidate>(const std::vector<QPointF> &)>;

/* Progress + cancel shared with a worker. pct 0..100. */
struct Progress {
    std::atomic<int>  pct{0};
    std::atomic<bool> cancel{false};
};

/* Steps 2-7 above, on whatever `decode` returns -- pure post-processing over the
   candidates, so it is testable without the model (tests/unit/tst_segmenter.cpp).
   gw/gh: the label grid. pctFrom..pctTo: the share of `progress` this stage reports.
   Returns null if cancelled. */
std::shared_ptr<const Segmentation> assemble(const DecodeFn &decode, int gw, int gh,
                                             Progress *progress = nullptr,
                                             int pctFrom = 0, int pctTo = 100);

/* The whole run: encode `guide` (the developed, output-oriented, UNCROPPED frame),
   then assemble(). threads = how many decoder nets run in parallel (~120 MB each,
   freed on return). Null with *error set if a model failed to load or the run was
   cancelled (error empty). Worker thread. */
std::shared_ptr<const Segmentation> segment(const QImage &guide, const QString &encoderPath,
                                            const QString &decoderPath, int threads,
                                            Progress *progress, QString *error);

/* ---- The submask's paramsJson: {"segments":[{"rings":[[x,y,x,y,...],...]},...]} ----
   x,y mask-normalized, rounded to 1e-5. Each entry is one selected segment; the
   submask is their union. */
QJsonObject segmentJson(const Segment &s);
QPainterPath pathFromJson(const QJsonObject &segment);   // one entry -> OddEvenFill path
QJsonArray selection(const QString &paramsJson);         // the "segments" array
QString paramsWith(const QJsonArray &segments);          // compact paramsJson
bool hasSelection(const QString &paramsJson);
/* The union of the selected segments as antialiased coverage, W*H row-major 0..1, in
   mask space (the render's rasterizer -- the overlay draws the same rings as paths).
   False if nothing is selected. */
bool rasterize(const QString &paramsJson, int W, int H, std::vector<float> &alpha);
/* The submask's coverage: the outlines rasterized at the guide's size, then snapped to
   the photo's own edges (SegRefine, Mode::Alpha) -- an outline traced from SAM's 256^2
   masks lands near the edge, the guided filter puts it on it. guide: the developed,
   output-oriented, uncropped frame. cov is w*h, 0..1. False if nothing is selected. */
bool matte(const QString &paramsJson, const QImage &guide, std::vector<float> &cov,
           int &w, int &h);

/*
    Job -- runs segment() on its own QThread and hands the result back on the GUI
    thread. One run at a time: start() refuses while busy, so a caller that wants a
    different image cancels and retries when `done` reports. GUI thread only.
*/
class Job
{
public:
    /* key: what the caller is segmenting (path + orientation); result null on failure
       or cancel, error non-empty on failure. Called on the GUI thread. */
    using Done = std::function<void(const QString &key,
                                    std::shared_ptr<const Segmentation> result,
                                    const QString &error)>;
    ~Job();
    bool start(const QString &key, const QImage &guide, const QString &encoderPath,
               const QString &decoderPath, int threads, Done done);
    void cancel();                          // the run ends at its next check
    void shutdown(int maxWaitMs = 5000);    // cancel AND wait (quit path)
    bool isRunning() const { return thread != nullptr; }
    QString key() const { return runningKey; }
    int progress() const { return prog ? prog->pct.load() : 0; }

private:
    QThread *thread = nullptr;
    std::shared_ptr<Progress> prog;
    QString runningKey;
    /* The finished run is posted to ctx, so a run outliving its Job is dropped by Qt;
       generation retires a result that was queued before a shutdown. */
    std::unique_ptr<QObject> ctx;
    int generation = 0;
};

}   // namespace Segmenter

#endif // SEGMENTER_H
