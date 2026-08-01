#ifndef MIGANFILL_H
#define MIGANFILL_H

#include <QImage>
#include <QVector>
#include "Develop/editstack.h"        // FillSpot

/*
    Regenerative spot fill (MI-GAN inpainting) through the unified inference layer (ORT ->
    CoreML on macOS / DirectML on Windows / CPU). MI-GAN was chosen over LaMa for spots: 27 MB,
    ~76 ms on the ANE, identical quality on small holes (the LaMa-vs-MI-GAN bake-off). The model
    is fixed 512x512; each spot is healed on a square context tile around it, composited back.

    apply() runs on the developed, oriented, FULL-FRAME image (the input to the geometry stage)
    so heals stay glued to the photo content when it is later cropped/straightened -- called
    from developCompositeStack just before CropTransform::applyGeometry. Warn-if-absent: with no
    model (or ORT not built), isAvailable() is false and apply() no-ops, matching the AI masks.
*/
namespace MiganFill {

/* True once migan.onnx is loaded and the inference runtime is available. */
bool isAvailable();

/* Heal every enabled spot into img (in place). Spots carry output-normalized brush
   geometry (FillSpotGeom), applied in order so each heals over the accumulation of prior
   ones. Returns true if anything was applied. */
bool apply(QImage &img, const QVector<FillSpot> &spots);

} // namespace MiganFill

#endif // MIGANFILL_H
