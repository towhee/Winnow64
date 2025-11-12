#include "stackfocusmeasure.h"
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
                          QString("Processing focus map %1 of %2 using method %3")
                              .arg(count)
                              .arg(total)
                              .arg(methodName),
                              // .arg(methods.at(method)),
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
            case EmulateZerene:     map = focusMapZerene(gray);    break;
            case Petteri:           map = focusMapPetteri(gray);   break;
        }


        maps.insert(it.key(), map);

        qDebug() << src << saveResults << !outputFolder.isEmpty();

        if (saveResults && !outputFolder.isEmpty()) {
            QDir dir(outputFolder);
            if (!dir.exists()) dir.mkpath(outputFolder);
            QString fName = QString("focus_raw_%1.png").arg(count, 3, 10, QChar('0'));
            const QString outPath = dir.filePath(fName); // PNG avoids JPEG artifacts on masks
            saveFocusImage(map, outPath);
            qDebug() << src << "Saved" << fName << outPath;
        }
    }

    emit updateStatus(false, "Focus maps computed.", src);
    qDebug() << src + "3";

    return maps;
}

// FocusMeasure.cpp (excerpt)
bool FocusMeasure::computeFocusMaps_Petteri(const QString& alignedPath,
                                            const QString& outFolder,
                                            int downsample,
                                            bool saveResults,
                                            float gaussRadius,
                                            float energyThresh)
{
    /*
    QDir(outFolder).mkpath(".");
    QStringList imgs = listAlignedImages(alignedPath);    // your helper

    const int N = imgs.size();
    for (int i = 0; i < N; ++i) {
        emit updateStatus(false, QString("Petteri FocusMeasure %1/%2").arg(i+1).arg(N), "FocusMeasure::Petteri");

        cv::Mat color = cv::imread(imgs[i].toStdString(), cv::IMREAD_COLOR);
        if (color.empty()) return false;
        if (downsample > 1) cv::resize(color, color, {}, 1.0/downsample, 1.0/downsample, cv::INTER_AREA);

        cv::Mat gray; cv::cvtColor(color, gray, cv::COLOR_BGR2GRAY);
        gray.convertTo(gray, CV_32F, 1.0/255.0);

        // Petteri: Sobel in x and y, accumulateSquare → magnitude^2
        cv::Mat sx, sy, mag2;
        cv::Sobel(gray, sx, CV_32F, 1, 0);
        cv::Sobel(gray, sy, CV_32F, 0, 1);
        cv::multiply(sx, sx, sx);
        cv::multiply(sy, sy, sy);
        cv::add(sx, sy, mag2);

        // Threshold weak responses
        if (energyThresh > 0.0f) mag2.setTo(0, mag2 < energyThresh);

        // Optional Gaussian blur over magnitude (Petteri uses radius→window=4*radius+1)
        if (gaussRadius > 0.0f) {
            int w = int(gaussRadius * 4) + 1;
            cv::GaussianBlur(mag2, mag2, cv::Size(w, w), gaussRadius, gaussRadius, cv::BORDER_REFLECT);
        }

        // Store as float32 .exr (great for later depth fitting), plus optional 8-bit preview
        QString base = QFileInfo(imgs[i]).completeBaseName();
        QString exrPath = outFolder + QString("/focus_petteri_%1.exr").arg(base);
        cv::imwrite(exrPath.toStdString(), mag2);

        if (saveResults) {
            cv::Mat vis; cv::sqrt(mag2, vis);
            cv::normalize(vis, vis, 0, 255, cv::NORM_MINMAX); vis.convertTo(vis, CV_8U);
            cv::imwrite((outFolder + QString("/focus_petteri_%1.png").arg(base)).toStdString(), vis);
        }
    }
    */
    return true;
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

QImage FocusMeasure::focusMapZerene(const QImage &gray)
{
    const int w = gray.width();
    const int h = gray.height();
    QImage out(w, h, QImage::Format_Grayscale8);
    out.fill(Qt::black);

    // --- Step 1: Gaussian pre-blur (σ≈1.2) to suppress noise ---
    QVector<QVector<float>> kernel = {
        {0.075f, 0.124f, 0.075f},
        {0.124f, 0.204f, 0.124f},
        {0.075f, 0.124f, 0.075f}
    };
    QImage blur(w, h, QImage::Format_Grayscale8);
    blur.fill(Qt::black);

    for (int y = 1; y < h - 1; ++y) {
        const uchar *p0 = gray.constScanLine(y - 1);
        const uchar *p1 = gray.constScanLine(y);
        const uchar *p2 = gray.constScanLine(y + 1);
        uchar *pb = blur.scanLine(y);
        for (int x = 1; x < w - 1; ++x) {
            float v = kernel[0][0]*p0[x-1] + kernel[0][1]*p0[x] + kernel[0][2]*p0[x+1] +
                      kernel[1][0]*p1[x-1] + kernel[1][1]*p1[x] + kernel[1][2]*p1[x+1] +
                      kernel[2][0]*p2[x-1] + kernel[2][1]*p2[x] + kernel[2][2]*p2[x+1];
            pb[x] = static_cast<uchar>(qBound(0, int(v + 0.5f), 255));
        }
    }

    // --- Step 2: Sobel gradients (same as SobelEnergy) ---
    QImage grad(w, h, QImage::Format_Grayscale8);
    grad.fill(Qt::black);

    for (int y = 1; y < h - 1; ++y) {
        const uchar *p0 = blur.constScanLine(y - 1);
        const uchar *p1 = blur.constScanLine(y);
        const uchar *p2 = blur.constScanLine(y + 1);
        uchar *pg = grad.scanLine(y);
        for (int x = 1; x < w - 1; ++x) {
            int gx = (-p0[x - 1] - 2 * p1[x - 1] - p2[x - 1]) +
                     ( p0[x + 1] + 2 * p1[x + 1] + p2[x + 1]);
            int gy = (-p0[x - 1] - 2 * p0[x]     - p0[x + 1]) +
                     ( p2[x - 1] + 2 * p2[x]     + p2[x + 1]);
            int mag2 = gx * gx + gy * gy;
            pg[x] = static_cast<uchar>(qMin(255, mag2 >> 8));
        }
    }

    // --- Step 3: Local average (5x5 box blur) for continuity ---
    for (int y = 2; y < h - 2; ++y) {
        uchar *po = out.scanLine(y);
        for (int x = 2; x < w - 2; ++x) {
            int sum = 0;
            for (int dy = -2; dy <= 2; ++dy) {
                const uchar *p = grad.constScanLine(y + dy);
                for (int dx = -2; dx <= 2; ++dx)
                    sum += p[x + dx];
            }
            po[x] = static_cast<uchar>(sum / 25);
        }
    }

    // --- Step 4: Normalize to 0–255 ---
    int minVal = 255, maxVal = 0;
    for (int y = 0; y < h; ++y) {
        const uchar *p = out.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            int v = p[x];
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
        }
    }
    if (maxVal > minVal) {
        for (int y = 0; y < h; ++y) {
            uchar *p = out.scanLine(y);
            for (int x = 0; x < w; ++x)
                p[x] = static_cast<uchar>((p[x] - minVal) * 255 / (maxVal - minVal));
        }
    }

    return out;
}

QImage FocusMeasure::focusMapPetteri(const QImage &input)
{
    QString src = "FocusMeasure::focusMapPetteri";

    cv::Mat color = FSUtils::qimageToMat(input);
    color.convertTo(color, CV_32F, 1.0 / 255.0);

    // --- Wavelet focus energy -------------------------------------------
    auto pyr = FSWavelet::decompose(color, 4);
    cv::Mat fm = FSWavelet::focusEnergy(pyr);

    // show for debugging
    bool debugFocusStack = true;
    if (debugFocusStack) {
        QDir debugDir(QFileInfo(outputFolder).dir());
        debugDir.mkdir("focus_debug");
        QString base = debugDir.filePath("focus_debug/focus_wavelet");

        for (int i = 0; i < (int)pyr.size(); ++i) {
            FSUtils::debugSaveMat(pyr[i].lh, QString("%1_L%2_lh.png").arg(base).arg(i));
            FSUtils::debugSaveMat(pyr[i].hl, QString("%1_L%2_hl.png").arg(base).arg(i));
            FSUtils::debugSaveMat(pyr[i].hh, QString("%1_L%2_hh.png").arg(base).arg(i));
        }
        FSUtils::debugSaveMat(fm, base + "_energy.png");
    }

    // --- Normalize and return -------------------------------------------
    cv::Mat fm8 = FSUtils::normalizeTo8U(fm);
    return FSUtils::matToQImage(fm8);

    // // Convert QImage → cv::Mat (float gray)
    // cv::Mat color = FSUtils::qimageToMat(input);
    // cv::Mat gray; cv::cvtColor(color, gray, cv::COLOR_BGR2GRAY);
    // gray.convertTo(gray, CV_32F, 1.0 / 255.0);

    // // Sobel X & Y gradients
    // cv::Mat sx, sy, mag2;
    // cv::Sobel(gray, sx, CV_32F, 1, 0);
    // cv::Sobel(gray, sy, CV_32F, 0, 1);
    // cv::multiply(sx, sx, sx);
    // cv::multiply(sy, sy, sy);
    // cv::add(sx, sy, mag2);

    // // Petteri’s soft Gaussian blur for stability
    // const float gaussRadius = 1.0f;
    // if (gaussRadius > 0.0f) {
    //     int w = int(gaussRadius * 4) + 1;
    //     cv::GaussianBlur(mag2, mag2, cv::Size(w, w), gaussRadius, gaussRadius, cv::BORDER_REFLECT);
    // }

    // // Normalize to 8-bit range for preview
    // cv::Mat norm; cv::normalize(mag2, norm, 0, 255, cv::NORM_MINMAX);
    // norm.convertTo(norm, CV_8U);

    // return FSUtils::matToQImage(FSUtils::normalizeTo8U(mag2));
}

void FocusMeasure::saveFocusImage(const QImage &map, const QString &path)
{
    if (map.isNull()) return;

    // Save raw linear focus map (used later by DepthMap)
    QImage raw = map;
    raw.save(path, "PNG", 0);   // no scaling or contrast adjustment

    // // Also save a normalized visualization for easy inspection
    // int minVal = 255, maxVal = 0;
    // const int w = raw.width(), h = raw.height();

    // for (int y = 0; y < h; ++y) {
    //     const uchar *p = raw.constScanLine(y);
    //     for (int x = 0; x < w; ++x) {
    //         int v = p[x];
    //         if (v < minVal) minVal = v;
    //         if (v > maxVal) maxVal = v;
    //     }
    // }
    // if (maxVal <= minVal) maxVal = minVal + 1;

    // QImage vis(raw.size(), QImage::Format_Grayscale8);
    // for (int y = 0; y < h; ++y) {
    //     const uchar *src = raw.constScanLine(y);
    //     uchar *dst = vis.scanLine(y);
    //     for (int x = 0; x < w; ++x)
    //         dst[x] = static_cast<uchar>((src[x] - minVal) * 255 / (maxVal - minVal));
    // }

    // QString visPath = path;
    // visPath.replace("focus_", "focus_vis_");
    // vis.save(visPath, "PNG", 0);

    // emit updateStatus(false,
    //                   QString("Saved focus maps: %1 (raw) and %2 (normalized)")
    //                       .arg(QFileInfo(path).fileName())
    //                       .arg(QFileInfo(visPath).fileName()),
    //                   "FocusMeasure::saveFocusImage");

    emit updateStatus(false,
                      "Saved focus maps",
                      "FocusMeasure::saveFocusImage");
}
