#ifndef CROPTRANSFORM_H
#define CROPTRANSFORM_H

#include <QImage>
#include <QPointF>
#include <QRectF>
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

}

#endif // CROPTRANSFORM_H
