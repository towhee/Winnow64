#include "Develop/develop.h"
#include <QtGlobal>

bool Develop::Apply(WorkingImage &img, const EditParams &p)
{
    if (!img.isValid()) return false;
    if (p.isIdentity()) return true;    // nothing to do; serve image as-is

    /* Fixed pipeline order. See notes/Documentation.txt "DEVELOP / IMAGE EDIT". */
    Denoise(img, p);
    WhiteBalance(img, p);
    Exposure(img, p);
    Contrast(img, p);
    ToneRegions(img, p);
    Texture(img, p);
    Dehaze(img, p);
    return true;
}

/*
    SCAFFOLD: all ops are identity no-ops. Real implementations operate on the scene-linear
    float buffer (img.rgb) and land in a later phase.
*/

void Develop::Denoise(WorkingImage &img, const EditParams &p)      { Q_UNUSED(img) Q_UNUSED(p) }
void Develop::WhiteBalance(WorkingImage &img, const EditParams &p) { Q_UNUSED(img) Q_UNUSED(p) }
void Develop::Exposure(WorkingImage &img, const EditParams &p)     { Q_UNUSED(img) Q_UNUSED(p) }
void Develop::Contrast(WorkingImage &img, const EditParams &p)     { Q_UNUSED(img) Q_UNUSED(p) }
void Develop::ToneRegions(WorkingImage &img, const EditParams &p)  { Q_UNUSED(img) Q_UNUSED(p) }
void Develop::Texture(WorkingImage &img, const EditParams &p)      { Q_UNUSED(img) Q_UNUSED(p) }
void Develop::Dehaze(WorkingImage &img, const EditParams &p)       { Q_UNUSED(img) Q_UNUSED(p) }
