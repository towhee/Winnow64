#ifndef DEVELOP_H
#define DEVELOP_H

#include "Develop/editparams.h"
#include "Develop/workingimage.h"

/*
    Applies parametric develop adjustments to a WorkingImage in place. Stateless and
    reentrant, constructed per decode (same discipline as Demosaic / RawFormat) so it
    carries no cross-thread state.

    The operation order is fixed and hard-coded (Lightroom-like); order matters and is
    not a caller concern. SCAFFOLD: every op below is an identity no-op for now, so Apply()
    leaves the image unchanged. Implementations land in a later phase (exposure / white
    balance / contrast / tone first; texture / dehaze / denoise after).

    Note: for RAW, white balance and denoise give better results applied inside the raw
    pipeline (RawFormat reads the same EditParams). When that lands those two ops here
    become conditional, but EditParams and this interface are unchanged.
*/
class Develop
{
public:
    Develop() = default;

    /* Apply p to img in place. Returns true on success (and trivially when p is
       identity, leaving img untouched). */
    bool Apply(WorkingImage &img, const EditParams &p);

private:
    void Denoise(WorkingImage &img, const EditParams &p);        // 1
    void WhiteBalance(WorkingImage &img, const EditParams &p);   // 2
    void Exposure(WorkingImage &img, const EditParams &p);       // 3
    void Contrast(WorkingImage &img, const EditParams &p);       // 4
    void ToneRegions(WorkingImage &img, const EditParams &p);    // 5 highlights/shadows/whites/blacks
    void Texture(WorkingImage &img, const EditParams &p);        // 6
    void Dehaze(WorkingImage &img, const EditParams &p);         // 7
};

#endif // DEVELOP_H
