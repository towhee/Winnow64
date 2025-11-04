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
        ok = fusePMax(depthMapPath, alignedFolderPath, outFile);
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
bool StackFusion::fusePMax(const QString &depthMapPath,
                           const QString &alignedFolderPath,
                           const QString &outputPath)
{
    Q_UNUSED(depthMapPath);

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
