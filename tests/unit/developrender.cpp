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

    The output is a 16-bit sRGB TIFF, rotated by the dump's orientation so it lines up
    with an export of the same raw.
*/
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTransform>
#include <cstdio>

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
                             "[recipe=<file.json>] [key=value ...]\n");
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
    for (int i = 3; i < argc; ++i) {
        const QString kv = QString::fromLocal8Bit(argv[i]);
        const QString k = kv.section('=', 0, 0), v = kv.section('=', 1);
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
    const EditParams p = EditStack::paramsFromJson(params);

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
    if (!out.save(QString::fromLocal8Bit(argv[2]), "TIFF")) {
        std::fprintf(stderr, "developrender: cannot write %s\n", argv[2]);
        return 1;
    }
    return 0;
}
