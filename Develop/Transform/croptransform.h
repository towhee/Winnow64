#ifndef CROPTRANSFORM_H
#define CROPTRANSFORM_H

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QTransform>
#include "Develop/editstack.h"   // Geometry

/*
    The perspective-warp PIXEL engine for the Develop Transform tool. Self-contained (OpenCV only,
    portable mac/Windows) so it can be driven either from ImageView::rectifyCrop (the interactive
    preview today) or, later, from the develop render pipeline as the geometry-applied-last stage.

    rectifyPerspective() takes a 4-point quadrilateral (the warp overlay's corners, in image-PIXEL
    coords, order TL,TR,BR,BL) and warps the WHOLE image so that quad becomes an axis-aligned
    rectangle -- i.e. it removes the keystone/perspective the quad traces. The returned image keeps
    the rest of the frame, perspective-corrected, with TRANSPARENT pixels where the warp leaves the
    original image. outCropNorm is set to the largest axis-aligned rectangle that lies entirely
    inside the valid (opaque) region, in normalized coords (0..1) of the RETURNED image -- the
    sensible default crop the user then refines. Returns a null QImage on failure (degenerate quad).
*/
namespace CropTransform {

QImage rectifyPerspective(const QImage &src, const QPointF quad[4], QRectF &outCropNorm);

/*
    Apply a stored Geometry (straighten -> warp -> crop) to a developed/oriented image. This is the
    geometry-last render stage: the develop pipeline calls it after the develop ops + EXIF rotation,
    and it is the same transform used for display and export. Returns src unchanged when the
    geometry is identity; the crop-only case is a fast QImage::copy of the sub-rectangle.
*/
QImage applyGeometry(const QImage &src, const Geometry &g);

/*
    The POINT twin of applyGeometry: the transform a pixel undergoes in the geometry
    stage, without touching any pixels. Maps a point in the stage's INPUT space (the
    developed/oriented full frame, srcW x srcH pixels) to the OUTPUT space (the
    cropped/straightened/warped result), in the same straighten -> warp -> crop order
    applyGeometry uses. outSize, when given, receives the output's pixel dimensions.

    This is what lets the mask / spot overlays -- whose coordinates are all normalized in
    the stage's INPUT space, because masks are rasterized before geometry -- be drawn on,
    and edited over, the geometry-applied image the loupe shows. Only the input frame's
    ASPECT matters: scaling srcW and srcH together scales the output the same way, so a
    proxy-sized frame maps identically.
*/
QTransform geometryTransform(double srcW, double srcH, const Geometry &g,
                             QSizeF *outSize = nullptr);

/*
    The largest axis-aligned rectangle (normalized in the straightened output canvas) that contains
    only image content after rotating a W x H frame by deg degrees -- i.e. the auto-crop that removes
    the rotation wedges. Analytic (rotatedRectWithMaxArea), so it needs only the frame aspect + angle
    and matches applyGeometry's straighten exactly. Identity (0,0,1,1) at deg == 0.
*/
QRectF straightenCropNorm(double W, double H, double deg);

}

#endif // CROPTRANSFORM_H
