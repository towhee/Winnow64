/*
    Winnow's tone operators, dumped as display-level curves, for fitting against Lightroom.

    WHY IT EXISTS. The fit needs to answer "what does Whites +100 do to a tone at level L"
    for many candidate constants. Getting that from the app means a manual export per
    iteration, and modelling it in Python does not work -- the view transform sits between
    the tone LUT and the display, and approximating it as a plain gamma was measured 13-27
    levels RMS wrong against the real exports. So the model has to BE the pipeline.

    WHAT IT DOES. Pushes a scene-linear ramp through Develop::Apply + OutputTransform at a
    given EditParams, twice -- once at identity, once with the slider -- and prints the map
    from the identity render's level to the slider render's level. That map is exactly what
    histogram-matching recovers from a pair of real exports, so the two are directly
    comparable, and the ramp version costs milliseconds instead of a GUI round trip.

    IT IS A CALIBRATION TOOL, NOT A TEST: it asserts nothing and is not registered with
    ctest. tst_toneregions is what guards this behaviour.
*/
#include <QCoreApplication>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <QImage>

#include "Develop/develop.h"
#include "Develop/workingimage.h"
#include "Develop/editparams.h"
#include "Develop/colorspace.h"
#include "Develop/outputtransform.h"

namespace {

/* The ramp spans the LUT's whole domain -- ~3 stops above white (kToneLutMaxN = 8) -- so
   the highlight headroom the endpoint moves scale is covered, not just the visible range.
   Scene-referred, because that is what a raw decode hands Develop and what the view
   transform is for; rendering display-referred would silently force the transform off. */
constexpr int   kN     = 4096;
constexpr float kMaxLin = 8.0f;

WorkingImage ramp()
{
    WorkingImage img;
    img.width = kN; img.height = 1; img.white = 1.0f;
    img.sceneReferred = true;
    img.space = ColorSpaceMath::kWorking;
    img.rgb.resize(size_t(kN) * 3);
    for (int i = 0; i < kN; ++i) {
        const float v = kMaxLin * std::pow(float(i) / (kN - 1), 2.2f);
        img.rgb[size_t(i)*3+0] = img.rgb[size_t(i)*3+1] = img.rgb[size_t(i)*3+2] = v;
    }
    return img;
}

std::vector<int> render(const EditParams &p, OutputTransform::ViewTransform view)
{
    WorkingImage img = ramp();
    Develop dev;
    dev.Apply(img, p);
    QImage out;
    OutputTransform ot;
    ot.ToImage(img, out, OutputTransform::Space::sRGB, view);
    std::vector<int> v(kN);
    const uchar *line = out.constScanLine(0);
    for (int i = 0; i < kN; ++i) v[i] = line[i * 3];
    return v;
}

} // namespace

int main(int argc, char **argv)
{
    /* usage: tonecurvedump <slider> <value> [more pairs...] */
    EditParams p;
    for (int i = 1; i + 1 < argc; i += 2) {
        const char *k = argv[i];
        const float v = float(atof(argv[i + 1]));
        if      (!strcmp(k, "blacks"))     p.blacks = v;
        else if (!strcmp(k, "shadows"))    p.shadows = v;
        else if (!strcmp(k, "highlights")) p.highlights = v;
        else if (!strcmp(k, "whites"))     p.whites = v;
        else if (!strcmp(k, "contrast"))   p.contrast = v;
        else { fprintf(stderr, "unknown slider: %s\n", k); return 2; }
    }

    const auto view = OutputTransform::ViewTransform::Filmic;   // the export default
    const std::vector<int> base = render(EditParams(), view);
    const std::vector<int> mod  = render(p, view);

    /* Collapse the ramp into a level -> level map. Several ramp samples land on one input
       level; average their outputs so the map is the operator rather than one sample's
       rounding. */
    double sum[256] = {0}; int cnt[256] = {0};
    for (int i = 0; i < kN; ++i) { sum[base[i]] += mod[i]; cnt[base[i]]++; }

    static const int SAMPLE[] = {8,16,32,48,64,96,128,160,192,224,240,250};
    for (int s : SAMPLE) {
        int lo = s; while (lo > 0   && !cnt[lo]) --lo;
        int hi = s; while (hi < 255 && !cnt[hi]) ++hi;
        const int use = cnt[s] ? s : (s - lo <= hi - s ? lo : hi);
        const int outv = cnt[use] ? int(sum[use] / cnt[use] + 0.5) : use;
        printf("%+6d", outv - s);
    }
    printf("\n");
    return 0;
}
