#include "Utilities/inference/miganfill.h"
#include "Utilities/inference/inferencesession.h"
#include "Develop/fillspot.h"
#include "Main/global.h"

#include <QDir>
#include <QCoreApplication>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/*
    See miganfill.h for the contract. MI-GAN packing (matches the bake-off / IOPaint):
      input  x: 1x4x512x512 = concat([keep-0.5, im*keep], ch), keep=1 where KEEP, im in [-1,1]
      output  : 1x3x512x512 tanh in [-1,1]; composite = im*keep + out*(1-keep)
    The hole (spot) is dilated a few px for the model (hard edges seam -- the bake-off's
    expand finding), while the ORIGINAL soft brush alpha composites the patch back, so the
    seam is feathered by the brush's feather.
*/

namespace {

const char *kModelFile = "migan.onnx";
constexpr int kNet = 512;              // MI-GAN fixed square input

InferenceSession *SharedSession()
{
    static std::once_flag once;
    static std::unique_ptr<InferenceSession> session;
    std::call_once(once, [] {
        const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(kModelFile);
        session = std::make_unique<InferenceSession>(path, InferenceDevice::Auto);
        if (!session->IsLoaded())
            qWarning("MiganFill: %s not found or failed to load at %s (spot fill disabled)",
                     kModelFile, path.toUtf8().constData());
        else if (G::isLogger)
            G::log("MiganFill::SharedSession loaded", session->BackendName());
    });
    return session.get();
}

/* Heal one spot's coverage `cov` (row-major W*H, 0..1) into RGB888 `full`, in place. */
void healOne(cv::Mat &full, const std::vector<float> &cov, int bx0, int by0, int bx1, int by1,
             InferenceSession *s, const std::string &inName, const std::string &outName)
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
        for (int x = 0; x < t; ++x) d[x] = cov[size_t(ty0 + y) * W + (tx0 + x)];
    }

    cv::Mat imgN, holeN;
    cv::resize(tileImg, imgN,  cv::Size(kNet, kNet), 0, 0, cv::INTER_AREA);
    cv::resize(hole,    holeN, cv::Size(kNet, kNet), 0, 0, cv::INTER_LINEAR);

    /* Hard, slightly dilated hole for the model; soft holeN alpha for compositing. */
    cv::Mat hard;
    cv::threshold(holeN, hard, 0.05, 1.0, cv::THRESH_BINARY);
    cv::dilate(hard, hard, cv::getStructuringElement(cv::MORPH_ELLIPSE, {7, 7}));

    const int P = kNet * kNet;
    Tensor in; in.shape = {1, 4, kNet, kNet}; in.data.resize(size_t(4) * P);
    for (int y = 0; y < kNet; ++y) {
        const cv::Vec3b *row = imgN.ptr<cv::Vec3b>(y);
        const float *hr = hard.ptr<float>(y);
        for (int x = 0; x < kNet; ++x) {
            const int o = y * kNet + x;
            const float keep = 1.0f - hr[x];             // 1 = keep, 0 = hole
            in.data[0 * P + o] = keep - 0.5f;
            in.data[1 * P + o] = ((row[x][0] / 127.5f) - 1.0f) * keep;   // R
            in.data[2 * P + o] = ((row[x][1] / 127.5f) - 1.0f) * keep;   // G
            in.data[3 * P + o] = ((row[x][2] / 127.5f) - 1.0f) * keep;   // B
        }
    }

    std::vector<Tensor> outs;
    if (!s->Run({inName}, {in}, {outName}, outs) || outs.empty()) return;
    const Tensor &out = outs[0];
    if (int(out.data.size()) != 3 * P) return;

    /* net-res healed patch = original in keep, model out in hole. */
    cv::Mat healN(kNet, kNet, CV_8UC3);
    for (int y = 0; y < kNet; ++y) {
        cv::Vec3b *hp = healN.ptr<cv::Vec3b>(y);
        const cv::Vec3b *row = imgN.ptr<cv::Vec3b>(y);
        const float *hr = hard.ptr<float>(y);
        for (int x = 0; x < kNet; ++x) {
            const int o = y * kNet + x;
            const float keep = 1.0f - hr[x];
            for (int c = 0; c < 3; ++c) {
                const float g = (out.data[c * P + o] + 1.0f) * 127.5f;   // to 0..255
                hp[x][c] = cv::saturate_cast<uchar>(row[x][c] * keep + g * (1.0f - keep));
            }
        }
    }

    cv::Mat heal, alpha;
    cv::resize(healN, heal,  cv::Size(t, t), 0, 0, cv::INTER_CUBIC);
    cv::resize(holeN, alpha, cv::Size(t, t), 0, 0, cv::INTER_LINEAR);
    /* The brushed area is now rasterized HARD (fillspot.h); feather the composite
       alpha here so the heal still blends seamlessly (band sized from the heal). */
    cv::GaussianBlur(alpha, alpha, cv::Size(),
                     std::clamp(0.06 * std::max(bx1 - bx0 + 1, by1 - by0 + 1),
                                2.0, 24.0) / 2.0);

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

namespace MiganFill {

bool isAvailable()
{
    InferenceSession *s = SharedSession();
    return s && s->IsLoaded();
}

bool apply(QImage &img, const QVector<FillSpot> &spots)
{
    if (img.isNull()) return false;
    InferenceSession *s = SharedSession();
    if (!s || !s->IsLoaded()) return false;

    bool any = false;
    for (const FillSpot &sp : spots)
        if (sp.enabled && !sp.paramsJson.isEmpty()) { any = true; break; }
    if (!any) return false;

    const int W = img.width(), H = img.height();
    QImage work = img.convertToFormat(QImage::Format_RGB888);
    work.detach();                                       // writable, unshared
    cv::Mat full(H, W, CV_8UC3, work.bits(), work.bytesPerLine());

    const std::string inName  = s->InputNames().empty()  ? "x"   : s->InputNames()[0];
    const std::string outName = s->OutputNames().empty() ? "out" : s->OutputNames()[0];

    bool applied = false;
    std::vector<float> cov;
    for (const FillSpot &sp : spots) {
        if (!sp.enabled || sp.paramsJson.isEmpty()) continue;
        const FillSpotGeom::Parsed p = FillSpotGeom::parse(sp.paramsJson);
        if (!p.valid()) continue;
        int bx0, by0, bx1, by1;
        FillSpotGeom::rasterize(p, W, H, cov, bx0, by0, bx1, by1);
        if (bx1 < bx0 || by1 < by0) continue;            // empty coverage
        healOne(full, cov, bx0, by0, bx1, by1, s, inName, outName);
        applied = true;
    }

    if (applied) img = work.convertToFormat(img.format());
    return applied;
}

} // namespace MiganFill
