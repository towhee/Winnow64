#include "Develop/lensdb.h"

#include <QDir>
#include <QMutex>
#include <QThreadPool>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QXmlStreamReader>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <mutex>

namespace LensDb {

namespace {

/* Lower case, letters and digits only: "NIKON CORPORATION" -> "nikoncorporation". */
QString norm(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar ch : s)
        if (ch.isLetterOrNumber()) out += ch.toLower();
    return out;
}

QString firstWord(const QString &s)
{
    return s.section(QLatin1Char(' '), 0, 0, QString::SectionSkipEmpty);
}

float toFloat(const QStringView v, float def)
{
    bool ok = false;
    const float f = v.toFloat(&ok);
    return ok ? f : def;
}

/* "1.5", "3:2" or "16:9" -> long / short. */
float parseAspect(const QString &s)
{
    const int colon = s.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        const float a = toFloat(QStringView(s).left(colon), 0.0f);
        const float b = toFloat(QStringView(s).mid(colon + 1), 0.0f);
        if (a > 0.0f && b > 0.0f) return std::max(a, b) / std::min(a, b);
        return 1.5f;
    }
    const float r = toFloat(QStringView(s), 1.5f);
    return r >= 1.0f ? r : (r > 0.0f ? 1.0f / r : 1.5f);
}

/* Focal range from a lens name ("... 24-70mm ..." / "... 50mm ..."), lensfun's fallback
   when the XML has no <focal>. */
bool focalFromName(const QString &name, float &lo, float &hi)
{
    static const QRegularExpression re(
        QStringLiteral("(\\d+(?:\\.\\d+)?)(?:\\s*-\\s*(\\d+(?:\\.\\d+)?))?\\s*mm"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(name);
    if (!m.hasMatch()) return false;
    lo = m.captured(1).toFloat();
    hi = m.captured(2).isEmpty() ? lo : m.captured(2).toFloat();
    return lo > 0.0f;
}

/* Hermite spline through y2 (t = 0) and y3 (t = 1) with Catmull-Rom tangents, falling
   back to the chord when a neighbour is missing (FLT_MAX). Lensfun's scheme. */
double hermite(double y1, double y2, double y3, double y4, double t)
{
    const double tg2 = (y1 == FLT_MAX) ? (y3 - y2) : (y3 - y1) * 0.5;
    const double tg3 = (y4 == FLT_MAX) ? (y3 - y2) : (y4 - y2) * 0.5;
    const double t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * y2 + (t3 - 2 * t2 + t) * tg2
         + (-2 * t3 + 3 * t2) * y3 + (t3 - t2) * tg3;
}

/*
    Interpolate a focal-keyed calibration list (sorted by focal) at `focal`. scaled(i)
    says whether term i follows the 1/f law and is therefore multiplied by its focal
    before the spline and divided by the target focal after.
*/
template <typename P, int N, typename Scaled>
bool splineAt(const QVector<P> &pts, float focal, float (&out)[N], Scaled scaled)
{
    if (pts.isEmpty()) return false;
    int hiIx = 0;
    while (hiIx < pts.size() && pts[hiIx].focal < focal) ++hiIx;
    if (hiIx < pts.size() && pts[hiIx].focal == focal) {
        std::copy(pts[hiIx].t, pts[hiIx].t + N, out);
        return true;
    }
    /* Outside the calibrated range: the nearest end, as lensfun does. */
    if (hiIx == 0 || hiIx == pts.size()) {
        const P &p = pts[hiIx == 0 ? 0 : pts.size() - 1];
        std::copy(p.t, p.t + N, out);
        return true;
    }
    const P *s0 = hiIx >= 2 ? &pts[hiIx - 2] : nullptr;
    const P &s1 = pts[hiIx - 1];
    const P &s2 = pts[hiIx];
    const P *s3 = hiIx + 1 < pts.size() ? &pts[hiIx + 1] : nullptr;
    const double t = (focal - s1.focal) / double(s2.focal - s1.focal);
    for (int i = 0; i < N; ++i) {
        const bool sc = scaled(i);
        auto val = [&](const P *p) -> double {
            if (!p) return FLT_MAX;
            return sc ? double(p->t[i]) * p->focal : double(p->t[i]);
        };
        double v = hermite(val(s0), val(&s1), val(&s2), val(s3), t);
        if (sc) v /= focal;
        out[i] = float(v);
    }
    return true;
}

}   // namespace

/* ------------------------------------------------------------------------------------ */

QString Lens::displayName() const
{
    const QString m = norm(maker);
    if (!m.isEmpty() && norm(model).startsWith(norm(firstWord(maker))))
        return model;
    return maker.isEmpty() ? model : maker + QLatin1Char(' ') + model;
}

QStringList Database::words(const QString &name)
{
    /* Runs of one character class: digits, punctuation, everything else. Single
       punctuation marks and a lone "f" are dropped (lensfun's rules). */
    QStringList out;
    QString cur;
    int curClass = -1;
    auto classOf = [](QChar c) {
        if (c.isDigit()) return 0;
        if (c.isSpace()) return 3;
        if (c.isPunct() || c.isSymbol()) return 1;
        return 2;
    };
    auto flush = [&]() {
        if (cur.isEmpty()) return;
        const bool singlePunct = (curClass == 1 && cur.size() == 1);
        const QString w = cur.toLower();
        if (!singlePunct && w != QLatin1String("f")) out << w;
        cur.clear();
    };
    for (const QChar c : name) {
        const int k = classOf(c);
        if (k == 3) { flush(); curClass = -1; continue; }
        if (k != curClass) { flush(); curClass = k; }
        cur += c;
    }
    flush();
    return out;
}

/* ------------------------------------------------------------------------------------
   Loading
   ------------------------------------------------------------------------------------ */

const Database &Database::instance()
{
    static Database *db = nullptr;
    static std::once_flag once;
    std::call_once(once, [] {
        db = new Database;
        db->load();
    });
    return *db;
}

QString Database::userDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base.isEmpty() ? QString() : base + QStringLiteral("/lensfun");
}

void Database::load()
{
    auto readDir = [this](const QString &dir) {
        QDir d(dir);
        if (!d.exists()) return;
        const QStringList files = d.entryList({QStringLiteral("*.xml")}, QDir::Files,
                                              QDir::Name);
        for (const QString &f : files) {
            QFile file(d.filePath(f));
            if (file.open(QIODevice::ReadOnly)) parseXml(file.readAll());
        }
    };
    readDir(QStringLiteral(":/lensfun"));
    const QString user = userDir();
    if (!user.isEmpty()) readDir(user);
    finish();
}

void Database::parseXml(const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    /* The current element's text; lang= variants of <maker>/<model> are kept apart. */
    auto readText = [&r]() { return r.readElementText(QXmlStreamReader::SkipChildElements)
                                     .trimmed(); };

    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement()) continue;
        const QStringView name = r.name();

        if (name == QLatin1String("mount")) {
            QString mname;
            QStringList compat;
            while (r.readNextStartElement()) {
                if (r.name() == QLatin1String("name")) mname = readText();
                else if (r.name() == QLatin1String("compat")) compat << readText();
                else r.skipCurrentElement();
            }
            if (!mname.isEmpty()) mountCompat[mname] = compat;
        }
        else if (name == QLatin1String("camera")) {
            Camera c;
            while (r.readNextStartElement()) {
                const bool lang = r.attributes().hasAttribute(QLatin1String("lang"));
                const QStringView n = r.name();
                if (n == QLatin1String("maker")) { const QString t = readText();
                                                   if (!lang) c.maker = t; }
                else if (n == QLatin1String("model")) { const QString t = readText();
                                                        if (!lang) c.model = t; }
                else if (n == QLatin1String("variant")) { const QString t = readText();
                                                          if (!lang) c.variant = t; }
                else if (n == QLatin1String("mount")) c.mount = readText();
                else if (n == QLatin1String("cropfactor"))
                    c.cropFactor = toFloat(QStringView(readText()), 1.0f);
                else r.skipCurrentElement();
            }
            if (c.maker.isEmpty() || c.model.isEmpty()) continue;
            const QString key = norm(c.maker) + QLatin1Char('|') + norm(c.model)
                                + QLatin1Char('|') + norm(c.variant);
            auto it = cameraIndex.constFind(key);
            if (it != cameraIndex.constEnd()) cameras[it.value()] = c;
            else { cameraIndex.insert(key, cameras.size()); cameras.append(c); }
        }
        else if (name == QLatin1String("lens")) {
            Lens L;
            bool focalSet = false;
            while (r.readNextStartElement()) {
                const bool lang = r.attributes().hasAttribute(QLatin1String("lang"));
                const QStringView n = r.name();
                if (n == QLatin1String("maker")) {
                    const QString t = readText();
                    if (!lang) L.maker = t;
                }
                else if (n == QLatin1String("model")) {
                    const QString t = readText();
                    if (lang) L.altModels << t; else L.model = t;
                }
                else if (n == QLatin1String("mount")) L.mounts << readText();
                else if (n == QLatin1String("cropfactor"))
                    L.cropFactor = toFloat(QStringView(readText()), 1.0f);
                else if (n == QLatin1String("aspect-ratio"))
                    L.aspect = parseAspect(readText());
                else if (n == QLatin1String("type")) L.type = readText();
                else if (n == QLatin1String("center")) {
                    const QXmlStreamAttributes a = r.attributes();
                    L.centerX = toFloat(a.value(QLatin1String("x")), 0.0f);
                    L.centerY = toFloat(a.value(QLatin1String("y")), 0.0f);
                    r.skipCurrentElement();
                }
                else if (n == QLatin1String("focal")) {
                    const QXmlStreamAttributes a = r.attributes();
                    if (a.hasAttribute(QLatin1String("value"))) {
                        L.minFocal = L.maxFocal = toFloat(a.value(QLatin1String("value")), 0);
                    } else {
                        L.minFocal = toFloat(a.value(QLatin1String("min")), 0.0f);
                        L.maxFocal = toFloat(a.value(QLatin1String("max")), L.minFocal);
                    }
                    focalSet = L.minFocal > 0.0f;
                    r.skipCurrentElement();
                }
                else if (n == QLatin1String("calibration")) {
                    while (r.readNextStartElement()) {
                        const QXmlStreamAttributes a = r.attributes();
                        const QString model = a.value(QLatin1String("model")).toString();
                        const float focal = toFloat(a.value(QLatin1String("focal")), 0.0f);
                        auto at = [&a](const char *k, float def) {
                            return toFloat(a.value(QLatin1String(k)), def);
                        };
                        const QStringView cn = r.name();
                        if (cn == QLatin1String("distortion")) {
                            using DM = LensProfile::DistModel;
                            DM m = DM::None;
                            DistPoint p;
                            p.focal = focal;
                            if (model == QLatin1String("poly3")) {
                                m = DM::Poly3; p.t[0] = at("k1", 0);
                            } else if (model == QLatin1String("poly5")) {
                                m = DM::Poly5; p.t[0] = at("k1", 0); p.t[1] = at("k2", 0);
                            } else if (model == QLatin1String("ptlens")) {
                                m = DM::PTLens;
                                p.t[0] = at("a", 0); p.t[1] = at("b", 0); p.t[2] = at("c", 0);
                            }
                            /* One model per lens (lensfun uses the first it meets). */
                            if (m != DM::None && (L.distModel == DM::None || L.distModel == m)) {
                                L.distModel = m;
                                L.dist.append(p);
                            }
                        }
                        else if (cn == QLatin1String("tca")) {
                            using TM = LensProfile::TcaModel;
                            TM m = TM::None;
                            TcaPoint p;
                            p.focal = focal;
                            if (model == QLatin1String("linear")) {
                                m = TM::Linear; p.t[0] = at("kr", 1); p.t[1] = at("kb", 1);
                            } else if (model == QLatin1String("poly3")) {
                                m = TM::Poly3;
                                p.t[0] = at("vr", 1); p.t[1] = at("vb", 1);
                                p.t[2] = at("cr", 0); p.t[3] = at("cb", 0);
                                p.t[4] = at("br", 0); p.t[5] = at("bb", 0);
                            }
                            if (m != TM::None && (L.tcaModel == TM::None || L.tcaModel == m)) {
                                L.tcaModel = m;
                                L.tca.append(p);
                            }
                        }
                        else if (cn == QLatin1String("vignetting")) {
                            if (model == QLatin1String("pa")) {
                                VigPoint p;
                                p.focal = focal;
                                p.aperture = at("aperture", 0);
                                p.distance = at("distance", 1000);
                                p.k[0] = at("k1", 0); p.k[1] = at("k2", 0); p.k[2] = at("k3", 0);
                                if (p.aperture > 0.0f && p.distance > 0.0f) L.vig.append(p);
                            }
                        }
                        r.skipCurrentElement();
                    }
                }
                else r.skipCurrentElement();
            }
            if (L.model.isEmpty()) continue;
            if (!focalSet) {
                float lo = 0, hi = 0;
                if (focalFromName(L.model, lo, hi)) { L.minFocal = lo; L.maxFocal = hi; }
            }
            const QString id = L.id();
            auto it = lensIndex.constFind(id);
            if (it != lensIndex.constEnd()) lenses[it.value()] = L;
            else { lensIndex.insert(id, lenses.size()); lenses.append(L); }
        }
    }
}

void Database::finish()
{
    for (Lens &L : lenses) {
        std::sort(L.dist.begin(), L.dist.end(),
                  [](const DistPoint &a, const DistPoint &b) { return a.focal < b.focal; });
        std::sort(L.tca.begin(), L.tca.end(),
                  [](const TcaPoint &a, const TcaPoint &b) { return a.focal < b.focal; });
        /* No focal range from <focal> or the name: take the calibrated one. */
        if (L.minFocal <= 0.0f) {
            float lo = FLT_MAX, hi = 0.0f;
            for (const DistPoint &p : L.dist) { lo = std::min(lo, p.focal); hi = std::max(hi, p.focal); }
            for (const TcaPoint &p : L.tca)   { lo = std::min(lo, p.focal); hi = std::max(hi, p.focal); }
            for (const VigPoint &p : L.vig)   { lo = std::min(lo, p.focal); hi = std::max(hi, p.focal); }
            if (hi > 0.0f) { L.minFocal = lo; L.maxFocal = hi; }
        }
        if (L.maxFocal < L.minFocal) L.maxFocal = L.minFocal;
    }
}

/* ------------------------------------------------------------------------------------
   Matching
   ------------------------------------------------------------------------------------ */

const Camera *Database::findCamera(const QString &make, const QString &model) const
{
    if (model.isEmpty()) return nullptr;
    const QString nModel = norm(model);
    const QString nMake = norm(make);
    const Camera *best = nullptr;
    for (const Camera &c : cameras) {
        const QString cModel = norm(c.model);
        const QString cMaker1 = norm(firstWord(c.maker));
        /* Winnow's model is maker-prefixed ("Nikon D810"); lensfun's sometimes is
           ("Nikon D810") and sometimes is not ("ILCE-7RM3"). */
        const bool modelHit = cModel == nModel || cMaker1 + cModel == nModel;
        if (!modelHit) continue;
        const bool makerHit = nMake.isEmpty() || nMake.contains(cMaker1)
                              || cMaker1.contains(nMake) || nModel.startsWith(cMaker1);
        if (!makerHit) continue;
        /* Prefer the plain body over an aspect-ratio <variant> copy. */
        if (!best || (!best->variant.isEmpty() && c.variant.isEmpty())) best = &c;
    }
    return best;
}

bool Database::mountFits(const Lens &lens, const QString &cameraMount) const
{
    if (cameraMount.isEmpty()) return true;
    if (lens.mounts.contains(cameraMount)) return true;
    const QStringList compat = mountCompat.value(cameraMount);
    for (const QString &m : lens.mounts)
        if (compat.contains(m)) return true;
    return false;
}

const Lens *Database::findLens(const Shot &shot, const Camera *camera, double *score) const
{
    if (score) *score = 0.0;
    const float camCrop = camera ? camera->cropFactor : 0.0f;
    auto usable = [&](const Lens &L) {
        if (!L.hasCalibration()) return false;
        if (camera && !mountFits(L, camera->mount)) return false;
        if (camCrop > 0.0f && camCrop / L.cropFactor < 0.96f) return false;
        /* 2% plus half a millimetre: EXIF focal lengths reach here rounded to whole
           mm, which puts a compact's 4.3 mm wide end at 4. */
        if (shot.focal > 0.0f && L.minFocal > 0.0f &&
            (shot.focal < L.minFocal * 0.98f - 0.5f || shot.focal > L.maxFocal * 1.02f + 0.5f))
            return false;
        return true;
    };

    /* A FIXED-LENS camera (a compact) puts its own, undeclared mount on its lens, and
       often writes no useful lens name -- the lens is the one that carries that mount. */
    if (camera && !camera->mount.isEmpty() && !mountCompat.contains(camera->mount)) {
        const Lens *only = nullptr;
        int n = 0;
        for (const Lens &L : lenses)
            if (L.mounts.contains(camera->mount) && L.hasCalibration()) { only = &L; ++n; }
        if (n == 1) { if (score) *score = 1.0; return only; }
    }

    if (shot.lens.trimmed().isEmpty()) return nullptr;
    QStringList P = words(shot.lens);
    if (P.isEmpty()) return nullptr;

    auto isNumber = [](const QString &w) { return !w.isEmpty() && w.at(0).isDigit(); };

    const Lens *best = nullptr;
    double bestScore = 0.0;
    for (const Lens &L : lenses) {
        if (!usable(L)) continue;
        /* The maker's words count for neither side: EXIF lens names usually omit it. */
        const QStringList makerWords = words(L.maker);
        QStringList p = P;
        for (const QString &w : makerWords) p.removeAll(w);
        if (p.isEmpty()) continue;

        QStringList spellings = L.altModels;
        spellings.prepend(L.model);
        for (const QString &sp : spellings) {
            QStringList m = words(sp);
            for (const QString &w : makerWords) m.removeAll(w);
            if (m.isEmpty()) continue;
            /* Numbers carry the focal range and aperture: they must agree exactly, both
               ways, or a 24-70 f/4 would match a 24-70 f/2.8. */
            QStringList pn, mn;
            for (const QString &w : p) if (isNumber(w)) pn << w;
            for (const QString &w : m) if (isNumber(w)) mn << w;
            std::sort(pn.begin(), pn.end());
            std::sort(mn.begin(), mn.end());
            if (pn != mn) continue;
            /* Multiset intersection. */
            QStringList rest = m;
            int hit = 0;
            for (const QString &w : p) {
                const int ix = rest.indexOf(w);
                if (ix >= 0) { ++hit; rest.removeAt(ix); }
            }
            const double recall = double(hit) / p.size();
            const double dice = 2.0 * hit / double(p.size() + m.size());
            if (recall < 0.75 || dice < 0.6) continue;
            /* Ties go to the lens calibrated nearest the camera's sensor size. */
            double s = dice;
            if (camCrop > 0.0f) s -= 0.001 * std::abs(camCrop - L.cropFactor);
            if (s > bestScore) { bestScore = s; best = &L; }
        }
    }
    if (score) *score = bestScore;
    return best;
}

const Lens *Database::lensById(const QString &id) const
{
    auto it = lensIndex.constFind(id);
    return it == lensIndex.constEnd() ? nullptr : &lenses[it.value()];
}

QVector<const Lens *> Database::lensesFor(const Camera *camera) const
{
    QVector<const Lens *> out;
    for (const Lens &L : lenses) {
        if (!L.hasCalibration()) continue;
        if (camera && !mountFits(L, camera->mount)) continue;
        out << &L;
    }
    std::sort(out.begin(), out.end(), [](const Lens *a, const Lens *b) {
        return QString::compare(a->displayName(), b->displayName(), Qt::CaseInsensitive) < 0;
    });
    return out;
}

/* ------------------------------------------------------------------------------------
   Interpolation
   ------------------------------------------------------------------------------------ */

bool Database::interpolate(const Lens &L, float cameraCrop, float focal, float aperture,
                           float distance, LensProfile::Coeffs &out) const
{
    out = LensProfile::Coeffs();
    if (cameraCrop <= 0.0f) cameraCrop = L.cropFactor;
    if (cameraCrop / L.cropFactor < 0.96f) return false;
    out.lensCrop = L.cropFactor;
    out.lensAspect = L.aspect;
    out.centerX = L.centerX;
    out.centerY = L.centerY;
    out.cameraCrop = cameraCrop;

    if (focal > 0.0f) {
        if (!L.dist.isEmpty() && splineAt(L.dist, focal, out.d, [](int) { return true; }))
            out.dist = L.distModel;
        /* TCA: kr/kb (linear) and vr/vb (poly3) are near 1 and constant in f; the rest
           follow 1/f. */
        if (!L.tca.isEmpty() && splineAt(L.tca, focal, out.t, [](int i) { return i >= 2; }))
            out.tca = L.tcaModel;
    }

    if (focal > 0.0f && aperture > 0.0f && !L.vig.isEmpty()) {
        if (distance <= 0.0f) distance = 1000.0f;
        const float df = L.maxFocal - L.minFocal;
        auto vdist = [&](const VigPoint &p) {
            float f1 = focal - L.minFocal, f2 = p.focal - L.minFocal;
            if (df != 0.0f) { f1 /= df; f2 /= df; }
            const float a1 = 4.0f / aperture, a2 = 4.0f / p.aperture;
            const float d1 = 0.1f / distance, d2 = 0.1f / p.distance;
            return std::sqrt((f2 - f1) * (f2 - f1) + (a2 - a1) * (a2 - a1)
                             + (d2 - d1) * (d2 - d1));
        };
        double acc[3] = {0, 0, 0}, wsum = 0.0, nearest = DBL_MAX;
        bool exact = false;
        for (const VigPoint &p : L.vig) {
            const double dd = vdist(p);
            if (dd < 1e-4) {
                std::copy(p.k, p.k + 3, out.v);
                exact = true;
                break;
            }
            nearest = std::min(nearest, dd);
            const double w = 1.0 / std::pow(dd, 3.5);
            for (int i = 0; i < 3; ++i) acc[i] += w * p.k[i];
            wsum += w;
        }
        if (exact) out.vig = true;
        else if (nearest <= 1.0 && wsum > 0.0) {
            for (int i = 0; i < 3; ++i) out.v[i] = float(acc[i] / wsum);
            out.vig = true;
        }
    }
    return out.any();
}

/* ------------------------------------------------------------------------------------ */

static Resolved resolveUncached(const Shot &shot, const QString &overrideId)
{
    const Database &db = Database::instance();
    Resolved r;
    r.camera = db.findCamera(shot.cameraMake, shot.cameraModel);

    if (!overrideId.isEmpty()) {
        r.lens = db.lensById(overrideId);
        r.manual = true;
        if (!r.lens) { r.reason = QStringLiteral("profile not in the database"); return r; }
    }
    else {
        r.lens = db.findLens(shot, r.camera);
        if (!r.lens) {
            r.reason = shot.lens.trimmed().isEmpty() ? QStringLiteral("no lens in EXIF")
                                                     : QStringLiteral("no profile found");
            return r;
        }
    }

    const Lens &L = *r.lens;
    const float camCrop = r.camera ? r.camera->cropFactor : L.cropFactor;
    if (camCrop / L.cropFactor < 0.96f) {
        r.reason = QStringLiteral("profiled on a smaller sensor");
        return r;
    }
    float focal = shot.focal;
    if (focal <= 0.0f && L.minFocal > 0.0f && L.minFocal == L.maxFocal) focal = L.minFocal;
    /* Back inside the lens's range when only the rounding put it outside (see findLens). */
    if (focal > 0.0f && L.minFocal > 0.0f) {
        if (focal < L.minFocal && focal >= L.minFocal - 0.6f) focal = L.minFocal;
        if (focal > L.maxFocal && focal <= L.maxFocal + 0.6f) focal = L.maxFocal;
    }
    if (focal <= 0.0f) { r.reason = QStringLiteral("no focal length in EXIF"); return r; }

    if (!db.interpolate(L, camCrop, focal, shot.aperture, shot.distance, r.coeffs))
        r.reason = QStringLiteral("no calibration at %1 mm").arg(double(focal));
    return r;
}

/* Memoised: the render workers ask on every tick, and a lens search scans the whole
   database. The database never changes after load, so an answer never goes stale. */
Resolved resolve(const Shot &shot, const QString &overrideId)
{
    static QMutex mutex;
    static QHash<QString, Resolved> memo;
    const QString key = shot.cameraMake + QLatin1Char('\x1f') + shot.cameraModel
                        + QLatin1Char('\x1f') + shot.lens + QLatin1Char('\x1f')
                        + QString::number(double(shot.focal)) + QLatin1Char('\x1f')
                        + QString::number(double(shot.aperture)) + QLatin1Char('\x1f')
                        + QString::number(double(shot.distance)) + QLatin1Char('\x1f')
                        + overrideId;
    {
        QMutexLocker lk(&mutex);
        auto it = memo.constFind(key);
        if (it != memo.constEnd()) return it.value();
    }
    const Resolved r = resolveUncached(shot, overrideId);
    QMutexLocker lk(&mutex);
    if (memo.size() > 512) memo.clear();
    memo.insert(key, r);
    return r;
}

void preload()
{
    QThreadPool::globalInstance()->start([] { Database::instance(); });
}

}   // namespace LensDb
