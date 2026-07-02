#include "depthpredictor.h"
#include "Utilities/segrefine.h"
#include <algorithm>
#include <cmath>

/*
    MiDaS v2.1 small monocular depth via OpenCV DNN. See the header + DEPTH RANGE MASK in
    notes/Documentation.txt. Upsampling shared with the AI masks in Utilities/segrefine.h.
*/

DepthPredictor::DepthPredictor(const QString& onnxPath, int inputSize)
    : imageSize(inputSize)
{
    if (G::isLogger)
        G::log("DepthPredictor::DepthPredictor", onnxPath);
    try {
        net = cv::dnn::readNetFromONNX(onnxPath.toStdString());
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception& e) {
        qWarning("Failed to load depth ONNX model: %s", e.what());
    }
}

bool DepthPredictor::isLoaded() const {
    return !net.empty();
}

bool DepthPredictor::predict(const QImage& image, std::vector<float>& depth, int& w, int& h)
{
    if (net.empty()) return false;

    const int sz = imageSize;
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888)
                            .scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    /* /255 then per-channel ImageNet mean/std (MiDaS small), built by hand (blobFromImage cannot do a
       per-channel std divide). */
    static const float mean[3] = {0.485f, 0.456f, 0.406f};
    static const float istd[3] = {1.0f/0.229f, 1.0f/0.224f, 1.0f/0.225f};
    std::vector<float> in(size_t(3) * sz * sz);
    const size_t plane = size_t(sz) * sz;
    for (int y = 0; y < sz; ++y) {
        const uchar *line = rgb.constScanLine(y);
        for (int x = 0; x < sz; ++x) {
            const size_t idx = size_t(y) * sz + x;
            for (int c = 0; c < 3; ++c)
                in[c * plane + idx] = (line[x * 3 + c] / 255.0f - mean[c]) * istd[c];
        }
    }

    const int dims[4] = {1, 3, sz, sz};
    cv::Mat blob(4, dims, CV_32F, in.data());

    std::vector<cv::String> outNames = net.getUnconnectedOutLayersNames();
    std::vector<cv::Mat> outs;
    try {
        net.setInput(blob);
        net.forward(outs, outNames);
    } catch (const cv::Exception& e) {
        qWarning("Depth inference failed: %s", e.what());
        return false;
    }
    if (outs.empty() || outs[0].empty()) return false;
    const cv::Mat out = outs[0];                  // [1,H,W] inverse depth (high = near)

    const int od = out.dims;
    const int H = out.size[od - 2], W = out.size[od - 1];
    cv::Mat inv(H, W, CV_32F, const_cast<float*>(out.ptr<float>()));

    /* invert=true: MiDaS is inverse depth (high=near); flip to 0=near..1=far after min-max, then
       guided-upsample against the photo (continuous field, edge-aware). */
    return SegRefine::guidedUpsample(inv, image, /*invert*/true, depth, w, h);
}
