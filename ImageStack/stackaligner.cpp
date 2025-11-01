#include "stackaligner.h"
#include <QDebug>

StackAligner::StackAligner(QObject *parent) : QObject(parent) {}

void StackAligner::setSearchRadius(int px)    { searchRadius = px; }
void StackAligner::setRotationStep(float deg) { rotationStepDeg = deg; }
void StackAligner::setDownsample(int f)       { downsample = qMax(1, f); }
void StackAligner::setUseEdgeMaskWeighting(bool enabled) { useEdgeMaskWeighting = enabled; }
void StackAligner::setUseGpu(bool enabled)    { useGpu = enabled; }

QList<QImage> StackAligner::align(const QList<QImage*> &images)
{
    const QString src = "StackAligner::align";
    if (images.isEmpty()) {
        emit updateStatus(false, "No images provided for alignment", src);
        return {};
    }

    emit updateStatus(false, QString("Starting alignment of %1 images").arg(images.size()), src);

    // --- Initial reference (will be updated to previous aligned each step) ---
    QImage refFull = *images.first();
    QImage refGray = toGray(refFull);
    if (downsample > 1) {
        refGray = refGray.scaled(refGray.width() / downsample,
                                 refGray.height() / downsample,
                                 Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation);
    }

    QVector<double> refWeights;
    if (useEdgeMaskWeighting) {
        emit updateStatus(false, "Computing edge weights for reference image", src);
        refWeights = computeEdgeWeights(refGray);
    }

    QList<QImage> aligned;
    aligned.reserve(images.size());
    aligned.append(refFull); // first image unchanged

    const int total = images.size() - 1;
    int count = 0;

    // --- Align each subsequent image to the *previous aligned* reference ----
    for (int i = 1; i < images.size(); ++i) {

        if (QThread::currentThread()->isInterruptionRequested()) {
            emit updateStatus(false, "Alignment cancelled by user.", src);
            break;
        }

        ++count;
        const QImage &curFull = *images[i];
        emit progress("Aligning image", count, total);
        emit updateStatus(false,
                          QString("Aligning image %1 of %2").arg(count).arg(total),
                          src);

        // Downsampled gray for search
        QImage imgGray = toGray(curFull);
        if (downsample > 1) {
            imgGray = imgGray.scaled(refGray.width(), refGray.height(),
                                     Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation);
        }

        int   bestDx = 0, bestDy = 0;
        float bestAngle = 0.0f;
        double bestScore = -1.0;

        // Angle candidates
        QVector<float> angles;
        if (rotationStepDeg > 0.01f) {
            for (float a = -rotationStepDeg; a <= rotationStepDeg + 0.001f; a += rotationStepDeg)
                angles.append(a);
        } else {
            angles.append(0.0f);
        }

        // Coarse integer search
        for (float a : angles) {
            QImage rotatedGray = (a == 0.0f) ? imgGray : rotate(imgGray, a);

            // (Optional) per-rotated weights (not needed now; refWeights is enough)
            // QVector<double> weights = useEdgeMaskWeighting ? computeEdgeWeights(rotatedGray) : QVector<double>();

            for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
                for (int dx = -searchRadius; dx <= searchRadius; ++dx) {

                    if (QThread::currentThread()->isInterruptionRequested()) {
                        emit updateStatus(false, "Alignment cancelled mid-loop.", src);
                        return aligned;
                    }

                    double score = ncc(refGray, rotatedGray, dx, dy,
                                       useEdgeMaskWeighting ? &refWeights : nullptr);
                    if (score > bestScore) {
                        bestScore = score;
                        bestDx = dx;
                        bestDy = dy;
                        bestAngle = a;
                    }
                }
            }
        }

        if (bestScore < 0.0) {
            emit updateStatus(false,
                              QString("⚠️ Alignment failed for image %1").arg(count),
                              src);
            aligned.append(curFull);
            // Keep reference as-is
            continue;
        }

        // Recreate the rotated image at the winning angle (downsampled gray) for refinement
        QImage bestRotGray = (bestAngle == 0.0f) ? imgGray : rotate(imgGray, bestAngle);

        // --- Subpixel refinement around best integer (±1 pixel neighborhood) ---
        QPointF refined = refineSubpixel(refGray, bestRotGray, bestDx, bestDy,
                                         useEdgeMaskWeighting ? &refWeights : nullptr);
        const float subDx = float(refined.x());
        const float subDy = float(refined.y());

        emit updateStatus(false,
                          QString("Best offset dx=%1 dy=%2 angle=%3 score=%4 (refined)")
                              .arg(subDx, 0, 'f', 3)
                              .arg(subDy, 0, 'f', 3)
                              .arg(bestAngle, 0, 'f', 2)
                              .arg(bestScore, 0, 'f', 4),
                          src);

        // --- Apply to full-res: rotate then translate with fractional dx/dy ---
        QImage rotatedFull = (bestAngle == 0.0f) ? curFull : rotate(curFull, bestAngle);
        QImage alignedImg  = translate(rotatedFull, subDx * downsample, subDy * downsample);
        aligned.append(alignedImg);

        // --- Update reference to reduce perceived drift when browsing frames ---
        refFull = alignedImg;
        refGray = toGray(refFull);
        if (downsample > 1) {
            refGray = refGray.scaled(refGray.width() / downsample,
                                     refGray.height() / downsample,
                                     Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation);
        }
        if (useEdgeMaskWeighting) {
            refWeights = computeEdgeWeights(refGray);
        }
    }

    emit updateStatus(false, "✅ Alignment complete.", src);
    return aligned;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QImage StackAligner::toGray(const QImage &img)
{
    return img.convertToFormat(QImage::Format_Grayscale8);
}

double StackAligner::ncc(const QImage &a, const QImage &b, int dx, int dy,
                         const QVector<double> *w)
{
    const int wA = a.width();
    const int hA = a.height();
    if (b.width() != wA || b.height() != hA) return -1.0;

    double sumA = 0, sumB = 0, sumAA = 0, sumBB = 0, sumAB = 0;
    double sumW = 0;
    int count = 0;

    // Overlap region only
    const int yStart = qMax(0, -dy);
    const int yEnd   = qMin(hA, hA - dy);
    const int xStart = qMax(0, -dx);
    const int xEnd   = qMin(wA, wA - dx);

    for (int y = yStart; y < yEnd; ++y) {
        const uchar *pa = a.constScanLine(y);
        const uchar *pb = b.constScanLine(y + dy);
        for (int x = xStart; x < xEnd; ++x) {
            double weight = 1.0;
            if (w) {
                const int idx = x + y * wA;
                if (idx >= 0 && idx < w->size())
                    weight = (*w)[idx];
            }

            const double va = pa[x];
            const double vb = pb[x + dx];

            sumA  += va * weight;
            sumB  += vb * weight;
            sumAA += va * va * weight;
            sumBB += vb * vb * weight;
            sumAB += va * vb * weight;
            sumW  += weight;
            ++count;
        }
    }

    if (count == 0 || sumW < 1e-6) return -1.0;

    const double meanA = sumA / sumW;
    const double meanB = sumB / sumW;

    const double num = (sumAB - sumW * meanA * meanB);
    const double denA = (sumAA - sumW * meanA * meanA);
    const double denB = (sumBB - sumW * meanB * meanB);
    const double den = qSqrt(qMax(denA, 0.0) * qMax(denB, 0.0));

    if (den <= 1e-8) return -1.0;
    return num / den;
}

QVector<double> StackAligner::computeEdgeWeights(const QImage &gray)
{
    const int w = gray.width();
    const int h = gray.height();
    QVector<double> weights(w * h, 0.0);

    for (int y = 1; y < h - 1; ++y) {
        const uchar *p0 = gray.constScanLine(y - 1);
        const uchar *p1 = gray.constScanLine(y);
        const uchar *p2 = gray.constScanLine(y + 1);
        for (int x = 1; x < w - 1; ++x) {
            int gx = (-p0[x - 1] - 2 * p1[x - 1] - p2[x - 1]) +
                     ( p0[x + 1] + 2 * p1[x + 1] + p2[x + 1]);
            int gy = (-p0[x - 1] - 2 * p0[x]     - p0[x + 1]) +
                     ( p2[x - 1] + 2 * p2[x]     + p2[x + 1]);
            double mag = qSqrt(double(gx * gx + gy * gy));
            weights[x + y * w] = mag;
        }
    }

    // Normalize with epsilon and soften with gamma to stabilize low-contrast scenes
    double maxVal = 0.0;
    for (double v : weights) maxVal = qMax(maxVal, v);
    const double eps = 1e-6;
    if (maxVal > 0.0) {
        const double inv = 1.0 / (maxVal + eps);
        const double gamma = 0.6;
        for (double &v : weights) {
            v = qPow(v * inv, gamma);
        }
    }
    return weights;
}

QPointF StackAligner::refineSubpixel(const QImage &ref, const QImage &img,
                                     int bestDx, int bestDy,
                                     const QVector<double> *weights)
{
    auto corr = [&](int dx, int dy) {
        return ncc(ref, img, dx, dy, weights);
    };

    const double c0   = corr(bestDx,     bestDy);
    const double cxp1 = corr(bestDx + 1, bestDy);
    const double cxm1 = corr(bestDx - 1, bestDy);
    const double cyp1 = corr(bestDx,     bestDy + 1);
    const double cym1 = corr(bestDx,     bestDy - 1);

    auto refine1D = [](double m1, double c, double p1) -> double {
        const double denom = (m1 - 2 * c + p1);
        if (qFuzzyIsNull(denom)) return 0.0;
        // parabola vertex offset in [-0.5, 0.5] typically
        return 0.5 * (m1 - p1) / denom;
    };

    const double subX = refine1D(cxm1, c0, cxp1);
    const double subY = refine1D(cym1, c0, cyp1);

    return QPointF(double(bestDx) + subX, double(bestDy) + subY);
}

// Fractional translation using QTransform (bilinear interpolation)
QImage StackAligner::translate(const QImage &src, float dx, float dy)
{
    QTransform T;
    T.translate(dx, dy);
    return src.transformed(T, Qt::SmoothTransformation);
}

QImage StackAligner::rotate(const QImage &src, float degrees)
{
    QTransform t;
    t.translate(src.width() / 2.0, src.height() / 2.0);
    t.rotate(degrees);
    t.translate(-src.width() / 2.0, -src.height() / 2.0);
    return src.transformed(t, Qt::SmoothTransformation);
}
