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
    const QString src = "FocusMeasure::computeFocusMaps";
    qDebug() << src + "0";

    if (stack.isEmpty()) {
        emit updateStatus(false, "Empty image stack — nothing to compute", src);
        qDebug() << src + "Empty image stack — nothing to compute";

        return {};
    }

    qDebug() << src + "1";
    const char *methodName = QMetaEnum::fromType<Method>().valueToKey(method);
    emit updateStatus(false, QString("Computing focus maps (%1)…").arg(methodName ? methodName : "Unknown"), src);

    QMap<int, QImage> maps;

    int total = stack.size();
    int count = 0;

    for (auto it = stack.constBegin(); it != stack.constEnd(); ++it) {
        if (QThread::currentThread() && QThread::currentThread()->isInterruptionRequested()) {
            emit updateStatus(false, "Focus map stage interrupted.", src);
            break;
        }

        qDebug() << src + "2  count =" << count;
        ++count;
        emit progress("FocusMeasure", count, total);
        emit updateStatus(false,
                          QString("Processing focus map %1 of %2").arg(count).arg(total),
                          src);

        QImage gray = toGray(it.value());
        if (downsample > 1) {
            gray = gray.scaled(gray.width() / downsample, gray.height() / downsample,
                               Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        QImage map;
        switch (method) {
        case LaplacianVariance: map = focusMapLaplacian(gray); break;
        case SobelEnergy:       map = focusMapSobel(gray);     break;
        case Tenengrad:         map = focusMapTenengrad(gray); break;
        }

        maps.insert(it.key(), map);

        if (saveResults && !outputFolder.isEmpty()) {
            QDir dir(outputFolder);
            if (!dir.exists()) dir.mkpath(outputFolder);
            const QString outPath =
                dir.filePath(QString("focus_raw_%1.png").
                             arg(count, 3, 10, QChar('0'))); // PNG avoids JPEG artifacts on masks
            saveFocusImage(map, outPath);
        }
    }

    emit updateStatus(false, "Focus maps computed.", src);
    qDebug() << src + "3";

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
            const int gx = (-p0[x - 1] - 2 * p1[x - 1] - p2[x - 1]) +
                           ( p0[x + 1] + 2 * p1[x + 1] + p2[x + 1]);
            const int gy = (-p0[x - 1] - 2 * p0[x]     - p0[x + 1]) +
                           ( p2[x - 1] + 2 * p2[x]     + p2[x + 1]);
            // Use |gx| + |gy| (L1) as a quick, robust proxy
            int mag = qAbs(gx) + qAbs(gy);
            mag = (mag >> 4);                      // cheap scale-down to 0..~255
            po[x] = static_cast<uchar>(qMin(255, mag));
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

    const int threshold = 30; // gradient threshold (tune as needed)

    for (int y = 1; y < h - 1; ++y) {
        const uchar *p0 = gray.constScanLine(y - 1);
        const uchar *p1 = gray.constScanLine(y);
        const uchar *p2 = gray.constScanLine(y + 1);
        uchar *po = out.scanLine(y);

        for (int x = 1; x < w - 1; ++x) {
            const int gx = (-p0[x - 1] - 2 * p1[x - 1] - p2[x - 1]) +
                           ( p0[x + 1] + 2 * p1[x + 1] + p2[x + 1]);
            const int gy = (-p0[x - 1] - 2 * p0[x]     - p0[x + 1]) +
                           ( p2[x - 1] + 2 * p2[x]     + p2[x + 1]);
            const int mag2 = gx * gx + gy * gy;
            po[x] = (mag2 > threshold * threshold)
                        ? static_cast<uchar>(qMin(255, (mag2 >> 8)))  // scale without sqrt
                        : 0;
        }
    }
    return out;
}

void FocusMeasure::saveFocusImage(const QImage &map, const QString &path)
{
    if (map.isNull()) return;

    // Save raw linear focus map (used later by DepthMap)
    QImage raw = map;
    raw.save(path, "PNG", 0);   // no scaling or contrast adjustment

    // Also save a normalized visualization for easy inspection
    int minVal = 255, maxVal = 0;
    const int w = raw.width(), h = raw.height();

    for (int y = 0; y < h; ++y) {
        const uchar *p = raw.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            int v = p[x];
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
        }
    }
    if (maxVal <= minVal) maxVal = minVal + 1;

    QImage vis(raw.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        const uchar *src = raw.constScanLine(y);
        uchar *dst = vis.scanLine(y);
        for (int x = 0; x < w; ++x)
            dst[x] = static_cast<uchar>((src[x] - minVal) * 255 / (maxVal - minVal));
    }

    QString visPath = path;
    visPath.replace("focus_", "focus_vis_");
    vis.save(visPath, "PNG", 0);

    emit updateStatus(false,
                      QString("Saved focus maps: %1 (raw) and %2 (normalized)")
                          .arg(QFileInfo(path).fileName())
                          .arg(QFileInfo(visPath).fileName()),
                      "FocusMeasure::saveFocusImage");
}
