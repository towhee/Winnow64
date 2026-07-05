#include "objectmaskpredictor.h"
#include "Utilities/segrefine.h"
#include "opencv2/imgproc.hpp"
#include <algorithm>
#include <cmath>

/*
    SAM 2 promptable object segmentation via OpenCV DNN. See the header for the two-phase role in the
    "Object Mask" develop tool. Documentation: OBJECT MASK in notes/Documentation.txt.

    Pipeline constants (SAM 2, confirmed against the samexporter reference and the sam_dnn spike):
      - encoder input 1024^2, RGB, (px/255 - mean)/std, ImageNet mean/std
      - point_coords scaled to the 1024 input space; a box is 2 points, labels 2 (TL) and 3 (BR)
      - mask_input is 256^2 unnormalized logits; has_mask_input=1 activates the dense prompt
      - decoder emits masks [1,3,256,256] (3 candidates) + iou_predictions [1,3]; pick best IoU
*/

/* Dense-prompt logit magnitude for the painted fill (validated in sam_dnn_brush_spike: +/-8 gives a
   confident prior without overriding the image evidence at the true edge). */
static constexpr float kMaskLogit = 8.0f;

ObjectMaskPredictor::ObjectMaskPredictor(const QString& encoderPath, const QString& decoderPath,
                                         int inputSize)
    : inputSize(inputSize)
{
    if (G::isLogger)
        G::log("ObjectMaskPredictor::ObjectMaskPredictor", encoderPath + " | " + decoderPath);
    try {
        encoder = cv::dnn::readNetFromONNX(encoderPath.toStdString());
        encoder.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        encoder.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception& e) {
        qWarning("Failed to load Object Mask encoder ONNX: %s", e.what());
    }
    try {
        decoder = cv::dnn::readNetFromONNX(decoderPath.toStdString());
        decoder.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        decoder.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception& e) {
        /* Most likely the dynamic-shape decoder (see header): needs the fixed-shape re-export. */
        qWarning("Failed to load Object Mask decoder ONNX (needs fixed-shape export?): %s", e.what());
    }
}

bool ObjectMaskPredictor::isLoaded() const { return !encoder.empty() && !decoder.empty(); }
bool ObjectMaskPredictor::hasImage() const { return imageSet; }

cv::Mat ObjectMaskPredictor::preprocess(const QImage& image) const
{
    const int S = inputSize;
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888)
                            .scaled(S, S, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    static const float mean[3] = {0.485f, 0.456f, 0.406f};
    static const float istd[3] = {1.0f/0.229f, 1.0f/0.224f, 1.0f/0.225f};

    const int dims[4] = {1, 3, S, S};
    cv::Mat blob(4, dims, CV_32F);                 // owns its data
    float* p = blob.ptr<float>();
    const size_t plane = size_t(S) * S;
    for (int y = 0; y < S; ++y) {
        const uchar* line = rgb.constScanLine(y);
        for (int x = 0; x < S; ++x) {
            const size_t idx = size_t(y) * S + x;
            for (int c = 0; c < 3; ++c)            // RGB888 is R,G,B in memory
                p[c * plane + idx] = (line[x*3 + c] / 255.0f - mean[c]) * istd[c];
        }
    }
    return blob;
}

bool ObjectMaskPredictor::setImage(const QImage& image)
{
    if (G::isLogger) G::log("ObjectMaskPredictor::setImage");
    imageSet = false;
    if (encoder.empty() || image.isNull()) return false;

    cv::Mat blob = preprocess(image);
    std::vector<cv::String> names = encoder.getUnconnectedOutLayersNames();
    std::vector<cv::Mat> outs;
    try {
        encoder.setInput(blob, "image");
        encoder.forward(outs, names);
    } catch (const cv::Exception& e) {
        qWarning("Object Mask encode failed: %s", e.what());
        return false;
    }

    /* Map by name (order is not guaranteed) and clone so the tensors outlive the net's scratch. */
    for (size_t i = 0; i < names.size(); ++i) {
        const std::string n(names[i]);
        if      (n == "image_embed")      imageEmbed    = outs[i].clone();
        else if (n == "high_res_feats_0") highResFeats0 = outs[i].clone();
        else if (n == "high_res_feats_1") highResFeats1 = outs[i].clone();
    }
    if (imageEmbed.empty() || highResFeats0.empty() || highResFeats1.empty()) {
        qWarning("Object Mask encode: missing expected encoder outputs");
        return false;
    }

    guideImage = image;                            // edge-refinement guide (output-oriented)
    imageSet = true;
    return true;
}

void ObjectMaskPredictor::cleanupMask(cv::Mat& logits256)
{
    /* Binary foreground from the logits, de-specked. */
    cv::Mat fg = logits256 > 0.0f;                 // CV_8U 0/255
    if (cv::countNonZero(fg) == 0) return;         // nothing to clean
    cv::morphologyEx(fg, fg, cv::MORPH_OPEN,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, {3, 3}));

    /* Keep the largest connected component (drops branch/occluder specks). */
    cv::Mat labels, stats, centroids;
    const int n = cv::connectedComponentsWithStats(fg, labels, stats, centroids, 8);
    if (n > 1) {
        int best = 1;
        for (int i = 2; i < n; ++i)
            if (stats.at<int>(i, cv::CC_STAT_AREA) > stats.at<int>(best, cv::CC_STAT_AREA)) best = i;
        cv::Mat kept = (labels == best);           // CV_8U

        /* Fill interior holes: flood the background from the border, invert to get holes. */
        cv::Mat ff = kept.clone();
        cv::copyMakeBorder(ff, ff, 1, 1, 1, 1, cv::BORDER_CONSTANT, 0);
        cv::floodFill(ff, {0, 0}, 255);
        cv::Mat holes = (ff(cv::Rect(1, 1, kept.cols, kept.rows)) == 0);
        cv::Mat filled = kept | holes;

        /* Rebuild logits: force everything outside the kept region to background, and lift filled
           holes to foreground, while leaving SAM's soft edge logits inside the region untouched so
           SegRefine can still snap the matte to the true boundary. */
        logits256.setTo(-kMaskLogit, filled == 0);
        logits256.setTo(kMaskLogit, holes);
    }
}

bool ObjectMaskPredictor::refine(const std::vector<float>& brushCov, int bw, int bh,
                                 std::vector<float>& cov, int& w, int& h)
{
    if (G::isLogger) G::log("ObjectMaskPredictor::refine");
    if (!imageSet || decoder.empty()) return false;
    if (bw <= 0 || bh <= 0 || brushCov.size() != size_t(bw) * size_t(bh)) return false;

    const int S = inputSize;

    /* Brush fill -> cv::Mat, and its bounding box (stroke extent) for the box prompt. */
    cv::Mat brush(bh, bw, CV_32F, const_cast<float*>(brushCov.data()));
    cv::Mat bin = brush > 0.5f;                     // CV_8U
    const cv::Rect bb = cv::boundingRect(bin);
    if (bb.width <= 0 || bb.height <= 0) return false;   // empty stroke

    /* Dense prompt: painted fill -> 256^2 logits (+kMaskLogit inside / -kMaskLogit outside). */
    cv::Mat brush256; cv::resize(brush, brush256, cv::Size(256, 256), 0, 0, cv::INTER_LINEAR);
    const int miDims[4] = {1, 1, 256, 256};
    cv::Mat mask_input(4, miDims, CV_32F);
    for (int y = 0; y < 256; ++y) {
        const float* b = brush256.ptr<float>(y);
        float* m = mask_input.ptr<float>(0, 0, y);
        for (int x = 0; x < 256; ++x) m[x] = b[x] > 0.5f ? kMaskLogit : -kMaskLogit;
    }
    const int hmDims[1] = {1};
    cv::Mat has_mask_input(1, hmDims, CV_32F);
    has_mask_input.at<float>(0) = 1.0f;             // activate the dense prompt

    /* Box prompt (stroke bbox), scaled to the 1024 input space; labels 2 (TL) and 3 (BR). */
    const int pcDims[3] = {1, 2, 2};
    cv::Mat point_coords(3, pcDims, CV_32F);
    float* pc = point_coords.ptr<float>();
    pc[0] = float(bb.x)            / bw * S;  pc[1] = float(bb.y)             / bh * S;
    pc[2] = float(bb.x + bb.width) / bw * S;  pc[3] = float(bb.y + bb.height) / bh * S;
    const int plDims[2] = {1, 2};
    cv::Mat point_labels(2, plDims, CV_32F);
    point_labels.ptr<float>()[0] = 2.0f; point_labels.ptr<float>()[1] = 3.0f;

    /* Decode. */
    std::vector<cv::String> names = decoder.getUnconnectedOutLayersNames();
    std::vector<cv::Mat> outs;
    try {
        decoder.setInput(imageEmbed,     "image_embed");
        decoder.setInput(highResFeats0,  "high_res_feats_0");
        decoder.setInput(highResFeats1,  "high_res_feats_1");
        decoder.setInput(point_coords,   "point_coords");
        decoder.setInput(point_labels,   "point_labels");
        decoder.setInput(mask_input,     "mask_input");
        decoder.setInput(has_mask_input, "has_mask_input");
        decoder.forward(outs, names);
    } catch (const cv::Exception& e) {
        qWarning("Object Mask decode failed: %s", e.what());
        return false;
    }

    /* Outputs: masks [1,3,256,256] + iou_predictions [1,3] (order not guaranteed -> pick by dims). */
    cv::Mat masks, iou;
    for (auto& o : outs) { if (o.dims == 4) masks = o; else iou = o; }
    if (masks.empty() || iou.empty()) return false;

    const float* iouP = iou.ptr<float>();
    int best = 0;
    for (int k = 1; k < 3; ++k) if (iouP[k] > iouP[best]) best = k;

    /* Best mask plane (logits) -> clean -> shared edge matting against the guide. */
    const int M = masks.size[2];                    // 256
    cv::Mat low(M, M, CV_32F, masks.ptr<float>(0, best));
    cv::Mat logits = low.clone();
    cleanupMask(logits);
    return SegRefine::refine(logits, guideImage, cov, w, h);
}

bool ObjectMaskPredictor::refinePoint(double onx, double ony,
                                      std::vector<float>& cov, int& w, int& h)
{
    if (G::isLogger) G::log("ObjectMaskPredictor::refinePoint");
    if (!imageSet || decoder.empty()) return false;

    const int S = inputSize;

    /* One positive point at the click (scaled to the 1024 input space), plus one padding point
       (label -1) to fill the decoder's fixed 2-point slot; no dense prompt (has_mask_input=0). */
    const int pcDims[3] = {1, 2, 2};
    cv::Mat point_coords(3, pcDims, CV_32F);
    float* pc = point_coords.ptr<float>();
    pc[0] = float(std::clamp(onx, 0.0, 1.0)) * S;  pc[1] = float(std::clamp(ony, 0.0, 1.0)) * S;
    pc[2] = 0.0f;                                   pc[3] = 0.0f;
    const int plDims[2] = {1, 2};
    cv::Mat point_labels(2, plDims, CV_32F);
    point_labels.ptr<float>()[0] = 1.0f; point_labels.ptr<float>()[1] = -1.0f;

    const int miDims[4] = {1, 1, 256, 256};
    cv::Mat mask_input(4, miDims, CV_32F);
    mask_input.setTo(0.0f);
    const int hmDims[1] = {1};
    cv::Mat has_mask_input(1, hmDims, CV_32F);
    has_mask_input.at<float>(0) = 0.0f;             // no dense prompt for the point path

    /* Decode. */
    std::vector<cv::String> names = decoder.getUnconnectedOutLayersNames();
    std::vector<cv::Mat> outs;
    try {
        decoder.setInput(imageEmbed,     "image_embed");
        decoder.setInput(highResFeats0,  "high_res_feats_0");
        decoder.setInput(highResFeats1,  "high_res_feats_1");
        decoder.setInput(point_coords,   "point_coords");
        decoder.setInput(point_labels,   "point_labels");
        decoder.setInput(mask_input,     "mask_input");
        decoder.setInput(has_mask_input, "has_mask_input");
        decoder.forward(outs, names);
    } catch (const cv::Exception& e) {
        qWarning("Brush AI auto-mask decode failed: %s", e.what());
        return false;
    }

    cv::Mat masks, iou;
    for (auto& o : outs) { if (o.dims == 4) masks = o; else iou = o; }
    if (masks.empty() || iou.empty()) return false;

    const float* iouP = iou.ptr<float>();
    int best = 0;
    for (int k = 1; k < 3; ++k) if (iouP[k] > iouP[best]) best = k;

    const int M = masks.size[2];                    // 256
    cv::Mat low(M, M, CV_32F, masks.ptr<float>(0, best));
    cv::Mat logits = low.clone();
    cleanupMask(logits);
    return SegRefine::refine(logits, guideImage, cov, w, h);
}
