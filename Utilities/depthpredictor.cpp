#include "depthpredictor.h"
#include "Utilities/inference/inferencesession.h"
#include "Utilities/segrefine.h"
#include <opencv2/imgproc.hpp>

/*
    Depth Anything V2 Small via the ONNX Runtime layer. See the header, and DEPTH RANGE
    MASK / LENS BLUR in notes/Documentation.md. Upsampling shared with the AI masks in
    Utilities/segrefine.h.
*/

DepthPredictor::DepthPredictor(const QString &onnxPath)
{
    if (G::isLogger) G::log("DepthPredictor::DepthPredictor", onnxPath);
    session = std::make_unique<InferenceSession>(onnxPath, InferenceDevice::CPU);
    if (!session->IsLoaded())
        qWarning("DepthPredictor: failed to load %s", onnxPath.toUtf8().constData());
}

DepthPredictor::~DepthPredictor() = default;

bool DepthPredictor::isLoaded() const
{
    return session && session->IsLoaded();
}

bool DepthPredictor::predict(const QImage &image, std::vector<float> &depth, int &w, int &h)
{
    if (!isLoaded() || image.isNull()) return false;

    /* Squashed to the model's square, bicubic, RGB 0..1 planar. */
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    const cv::Mat in(rgb.height(), rgb.width(), CV_8UC3,
                     const_cast<uchar *>(rgb.constBits()), size_t(rgb.bytesPerLine()));
    cv::Mat sq;
    cv::resize(in, sq, cv::Size(kSize, kSize), 0, 0, cv::INTER_CUBIC);

    Tensor t;
    t.shape = {1, 3, kSize, kSize};
    t.data.resize(size_t(3) * kSize * kSize);
    const size_t plane = size_t(kSize) * kSize;
    for (int y = 0; y < kSize; ++y) {
        const uchar *p = sq.ptr<uchar>(y);
        for (int x = 0; x < kSize; ++x) {
            const size_t i = size_t(y) * kSize + x;
            for (int c = 0; c < 3; ++c) t.data[c * plane + i] = p[3 * x + c] / 255.0f;
        }
    }

    std::vector<Tensor> out;
    if (!session->Run({"pixels"}, {t}, {"depth"}, out) || out.empty()) {
        qWarning("DepthPredictor: inference failed");
        return false;
    }
    const Tensor &d = out[0];
    if (d.shape.size() < 2) return false;
    const int H = int(d.shape[d.shape.size() - 2]), W = int(d.shape[d.shape.size() - 1]);
    if (size_t(W) * size_t(H) != d.data.size()) return false;
    const cv::Mat disp(H, W, CV_32F, const_cast<float *>(d.data.data()));

    /* invert = true: disparity is high = near; flip to 0 = near .. 1 = far after the
       min-max, then guided-upsample against the photo (continuous, edge-aware). */
    return SegRefine::guidedUpsample(disp, image, /*invert*/true, depth, w, h);
}
