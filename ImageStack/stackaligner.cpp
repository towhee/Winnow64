#include "stackaligner.h"

StackAligner::StackAligner(QObject *parent) : QObject(parent) {}


void StackAligner::setSearchRadius(int px)    { searchRadius = px; }
void StackAligner::setRotationStep(float deg) { rotationStepDeg = deg; }
void StackAligner::setDownsample(int f)       { downsample = qMax(1, f); }
void StackAligner::setUseEdgeMaskWeighting(bool enabled) { useEdgeMaskWeighting = enabled; }
void StackAligner::setUseGpu(bool enabled)    { useGpu = enabled; }

// OpenCV version
QList<QImage> StackAligner::alignECC(const QList<QImage*> &images)
{
    const QString src = "StackAligner::alignECC";
    if (images.isEmpty()) {
        emit updateStatus(false, "No images provided for ECC alignment", src);
        return {};
    }

    emit updateStatus(false, QString("Starting ECC alignment of %1 images").arg(images.size()), src);

    // --- Convert first image to cv::Mat (reference) ---
    QImage refQ = images.first()->convertToFormat(QImage::Format_BGR888);
    cv::Mat ref(refQ.height(), refQ.width(), CV_8UC3,
                (void*)refQ.constBits(), refQ.bytesPerLine());
    cv::Mat refGray;
    cv::cvtColor(ref, refGray, cv::COLOR_BGR2GRAY);

    QList<QImage> aligned;
    aligned.reserve(images.size());
    aligned.append(*images.first());  // keep first image as-is

    int total = images.size() - 1;

    // --- ECC setup ---
    const int warpMode = cv::MOTION_AFFINE;   // or MOTION_EUCLIDEAN
    const cv::TermCriteria criteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, 1e-6);

    for (int i = 1; i < images.size(); ++i) {
        emit progress("Aligning image", i, total);
        qDebug() << "StackAligner::alignECC" << i << total;
        emit updateStatus(false,
                          QString("Aligning image %1 of %2").arg(i).arg(total),
                          src);

        QImage curQ = images[i]->convertToFormat(QImage::Format_BGR888);
        cv::Mat im2(curQ.height(), curQ.width(), CV_8UC3,
                    (void*)curQ.constBits(), curQ.bytesPerLine());
        cv::Mat im2Gray;
        cv::cvtColor(im2, im2Gray, cv::COLOR_BGR2GRAY);

        cv::Mat warpMat = cv::Mat::eye(2, 3, CV_32F);
        try {
            double cc = cv::findTransformECC(refGray, im2Gray, warpMat, warpMode, criteria);
            qDebug() << "  ECC correlation:" << cc;

            cv::Mat im2Aligned;
            cv::warpAffine(im2, im2Aligned, warpMat, refGray.size(),
                           cv::INTER_LANCZOS4 + cv::WARP_INVERSE_MAP);

            // Convert back to QImage
            QImage alignedQ(im2Aligned.data, im2Aligned.cols, im2Aligned.rows,
                            im2Aligned.step, QImage::Format_BGR888);
            aligned.append(alignedQ.copy()); // copy so memory persists

        } catch (const cv::Exception &e) {
            qWarning() << "ECC alignment failed for image" << i
                       << ":" << e.what();
            aligned.append(*images[i]);
        }
    }

    emit updateStatus(false, "ECC alignment complete.", src);
    return aligned;
}

QList<QImage> StackAligner::align(const QList<QImage*> &images)
{
    const QString src = "StackAligner::align";
    if (images.isEmpty()) {
        emit updateStatus(false, "No images provided for alignment", src);
        return {};
    }

    emit updateStatus(false, QString("Starting alignment of %1 images").arg(images.size()), src);

    // --- Reference image ----------------------------------------------------
    const QImage &refFull = *images.first();
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

    QList<QImage> aligned;
    aligned.reserve(images.size());
    aligned.append(refFull); // first image unchanged

    const int total = images.size() - 1;
    int count = 0;

    // --- Align each subsequent image ---------------------------------------
    for (int i = 1; i < images.size(); ++i) {

        // ✅ NEW: allow user abort mid-process
        if (QThread::currentThread()->isInterruptionRequested()) {
            emit updateStatus(false, "Alignment cancelled by user.", src);
            break;
        }

        ++count;
        const QImage &curFull = *images[i];
        emit progress("Aligning image", count, total);
        qDebug() << "StackAligner::align" << count << total;
        emit updateStatus(false,
                          QString("Aligning image %1 of %2").arg(count).arg(total),
                          src);

        QImage imgGray = toGray(curFull);
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

                    // ✅ NEW: occasional interruption check inside inner loop
                    if (QThread::currentThread()->isInterruptionRequested()) {
                        emit updateStatus(false, "Alignment cancelled mid-loop.", src);
                        return aligned;
                    }

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
            aligned.append(curFull);
            continue;
        }

        // --- NEW: sub-pixel refinement around the best integer peak ----------
        QImage bestGray = (qFuzzyIsNull(bestAngle) ? imgGray : rotate(imgGray, bestAngle));
        QVector<double> bestWeights;
        if (useEdgeMaskWeighting) bestWeights = computeEdgeWeights(bestGray);

        const QPointF sub = refineSubpixelPeak(
            refGray, bestGray, bestDx, bestDy,
            useEdgeMaskWeighting ? &refWeights : nullptr
            );
        const double subDx = sub.x();
        const double subDy = sub.y();

        // Scale subpixel (downsampled) shift back to full resolution
        const double dxFull = subDx * double(downsample);
        const double dyFull = subDy * double(downsample);

        QString bestOffset = QString(
                                 "Best offset dx=%1 dy=%2 angle=%3 score=%4  | refined dx=%5 dy=%6  (full dx=%7 dy=%8)")
                                 .arg(bestDx).arg(bestDy)
                                 .arg(bestAngle, 0, 'f', 2)
                                 .arg(bestScore, 0, 'f', 4)
                                 .arg(subDx, 0, 'f', 3).arg(subDy, 0, 'f', 3)
                                 .arg(dxFull, 0, 'f', 3).arg(dyFull, 0, 'f', 3);
        qDebug() << bestOffset;

        // Apply rotation first (full-res), then *fractional* translation
        QImage rotatedFull = qFuzzyIsNull(bestAngle) ? curFull : rotate(curFull, bestAngle);
        QImage alignedImg = translateF(rotatedFull, dxFull, dyFull);

        // --- NEW: residual self-check & auto-correct sign if needed -----------------
        static const double RES_THRESH = 0.5; // px
        const QImage refFullGray = toGrayFast(refFull);        // cached outside loop if you prefer
        const QImage candFullGray = toGrayFast(alignedImg);

        const QPointF residual = measureResidualShift(refFullGray, candFullGray, /*search*/ 8,
                                                      useEdgeMaskWeighting ? &refWeights : nullptr);

        qDebug() << "Residual after apply (full px):"
                 << residual.x() << residual.y();

        // If residual remains large and points roughly the same direction as (dxFull,dyFull),
        // flip the sign and re-apply once.
        const double dot = residual.x()*dxFull + residual.y()*dyFull;
        const double residMag = qSqrt(residual.x()*residual.x() + residual.y()*residual.y());

        if (residMag > RES_THRESH && dot > 0.0) {
            // suspect sign mismatch -> flip once
            const double fixDx = -dxFull;
            const double fixDy = -dyFull;

            QImage alignedImg2 = translateF(rotatedFull, fixDx, fixDy);

            const QImage candFullGray2 = toGrayFast(alignedImg2);
            const QPointF residual2 = measureResidualShift(refFullGray, candFullGray2, 8,
                                                           useEdgeMaskWeighting ? &refWeights : nullptr);

            const double residMag2 = qSqrt(residual2.x()*residual2.x() + residual2.y()*residual2.y());

            qDebug() << "Auto-corrected sign. New residual (full px):"
                     << residual2.x() << residual2.y();

            // Keep whichever is better
            if (residMag2 + 1e-6 < residMag) {
                alignedImg = std::move(alignedImg2);
            } else {
                // original was better; keep it (rare)
            }
        }

        aligned.append(alignedImg);
    }

    emit updateStatus(false, "✅ Alignment complete.", src);
    return aligned;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QImage StackAligner::toGrayFast(const QImage &img)
{
    // Same as toGray for now; kept separate in case you want a faster path later.
    return toGray(img);
}

QPointF StackAligner::measureResidualShift(const QImage &refFullGray,
                                           const QImage &testFullGray,
                                           int search,
                                           const QVector<double> *w)
{
    // Assumes same size
    if (refFullGray.size() != testFullGray.size()) return QPointF(0,0);

    // Small symmetric search around 0 to detect residual
    double best = -1.0;
    int bestDx = 0, bestDy = 0;
    for (int dy = -search; dy <= search; ++dy) {
        for (int dx = -search; dx <= search; ++dx) {
            const double s = nccOverlap(refFullGray, testFullGray, dx, dy, w);
            if (s > best) { best = s; bestDx = dx; bestDy = dy; }
        }
    }

    // Sub-pixel refinement around the local peak
    const QPointF sub = refineSubpixelPeak(refFullGray, testFullGray, bestDx, bestDy, w);
    return sub; // full-res pixels (since inputs are full-res)
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

double StackAligner::nccOverlap(const QImage &a, const QImage &b, int dx, int dy,
                                const QVector<double> *w)
{
    const int W = a.width();
    const int H = a.height();
    if (b.width() != W || b.height() != H) return -1.0;

    // Compute strict overlap rectangle to avoid black borders bias
    const int x0 = dx >= 0 ? dx : 0;
    const int y0 = dy >= 0 ? dy : 0;
    const int x1 = dx >= 0 ? W : W + dx;   // dx<0 shrinks right edge
    const int y1 = dy >= 0 ? H : H + dy;

    if (x0 >= x1 || y0 >= y1) return -1.0;

    double sumA = 0, sumB = 0, sumAA = 0, sumBB = 0, sumAB = 0, sumW = 0;
    int count = 0;

    for (int y = y0; y < y1; ++y) {
        const uchar *pa = a.constScanLine(y);
        const uchar *pb = b.constScanLine(y - dy); // inverse shift on B's index

        for (int x = x0; x < x1; ++x) {
            const int xb = x - dx; // corresponding x in B
            double weight = 1.0;
            if (w) {
                const int idx = x + y * W;
                if (idx >= 0 && idx < w->size()) weight = (*w)[idx];
            }

            const double va = pa[x];
            const double vb = pb[xb];

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
    const double num   = sumAB - sumW * meanA * meanB;
    const double den   = qSqrt((sumAA - sumW * meanA * meanA) *
                             (sumBB - sumW * meanB * meanB));
    if (den <= 1e-8) return -1.0;
    return num / den;
}

QPointF StackAligner::refineSubpixelPeak(const QImage &a, const QImage &b, int dx, int dy,
                                         const QVector<double> *w)
{
    const auto S = [&](int ox, int oy) {
        return nccOverlap(a, b, dx + ox, dy + oy, w);
    };

    const double c  = S(0, 0);
    const double l  = S(-1, 0);
    const double r  = S(+1, 0);
    const double u  = S(0, -1);
    const double d  = S(0, +1);

    // Fallback if neighborhood invalid
    if (c < -0.5 || l < -0.5 || r < -0.5 || u < -0.5 || d < -0.5)
        return QPointF(dx, dy);

    const double denomX = (l - 2.0*c + r);
    const double denomY = (u - 2.0*c + d);

    double fx = 0.0;
    if (qAbs(denomX) > 1e-9) fx = 0.5 * (l - r) / denomX;
    double fy = 0.0;
    if (qAbs(denomY) > 1e-9) fy = 0.5 * (u - d) / denomY;

    // Clamp to a reasonable refinement window (avoid wild jumps on flat peaks)
    fx = qBound(-0.75, fx, 0.75);
    fy = qBound(-0.75, fy, 0.75);

    return QPointF(double(dx) + fx, double(dy) + fy);
}

QImage StackAligner::translateF(const QImage &src, double dx, double dy)
{
    QImage dst(src.size(), src.format());
    dst.fill(Qt::black);

    QPainter p(&dst);
    // Use high-quality translation (keeps sub-pixel)
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QTransform t;
    t.translate(dx, dy);
    p.setTransform(t);
    p.drawImage(0, 0, src);
    p.end();

    return dst;
}
