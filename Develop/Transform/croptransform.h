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
    Apply a stored Geometry (straighten -> warp -> crop) to a developed/oriented image.
    The kaleidoscope is NOT part of it (it is a per-scope Effect -- applyKaleidoscope*). This is the
    geometry-last render stage: the develop pipeline calls it after the develop ops + EXIF rotation,
    and it is the same transform used for display and export. Returns src unchanged when the
    geometry is identity; the crop-only case is a fast QImage::copy of the sub-rectangle.
*/
QImage applyGeometry(const QImage &src, const Geometry &g);

/*
    A kaleidoscope's settings, from one scope's EditParams (Effects > Kaleidoscope). The
    centre is normalized in the frame being folded -- the CROPPED frame, since the fold
    runs after applyGeometry.
*/
struct Kaleido {
    int    segments = 0;            // 0 = off, else even 2..32
    double x = 0.5, y = 0.5;        // centre, normalized
    double angle = 0.0;             // deg, y-down: turns the mirror lines + the slice
    double source = 0.0;            // deg: turns only the sampled slice
    bool on() const { return segments >= 2; }
    static Kaleido from(const EditParams &p) {
        Kaleido k;
        k.segments = p.kaleidoSegments;
        k.x = p.kaleidoX; k.y = p.kaleidoY;
        k.angle = p.kaleidoAngle; k.source = p.kaleidoSource;
        return k;
    }
};

/*
    The kaleidoscope fold (MW::developCompositeStack runs it after applyGeometry). Every
    output pixel is put in polar form about the centre, its angle is folded into ONE
    wedge of 360/N degrees -- reflected in every other wedge, so neighbouring wedges
    meet as mirror images -- and the source is sampled at the same radius along the
    folded angle (bilinear). A ray that leaves the frame is reflected at the border, so
    there are no empty corners. Output size and format family match the input (8-bit ->
    ARGB32, 16-bit -> RGBA64). Returns src unchanged when off.
*/
QImage applyKaleidoscope(const QImage &src, const Kaleido &k);

/*
    The MASKED fold (a mask scope's kaleidoscope): only the masked pixels are folded --
    the unmasked part of the frame is treated as transparent -- and the result is laid
    over the frame. mask is the scope's coverage in the SAME (cropped) frame, any format
    (read as grey, 0..255), the same size as frame. Folding colour and coverage with the
    one map and blending by the folded coverage is lerp(frame, fold(frame), fold(mask)),
    which equals fold(frame x mask) over the frame except within a bilinear tap of a hard
    mask edge. Returns frame unchanged when off or when the mask does not match.
*/
QImage applyKaleidoscopeMasked(const QImage &frame, const QImage &mask, const Kaleido &k);

/*
    The POINT form of the fold: where in the kaleidoscope's INPUT (the cropped frame,
    normalized) the output point outN samples. Lets a picker clicked on the mirrored
    image find the photo pixel it is really looking at.
*/
QPointF kaleidoSourceNorm(QPointF outN, double W, double H, const Kaleido &k);

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
