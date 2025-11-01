#include "stackaligner.h"

StackAligner::StackAligner(QObject *parent) : QObject(parent) {}

void StackAligner::setSearchRadius(int px)    { searchRadius = px; }
void StackAligner::setRotationStep(float deg) { rotationStepDeg = deg; }
void StackAligner::setDownsample(int f)       { downsample = qMax(1, f); }
void StackAligner::setUseEdgeMaskWeighting(bool enabled) { useEdgeMaskWeighting = enabled; }

QMap<int, QImage> StackAligner::align(const QMap<int, QImage> &stack)
{
    QString src = "StackAligner::align";
    if (stack.isEmpty()) {
        emit updateStatus(false, "No images provided for alignment", src);
        return {};
    }

    emit updateStatus(false, QString("Starting alignment of %1 images").arg(stack.size()), src);

    auto it = stack.begin();
    const QImage refFull = it.value();
    QImage refGray = toGray(refFull);
    if (downsample > 1)
        refGray = refGray.scaled(refGray.width() / downsample,
                                 refGray.height() / downsample,
                                 Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation);

    QVector<double> refWeights;
    if (useEdgeMaskWeighting) {
        emit updateStatus(false, "Computing edge weights for reference image", src);
        refWeights = computeEdgeWeights(refGray);
    }

    QMap<int, QImage> aligned;
    aligned.insert(it.key(), refFull); // reference unchanged
    ++it;

    int total = stack.size() - 1;
    int count = 0;

    for (; it != stack.end(); ++it) {
        ++count;
        emit progress("Aligning image", count, total);
        emit updateStatus(false,
                          QString("Aligning image %1 of %2").arg(count).arg(total),
                          src);

        QImage imgGray = toGray(it.value());
        if (downsample > 1)
            imgGray = imgGray.scaled(refGray.width(), refGray.height(),
                                     Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation);

        int bestDx = 0, bestDy = 0;
        float bestAngle = 0;
        double bestScore = -1.0;

        QVector<float> angles;
        if (rotationStepDeg > 0.01f) {
            for (float a = -rotationStepDeg; a <= rotationStepDeg + 0.001f; a += rotationStepDeg)
                angles.append(a);
        } else {
            angles.append(0);
        }

        for (float a : angles) {
            QImage rotated = (a == 0) ? imgGray : rotate(imgGray, a);
            QVector<double> weights;
            if (useEdgeMaskWeighting)
                weights = computeEdgeWeights(rotated);

            for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
                for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
                    double score = ncc(refGray, rotated, dx, dy,
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
            aligned.insert(it.key(), it.value());
            continue;
        }

        emit updateStatus(false,
                          QString("Best offset dx=%1 dy=%2 angle=%3 score=%4")
                              .arg(bestDx).arg(bestDy)
                              .arg(bestAngle, 0, 'f', 2)
                              .arg(bestScore, 0, 'f', 4),
                          src);

        QImage alignedImg = translate(it.value(), bestDx * downsample, bestDy * downsample);
        if (!qFuzzyIsNull(bestAngle))
            alignedImg = rotate(alignedImg, bestAngle);

        aligned.insert(it.key(), alignedImg);
    }

    emit updateStatus(false, "✅ Alignment complete.", src);
    return aligned;
}

QImage StackAligner::toGray(const QImage &img)
{
    return img.convertToFormat(QImage::Format_Grayscale8);
}

double StackAligner::ncc(const QImage &a, const QImage &b, int dx, int dy,
                         const QVector<double> *w)
{
    const int wA = a.width();
    const int hA = a.height();
    if (b.width() != wA || b.height() != hA) return -1;

    double sumA = 0, sumB = 0, sumAA = 0, sumBB = 0, sumAB = 0;
    double sumW = 0;
    int count = 0;

    for (int y = qMax(0, -dy); y < hA && y + dy < hA; ++y) {
        const uchar *pa = a.constScanLine(y);
        const uchar *pb = b.constScanLine(y + dy);
        for (int x = qMax(0, -dx); x < wA && x + dx < wA; ++x) {
            double weight = 1.0;
            if (w && x + y * wA < w->size())
                weight = (*w)[x + y * wA];

            double va = pa[x];
            double vb = pb[x + dx];
            sumA += va * weight;
            sumB += vb * weight;
            sumAA += va * va * weight;
            sumBB += vb * vb * weight;
            sumAB += va * vb * weight;
            sumW += weight;
            ++count;
        }
    }

    if (count == 0 || sumW < 1e-6) return -1.0;

    double meanA = sumA / sumW;
    double meanB = sumB / sumW;

    double numerator = sumAB - sumW * meanA * meanB;
    double denom = qSqrt((sumAA - sumW * meanA * meanA) *
                         (sumBB - sumW * meanB * meanB));
    if (denom <= 1e-8) return -1.0;

    return numerator / denom;
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
                     (p0[x + 1] + 2 * p1[x + 1] + p2[x + 1]);
            int gy = (-p0[x - 1] - 2 * p0[x] - p0[x + 1]) +
                     (p2[x - 1] + 2 * p2[x] + p2[x + 1]);
            double mag = qSqrt(double(gx * gx + gy * gy));
            weights[x + y * w] = mag;
        }
    }

    double maxVal = 0;
    for (double v : weights) maxVal = qMax(maxVal, v);
    if (maxVal > 0)
        for (double &v : weights) v /= maxVal;

    return weights;
}

QImage StackAligner::translate(const QImage &src, int dx, int dy)
{
    QImage dst(src.size(), src.format());
    dst.fill(Qt::black);
    QPainter p(&dst);
    p.drawImage(dx, dy, src);
    p.end();
    return dst;
}

QImage StackAligner::rotate(const QImage &src, float degrees)
{
    QTransform t;
    t.translate(src.width() / 2.0, src.height() / 2.0);
    t.rotate(degrees);
    t.translate(-src.width() / 2.0, -src.height() / 2.0);
    return src.transformed(t, Qt::SmoothTransformation);
}
