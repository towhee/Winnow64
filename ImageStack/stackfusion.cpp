#include "ImageStack/stackfusion.h"

StackFusion::StackFusion(QObject *parent)
    : QObject(parent)
{
}

bool StackFusion::fuse(Method method,
                       const QString &alignedFolderPath,
                       const QString &depthMapPath,
                       const QString &outputFolderPath)
{
    QString src = "StackFusion::fuse";
    emit updateStatus(false,
                      QString("Starting fusion (%1)...")
                          .arg(QMetaEnum::fromType<Method>().valueToKey(method)),
                      src);

    QDir().mkpath(outputFolderPath);
    QString outFile;

    bool ok = false;
    switch (method) {
    case Naive:
        outFile = outputFolderPath + "/fused_naive.png";
        ok = fuseNaive(depthMapPath, alignedFolderPath, outFile);
        break;
    case PMax:
        outFile = outputFolderPath + "/fused_pmax.png";
        ok = fusePMax(alignedFolderPath, outFile);
        break;
    case PMax1:
        outFile = outputFolderPath + "/fused_pmax1.png";
        ok = fusePMax(alignedFolderPath, outFile);
        break;
    case PMax2:
        outFile = outputFolderPath + "/fused_pmax2.png";
        ok = fusePMax(alignedFolderPath, outFile);
        break;
    }

    if (ok)
        emit updateStatus(false, QString("Fusion complete: %1").arg(outFile), src);
    else
        emit updateStatus(false, QString("Fusion failed (%1)").arg(outFile), src);

    emit finished(outputFolderPath);
    return ok;
}

// -----------------------------------------------------------------------------
// Naive fusion: choose pixel from slice with matching depth index
// -----------------------------------------------------------------------------
bool StackFusion::fuseNaive(const QString &depthMapPath,
                            const QString &alignedFolderPath,
                            const QString &outputPath)
{
    QString src = "StackFusion::fuseNaive";
    emit updateStatus(false, "Running naive fusion...", src);

    QImage depth(depthMapPath);
    if (depth.isNull()) return false;

    QStringList alignedFiles =
        QDir(alignedFolderPath).entryList(QStringList() << "*.png" << "*.jpg" << "*.tif", QDir::Files);
    if (alignedFiles.isEmpty()) return false;

    QVector<QImage> stack;
    for (const QString &f : alignedFiles)
        stack.append(QImage(alignedFolderPath + "/" + f).convertToFormat(QImage::Format_RGB32));

    int w = stack.first().width();
    int h = stack.first().height();
    QImage fused(w, h, QImage::Format_RGB32);

    for (int y = 0; y < h; ++y) {
        QRgb *fLine = reinterpret_cast<QRgb *>(fused.scanLine(y));
        const QRgb *dLine = reinterpret_cast<const QRgb *>(depth.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int idx = qRed(dLine[x]) * stack.size() / 256;
            idx = qBound(0, idx, stack.size() - 1);
            fLine[x] = stack[idx].pixel(x, y);
        }
    }

    bool saved = fused.save(outputPath);
    emit updateStatus(false, QString("Naive fusion saved: %1").arg(outputPath), src);
    return saved;
}

// -----------------------------------------------------------------------------
// PMax fusion: local gradient energy weighting
// -----------------------------------------------------------------------------
bool StackFusion::fusePMax(const QString &alignedFolderPath,
                           const QString &outputPath)
{

    QString src = "StackFusion::fusePMax";
    emit updateStatus(false, "Running PMax fusion...", src);

    QStringList alignedFiles =
        QDir(alignedFolderPath).entryList(QStringList() << "*.png" << "*.jpg" << "*.tif", QDir::Files);
    if (alignedFiles.isEmpty()) return false;

    QVector<QImage> stack;
    for (const QString &f : alignedFiles)
        stack.append(QImage(alignedFolderPath + "/" + f).convertToFormat(QImage::Format_RGB32));

    int w = stack.first().width();
    int h = stack.first().height();
    QImage fused(w, h, QImage::Format_RGB32);
    fused.fill(Qt::black);

    QVector<QVector<float>> energy(stack.size(), QVector<float>(w * h));

    // Compute local gradient energy
    for (int i = 0; i < stack.size(); ++i) {
        const QImage &img = stack[i];
        for (int y = 1; y < h - 1; ++y) {
            const QRgb *prev = reinterpret_cast<const QRgb *>(img.scanLine(y - 1));
            const QRgb *curr = reinterpret_cast<const QRgb *>(img.scanLine(y));
            const QRgb *next = reinterpret_cast<const QRgb *>(img.scanLine(y + 1));
            for (int x = 1; x < w - 1; ++x) {
                int gx = qGray(curr[x + 1]) - qGray(curr[x - 1]);
                int gy = qGray(next[x]) - qGray(prev[x]);
                energy[i][y * w + x] = std::sqrt(gx * gx + gy * gy);
            }
        }
        emit progress("PMax energy", i + 1, stack.size());
    }

    // Select pixel with maximum energy
    for (int y = 0; y < h; ++y) {
        QRgb *fLine = reinterpret_cast<QRgb *>(fused.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int bestIdx = 0;
            float bestE = energy[0][y * w + x];
            for (int i = 1; i < stack.size(); ++i) {
                float e = energy[i][y * w + x];
                if (e > bestE) { bestE = e; bestIdx = i; }
            }
            fLine[x] = stack[bestIdx].pixel(x, y);
        }
    }

    bool saved = fused.save(outputPath);
    emit updateStatus(false, QString("PMax fusion saved: %1").arg(outputPath), src);
    return saved;
}

bool StackFusion::fusePMax1(const QString &alignedFolderPath,
                            const QString &outputPath)
{
    QString src = "StackFusion::fusePMax1";
    emit updateStatus(false, "Running enhanced PMax fusion...", src);

    QStringList alignedFiles =
        QDir(alignedFolderPath)
            .entryList(QStringList() << "*.png" << "*.jpg" << "*.tif", QDir::Files);
    if (alignedFiles.isEmpty()) {
        emit updateStatus(false, "No aligned images found.", src);
        return false;
    }

    QVector<QImage> stack;
    stack.reserve(alignedFiles.size());
    for (const QString &f : alignedFiles) {
        QImage img(alignedFolderPath + "/" + f);
        if (img.isNull()) continue;
        stack.append(img.convertToFormat(QImage::Format_RGB32));
    }

    qDebug() << "StackFusion::fusePMax1 0";

    if (stack.isEmpty()) return false;
    int n = stack.size();
    int w = stack.first().width();
    int h = stack.first().height();

    QImage fused(w, h, QImage::Format_RGB32);
    fused.fill(Qt::black);

    // Precompute energy maps
    QVector<QVector<float>> energy(n, QVector<float>(w * h, 0.0f));

    qDebug() << "StackFusion::fusePMax1 1";
    // --- Compute multi-scale, contrast-normalized energy ---------------------
    for (int i = 0; i < n; ++i) {
        const QImage &img = stack[i];
        QImage small = img.scaled(w / 2, h / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        for (int y = 1; y < h - 1; ++y) {
            const QRgb *prev = reinterpret_cast<const QRgb *>(img.scanLine(y - 1));
            const QRgb *curr = reinterpret_cast<const QRgb *>(img.scanLine(y));
            const QRgb *next = reinterpret_cast<const QRgb *>(img.scanLine(y + 1));

            for (int x = 1; x < w - 1; ++x) {
                float gx = float(qGray(curr[x + 1]) - qGray(curr[x - 1]));
                float gy = float(qGray(next[x]) - qGray(prev[x]));
                float grad = std::sqrt(gx * gx + gy * gy);

                // Local mean (5-pixel cross)
                float mean = (qGray(curr[x - 1]) + qGray(curr[x]) + qGray(curr[x + 1]) +
                              qGray(prev[x]) + qGray(next[x])) / 5.0f;
                float val = float(qGray(curr[x]));
                float contrast = std::abs(val - mean);

                // Combine gradient and local contrast
                float eFine = grad * (1.0f + contrast / 64.0f);

                // Coarse level (downsampled)
                int sx = x / 2;
                int sy = y / 2;
                sx = qBound(1, sx, w / 2 - 2);
                sy = qBound(1, sy, h / 2 - 2);

                const QRgb *sprev = reinterpret_cast<const QRgb *>(small.scanLine(sy - 1));
                const QRgb *scurr = reinterpret_cast<const QRgb *>(small.scanLine(sy));
                const QRgb *snext = reinterpret_cast<const QRgb *>(small.scanLine(sy + 1));
                float gx2 = float(qGray(scurr[sx + 1]) - qGray(scurr[sx - 1]));
                float gy2 = float(qGray(snext[sx]) - qGray(sprev[sx]));
                float eCoarse = std::sqrt(gx2 * gx2 + gy2 * gy2);

                // Weighted multi-scale energy
                energy[i][y * w + x] = 0.7f * eFine + 0.3f * eCoarse;
            }
        }
        emit progress("PMax energy", i + 1, n);
    }

    // --- Soft-weighted fusion -----------------------------------------------
    for (int y = 0; y < h; ++y) {
        QRgb *fLine = reinterpret_cast<QRgb *>(fused.scanLine(y));

        for (int x = 0; x < w; ++x) {
            double sumR = 0.0, sumG = 0.0, sumB = 0.0, sumW = 0.0;

            for (int i = 0; i < n; ++i) {
                float e = energy[i][y * w + x];
                if (e <= 0.0f) continue;
                double wgt = std::pow(e, 4.0); // emphasize strong edges

                QColor c(stack[i].pixel(x, y));
                sumR += wgt * c.red();
                sumG += wgt * c.green();
                sumB += wgt * c.blue();
                sumW += wgt;
            }

            if (sumW > 0.0) {
                int r = int(sumR / sumW);
                int g = int(sumG / sumW);
                int b = int(sumB / sumW);
                fLine[x] = qRgb(r, g, b);
            }
        }
    }

    // --- Gentle tone/contrast stretch ---------------------------------------
    // for (int y = 0; y < h; ++y) {
    //     QRgb *line = reinterpret_cast<QRgb *>(fused.scanLine(y));
    //     for (int x = 0; x < w; ++x) {
    //         QColor c(line[x]);
    //         int r = qBound(0, int((c.red() - 128) * 1.15 + 128), 255);
    //         int g = qBound(0, int((c.green() - 128) * 1.15 + 128), 255);
    //         int b = qBound(0, int((c.blue() - 128) * 1.15 + 128), 255);
    //         line[x] = qRgb(r, g, b);
    //     }
    // }

    bool saved = fused.save(outputPath);
    emit updateStatus(false,
                      saved
                          ? QString("Enhanced PMax fusion saved: %1").arg(outputPath)
                          : QString("Failed to save fusion: %1").arg(outputPath),
                      src);
    return saved;
}

bool StackFusion::fusePMax2(const QString &alignedFolderPath,
                            const QString &outputPath)
{
    QString src = "StackFusion::fusePMax2";
    emit updateStatus(false, "Running advanced PMax2 fusion...", src);

    QStringList alignedFiles =
        QDir(alignedFolderPath)
            .entryList(QStringList() << "*.png" << "*.jpg" << "*.tif", QDir::Files);
    if (alignedFiles.isEmpty()) {
        emit updateStatus(false, "No aligned images found.", src);
        return false;
    }

    // --- Tunable parameters --------------------------------------------------
    const float preBlurRadius = 0.0f;     // Gaussian blur sigma before gradient
    const float energyPower   = 4.0f;     // exponent for energy weighting
    const float contrastGain  = 1.00f;    // tone stretch multiplier
    const float fineWeight    = 0.3f;     // multi-scale weights
    const float midWeight     = 0.3f;
    const float coarseWeight  = 0.4f;

    QVector<QImage> stack;
    stack.reserve(alignedFiles.size());
    for (const QString &f : alignedFiles) {
        QImage img(alignedFolderPath + "/" + f);
        if (img.isNull()) continue;
        stack.append(img.convertToFormat(QImage::Format_RGB32));
    }

    if (stack.isEmpty()) return false;
    int n = stack.size();
    int w = stack.first().width();
    int h = stack.first().height();

    QImage fused(w, h, QImage::Format_RGB32);
    fused.fill(Qt::black);

    QVector<QVector<float>> energy(n, QVector<float>(w * h, 0.0f));

    // --- Compute 3-level multi-scale contrast-normalized gradient ------------
    for (int i = 0; i < n; ++i) {
        QImage img = stack[i];

        // Optional pre-blur (simple separable 3x1 kernel)
        if (preBlurRadius > 0.0f) {
            QImage tmp = img;
            for (int y = 0; y < h; ++y) {
                QRgb *line = reinterpret_cast<QRgb *>(tmp.scanLine(y));
                for (int x = 1; x < w - 1; ++x) {
                    QColor a(line[x - 1]);
                    QColor b(line[x]);
                    QColor c(line[x + 1]);
                    int r = (a.red() + 2 * b.red() + c.red()) / 4;
                    int g = (a.green() + 2 * b.green() + c.green()) / 4;
                    int bl = (a.blue() + 2 * b.blue() + c.blue()) / 4;
                    line[x] = qRgb(r, g, bl);
                }
            }
            img = tmp;
        }

        // Create 3 pyramid levels
        QImage imgMid = img.scaled(w / 2, h / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QImage imgCoarse = img.scaled(w / 4, h / 4, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        for (int y = 1; y < h - 1; ++y) {
            const QRgb *prev = reinterpret_cast<const QRgb *>(img.scanLine(y - 1));
            const QRgb *curr = reinterpret_cast<const QRgb *>(img.scanLine(y));
            const QRgb *next = reinterpret_cast<const QRgb *>(img.scanLine(y + 1));

            for (int x = 1; x < w - 1; ++x) {
                float gx = float(qGray(curr[x + 1]) - qGray(curr[x - 1]));
                float gy = float(qGray(next[x]) - qGray(prev[x]));
                float gradFine = std::sqrt(gx * gx + gy * gy);

                float mean = (qGray(curr[x - 1]) + qGray(curr[x]) + qGray(curr[x + 1]) +
                              qGray(prev[x]) + qGray(next[x])) / 5.0f;
                float val = float(qGray(curr[x]));
                float contrast = std::abs(val - mean);
                float eFine = gradFine * (1.0f + contrast / 64.0f);

                // Mid scale
                int mx = x / 2;
                int my = y / 2;
                mx = qBound(1, mx, w / 2 - 2);
                my = qBound(1, my, h / 2 - 2);
                const QRgb *mprev = reinterpret_cast<const QRgb *>(imgMid.scanLine(my - 1));
                const QRgb *mcurr = reinterpret_cast<const QRgb *>(imgMid.scanLine(my));
                const QRgb *mnext = reinterpret_cast<const QRgb *>(imgMid.scanLine(my + 1));
                float gxM = float(qGray(mcurr[mx + 1]) - qGray(mcurr[mx - 1]));
                float gyM = float(qGray(mnext[mx]) - qGray(mprev[mx]));
                float eMid = std::sqrt(gxM * gxM + gyM * gyM);

                // Coarse scale
                int cx = x / 4;
                int cy = y / 4;
                cx = qBound(1, cx, w / 4 - 2);
                cy = qBound(1, cy, h / 4 - 2);
                const QRgb *cprev = reinterpret_cast<const QRgb *>(imgCoarse.scanLine(cy - 1));
                const QRgb *ccurr = reinterpret_cast<const QRgb *>(imgCoarse.scanLine(cy));
                const QRgb *cnext = reinterpret_cast<const QRgb *>(imgCoarse.scanLine(cy + 1));
                float gxC = float(qGray(ccurr[cx + 1]) - qGray(ccurr[cx - 1]));
                float gyC = float(qGray(cnext[cx]) - qGray(cprev[cx]));
                float eCoarse = std::sqrt(gxC * gxC + gyC * gyC);

                energy[i][y * w + x] =
                    fineWeight * eFine + midWeight * eMid + coarseWeight * eCoarse;
            }
        }
        emit progress("PMax2 energy", i + 1, n);
    }

    // --- Soft-weighted fusion ------------------------------------------------
    for (int y = 0; y < h; ++y) {
        QRgb *fLine = reinterpret_cast<QRgb *>(fused.scanLine(y));
        for (int x = 0; x < w; ++x) {
            double sumR = 0.0, sumG = 0.0, sumB = 0.0, sumW = 0.0;
            for (int i = 0; i < n; ++i) {
                float e = energy[i][y * w + x];
                if (e <= 0.0f) continue;
                double wgt = std::pow(double(e), energyPower);
                QColor c(stack[i].pixel(x, y));
                sumR += wgt * c.red();
                sumG += wgt * c.green();
                sumB += wgt * c.blue();
                sumW += wgt;
            }
            if (sumW > 0.0) {
                int r = int(sumR / sumW);
                int g = int(sumG / sumW);
                int b = int(sumB / sumW);
                fLine[x] = qRgb(r, g, b);
            }
        }
    }

    // --- Gentle tone/contrast stretch ---------------------------------------
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(fused.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QColor c(line[x]);
            int r = qBound(0, int((c.red() - 128) * contrastGain + 128), 255);
            int g = qBound(0, int((c.green() - 128) * contrastGain + 128), 255);
            int b = qBound(0, int((c.blue() - 128) * contrastGain + 128), 255);
            line[x] = qRgb(r, g, b);
        }
    }

    bool saved = fused.save(outputPath);
    emit updateStatus(false,
                      saved
                          ? QString("Advanced PMax2 fusion saved: %1").arg(outputPath)
                          : QString("Failed to save fusion: %1").arg(outputPath),
                      src);
    return saved;
}
