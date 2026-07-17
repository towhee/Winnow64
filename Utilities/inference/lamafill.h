#ifndef LAMAFILL_H
#define LAMAFILL_H

#include <QImage>
#include <QVector>
#include "Develop/editstack.h"        // FillSpot

/*
    The Fill Replace heal engine. Each FillSpot's paramsJson "kind" (Develop/fillspot.h,
    set by the Replace panel mode) picks how it heals:
      spot / fill : exemplar CLONE heal -- the best-matching nearby source patch is
                    template-matched, Poisson-blended over the hole and feathered by the
                    brush alpha (Lightroom heal-brush algorithm; real texture at every
                    frequency, no model). Falls back to the model path when no clean
                    source exists (e.g. the hole touches a frame edge).
      object      : regenerative model fill with LaMa inpainting through the unified
                    inference layer, on the Apple GPU (ORT CoreML EP, CPUAndGPU) -- see
                    project_lama_coreml_gpu. LaMa contract:
                      image: 1x3x512x512 RGB [0,1]; mask: 1x1x512x512, 1 = inpaint
                      output: 1x3x512x512 RGB [0,255]
    MiganFill stays available and is selected instead when G::useLamaSpotFill is false.

    apply() runs on the developed, oriented, FULL-FRAME image (before the geometry stage)
    so heals stay glued to content when later cropped/straightened -- called from
    developCompositeStack just before CropTransform::applyGeometry. Warn-if-absent: with
    no model (or ORT not built) the MODEL path no-ops (clone heals still work), like the
    masks.
*/
namespace LamaFill {

/* True once lama.onnx is loaded and the inference runtime is available. */
bool isAvailable();

/* Heal every enabled spot into img (in place). Spots carry output-normalized brush
   geometry (FillSpotGeom), applied in order so each heals over the accumulation of
   prior ones. cachePath (the image's file path) keys the pinned-source-offset cache so
   a spot's clone geometry stays stable across re-renders (develop-slider drags) --
   empty disables pinning. Returns true if anything was applied. */
bool apply(QImage &img, const QVector<FillSpot> &spots,
           const QString &cachePath = QString());

} // namespace LamaFill

#endif // LAMAFILL_H
