#include "skypredictor.h"
#include "Utilities/segrefine.h"
#include <algorithm>
#include <cmath>

/*
    Single-channel sky segmentation via OpenCV DNN. See the header + SELECT SKY MASK in
    notes/Documentation.txt. The heavy lifting (edge refinement) is shared with the Subject mask in
    Utilities/segrefine.h.
*/

SkyPredictor::SkyPredictor(const QString& onnxPath, int inputSize)
    : imageSize(inputSize)
{
    if (G::isLogger)
        G::log("SkyPredictor::SkyPredictor", onnxPath);
    try {
        net = cv::dnn::readNetFromONNX(onnxPath.toStdString());
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception& e) {
        qWarning("Failed to load sky ONNX model: %s", e.what());
    }
}

bool SkyPredictor::isLoaded() const {
    return !net.empty();
}

bool SkyPredictor::predict(const QImage& image, std::vector<float>& cov, int& w, int& h)
{
    if (net.empty()) return false;

    const int sz = imageSize;
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888)
                            .scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    /* Standard segmentation preprocessing: /255 then per-channel ImageNet mean/std. Build the NCHW
       tensor by hand (blobFromImage cannot apply a per-channel std divide). */
    static const float mean[3] = {0.485f, 0.456f, 0.406f};
    static const float istd[3] = {1.0f/0.229f, 1.0f/0.224f, 1.0f/0.225f};
    std::vector<float> in(size_t(3) * sz * sz);
    const size_t plane = size_t(sz) * sz;
    for (int y = 0; y < sz; ++y) {
        const uchar *line = rgb.constScanLine(y);
        for (int x = 0; x < sz; ++x) {
            const size_t idx = size_t(y) * sz + x;
            for (int c = 0; c < 3; ++c)          // RGB888 is R,G,B in memory
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
        qWarning("Sky inference failed: %s", e.what());
        return false;
    }
    if (outs.empty() || outs[0].empty()) return false;
    const cv::Mat out = outs[0];                  // d0: the fused full-res sky map (confirmed by
                                                  // dumping all 7 outputs -- outs[0..1] are crisp)

    /* out is [1,C,H,W]. A binary sky net is C=1 (sky prob); a 2-class net is C=2 (bg, sky) -> take
       the sky channel (last). Wrap that plane as a 2D float image for the shared refiner. */
    const int od = out.dims;
    const int H = out.size[od - 2], W = out.size[od - 1];
    const int C = (od >= 3) ? out.size[od - 3] : 1;
    const int ch = (C == 2) ? 1 : 0;
    cv::Mat sal(H, W, CV_32F, const_cast<float*>(out.ptr<float>()) + size_t(ch) * H * W);

    return SegRefine::refine(sal, image, cov, w, h);
}
