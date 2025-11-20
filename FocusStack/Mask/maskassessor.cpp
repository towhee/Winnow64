#include "maskassessor.h"
#include <QImage>
#include <QPainter>
#include <QColor>
#include <QtMath>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

/*
Future Enhancements (Already Designed For)
    1.	Vector contours
    •	Draw outline using vector paths instead of per-pixel points
    •	Could use QPainterPath or OpenCV findContours
    2.	Adjustable boundary thickness
    •	Increase stroke width or dilate mask before outlining
    3.	Heatmap mode
    •	Map mask values to a colormap such as jet, magma, or turbo
    •	Useful for visualizing confidence maps instead of binary masks
    4.	Alpha transparency in final image
    •	Save output with alpha channel instead of flattening onto opaque PNG
    •	Allows compositing in external tools
    5.	Tile-based overlay for very large images
    •	Process image in chunks to reduce memory usage
    •	Required for multi-hundred-megapixel TIFFs
    6.	GPU-based overlay
    •	Implement blending in a shader or using Qt Quick
    •	Useful for real-time preview in the UI
    7.	Flexible colormap selection
    •	Allow user to choose mask tint, heatmap scheme, opacity, and blend mode
    8.	Mask comparison mode
    •	Overlay multiple masks in alternating colors or flicker between them
    •	Useful for comparing FusionPMaxBasic vs FusionWavePMax masks
    9.	Soft mask visualization
    •	Support floating-point masks (0-1) with smooth gradients
    •	Helps visualize confidence instead of binary regions
*/

MaskAssessor::MaskAssessor()
{
}

bool MaskAssessor::overlay(const QString &maskPath,
                           const QString &baseImagePath,
                           const QString &outputPath,
                           const MaskAssessOptions &options)
{
    // --- Load mask ---
    QImage mask(maskPath);
    if (mask.isNull()) {
        qWarning() << "MaskAssessor: Failed to load mask:" << maskPath;
        return false;
    }

    // Ensure mask is grayscale, convert if necessary
    if (mask.format() != QImage::Format_Grayscale8) {
        mask = mask.convertToFormat(QImage::Format_Grayscale8);
    }

    // --- Load base image ---
    QImage base(baseImagePath);
    if (base.isNull()) {
        qWarning() << "MaskAssessor: Failed to load base image:" << baseImagePath;
        return false;
    }

    // Optional: convert base to grayscale BEFORE compositing
    if (options.grayscaleBase) {
        base = base.convertToFormat(QImage::Format_Grayscale8)
        .convertToFormat(QImage::Format_ARGB32);
    }
    else {
        // Ensure base has alpha channel for blending
        if (base.format() != QImage::Format_ARGB32) {
            base = base.convertToFormat(QImage::Format_ARGB32);
        }
    }

    // --- Sanity check: sizes must match ---
    if (base.size() != mask.size()) {
        qWarning() << "MaskAssessor: Mask and base image dimensions differ."
                   << base.size() << mask.size();
        return false;
    }

    // Create output canvas
    QImage out = base.copy();

    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);

    // --- Overlay mask as tint ---
    QColor tint = options.maskColor;
    tint.setAlphaF(options.maskOpacity);

    for (int y = 0; y < mask.height(); ++y) {
        const uchar *mline = mask.constScanLine(y);
        QRgb *oline = reinterpret_cast<QRgb*>(out.scanLine(y));

        for (int x = 0; x < mask.width(); ++x) {
            uchar mv = mline[x];

            // If mask value is zero, skip
            if (mv == 0)
                continue;

            // Blend masked pixel
            // Linear interpolation: out = (1-a)*orig + a*tint
            float a = options.maskOpacity * (mv / 255.0f);

            QRgb orig = oline[x];
            int r = qRed(orig);
            int g = qGreen(orig);
            int b = qBlue(orig);

            r = (1 - a) * r + a * tint.red();
            g = (1 - a) * g + a * tint.green();
            b = (1 - a) * b + a * tint.blue();

            oline[x] = qRgba(r, g, b, 255);
        }
    }

    // --- Draw boundary outline (optional) ---
    if (options.drawBoundary) {
        // Simple outline using 1px dilate+xor method
        QImage edge(mask.size(), QImage::Format_Grayscale8);
        edge.fill(0);

        for (int y = 1; y < mask.height() - 1; ++y) {
            const uchar *row = mask.constScanLine(y);
            for (int x = 1; x < mask.width() - 1; ++x) {
                if (row[x] == 0) continue;

                // Check neighbors
                bool boundary = false;
                for (int dy = -1; dy <= 1 && !boundary; ++dy) {
                    const uchar *nrow = mask.constScanLine(y + dy);
                    for (int dx = -1; dx <= 1 && !boundary; ++dx) {
                        if (nrow[x + dx] == 0)
                            boundary = true;
                    }
                }

                if (boundary)
                    edge.setPixel(x, y, 255);
            }
        }

        // Draw boundary in contrasting color
        QColor edgeColor = Qt::yellow;
        edgeColor.setAlpha(255);

        p.setPen(edgeColor);
        for (int y = 0; y < edge.height(); ++y) {
            const uchar *line = edge.constScanLine(y);
            for (int x = 0; x < edge.width(); ++x) {
                if (line[x] > 0)
                    p.drawPoint(x, y);
            }
        }
    }

    p.end();

    // Ensure directory exists
    QDir d = QFileInfo(outputPath).absoluteDir();
    if (!d.exists())
        d.mkpath(".");

    bool ok = out.save(outputPath, "PNG");
    if (!ok) {
        qWarning() << "MaskAssessor: Failed to save overlay PNG:" << outputPath;
    }

    return ok;
}
