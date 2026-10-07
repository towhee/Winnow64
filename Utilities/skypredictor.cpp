#include "skypredictor.h"
#include "Utilities/skyrefine.h"
#include <algorithm>
#include <cmath>

/*
    Two-model sky segmentation via OpenCV DNN. See the header, Utilities/skyrefine.h and
    SELECT SKY MASK in notes/Documentation.txt.
*/

namespace {

constexpr int kSkysegSize = 320;
constexpr int kSegformerSize = 512;
constexpr int kAdeSkyClass = 2;

cv::dnn::Net loadNet(const QString &path, const char *what)
{
    cv::dnn::Net net;
    try {
        net = cv::dnn::readNetFromONNX(path.toStdString());
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception &e) {
        qWarning("Failed to load %s: %s", what, e.what());
    }
    return net;
}

/* NCHW blob of rgb (CV_32FC3, RGB, 0..1) resized to sz x sz, ImageNet-normalized. Built
   by hand: blobFromImage cannot apply a per-channel std divide. */
cv::Mat blob(const cv::Mat &rgb, int sz)
{
    static const float mean[3] = {0.485f, 0.456f, 0.406f};
    static const float istd[3] = {1.0f / 0.229f, 1.0f / 0.224f, 1.0f / 0.225f};
    cv::Mat small;
    cv::resize(rgb, small, cv::Size(sz, sz), 0, 0, cv::INTER_AREA);
    const int dims[4] = {1, 3, sz, sz};
    cv::Mat b(4, dims, CV_32F);
    float *o = b.ptr<float>();
    const size_t plane = size_t(sz) * sz;
    for (int y = 0; y < sz; ++y) {
        const cv::Vec3f *p = small.ptr<cv::Vec3f>(y);
        for (int x = 0; x < sz; ++x)
            for (int c = 0; c < 3; ++c)
                o[c * plane + size_t(y) * sz + x] = (p[x][c] - mean[c]) * istd[c];
    }
    return b;
}

/* Forward pass returning the FIRST unconnected output. For skyseg that is d0, the fused
   full-res map (forward() with no name returns the last-registered layer, a coarse side
   output -- the old Subject "rough selection" bug). */
cv::Mat forwardFirst(cv::dnn::Net &net, const cv::Mat &in)
{
    std::vector<cv::Mat> outs;
    net.setInput(in);
    net.forward(outs, net.getUnconnectedOutLayersNames());
    return outs.empty() ? cv::Mat() : outs[0];
}

}   // namespace

SkyPredictor::SkyPredictor(const QString &skysegPath, const QString &segformerPath)
{
    if (G::isLogger) G::log("SkyPredictor::SkyPredictor", skysegPath + " | " + segformerPath);
    skyNet = loadNet(skysegPath, "skyseg.onnx");
    segNet = loadNet(segformerPath, "sky_segformer.onnx");
}

bool SkyPredictor::isLoaded() const
{
    return !skyNet.empty() && !segNet.empty();
}

bool SkyPredictor::predict(const QImage &image, std::vector<uint8_t> &alpha, int &w, int &h)
{
    if (!isLoaded() || image.isNull()) return false;

    const QImage rgb8 = image.convertToFormat(QImage::Format_RGB888);
    w = rgb8.width();
    h = rgb8.height();
    cv::Mat rgb(h, w, CV_32FC3);
    for (int y = 0; y < h; ++y) {
        const uchar *line = rgb8.constScanLine(y);
        cv::Vec3f *o = rgb.ptr<cv::Vec3f>(y);
        for (int x = 0; x < w; ++x)                 // RGB888 is R,G,B in memory
            o[x] = cv::Vec3f(line[x * 3] / 255.0f, line[x * 3 + 1] / 255.0f,
                             line[x * 3 + 2] / 255.0f);
    }

    cv::Mat skyOut, segOut;
    try {
        skyOut = forwardFirst(skyNet, blob(rgb, kSkysegSize));
        segOut = forwardFirst(segNet, blob(rgb, kSegformerSize));
    } catch (const cv::Exception &e) {
        qWarning("Sky inference failed: %s", e.what());
        return false;
    }
    if (skyOut.empty() || segOut.empty() || skyOut.dims < 3 || segOut.dims != 4) return false;

    /* skyseg: [1,1,H,W] sigmoid sky probability, used as is (calibrated -- see
       skyrefine.h step 1). */
    const int sH = skyOut.size[skyOut.dims - 2], sW = skyOut.size[skyOut.dims - 1];
    const cv::Mat pSky(sH, sW, CV_32F, skyOut.ptr<float>());

    /* SegFormer: [1,150,H,W] logits -> softmax probability of the sky class. */
    const int C = segOut.size[1], fH = segOut.size[2], fW = segOut.size[3];
    if (C <= kAdeSkyClass) return false;
    const float *lg = segOut.ptr<float>();
    const size_t plane = size_t(fH) * fW;
    cv::Mat pSeg(fH, fW, CV_32F);
    float *ps = pSeg.ptr<float>();
    for (size_t i = 0; i < plane; ++i) {
        float mx = lg[i];
        for (int c = 1; c < C; ++c) mx = std::max(mx, lg[c * plane + i]);
        double sum = 0.0;
        for (int c = 0; c < C; ++c) sum += std::exp(double(lg[c * plane + i] - mx));
        ps[i] = float(std::exp(double(lg[kAdeSkyClass * plane + i] - mx)) / sum);
    }

    const cv::Mat a = SkyRefine::refine(pSky, pSeg, rgb);
    alpha.resize(size_t(w) * h);
    for (int y = 0; y < h; ++y) {
        const float *ap = a.ptr<float>(y);
        uint8_t *op = alpha.data() + size_t(y) * w;
        for (int x = 0; x < w; ++x) op[x] = uint8_t(std::lround(ap[x] * 255.0f));
    }
    return true;
}
