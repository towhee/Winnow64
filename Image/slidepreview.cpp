#include "Image/slidepreview.h"
#include "Image/pixmap.h"
#include "Cache/devpreviewcache.h"
#include "Cache/imagedecoder.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Utilities/icc.h"
#include "Utilities/versionkey.h"
#include "Main/global.h"
#include <QFile>
#include <QFileInfo>

namespace SlidePreview {

Job gather(DataModel *dm, Metadata *metadata, const QString &rowKey)
{
    Job j;
    if (!dm || !metadata || rowKey.isEmpty()) return j;
    const int row = dm->rowFromKey(rowKey);
    if (row < 0) return j;
    auto col = [&](int c) { return dm->index(row, c).data(); };

    j.rowKey = rowKey;
    j.srcPath = VersionKey::sourceOf(rowKey);
    j.ext = QFileInfo(j.srcPath).completeSuffix().toLower();
    if (metadata->videoFormats.contains(j.ext)) return j;

    /* The devPreview only in Developed: with Show Edits off (Original) the user asked for
       the as-shot picture. The same rule as ImageDecoder::loadDevPreview. */
    if (G::previewSource == G::PreviewSource::Developed) {
        j.devKey = ImageDecoder::devPreviewKeyFor(col(G::DevPreviewKeyColumn).toString(),
                                                  rowKey, j.srcPath, j.ext);
        j.displayDims = col(G::CroppedDimensionsColumn).toString();
    }
    j.hasJpg = metadata->hasJpg.contains(j.ext);
    j.offsetFull = col(G::OffsetFullColumn).toUInt();
    j.lengthFull = col(G::LengthFullColumn).toUInt();
    j.rotate = metadata->rotateFormats.contains(j.ext);
    j.orientation = col(G::OrientationColumn).toInt();
    j.rotationDegrees = col(G::RotationDegreesColumn).toInt();
    j.colorManage = metadata->iccFormats.contains(j.ext);
    if (j.colorManage) j.iccBuf = col(G::ICCBufColumn).toByteArray();
    j.valid = true;
    return j;
}

static void toDisplay(QImage &image, const QByteArray &iccBuf)
{
    // ICC::transform assumes 4 bytes per pixel (see ImageDecoder::colorManage)
    if (image.format() != QImage::Format_ARGB32 && image.format() != QImage::Format_RGB32)
        image = image.convertToFormat(QImage::Format_ARGB32);
    if (!image.isNull()) ICC::transform(iccBuf, image);
}

static void cap(QImage &image, int maxEdge)
{
    if (maxEdge <= 0 || image.isNull()) return;
    if (qMax(image.width(), image.height()) <= maxEdge) return;
    /* the size it stands for: an existing tag (a reduced devPreview) already says so */
    QString dims = image.text(DevPreviewCache::kDisplaySizeText);
    if (dims.isEmpty())
        dims = QString::number(image.width()) + "x" + QString::number(image.height());
    image = image.scaled(maxEdge, maxEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    image.setText(DevPreviewCache::kDisplaySizeText, dims);
}

static QImage decodeFull(const Job &j, bool *fromDevPreview);

QImage decode(const Job &j, bool *fromDevPreview, int maxEdge)
{
    QImage image = decodeFull(j, fromDevPreview);
    cap(image, maxEdge);
    return image;
}

static QImage decodeFull(const Job &j, bool *fromDevPreview)
{
    if (fromDevPreview) *fromDevPreview = false;
    if (!j.valid) return QImage();
    QImage image;

    // 1. the devPreview: oriented, cropped and developed already, in sRGB
    if (!j.devKey.isEmpty()) {
        const QByteArray jpg = DevPreviewCache::instance().get(j.rowKey, j.devKey.toLatin1());
        if (!jpg.isEmpty() && image.loadFromData(jpg, "JPG") && !image.isNull()) {
            if (j.colorManage) toDisplay(image, QByteArray());     // empty = sRGB
            DevPreviewCache::tagDisplaySize(image, j.displayDims);
            if (fromDevPreview) *fromDevPreview = true;
            return image;
        }
        image = QImage();
    }

    // 2. the embedded / full JPEG
    if (j.hasJpg && j.lengthFull > 0) {
        QFile f(j.srcPath);
        if (!f.open(QIODevice::ReadOnly) || !f.seek(j.offsetFull)) return QImage();
        const QByteArray buf = f.read(j.lengthFull);
        if (!image.loadFromData(buf, "JPEG")) return QImage();
    }
    // TIFF / HEIC: Pixmap::load has the decoders (the caller falls back to it)
    else if (j.ext == "tif" || j.ext == "tiff" || j.ext == "heic" || j.ext == "heif") {
        return QImage();
    }
    // 3. anything QImage reads
    else if (!image.load(j.srcPath)) {
        return QImage();
    }
    if (image.isNull()) return QImage();

    if (j.rotate) Pixmap::applyOrientation(image, j.orientation, j.rotationDegrees);
    if (j.colorManage) toDisplay(image, j.iccBuf);
    return image;
}

}   // namespace SlidePreview
