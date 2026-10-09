#include "Utilities/inference/imageembedder.h"
#include "Utilities/inference/inferencesession.h"
#include "Utilities/modelstore.h"
#include "Main/global.h"

#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <memory>
#include <opencv2/imgproc.hpp>

namespace ImageEmbedder {

namespace {

const char *kModelFile = "siglip2_image.onnx";

/* The batch the model is run at. Inputs are processed in chunks of this size, the last
   one padded. */
constexpr int kBatch = 16;

/* Where the session runs: the GPU, not the Neural Engine (ANE 46 img/s, GPU 220). See
   tools/export_siglip.py for the measurements. Windows maps any accelerator to
   DirectML. */
constexpr InferenceDevice kDevice = InferenceDevice::GPU;

QString compileCacheDir(bool pruneOld = false)
{
/*
    CoreML's compiled ML Program for this model, ~1 GB, rebuilt in ~90 s when absent --
    so it belongs in the Caches folder (the system may purge it; that only costs one
    slow start). One directory per catalog VERSION, because ORT reuses a compiled graph
    by key and a re-exported model must not run the old one. pruneOld removes the
    directories earlier versions left, so a model update does not strand a gigabyte --
    only when the session loads, on the worker thread, never from a cheap query.
*/
#ifdef Q_OS_MAC
    const QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                         + "/CoreML";
    const QString mine = QString("siglip2_image-v%1")
        .arg(ModelStore::info(ModelStore::Model::SigLipImage).version);
    const QDir dir(root);
    if (pruneOld)
        for (const QString &old : dir.entryList({"siglip2_image-v*"}, QDir::Dirs))
            if (old != mine) QDir(dir.filePath(old)).removeRecursively();
    return dir.filePath(mine);
#else
    Q_UNUSED(pruneOld)
    return {};
#endif
}

InferenceSession *SharedSession()
{
/*
    RETRYABLE, like MiganFill::SharedSession: siglip2_image.onnx is downloaded on demand,
    so a first miss must not be latched for the life of the process.
*/
    static QMutex mutex;
    static std::unique_ptr<InferenceSession> session;
    QMutexLocker lock(&mutex);
    if (session && session->IsLoaded()) return session.get();

    const QString path = ModelStore::path(ModelStore::Model::SigLipImage);
    if (path.isEmpty()) return nullptr;

    session = std::make_unique<InferenceSession>(path, kDevice, compileCacheDir(true));
    if (!session->IsLoaded()) {
        qWarning("ImageEmbedder: %s failed to load at %s (Audit Keywords disabled)",
                 kModelFile, path.toUtf8().constData());
        return nullptr;
    }
    if (G::isLogger) G::log("ImageEmbedder::SharedSession loaded", session->BackendName());
    return session.get();
}

/* One image into its kSize x kSize x 3 slot of an NCHW float tensor. */
void preprocess(const QImage &src, float *dst)
{
    const QImage rgb = src.convertToFormat(QImage::Format_RGB888);
    const cv::Mat in(rgb.height(), rgb.width(), CV_8UC3,
                     const_cast<uchar *>(rgb.constBits()), size_t(rgb.bytesPerLine()));
    cv::Mat sq;
    cv::resize(in, sq, cv::Size(kSize, kSize), 0, 0, cv::INTER_CUBIC);

    const int plane = kSize * kSize;
    for (int y = 0; y < kSize; ++y) {
        const uchar *p = sq.ptr<uchar>(y);
        for (int x = 0; x < kSize; ++x) {
            const int i = y * kSize + x;
            dst[i]             = p[3 * x]     / 127.5f - 1.0f;
            dst[plane + i]     = p[3 * x + 1] / 127.5f - 1.0f;
            dst[2 * plane + i] = p[3 * x + 2] / 127.5f - 1.0f;
        }
    }
}

}   // namespace

QString ModelTag()
{
    return QString("siglip2-b16-256/%1")
        .arg(ModelStore::info(ModelStore::Model::SigLipImage).version);
}

bool IsSupportedBuild()
{
    return InferenceSession::BackendCompiledIn();
}

bool IsModelPresent()
{
    return ModelStore::isAvailable(ModelStore::Model::SigLipImage);
}

bool NeedsFirstCompile()
{
#ifdef Q_OS_MAC
    const QDir dir(compileCacheDir());
    return !dir.exists() || dir.isEmpty();
#else
    return false;
#endif
}

QString BackendName()
{
    InferenceSession *s = SharedSession();
    return s ? s->BackendName() : QString("none");
}

bool Embed(const QVector<QImage> &images, QVector<QVector<float>> &out)
{
    out = QVector<QVector<float>>(images.size());
    InferenceSession *s = SharedSession();
    if (!s) return false;

    const int plane = 3 * kSize * kSize;
    for (int start = 0; start < images.size(); start += kBatch) {
        Tensor in;
        in.shape = {kBatch, 3, kSize, kSize};
        in.data.assign(size_t(kBatch) * plane, 0.0f);
        QVector<int> slot;                     // input index of each filled batch row
        for (int i = start; i < images.size() && i < start + kBatch; ++i) {
            if (images[i].isNull()) continue;
            preprocess(images[i], in.data.data() + size_t(slot.size()) * plane);
            slot << i;
        }
        if (slot.isEmpty()) continue;

        std::vector<Tensor> res;
        if (!s->Run({"pixels"}, {in}, {"embedding"}, res) || res.empty()
            || res[0].data.size() != size_t(kBatch) * kDim)
            return false;
        for (int b = 0; b < slot.size(); ++b) {
            const float *v = res[0].data.data() + size_t(b) * kDim;
            out[slot[b]] = QVector<float>(v, v + kDim);
        }
    }
    return true;
}

}   // namespace ImageEmbedder
