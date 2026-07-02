#include "subjectpredictor.h"
#include "Utilities/segrefine.h"
#include <algorithm>
#include <cmath>

/*
    U^2-Net subject (salient object) segmentation via OpenCV DNN. See the header for the role in the
    "Select Subject" develop mask. Documentation: SELECT SUBJECT MASK in notes/Documentation.txt.
*/

SubjectPredictor::SubjectPredictor(const QString& onnxPath, int inputSize)
    : imageSize(inputSize)
{
    if (G::isLogger)
        G::log("SubjectPredictor::SubjectPredictor", onnxPath);
    try {
        net = cv::dnn::readNetFromONNX(onnxPath.toStdString());
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception& e) {
        qWarning("Failed to load subject ONNX model: %s", e.what());
    }
}

bool SubjectPredictor::isLoaded() const {
    return !net.empty();
}

bool SubjectPredictor::predict(const QImage& image, std::vector<float>& cov, int& w, int& h)
{
    if (net.empty()) return false;

    const int sz = imageSize;

    /* Resize to the model's square input (aspect ignored -- coverage is sampled by normalized
       coords, so the squish cancels between input and output). RGB, 8-bit. */
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888)
                            .scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    /* U^2-Net preprocessing (matches the reference ToTensorLab): divide by the image's max pixel
       value, then per-channel ImageNet mean/std. Build the NCHW tensor by hand because blobFromImage
       cannot apply a per-channel std divide. */
    float mx = 1e-5f;
    for (int y = 0; y < sz; ++y) {
        const uchar *line = rgb.constScanLine(y);
        for (int x = 0; x < sz * 3; ++x)
            mx = std::max(mx, float(line[x]));
    }
    const float invMax = 1.0f / mx;
    static const float mean[3] = {0.485f, 0.456f, 0.406f};
    static const float istd[3] = {1.0f/0.229f, 1.0f/0.224f, 1.0f/0.225f};

    std::vector<float> in(size_t(3) * sz * sz);
    const size_t plane = size_t(sz) * sz;
    for (int y = 0; y < sz; ++y) {
        const uchar *line = rgb.constScanLine(y);
        for (int x = 0; x < sz; ++x) {
            const size_t idx = size_t(y) * sz + x;
            for (int c = 0; c < 3; ++c)          // RGB888 is R,G,B in memory
                in[c * plane + idx] = (line[x * 3 + c] * invMax - mean[c]) * istd[c];
        }
    }

    const int dims[4] = {1, 3, sz, sz};
    cv::Mat blob(4, dims, CV_32F, in.data());

    /* U^2-Net exposes 7 outputs (d0..d6). OpenCV's forward() with no name returns only the last-
       registered layer, which for this graph is a COARSE side output (d6 -- a fuzzy blob). Fetch the
       outputs by name and use outs[0] = d0, the fused full-resolution saliency (confirmed by dumping
       every output: outs[0..3] are crisp full silhouettes, outs[4..6] progressively coarser). */
    std::vector<cv::String> outNames = net.getUnconnectedOutLayersNames();
    std::vector<cv::Mat> outs;
    try {
        net.setInput(blob);
        net.forward(outs, outNames);
    } catch (const cv::Exception& e) {
        qWarning("Subject inference failed: %s", e.what());
        return false;
    }
    if (outs.empty() || outs[0].empty()) return false;
    const cv::Mat out = outs[0];                  // d0 (fused saliency, sigmoid-activated)

    /* out is [1,1,sz,sz]; wrap the plane as a 2D float image and hand it to the shared refiner
       (min-max normalize -> upsample -> guided-filter against the photo -> contrast). */
    cv::Mat sal(sz, sz, CV_32F, const_cast<float*>(out.ptr<float>()));
    return SegRefine::refine(sal, image, cov, w, h);
}
