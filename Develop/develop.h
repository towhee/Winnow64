#ifndef DEVELOP_H
#define DEVELOP_H

#include "Develop/editparams.h"
#include "Develop/workingimage.h"

/*
    Applies parametric develop adjustments to a WorkingImage in place. Stateless and
    reentrant, constructed per decode (same discipline as Demosaic / RawFormat) so it
    carries no cross-thread state.

    The operation order is fixed and hard-coded (Lightroom-like); order matters and is not a
    caller concern. Ops are split by cost (see notes/Documentation.txt "Layer & masking
    model"):

        SPATIAL ops (denoise) need a neighbourhood, so each owns a full-image pass. Deferred;
        currently a no-op.

        POINT ops (white balance, exposure, contrast, tone regions, texture, dehaze) are pure
        per-pixel functions, so they are FUSED into a single parallel pass: coefficients are
        precomputed once from EditParams, then applied per pixel in one loop over the image.
        This is the slider-drag hot path. PHASE 1 implements exposure + contrast; the rest are
        identity no-ops that slot into the same pass as they land.
*/
class Develop
{
public:
    Develop() = default;

    /* Apply p to img in place. Returns true on success (and trivially when p is identity,
       leaving img untouched). */
    bool Apply(WorkingImage &img, const EditParams &p);

private:
    /* Spatial op: owns a full-image pass. Deferred -- no-op. */
    void Denoise(WorkingImage &img, const EditParams &p);

    /* Precomputed once per Apply(); the fused point pass reads only these. active == false
       means no implemented point op would change a pixel, so the pass is skipped entirely. */
    struct PointCoeffs {
        bool  active        = false;
        float exposureGain  = 1.0f;   // 2^EV
        float contrastSlope = 1.0f;   // perceptual-domain slope about mid-grey; 1 = identity
        float white         = 1.0f;   // linear value that maps to display white
    };
    static PointCoeffs buildPointCoeffs(const EditParams &p, const WorkingImage &img);

    /* The fused per-pixel pass, parallelised over row ranges. */
    void applyPointOps(WorkingImage &img, const PointCoeffs &c);
};

#endif // DEVELOP_H
