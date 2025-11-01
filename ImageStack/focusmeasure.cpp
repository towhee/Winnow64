#include "focusmeasure.h"
#include <QFileInfo>
#include <QDir>

FocusMeasure::FocusMeasure(QObject *parent)
    : QObject(parent)
{
}

void FocusMeasure::setMethod(Method m) { method = m; }
void FocusMeasure::setDownsample(int f) { downsample = qMax(1, f); }
void FocusMeasure::setSaveResults(bool enabled) { saveResults = enabled; }
void FocusMeasure::setOutputFolder(const QString &path) { outputFolder = path; }

QMap<int, QImage> FocusMeasure::computeFocusMaps(const QMap<int, QImage> &stack)
{
    QString src = "FocusMeasure::computeFocusMaps";

    if (stack.isEmpty()) {
        emit updateStatus(false, "Empty image stack — nothing to compute", src);
        return {};
    }

    emit updateStatus(false, QString("Computing focus maps (%1)...")
                                 .arg(QMetaEnum::fromType<Method>().valueToKey(method)),
                      src);

    QMap<int, QImage> maps;
    int total = stack.size();
    int count = 0;

    for (auto it = stack.begin(); it != stack.end(); ++it) {
        ++count;
        emit progress("FocusMeasure", count, total);
        emit updateStatus(false,
                          QString("Processing focus map %1 of %2").arg(count).arg(total),
                          src);

        QImage gray = toGray(it.value());
        if (downsample > 1)
            gray = gray.scaled(gray.width() / downsample, gray.height() / downsample,
                               Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        QImage map;
        switch (method) {
        case LaplacianVariance: map = focusMapLaplacian(gray); break;
        case SobelEnergy:       map = focusMapSobel(gray);     break;
        case Tenengrad:         map = focusMapTenengrad(gray); break;
        }

        maps.insert(it.key(), map);

        if (saveResults && !outputFolder.isEmpty()) {
            QDir dir(outputFolder);
            QString outPath = dir.filePath(QString("focus_%1.jpg").arg(count, 3, 10, QChar('0')));
            saveFocusImage(map, outPath);
        }
    }

    emit updateStatus(false, "✅ Focus maps computed.", src);
    return maps;
}

QImage FocusMeasure::toGray(const QImage &img)
{
    return img.convertToFormat(QImage::Format_Grayscale8);
}

QImage FocusMeasure::focusMapLaplacian(const QImage &gray)
{
    const int w = gray.width();
    const int h = gray.height();
    QImage out(w, h, QImage::Format_Grayscale8);
    out.fill(Qt::black);

    // Simple 3×3 Laplacian kernel
    for (int y = 1; y < h - 1; ++y) {
        const uchar *p0 = gray.constScanLine(y - 1);
        const uchar *p1 = gray.constScanLine(y);
        const uchar *p2 = gray.constScanLine(y + 1);
        uchar *po = out.scanLine(y);
        for (int x = 1; x < w - 1; ++x) {
            int val = -p0[x - 1] - p0[x] - p0[x + 1]
                      -p1[x - 1] + 8 * p1[x] - p1[x + 1]
                      -p2[x - 1] - p2[x] - p2[x + 1];
            val = qBound(0, qAbs(val), 255);
            po[x] = static_cast<uchar>(val);
        }
    }
    return out;
}

QImage FocusMeasure::focusMapSobel(const QImage &gray)
{
    const int w = gray.width();
    const int h = gray.height();
    QImage out(w, h, QImage::Format_Grayscale8);
    out.fill(Qt::black);

    for (int y = 1; y < h - 1; ++y) {
        const uchar *p0 = gray.constScanLine(y - 1);
        const uchar *p1 = gray.constScanLine(y);
        const uchar *p2 = gray.constScanLine(y + 1);
        uchar *po = out.scanLine(y);
        for (int x = 1; x < w - 1; ++x) {
            int gx = (-p0[x - 1] - 2 * p1[x - 1] - p2[x - 1]) +
                     (p0[x + 1] + 2 * p1[x + 1] + p2[x + 1]);
            int gy = (-p0[x - 1] - 2 * p0[x] - p0[x + 1]) +
                     (p2[x - 1] + 2 * p2[x] + p2[x + 1]);
            int mag = qMin(255, int(qSqrt(gx * gx + gy * gy)));
            po[x] = static_cast<uchar>(mag);
        }
    }
    return out;
}

QImage FocusMeasure::focusMapTenengrad(const QImage &gray)
{
    const int w = gray.width();
    const int h = gray.height();
    QImage out(w, h, QImage::Format_Grayscale8);
    out.fill(Qt::black);

    const int threshold = 30; // empirical gradient threshold

    for (int y = 1; y < h - 1; ++y) {
        const uchar *p0 = gray.constScanLine(y - 1);
        const uchar *p1 = gray.constScanLine(y);
        const uchar *p2 = gray.constScanLine(y + 1);
        uchar *po = out.scanLine(y);
        for (int x = 1; x < w - 1; ++x) {
            int gx = (-p0[x - 1] - 2 * p1[x - 1] - p2[x - 1]) +
                     (p0[x + 1] + 2 * p1[x + 1] + p2[x + 1]);
            int gy = (-p0[x - 1] - 2 * p0[x] - p0[x + 1]) +
                     (p2[x - 1] + 2 * p2[x] + p2[x + 1]);
            int mag2 = gx * gx + gy * gy;
            po[x] = (mag2 > threshold * threshold)
                        ? static_cast<uchar>(qMin(255, int(qSqrt(mag2))))
                        : 0;
        }
    }
    return out;
}

void FocusMeasure::saveFocusImage(const QImage &map, const QString &path)
{
    if (!map.isNull()) {
        map.save(path, "JPEG", 90);
        emit updateStatus(false, "Saved focus map: " + QFileInfo(path).fileName(),
                          "FocusMeasure::saveFocusImage");
    }
}
