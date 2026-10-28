/*
    developrender -- a CALIBRATION TOOL, not a test (like tonecurvedump): it asserts
    nothing.

    Renders a calibration dump (.wkg, see Develop/workingdump.h) through the real
    Develop::Apply and OutputTransform::ToImage16, so a develop operator can be compared
    against Lightroom's exports of the same raw for any recipe without a round trip
    through the app. The dump is scene-linear working-space data with its white balance
    applied and no exposure, so it is exact for a profile with no look (Winnow Standard).

        developrender <in.wkg> <out.tif> [recipe=<file.json>] [key=value ...]

    recipe   a Winnow sidecar / EditStack JSON (its GLOBAL scope is rendered; masks,
             geometry and spot fills are not -- a warning says so) or a bare params
             object in the same keys.
    key=val  any EditParams field by its PUBLISHED sidecar key (EditStack::paramsToJson:
             exposure, highlights, shadows, whites, blacks, contrast, texture, clarity,
             dehaze, saturation, viewTransform, curves, ...), applied over the recipe.
             "view" is an alias for viewTransform. A value that is not a number is passed
             as a string (curves is ToneCurve's string encoding).

    autotone run AutoTone::Solve (the Basic panel's Auto) on the dump with the params
             built so far, print its eight values on one line ("autotone exposure=0.42
             contrast=6 ...") and render with them. The solve resets those eight first,
             so key=value overrides of them are ignored; any other key still applies.
    long=<px> shrink the OUTPUT to this long edge (the render itself is full size, so
             it is exact, only smaller). For tone comparisons that need no 300 MB TIFF.

    The output is a 16-bit sRGB TIFF, rotated by the dump's orientation so it lines up
    with an export of the same raw. An out path of "-" skips the render (autotone only).
*/
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTransform>
#include <algorithm>
#include <cstdio>

#include "Develop/autotone.h"
#include "Develop/develop.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"
#include "Develop/outputtransform.h"
#include "Develop/workingdump.h"
#include "Develop/workingimage.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 3) {
        std::fprintf(stderr, "usage: developrender <in.wkg> <out.tif> "
                             "[recipe=<file.json>] [autotone] [long=<px>] "
                             "[key=value ...]\n");
        return 2;
    }

    WorkingImage img;
    int degrees = 0;
    if (!WorkingDump::read(QString::fromLocal8Bit(argv[1]), img, degrees)) {
        std::fprintf(stderr, "developrender: not a readable .wkg dump: %s\n", argv[1]);
        return 1;
    }

    /* The recipe, then the overrides, merged as sidecar JSON -- the one format every
       EditParams field already has a published key in. */
    QJsonObject params = EditStack::paramsToJson(EditParams());
    bool autoTone = false;
    int longEdge = 0;
    for (int i = 3; i < argc; ++i) {
        const QString kv = QString::fromLocal8Bit(argv[i]);
        const QString k = kv.section('=', 0, 0), v = kv.section('=', 1);
        if (k == "autotone") { autoTone = true; continue; }
        if (k == "long")     { longEdge = v.toInt(); continue; }
        if (k == "recipe") {
            QFile rf(v);
            if (!rf.open(QIODevice::ReadOnly)) {
                std::fprintf(stderr, "developrender: cannot read recipe %s\n", qPrintable(v));
                return 1;
            }
            const QJsonObject o = QJsonDocument::fromJson(rf.readAll()).object();
            if (o.contains("scopes")) {
                const EditStack stack = EditStack::fromJson(o);
                params = EditStack::paramsToJson(stack.scopes.isEmpty()
                                                     ? EditParams()
                                                     : stack.scopes.first().params);
                if (stack.scopes.size() > 1 || !stack.geometry.isIdentity() ||
                    !stack.spots.isEmpty())
                    std::fprintf(stderr, "developrender: rendering the GLOBAL scope only; "
                                         "masks, geometry and fills are ignored\n");
            }
            else {
                for (auto it = o.begin(); it != o.end(); ++it) params[it.key()] = it.value();
            }
            continue;
        }
        const QString key = (k == "view") ? QStringLiteral("viewTransform") : k;
        if (!params.contains(key)) {
            std::fprintf(stderr, "developrender: unknown key %s\n", qPrintable(k));
            return 2;
        }
        bool numeric = false;
        const double d = v.toDouble(&numeric);
        if (numeric) params[key] = d;
        else         params[key] = v;          // e.g. "curves", ToneCurve's string encoding
    }
    EditParams p = EditStack::paramsFromJson(params);

    if (autoTone) {
        AutoTone::Result r;
        if (!AutoTone::Solve(img, p, r)) {
            std::fprintf(stderr, "developrender: Auto tone could not solve\n");
            return 1;
        }
        p.exposure   = r.exposure;
        p.contrast   = r.contrast;
        p.highlights = r.highlights;
        p.shadows    = r.shadows;
        p.whites     = r.whites;
        p.blacks     = r.blacks;
        p.vibrance   = r.vibrance;
        p.saturation = r.saturation;
        std::fprintf(stdout, "autotone exposure=%.2f contrast=%.0f highlights=%.0f "
                     "shadows=%.0f whites=%.0f blacks=%.0f vibrance=%.0f saturation=%.0f "
                     "renders=%d\n", r.exposure, r.contrast, r.highlights, r.shadows,
                     r.whites, r.blacks, r.vibrance, r.saturation, r.renders);
        std::fflush(stdout);
    }
    if (QString::fromLocal8Bit(argv[2]) == "-") return 0;

    Develop dev;
    if (!dev.Apply(img, p)) { std::fprintf(stderr, "developrender: Apply failed\n"); return 1; }
    QImage out;
    OutputTransform ot;
    if (!ot.ToImage16(img, out, OutputTransform::Space::sRGB,
                      OutputTransform::ViewFromInt(p.viewTransform))) {
        std::fprintf(stderr, "developrender: ToImage16 failed\n");
        return 1;
    }
    if (degrees % 360 != 0) out = out.transformed(QTransform().rotate(degrees));
    if (longEdge > 0 && std::max(out.width(), out.height()) > longEdge)
        out = out.scaled(longEdge, longEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!out.save(QString::fromLocal8Bit(argv[2]), "TIFF")) {
        std::fprintf(stderr, "developrender: cannot write %s\n", argv[2]);
        return 1;
    }
    return 0;
}
