#include "ImageFormats/Raw/cameramodel.h"

#include <QRegularExpression>

namespace {

/*
    EXIF Make -> the maker word both tables spell the model with.

    Matched as a case-insensitive PREFIX of the simplified Make, because makers pad it
    with a corporate tail that changes between bodies and firmware: "OLYMPUS IMAGING
    CORP.", "OLYMPUS CORPORATION", "RICOH IMAGING COMPANY, LTD.".

    OM Digital Solutions maps to "Olympus" deliberately -- see cameraModelAliases().
*/
struct MakerRule { const char *makePrefix; const char *maker; };

const MakerRule kMakers[] = {
    { "OLYMPUS",              "Olympus"     },
    { "OM DIGITAL SOLUTIONS", "Olympus"     },
    { "FUJIFILM",             "Fujifilm"    },
    { "PANASONIC",            "Panasonic"   },
    { "SONY",                 "Sony"        },
    { "NIKON",                "Nikon"       },
    { "CANON",                "Canon"       },
    { "LEICA",                "Leica"       },
    { "PENTAX",               "Pentax"      },
    { "RICOH",                "Ricoh"       },
    { "SAMSUNG",              "Samsung"     },
    { "SIGMA",                "Sigma"       },
    { "HASSELBLAD",           "Hasselblad"  },
    { "PHASE ONE",            "Phase One"   },
    { "EPSON",                "Epson"       },
    { "KODAK",                "Kodak"       },
    { "MINOLTA",              "Minolta"     },
    { "APPLE",                "Apple"       },
    { "GOOGLE",               "Google"      },
    { "DJI",                  "DJI"         },
};

/*
    Maker words that name the SAME bodies. Listed as groups so the alias is symmetric: a
    model canonicalised one way still finds data filed the other.
*/
const char *const kOlympusGroup[] = { "Olympus", "OM Digital Solutions" };

/* Every canonical maker word, for "does this model already carry its maker?". */
bool startsWithAMaker(const QString &model, QString *maker = nullptr)
{
    for (const MakerRule &r : kMakers) {
        const QString m = QString::fromLatin1(r.maker);
        if (model.startsWith(m + QLatin1Char(' '), Qt::CaseInsensitive)) {
            if (maker) *maker = m;
            return true;
        }
    }
    for (const char *const g : kOlympusGroup) {
        const QString m = QString::fromLatin1(g);
        if (model.startsWith(m + QLatin1Char(' '), Qt::CaseInsensitive)) {
            if (maker) *maker = m;
            return true;
        }
    }
    return false;
}

/*
    Space out the two gluings EXIF uses and the tables do not.

    "Mark": Olympus writes "E-M1MarkIII" and "OM-1MarkII" where both tables carry
    "E-M1 Mark III" and "OM-1 Mark II". The trailing lookahead keeps this to a version
    suffix -- only a roman numeral or a digit may follow -- so a model with "Mark" in it
    for any other reason is left alone, and a name that is already spaced is unchanged.

    "GFX": Fujifilm writes "GFX50S II" and "GFX100S"; both tables carry "GFX 50S II" and
    "GFX 100S". Anchored at the start and Fujifilm-only, because the same rule applied
    generally would break the X100 family ("X100V" is not "X 100V").
*/
QString spaceOutModel(const QString &model, const QString &maker)
{
    static const QRegularExpression mark("\\s*Mark\\s*(?=[IVX0-9])");
    QString out = model;
    out.replace(mark, " Mark ");
    if (maker == QLatin1String("Fujifilm")) {
        static const QRegularExpression gfx("^GFX(?=\\d)");
        out.replace(gfx, "GFX ");
    }
    return out.simplified();
}

} // namespace

QString canonicalCameraModel(const QString &make, const QString &model)
{
    const QString rawModel = model.simplified();
    if (rawModel.isEmpty()) return QString();

    /* Already maker-prefixed (Nikon, Canon, and anything a parser prefixed itself): keep
       the maker it names rather than second-guessing it from Make, which for a converted
       or re-tagged file may not agree. Still spaced out, since the gluing is the maker's
       habit and survives the prefix. */
    QString maker;
    if (startsWithAMaker(rawModel, &maker))
        return spaceOutModel(rawModel, maker);

    const QString rawMake = make.simplified();
    for (const MakerRule &r : kMakers) {
        if (!rawMake.startsWith(QString::fromLatin1(r.makePrefix), Qt::CaseInsensitive))
            continue;
        maker = QString::fromLatin1(r.maker);
        return maker + QLatin1Char(' ') + spaceOutModel(rawModel, maker);
    }

    /* An unlisted maker. Make + Model is the convention both tables follow, so it is a
       better guess than the bare model -- and a maker with no entry in either table loses
       nothing by it. Make alone being empty leaves the model as it stands. */
    if (rawMake.isEmpty()) return spaceOutModel(rawModel, QString());
    return rawMake + QLatin1Char(' ') + spaceOutModel(rawModel, QString());
}

QStringList cameraModelAliases(const QString &canonicalModel)
{
    QStringList out;
    const QString m = canonicalModel.simplified();
    if (m.isEmpty()) return out;

    for (const char *const from : kOlympusGroup) {
        const QString prefix = QString::fromLatin1(from) + QLatin1Char(' ');
        if (!m.startsWith(prefix, Qt::CaseInsensitive)) continue;
        const QString body = m.mid(prefix.size());
        for (const char *const to : kOlympusGroup) {
            const QString alias = QString::fromLatin1(to) + QLatin1Char(' ') + body;
            if (alias.compare(m, Qt::CaseInsensitive) != 0) out << alias;
        }
        break;
    }
    return out;
}
