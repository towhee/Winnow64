#include "ImageStack/stackfusion.h"

StackFusion::StackFusion(QObject *parent)
    : QObject(parent)
{
    // cv::setNumThreads(1);
}

bool StackFusion::fuse(Method method,
                       const QString &alignedFolderPath,
                       const QString &depthMapPath,
                       const QString &outputFolderPath)
{
    QString src = "StackFusion::fuse";
    const QString methodName =
        QMetaEnum::fromType<Method>().valueToKey(method);

    emit updateStatus(false,
                      QString("Starting fusion (%1)...").arg(methodName),
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

    case Petteri:
        outFile = outputFolderPath + "/fused_petteri.png";
        ok = fusePetteri(alignedFolderPath, depthMapPath, outFile);
        break;
    }

    emit updateStatus(false,
                      ok ? QString("Fusion complete: %1").arg(outFile)
                         : QString("Fusion failed: %1").arg(outFile),
                      src);

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

// ---------------------------------------------------------------------------
// Petteri fusion — wavelet blend
// ---------------------------------------------------------------------------

bool StackFusion::fusePetteri(const QString &alignedFolderPath,
                              const QString &depthMapPath,
                              const QString &outputPath)
{
    const QString src = "StackFusion::fusePetteri";
    emit updateStatus(false, "Running full Petteri wavelet fusion...", src);
    qDebug() << src;

    cv::setNumThreads(1);
    cv::setUseOptimized(false);

    // --- Load aligned stack ------------------------------------------------
    QStringList alignedFiles = QDir(alignedFolderPath)
                                   .entryList(QStringList() << "*.png" << "*.jpg" << "*.tif",
                                              QDir::Files | QDir::Readable, QDir::Name);
    if (alignedFiles.isEmpty()) {
        emit updateStatus(false, "No aligned images found.", src);
        return false;
    }

    // Convert aligned images to linear CV_32F in [0,1]
    std::vector<cv::Mat> stack32f;
    stack32f.reserve(alignedFiles.size());
    for (const QString &f : alignedFiles) {
        cv::Mat img8 = cv::imread(QDir(alignedFolderPath).filePath(f).toStdString(),
                                  cv::IMREAD_COLOR);                 // BGR 8U
        if (img8.empty()) continue;
        cv::Mat img32;  img8.convertTo(img32, CV_32F, 1.0/255.0);    // [0,1] sRGB
        cv::pow(img32, 2.2, img32);                                  // sRGB → linear
        stack32f.push_back(std::move(img32));
    }
    const int n = (int)stack32f.size();
    if (n == 0) return false;

    // ✅ Run one-time verification on the first image
    FSWavelet::testHaarRoundTrip(stack32f.front());

    // --- Validity masks (1.0 valid, 0.0 padded) ---------------------------
    std::vector<cv::Mat> validFull; validFull.reserve(n);
    for (const cv::Mat& im : stack32f) {
        cv::Mat gray, v;
        if (im.channels() == 3) cv::cvtColor(im, gray, cv::COLOR_BGR2GRAY);
        else                    gray = im;
        cv::compare(gray, 1e-8, v, cv::CMP_GT);  // 0/255
        v.convertTo(v, CV_32F, 1.0/255.0);       // 0.0 / 1.0
        cv::erode(v, v, cv::Mat(), cv::Point(-1,-1), 1);
        validFull.push_back(std::move(v));
    }

    // --- Load μ/σ/A (gaussian fit) ---------------------------------------
    QFileInfo depthInfo(depthMapPath);
    QDir depthDir = depthInfo.dir();
    cv::Mat mu  = FSUtils::loadFloatEXR(depthDir.filePath("gauss_mean.exr"));
    cv::Mat sig = FSUtils::loadFloatEXR(depthDir.filePath("gauss_dev.exr"));
    cv::Mat amp = FSUtils::loadFloatEXR(depthDir.filePath("gauss_amp.exr"));

    if (mu.empty()) {
        cv::Mat idx8 = cv::imread(depthDir.filePath("depth_index.png").toStdString(),
                                  cv::IMREAD_GRAYSCALE);
        if (!idx8.empty()) idx8.convertTo(mu, CV_32F, float(n-1)/255.0f);
    }
    if (sig.empty()) sig = cv::Mat(mu.size(), CV_32F, cv::Scalar(1.0f));
    if (amp.empty()) amp = cv::Mat(mu.size(), CV_32F, cv::Scalar(1.0f));
    if (mu.empty() || sig.empty()) {
        emit updateStatus(false, "Missing μ/σ maps, aborting fusion.", src);
        return false;
    }

    // --- Ensure μ/σ/A match stack size & are 32F single-channel ----------
    const cv::Size fullSize = stack32f[0].size();
    auto ensureSameSize = [&](cv::Mat &m) {
        if (m.empty()) return;
        if (m.channels() > 1) cv::cvtColor(m, m, cv::COLOR_BGR2GRAY);
        if (m.size() != fullSize) cv::resize(m, m, fullSize, 0, 0, cv::INTER_LINEAR);
        if (m.type() != CV_32F)   m.convertTo(m, CV_32F, 1.0/255.0);
    };
    ensureSameSize(mu);
    ensureSameSize(sig);
    ensureSameSize(amp);

    // --- Fuse in linear space --------------------------------------------
    cv::Mat fused_linear = FSWavelet::fuseWavelet(stack32f, mu, sig, amp, 4);

    // Safety clamp (still linear)
    cv::patchNaNs(fused_linear, 0.0);
    cv::max(fused_linear, 0.0f, fused_linear);
    cv::min(fused_linear, 1.0f, fused_linear);

    // --- Display conversion: linear → sRGB (fixes the “too dark” look) ---
    cv::Mat fused_srgb;
    cv::pow(fused_linear, 1.0/2.2, fused_srgb);

    // (Optional) tiny guard for numeric drift
    cv::max(fused_srgb, 0.0f, fused_srgb);
    cv::min(fused_srgb, 1.0f, fused_srgb);

    // --- Debug saves ------------------------------------------------------
    {
        QDir outDir(QFileInfo(outputPath).dir());
        outDir.mkdir("fusion_debug");

        FSUtils::debugSaveMat(mu,  outDir.filePath("fusion_debug/mu.png"));
        FSUtils::debugSaveMat(sig, outDir.filePath("fusion_debug/sigma.png"));
        FSUtils::debugSaveMat(amp, outDir.filePath("fusion_debug/amp.png"));

        // Save a pyramid energy preview (from the first slice) just as before
        auto pyrs0 = FSWavelet::decompose(stack32f.front(), 4);
        for (int i = 0; i < (int)pyrs0.size(); ++i) {
            cv::Mat mag = cv::abs(pyrs0[i].lh) + cv::abs(pyrs0[i].hl) + cv::abs(pyrs0[i].hh);
            FSUtils::debugSaveMat(mag, outDir.filePath(QString("fusion_debug/level_%1_mag.png").arg(i)));
        }

        // Save both linear EXR and display PNG for inspection
        FSUtils::saveFloatEXR(outDir.filePath("fusion_debug/fused_linear.exr"), fused_linear);
        FSUtils::debugSaveMat(fused_srgb, outDir.filePath("fusion_debug/fused_display.png"));
    }

    // --- Save final result -------------------------------------------------
    // Auto-increment filename if exists
    QDir outDir = QFileInfo(outputPath).dir();
    QDir().mkpath(outDir.absolutePath());
    QString base = QFileInfo(outputPath).completeBaseName();
    QString ext  = QFileInfo(outputPath).suffix();
    QString path = outputPath;
    int version = 1;
    while (QFile::exists(path)) {
        path = outDir.filePath(QString("%1_%2.%3").arg(base).arg(version++).arg(ext));
    }

    // Write the visually-correct (sRGB) PNG/JPG/TIF
    cv::Mat out8U; fused_srgb.convertTo(out8U, CV_8U, 255.0);
    bool saved = cv::imwrite(path.toStdString(), out8U);

    // Also drop a linear EXR with the same base name (nice to keep around)
    {
        QString exrPath = QFileInfo(path).dir().filePath(QFileInfo(path).completeBaseName() + "_linear.exr");
        FSUtils::saveFloatEXR(exrPath, fused_linear);
    }

    emit updateStatus(false, QString("Petteri fusion complete — saved %1").arg(path), src);
    qDebug() << src << QString("Petteri fusion complete — saved %1").arg(path);
    return saved;
}

/* Pre focuswavelet addition
bool StackFusion::fusePetteri(const QString &alignedFolderPath,
                              const QString &depthMapPath,
                              const QString &outputPath)
{
    QString src = "StackFusion::fusePetteri";
    emit updateStatus(false, "Running Petteri multi-scale fusion...", src);

    // --- Load auxiliary maps ------------------------------------------------
    QFileInfo di(depthMapPath);  QDir dd = di.dir();
    cv::Mat amp = cv::imread(dd.filePath("gauss_amp.png").toStdString(),  cv::IMREAD_GRAYSCALE);
    cv::Mat dev = cv::imread(dd.filePath("gauss_dev.png").toStdString(),  cv::IMREAD_GRAYSCALE);
    cv::Mat mu  = cv::imread(dd.filePath("gauss_mean.png").toStdString(), cv::IMREAD_GRAYSCALE);

    // --- Load stack ---------------------------------------------------------
    QStringList fns = QDir(alignedFolderPath).entryList(QStringList() << "*.tif" << "*.png" << "*.jpg", QDir::Files);
    if (fns.isEmpty()) return false;

    std::vector<cv::Mat> stack;
    for (auto &fn : fns) {
        cv::Mat img = cv::imread(QDir(alignedFolderPath).filePath(fn).toStdString());
        if (!img.empty()) {
            img.convertTo(img, CV_32F, 1.0 / 255.0);
            stack.push_back(img);
        }
    }
    const int n = (int)stack.size(), h = stack[0].rows, w = stack[0].cols;

    // --- Normalize aux maps -------------------------------------------------
    if (!amp.empty()) amp.convertTo(amp, CV_32F, 1.0 / 255.0);
    if (!dev.empty()) dev.convertTo(dev, CV_32F, 1.0 / 255.0);
    if (!mu.empty())  mu.convertTo(mu,  CV_32F, (float)n / 255.0f);

    cv::Mat weightBase(h, w, CV_32F, cv::Scalar(1));
    if (!amp.empty() && !dev.empty()) {
        weightBase = amp / (dev.mul(dev) + 1e-3f);              // stronger σ penalty
        cv::normalize(weightBase, weightBase, 0, 1, cv::NORM_MINMAX);
    }
    cv::GaussianBlur(weightBase, weightBase, cv::Size(5,5), 0.8);

    // --- Accumulators -------------------------------------------------------
    cv::Mat fused(h, w, CV_32FC3, cv::Scalar(0,0,0));
    cv::Mat sumW(h, w, CV_32F, cv::Scalar(0));

    // --- Multi-scale fusion -------------------------------------------------
    for (int i = 0; i < n; ++i) {
        emit progress("Petteri fusion", i + 1, n);

        cv::Mat gray; cv::cvtColor(stack[i], gray, cv::COLOR_BGR2GRAY);

        // Base gradient energy
        cv::Mat gx, gy, gradE;
        cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
        cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
        cv::magnitude(gx, gy, gradE);
        double maxE; cv::minMaxLoc(gradE, nullptr, &maxE);
        if (maxE > 0) gradE /= maxE;

        // Fine-scale Laplacian energy
        cv::Mat lap, fineE;
        cv::Laplacian(gray, lap, CV_32F, 3);
        cv::pow(lap, 2, fineE);
        cv::normalize(fineE, fineE, 0, 1, cv::NORM_MINMAX);

        // Medium band (Gaussian-down→up)
        cv::Mat small, medE;
        cv::pyrDown(gray, small);
        cv::pyrUp(small, medE, gray.size());
        medE = cv::abs(gray - medE);
        cv::normalize(medE, medE, 0, 1, cv::NORM_MINMAX);

        // Combine energy bands (Petteri-like weights)
        cv::Mat energy = 0.5f*gradE + 0.3f*fineE + 0.2f*medE;

        // Depth-consistency gate
        cv::Mat gate(h, w, CV_32F, cv::Scalar(1.0f));
        if (!mu.empty() && !dev.empty()) {
            cv::Mat diff = cv::abs(mu - float(i));
            cv::exp(- (diff.mul(diff)) / (2.0 * (dev + 1e-3f).mul(dev + 1e-3f)), gate);
        }

        // Total weight
        cv::Mat wgt = energy.mul(weightBase).mul(gate);
        cv::threshold(wgt, wgt, 0, 0, cv::THRESH_TOZERO);
        cv::patchNaNs(wgt, 0.0f);
        wgt += 1e-3f;

        // Accumulate
        std::vector<cv::Mat> ch(3);
        cv::split(stack[i], ch);
        for (int c = 0; c < 3; ++c) ch[c] = ch[c].mul(wgt);
        cv::Mat merged; cv::merge(ch, merged);

        fused += merged;
        sumW += wgt;
    }

    // --- Fill small holes (morph fallback) ----------------------------------
    cv::Mat holes = (sumW <= 1e-6f);
    if (cv::countNonZero(holes))
    {
        cv::Mat filled = sumW.clone();
        cv::dilate(filled, filled, cv::Mat(), cv::Point(-1,-1), 3);
        cv::GaussianBlur(filled, filled, cv::Size(5,5), 0);
        sumW = cv::max(sumW, filled);
    }

    // --- Normalize & save ---------------------------------------------------
    cv::Mat denom; cv::max(sumW, 1e-6f, denom);
    cv::Mat denom3; cv::merge(std::vector<cv::Mat>{denom,denom,denom}, denom3);
    cv::divide(fused, denom3, fused);

    cv::Mat out8U; fused.convertTo(out8U, CV_8U, 255.0);
    cv::imwrite(outputPath.toStdString(), out8U);

    // optional debug
    cv::imwrite(dd.filePath("sumW.png").toStdString(), sumW*255);
    return true;
}
*/

/* Before debugging patch
bool StackFusion::fusePetteri(const QString &alignedFolderPath,
                              const QString &depthMapPath,
                              const QString &outputPath)
{
    QString src = "StackFusion::fusePetteri";
    emit updateStatus(false, "Running Petteri fusion (wavelet/consistency)...", src);
    qDebug() << src << outputPath;

    // --- Locate auxiliary maps --------------------------------------------
    QFileInfo depthInfo(depthMapPath);
    QDir depthDir = depthInfo.dir();
    QString confPath   = depthDir.filePath("confidence_mask.png");
    QString ampPath    = depthDir.filePath("gauss_amp.png");
    QString devPath    = depthDir.filePath("gauss_dev.png");

    cv::Mat conf, amp, dev;
    if (QFile::exists(confPath)) conf = cv::imread(confPath.toStdString(), cv::IMREAD_GRAYSCALE);
    if (QFile::exists(ampPath))  amp  = cv::imread(ampPath.toStdString(),  cv::IMREAD_GRAYSCALE);
    if (QFile::exists(devPath))  dev  = cv::imread(devPath.toStdString(),  cv::IMREAD_GRAYSCALE);

    // --- Load aligned stack -----------------------------------------------
    QStringList alignedFiles =
        QDir(alignedFolderPath).entryList(QStringList() << "*.png" << "*.jpg" << "*.tif", QDir::Files);
    if (alignedFiles.isEmpty()) {
        emit updateStatus(false, "No aligned images found.", src);
        return false;
    }

    std::vector<cv::Mat> stack;
    for (const QString &f : alignedFiles) {
        cv::Mat img = cv::imread(QDir(alignedFolderPath).filePath(f).toStdString());
        if (img.empty()) continue;
        img.convertTo(img, CV_32F, 1.0 / 255.0);
        stack.push_back(img);
    }

    const int n = (int)stack.size();
    if (n == 0) return false;

    int h = stack[0].rows;
    int w = stack[0].cols;

    // --- Normalize auxiliary maps to float [0..1] --------------------------
    if (!conf.empty()) conf.convertTo(conf, CV_32F, 1.0 / 255.0);
    if (!amp.empty())  amp.convertTo(amp,  CV_32F, 1.0 / 255.0);
    if (!dev.empty())  dev.convertTo(dev,  CV_32F, 1.0 / 255.0);

    // --- Build combined confidence weight map ------------------------------
    cv::Mat weightBase;
    if (!conf.empty()) {
        weightBase = conf.clone();
    } else if (!amp.empty() && !dev.empty()) {
        // Petteri-style: high amplitude, low deviation → strong confidence
        weightBase = amp / (dev + 1e-3f);
        cv::normalize(weightBase, weightBase, 0, 1, cv::NORM_MINMAX);
    } else {
        weightBase = cv::Mat(h, w, CV_32F, cv::Scalar(1.0f));
    }

    // Optional gentle blur for continuity
    cv::GaussianBlur(weightBase, weightBase, cv::Size(5,5), 0.8);

    // --- Prepare output accumulation buffers -------------------------------
    cv::Mat fused(h, w, CV_32FC3, cv::Scalar(0,0,0));
    cv::Mat sumW(h, w, CV_32F, cv::Scalar(0));

    // --- Main consistency-weighted blending -------------------------------
    for (int i = 0; i < n; ++i) {
        emit progress("Petteri fusion", i + 1, n);
        qDebug() << src << i;
        cv::Mat gray;
        cv::cvtColor(stack[i], gray, cv::COLOR_BGR2GRAY);

        // Compute local energy as focus consistency term
        cv::Mat gx, gy, energy;
        cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
        cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
        cv::magnitude(gx, gy, energy);

        // Normalize energy per slice
        double maxE;
        cv::minMaxLoc(energy, nullptr, &maxE);
        if (maxE > 0) energy /= maxE;

        // Combine with global weight base
        cv::Mat w = energy.mul(weightBase);

        // Accumulate weighted color
        std::vector<cv::Mat> channels(3);
        cv::split(stack[i], channels);
        for (int c = 0; c < 3; ++c)
            channels[c] = channels[c].mul(w);

        cv::Mat merged;
        cv::merge(channels, merged);
        fused += merged;
        sumW += w;
    }

    qDebug() << src << "a";
    // --- Normalize final output -------------------------------------------
    cv::Mat denom;
    cv::max(sumW, 1e-6f, denom); // prevent division by zero

    if (denom.channels() == 1 && fused.channels() == 3)
    {
        cv::Mat denom3;
        cv::merge(std::vector<cv::Mat>{denom, denom, denom}, denom3);
        cv::divide(fused, denom3, fused);
    }
    else
    {
        cv::divide(fused, denom, fused);
    }    cv::Mat out8U;
    fused.convertTo(out8U, CV_8U, 255.0);

    qDebug() << src << "b";
    // --- Save & log --------------------------------------------------------
    QDir().mkpath(QFileInfo(outputPath).dir().absolutePath());
    bool saved = cv::imwrite(outputPath.toStdString(), out8U);

    emit updateStatus(false,
                      QString("Petteri fusion complete — saved %1").arg(outputPath),
                      src);
    return saved;
}
*/


