#ifndef EDITSTACK_H
#define EDITSTACK_H

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "Develop/editparams.h"

/*
    Per-image Develop edit state, persisted in the image's XMP sidecar (winnow:Develop).

    An EditStack is an ordered list of EditScopes; each scope carries a mask (reserved -- mask
    editing and the compositor land in a later increment, so today the mask is always empty =
    global) and an EditParams (the slider values). This is the on-disk shape the docs call the
    "EditStack / Scope & masking model": it stores scope = mask + settings so the format does not
    change when masks arrive.

    INCREMENT 1: the dock edits a single (global) scope; the renderer reads effectiveParams()
    which today returns the first enabled scope's params. The multi-scope compositor (opacity /
    mask blending over N scopes) is deferred (roadmap item 2), but the format already supports it.

    Serialization is JSON, then base64 for the sidecar attribute (raw JSON quotes are awkward in
    an XML attribute value). It is VERSIONED and FORWARD-TOLERANT: unknown keys are ignored and
    missing keys fall back to identity, so older builds tolerate newer sidecars and vice versa.
    This header is intentionally free of any dependency on the render hot path (develop.cpp does
    NOT include it), so pulling in Qt's JSON types here costs nothing there.
*/

/*
    Masking model (UI increment -- compositor + canvas overlay deferred). A mask's
    effect area is the combination of an ordered list of MaskComponents. Each component is created
    with a tool (gradient/brush/range/AI-select), contributes (Add) or removes (Subtract) area,
    and may be inverted/feathered. The whole scope combines its Add components by MaskCombine
    (Union = any, Intersect = overlap); Subtract components are removed from that result. Tool
    geometry (gradient line, brush strokes, sampled colours) lives in paramsJson, kept opaque here
    so the sidecar format does not change as tools gain parameters. tool/op/combine are stored as
    int for trivial, forward-tolerant JSON.
*/
enum class MaskTool {
    LinearGradient, RadialGradient, Brush,
    ColorRange, LuminanceRange, Subject, Sky, Background, Depth,
    Object                              // APPEND ONLY (tool is serialized as int; keep order stable)
};
enum class MaskOp      { Add, Subtract, Intersect };   // APPEND ONLY (serialized as int)
enum class MaskCombine { Union, Intersect };

struct MaskComponent {
    int     tool     = int(MaskTool::LinearGradient);
    int     op       = int(MaskOp::Add);
    bool    enabled  = true;
    bool    inverted = false;
    float   feather  = 50.0f;
    QString paramsJson;                 // opaque per-tool geometry/sample blob (forward-tolerant)
};

struct EditScope {
    QString    name    = "Global";      // index 0 = Global; masks are "Mask 1", "Mask 2", ...
    QString    mask;                    // empty = global; reserved (legacy single-mask slot)
    float      opacity = 1.0f;          // reserved (compositor deferred)
    bool       enabled = true;
    int        combine = int(MaskCombine::Union);   // how this scope's masks combine
    QVector<MaskComponent> components;       // empty (Global scope) = whole image
    EditParams params;

    /* Preview (show/ignore) flags for the panel's per-section eye toggles. NON-DESTRUCTIVE: false
       bypasses that group at render (its fields fold to identity in effectiveScopeParams) while the
       stored values in `params` are kept, so re-showing restores them. showScope bypasses the whole
       scope's params (Basic+Color+Effects together) and is distinct from `enabled` -- `enabled`
       drives the compositor's scope on/off, showScope is the editing-side preview and leaves the
       compositor semantics untouched. All default true so older sidecars are unaffected. */
    bool showScope    = true;
    bool showBasic    = true;
    bool showColor    = true;
    bool showColorMix = true;
    bool showEffects  = true;
};

/* The params the renderer should apply for one scope: a COPY of the stored params with any
   previewed-off group folded back to identity. Never mutates the stored scope. */
inline EditParams effectiveScopeParams(const EditScope &l) {
    EditParams p = l.params;
    if (!l.showScope) {
        EditParams::resetGroup(p, EditParams::Group::Basic);
        EditParams::resetGroup(p, EditParams::Group::Color);
        EditParams::resetGroup(p, EditParams::Group::ColorMix);
        EditParams::resetGroup(p, EditParams::Group::Effects);
        return p;
    }
    if (!l.showBasic)    EditParams::resetGroup(p, EditParams::Group::Basic);
    if (!l.showColor)    EditParams::resetGroup(p, EditParams::Group::Color);
    if (!l.showColorMix) EditParams::resetGroup(p, EditParams::Group::ColorMix);
    if (!l.showEffects)  EditParams::resetGroup(p, EditParams::Group::Effects);
    return p;
}

/*
    Geometry (Transform tool): crop + straighten + 4-point perspective warp. PER-IMAGE (not per
    scope) and applied LAST in the render -- after the develop ops + EXIF orientation -- so it is
    non-destructive and the develop edits stay calibrated to the full frame. Pipeline order on the
    output image is: straighten (rotate) -> warp (perspective) -> crop. Identity defaults => no-op.

    Coordinate spaces (all normalized 0..1): quad corners are in the developed/oriented image (the
    input to the geometry stage), order TL,TR,BR,BL; crop is in the POST-warp output image. Plain
    doubles so JSON round-trips trivially.
*/
struct Geometry {
    double cropX = 0.0, cropY = 0.0, cropW = 1.0, cropH = 1.0;   // crop rect, post-warp output space
    double straighten = 0.0;                                     // degrees (deferred UI)
    bool   hasWarp = false;                                      // 4-point perspective active
    double quad[8] = {0,0, 1,0, 1,1, 0,1};                       // TL,TR,BR,BL, oriented-image space
    bool   fillCanvas = false;                                   // content-aware fill (deferred)
    bool   show = true;                                          // Transform Preview: false bypasses
                                                                 // the geometry stage at render, but
                                                                 // keeps the stored crop/warp so the
                                                                 // overlay still draws (default true)

    bool cropIsIdentity() const {
        return cropX == 0.0 && cropY == 0.0 && cropW == 1.0 && cropH == 1.0;
    }
    bool isIdentity() const {
        return cropIsIdentity() && straighten == 0.0 && !hasWarp;
    }
};

/*
    Regenerative fill (spot removal). PER-IMAGE like Geometry: an ordered history of
    spot heals. Each spot is a brush region synthesized by MI-GAN and composited BEFORE
    the geometry stage, so spots stay glued to the photo content when it is later cropped
    or straightened. Surfaced in the dock as a single pinned "Fill" pseudo-scope (always
    last); it carries no adjustment params.

    Parametric + non-destructive: only the brush geometry is stored (points in oriented-
    image normalized coords -- the geometry stage's input space -- plus size/feather, all
    opaque in paramsJson, forward-tolerant like MaskComponent). Heals are re-synthesized
    deterministically at render (MI-GAN is deterministic) and cached in memory. Spots
    apply in order so each heals over the accumulation of prior ones. `enabled` toggles a
    history entry off without deleting it.
*/
struct FillSpot {
    bool    enabled = true;
    QString paramsJson;   // stroke blob: points, size, feather
};

struct EditStack {
    /* Sidecar format version this build WRITES. Bump only when the JSON shape changes in
       a way older builds cannot read; a stack stamped higher than this was written by a
       newer Winnow and is reported to the user on load (see fromBase64). */
    static constexpr int kVersion = 1;

    int                 version = 1;
    QVector<EditScope>  scopes;
    Geometry            geometry;       // crop/straighten/warp, applied last
    QVector<FillSpot>   spots;          // per-image spot heals (applied before geometry)

    /* The params the renderer should apply. Today: the first enabled scope's params (one scope
       in Increment 1). When the compositor lands this becomes a true multi-scope composite. */
    EditParams effectiveParams() const {
        for (const EditScope &l : scopes)
            if (l.enabled) return effectiveScopeParams(l);
        return EditParams();
    }

    /* True when nothing would change a pixel -- lets callers skip Develop entirely and avoids
       writing a sidecar for an untouched image. */
    bool isIdentity() const {
        if (!geometry.isIdentity() || !geometry.show) return false;
        for (const FillSpot &s : spots)
            if (s.enabled) return false;
        for (const EditScope &l : scopes)
            if (l.enabled && (!l.params.isIdentity() || !l.components.isEmpty() ||
                              !l.showScope || !l.showBasic || !l.showColor ||
                              !l.showColorMix || !l.showEffects))
                return false;
        return true;
    }

    /* The scope the dock edits (Increment 1: scope 0), created from identity if the stack is
       empty. */
    EditScope &activeScope() {
        if (scopes.isEmpty()) scopes.append(EditScope());
        return scopes.first();
    }

    /* ---- Serialization ------------------------------------------------------------------- */

    static QJsonObject paramsToJson(const EditParams &p) {
        QJsonObject o;
        o["temp"]            = p.temp;      // Kelvin; 0 = as shot
        o["tint"]            = p.tint;
        o["wbPreset"]        = p.wbPreset;
        o["exposure"]        = p.exposure;
        o["contrast"]        = p.contrast;
        o["highlights"]      = p.highlights;
        o["shadows"]         = p.shadows;
        o["whites"]          = p.whites;
        o["blacks"]          = p.blacks;
        o["toneShadowCenter"]    = p.toneShadowCenter;
        o["toneCrossover"]       = p.toneCrossover;
        o["toneHighlightCenter"] = p.toneHighlightCenter;
        o["texture"]         = p.texture;
        o["dehaze"]          = p.dehaze;
        o["red"]             = p.red;
        o["green"]           = p.green;
        o["blue"]            = p.blue;
        o["hue"]             = p.hue;
        o["saturation"]      = p.saturation;
        o["vibrance"]        = p.vibrance;
        o["luminance"]       = p.luminance;
        o["gradeShadowHue"]  = p.gradeShadowHue;
        o["gradeShadowSat"]  = p.gradeShadowSat;
        o["gradeShadowLum"]  = p.gradeShadowLum;
        o["gradeMidHue"]     = p.gradeMidHue;
        o["gradeMidSat"]     = p.gradeMidSat;
        o["gradeMidLum"]     = p.gradeMidLum;
        o["gradeHighHue"]    = p.gradeHighHue;
        o["gradeHighSat"]    = p.gradeHighSat;
        o["gradeHighLum"]    = p.gradeHighLum;
        o["denoiseLuma"]     = p.denoiseLuma;
        o["denoiseChroma"]   = p.denoiseChroma;
        o["localDenoiseLuma"]= p.localDenoiseLuma;
        o["localDenoiseChroma"]= p.localDenoiseChroma;
        o["vignetteExposure"]= p.vignetteExposure;
        o["vignetteFeather"] = p.vignetteFeather;
        o["grainAmount"]     = p.grainAmount;
        o["grainSize"]       = p.grainSize;
        o["grainRoughness"]  = p.grainRoughness;
        o["version"]         = p.version;
        return o;
    }

    /* Forward-tolerant: any field absent from o keeps its identity default. */
    static EditParams paramsFromJson(const QJsonObject &o) {
        EditParams p;
        p.temp            = static_cast<float>(o.value("temp").toDouble(p.temp));
        p.tint            = static_cast<float>(o.value("tint").toDouble(p.tint));
        p.wbPreset        = o.value("wbPreset").toInt(p.wbPreset);
        p.exposure        = static_cast<float>(o.value("exposure").toDouble(p.exposure));
        p.contrast        = static_cast<float>(o.value("contrast").toDouble(p.contrast));
        p.highlights      = static_cast<float>(o.value("highlights").toDouble(p.highlights));
        p.shadows         = static_cast<float>(o.value("shadows").toDouble(p.shadows));
        p.whites          = static_cast<float>(o.value("whites").toDouble(p.whites));
        p.blacks          = static_cast<float>(o.value("blacks").toDouble(p.blacks));
        p.toneShadowCenter    = static_cast<float>(o.value("toneShadowCenter").toDouble(p.toneShadowCenter));
        p.toneCrossover       = static_cast<float>(o.value("toneCrossover").toDouble(p.toneCrossover));
        p.toneHighlightCenter = static_cast<float>(o.value("toneHighlightCenter").toDouble(p.toneHighlightCenter));
        p.texture         = static_cast<float>(o.value("texture").toDouble(p.texture));
        p.dehaze          = static_cast<float>(o.value("dehaze").toDouble(p.dehaze));
        p.red             = static_cast<float>(o.value("red").toDouble(p.red));
        p.green           = static_cast<float>(o.value("green").toDouble(p.green));
        p.blue            = static_cast<float>(o.value("blue").toDouble(p.blue));
        p.hue             = static_cast<float>(o.value("hue").toDouble(p.hue));
        p.saturation      = static_cast<float>(o.value("saturation").toDouble(p.saturation));
        p.vibrance        = static_cast<float>(o.value("vibrance").toDouble(p.vibrance));
        p.luminance       = static_cast<float>(o.value("luminance").toDouble(p.luminance));
        p.gradeShadowHue  = static_cast<float>(o.value("gradeShadowHue").toDouble(p.gradeShadowHue));
        p.gradeShadowSat  = static_cast<float>(o.value("gradeShadowSat").toDouble(p.gradeShadowSat));
        p.gradeShadowLum  = static_cast<float>(o.value("gradeShadowLum").toDouble(p.gradeShadowLum));
        p.gradeMidHue     = static_cast<float>(o.value("gradeMidHue").toDouble(p.gradeMidHue));
        p.gradeMidSat     = static_cast<float>(o.value("gradeMidSat").toDouble(p.gradeMidSat));
        p.gradeMidLum     = static_cast<float>(o.value("gradeMidLum").toDouble(p.gradeMidLum));
        p.gradeHighHue    = static_cast<float>(o.value("gradeHighHue").toDouble(p.gradeHighHue));
        p.gradeHighSat    = static_cast<float>(o.value("gradeHighSat").toDouble(p.gradeHighSat));
        p.gradeHighLum    = static_cast<float>(o.value("gradeHighLum").toDouble(p.gradeHighLum));
        p.denoiseLuma     = static_cast<float>(o.value("denoiseLuma").toDouble(p.denoiseLuma));
        p.denoiseChroma   = static_cast<float>(o.value("denoiseChroma").toDouble(p.denoiseChroma));
        p.localDenoiseLuma= static_cast<float>(o.value("localDenoiseLuma").toDouble(p.localDenoiseLuma));
        p.localDenoiseChroma= static_cast<float>(o.value("localDenoiseChroma").toDouble(p.localDenoiseChroma));
        p.vignetteExposure= static_cast<float>(o.value("vignetteExposure").toDouble(p.vignetteExposure));
        p.vignetteFeather = static_cast<float>(o.value("vignetteFeather").toDouble(p.vignetteFeather));
        p.grainAmount     = static_cast<float>(o.value("grainAmount").toDouble(p.grainAmount));
        p.grainSize       = static_cast<float>(o.value("grainSize").toDouble(p.grainSize));
        p.grainRoughness  = static_cast<float>(o.value("grainRoughness").toDouble(p.grainRoughness));
        p.version         = o.value("version").toInt(p.version);
        return p;
    }

    QJsonObject toJson() const {
        QJsonArray arr;
        for (const EditScope &l : scopes) {
            QJsonObject lo;
            lo["name"]    = l.name;
            lo["mask"]    = l.mask;
            lo["opacity"] = l.opacity;
            lo["enabled"] = l.enabled;
            lo["combine"] = l.combine;
            /* Preview flags: only emit the non-default (false = previewed off) ones, so a normal
               untouched scope serializes exactly as before (forward/backward tolerant). */
            if (!l.showScope)    lo["showLayer"]    = false;
            if (!l.showBasic)    lo["showBasic"]    = false;
            if (!l.showColor)    lo["showColor"]    = false;
            if (!l.showColorMix) lo["showColorMix"] = false;
            if (!l.showEffects)  lo["showEffects"]  = false;
            QJsonArray marr;
            for (const MaskComponent &m : l.components) {
                QJsonObject mo;
                mo["tool"]     = m.tool;
                mo["op"]       = m.op;
                mo["enabled"]  = m.enabled;
                mo["inverted"] = m.inverted;
                mo["feather"]  = m.feather;
                if (!m.paramsJson.isEmpty()) mo["params"] = m.paramsJson;
                marr.append(mo);
            }
            lo["masks"]   = marr;
            lo["params"]  = paramsToJson(l.params);
            arr.append(lo);
        }
        QJsonObject o;
        o["version"] = version;
        o["scopes"]  = arr;      // wire key kept: sidecars written before the rename
        if (!geometry.isIdentity() || !geometry.show) o["geometry"] = geometryToJson(geometry);
        if (!spots.isEmpty()) {
            QJsonArray sarr;
            for (const FillSpot &s : spots) {
                QJsonObject so;
                so["enabled"] = s.enabled;
                if (!s.paramsJson.isEmpty()) so["params"] = s.paramsJson;
                sarr.append(so);
            }
            o["fill"] = sarr;
        }
        return o;
    }

    static QJsonObject geometryToJson(const Geometry &g) {
        QJsonObject o;
        o["cropX"] = g.cropX; o["cropY"] = g.cropY;
        o["cropW"] = g.cropW; o["cropH"] = g.cropH;
        o["straighten"] = g.straighten;
        o["hasWarp"] = g.hasWarp;
        o["fillCanvas"] = g.fillCanvas;
        if (!g.show) o["show"] = false;         // only when previewed off (default true)
        if (g.hasWarp) {
            QJsonArray q;
            for (double v : g.quad) q.append(v);
            o["quad"] = q;
        }
        return o;
    }

    static Geometry geometryFromJson(const QJsonObject &o) {
        Geometry g;
        g.cropX = o.value("cropX").toDouble(g.cropX);
        g.cropY = o.value("cropY").toDouble(g.cropY);
        g.cropW = o.value("cropW").toDouble(g.cropW);
        g.cropH = o.value("cropH").toDouble(g.cropH);
        g.straighten = o.value("straighten").toDouble(g.straighten);
        g.hasWarp = o.value("hasWarp").toBool(g.hasWarp);
        g.fillCanvas = o.value("fillCanvas").toBool(g.fillCanvas);
        g.show = o.value("show").toBool(g.show);
        const QJsonArray q = o.value("quad").toArray();
        if (q.size() == 8) for (int i = 0; i < 8; ++i) g.quad[i] = q[i].toDouble(g.quad[i]);
        return g;
    }

    static EditStack fromJson(const QJsonObject &o) {
        EditStack s;
        s.version = o.value("version").toInt(s.version);
        if (o.contains("geometry")) s.geometry = geometryFromJson(o.value("geometry").toObject());
        const QJsonArray sarr = o.value("fill").toArray();
        for (const QJsonValue &sv : sarr) {
            const QJsonObject so = sv.toObject();
            FillSpot fs;
            fs.enabled    = so.value("enabled").toBool(fs.enabled);
            fs.paramsJson = so.value("params").toString();
            s.spots.append(fs);
        }
        const QJsonArray arr = o.value("scopes").toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject lo = v.toObject();
            EditScope l;
            l.name    = lo.value("name").toString(l.name);
            l.mask    = lo.value("mask").toString(l.mask);
            l.opacity = static_cast<float>(lo.value("opacity").toDouble(l.opacity));
            l.enabled = lo.value("enabled").toBool(l.enabled);
            l.combine = lo.value("combine").toInt(l.combine);
            l.showScope    = lo.value("showLayer").toBool(l.showScope);
            l.showBasic    = lo.value("showBasic").toBool(l.showBasic);
            l.showColor    = lo.value("showColor").toBool(l.showColor);
            l.showColorMix = lo.value("showColorMix").toBool(l.showColorMix);
            l.showEffects  = lo.value("showEffects").toBool(l.showEffects);
            const QJsonArray marr = lo.value("masks").toArray();
            for (const QJsonValue &mv : marr) {
                const QJsonObject mo = mv.toObject();
                MaskComponent m;
                m.tool     = mo.value("tool").toInt(m.tool);
                m.op       = mo.value("op").toInt(m.op);
                m.enabled  = mo.value("enabled").toBool(m.enabled);
                m.inverted = mo.value("inverted").toBool(m.inverted);
                m.feather  = static_cast<float>(mo.value("feather").toDouble(m.feather));
                m.paramsJson = mo.value("params").toString();
                l.components.append(m);
            }
            l.params  = paramsFromJson(lo.value("params").toObject());
            s.scopes.append(l);
        }
        return s;
    }

    /* Base64(compact JSON) -- the value stored in the winnow:Develop sidecar
       attribute. */
    QString toBase64() const {
        const QByteArray json = QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
        return QString::fromLatin1(json.toBase64());
    }

    /* ----------------------------------------------------------------------------------
       Loading a POSSIBLY CORRUPT sidecar.

       A sidecar is a user file: it can be truncated by a crash, mangled by a sync client,
       hand-edited, or written by a future build. Forward tolerance (unknown keys ignored,
       absent keys defaulted) handles VERSION drift, but it cannot tell "this image has no
       edits" apart from "this image's edits could not be read" -- both used to come back
       as a default EditStack, silently. Callers need that distinction, because the second
       case means the user's work is missing from the panel and their next edit will
       overwrite whatever is still on disk.

       So the load reports a Status and an issue list, and every value that reaches the
       render pipeline is range-checked by sanitize(). See notes/Documentation.txt
       "Corrupted or changed develop settings".
       ------------------------------------------------------------------------------- */

    enum class Status {
        Ok,         // parsed cleanly (possibly with repairs -- see the issue list)
        Absent,     // no sidecar / empty attribute: a normal unedited image, NOT an error
        Corrupt     // present but unreadable: bad base64, bad JSON, or not a JSON object
    };

    /* Clamp helper: rejects NaN / infinity as well as out-of-range. JSON cannot encode a
       non-finite number, but a hand-edited or partially-overwritten file can still yield
       one via a huge exponent, and a single NaN reaching the pipeline turns the whole
       image black. Returns true when v had to be changed. */
    static bool clampF(float &v, float lo, float hi, float fallback) {
        if (!(v == v) || v < -3.0e38f || v > 3.0e38f) { v = fallback; return true; }
        if (v < lo) { v = lo; return true; }
        if (v > hi) { v = hi; return true; }
        return false;
    }

    static bool clampD(double &v, double lo, double hi, double fallback) {
        if (!(v == v) || v < -1.0e300 || v > 1.0e300) { v = fallback; return true; }
        if (v < lo) { v = lo; return true; }
        if (v > hi) { v = hi; return true; }
        return false;
    }

    /* Force one EditParams into its documented ranges (the same limits the panel sliders
       enforce, so a repaired value is always one the user could have dialled in). Appends
       a short description of anything it had to change. */
    static void sanitizeParams(EditParams &p, const QString &where, QStringList *issues) {
        const EditParams def;
        int fixed = 0;
        /* Temp is ABSOLUTE Kelvin with 0 as the "as shot" sentinel, so 0 is legal but
           anything between 0 and kMinKelvin is not (see Develop/whitebalance.h --
           duplicated as literals here to keep this header off the render hot path). */
        if (p.temp != 0.0f && clampF(p.temp, 2000.0f, 50000.0f, def.temp)) ++fixed;
        if (clampF(p.tint, -150.0f, 150.0f, def.tint)) ++fixed;
        if (p.wbPreset < 0 || p.wbPreset > 32) { p.wbPreset = def.wbPreset; ++fixed; }

        if (clampF(p.exposure,   -5.0f,   5.0f,   def.exposure))   ++fixed;
        if (clampF(p.contrast,   -100.0f, 100.0f, def.contrast))   ++fixed;
        if (clampF(p.highlights, -100.0f, 100.0f, def.highlights)) ++fixed;
        if (clampF(p.shadows,    -100.0f, 100.0f, def.shadows))    ++fixed;
        if (clampF(p.whites,     -100.0f, 100.0f, def.whites))     ++fixed;
        if (clampF(p.blacks,     -100.0f, 100.0f, def.blacks))     ++fixed;
        if (clampF(p.texture,    -100.0f, 100.0f, def.texture))    ++fixed;
        if (clampF(p.dehaze,     -100.0f, 100.0f, def.dehaze))     ++fixed;

        if (clampF(p.toneShadowCenter,    0.0f, 1.0f, def.toneShadowCenter))    ++fixed;
        if (clampF(p.toneCrossover,       0.0f, 1.0f, def.toneCrossover))       ++fixed;
        if (clampF(p.toneHighlightCenter, 0.0f, 1.0f, def.toneHighlightCenter)) ++fixed;
        /* The splits must stay ordered or the tone curve's region weights invert. */
        if (!(p.toneShadowCenter <= p.toneCrossover &&
              p.toneCrossover <= p.toneHighlightCenter)) {
            p.toneShadowCenter    = def.toneShadowCenter;
            p.toneCrossover       = def.toneCrossover;
            p.toneHighlightCenter = def.toneHighlightCenter;
            ++fixed;
        }

        if (clampF(p.red,        -100.0f, 100.0f, def.red))        ++fixed;
        if (clampF(p.green,      -100.0f, 100.0f, def.green))      ++fixed;
        if (clampF(p.blue,       -100.0f, 100.0f, def.blue))       ++fixed;
        if (clampF(p.hue,        -100.0f, 100.0f, def.hue))        ++fixed;
        if (clampF(p.saturation, -100.0f, 100.0f, def.saturation)) ++fixed;
        if (clampF(p.vibrance,   -100.0f, 100.0f, def.vibrance))   ++fixed;
        if (clampF(p.luminance,  -100.0f, 100.0f, def.luminance))  ++fixed;

        /* Colour grading: hue degrees wrap, sat 0..1, lum -100..100. */
        float *hues[] = {&p.gradeShadowHue, &p.gradeMidHue, &p.gradeHighHue};
        float *sats[] = {&p.gradeShadowSat, &p.gradeMidSat, &p.gradeHighSat};
        float *lums[] = {&p.gradeShadowLum, &p.gradeMidLum, &p.gradeHighLum};
        for (int i = 0; i < 3; ++i) {
            if (clampF(*hues[i], 0.0f, 360.0f, 0.0f)) ++fixed;
            if (clampF(*sats[i], 0.0f, 1.0f,   0.0f)) ++fixed;
            if (clampF(*lums[i], -100.0f, 100.0f, 0.0f)) ++fixed;
        }

        if (clampF(p.denoiseLuma,   0.0f, 1.0f, def.denoiseLuma))   ++fixed;
        if (clampF(p.denoiseChroma, 0.0f, 1.0f, def.denoiseChroma)) ++fixed;
        if (clampF(p.localDenoiseLuma,   0.0f, 1.0f, 0.0f)) ++fixed;
        if (clampF(p.localDenoiseChroma, 0.0f, 1.0f, 0.0f)) ++fixed;

        if (clampF(p.vignetteExposure, -5.0f, 5.0f, 0.0f)) ++fixed;
        if (clampF(p.vignetteFeather,  0.0f, 1.0f, def.vignetteFeather)) ++fixed;
        if (clampF(p.grainAmount,    0.0f, 1.0f, 0.0f)) ++fixed;
        if (clampF(p.grainSize,      0.0f, 1.0f, def.grainSize)) ++fixed;
        if (clampF(p.grainRoughness, 0.0f, 1.0f, def.grainRoughness)) ++fixed;

        if (fixed && issues)
            issues->append(QStringLiteral("%1: %2 adjustment%3 out of range")
                           .arg(where).arg(fixed).arg(fixed == 1 ? "" : "s"));
    }

    /* Range-check a whole stack in place. Anything that cannot be repaired into a legal
       value is DROPPED rather than rendered: an unknown mask tool has no geometry we can
       interpret, so keeping it would paint an arbitrary area. */
    static void sanitize(EditStack &s, QStringList *issues = nullptr) {
        /* Geometry. A zero / negative / non-finite crop divides by zero downstream. */
        Geometry &g = s.geometry;
        bool geomFixed = false;
        geomFixed |= clampD(g.cropX, 0.0, 1.0, 0.0);
        geomFixed |= clampD(g.cropY, 0.0, 1.0, 0.0);
        geomFixed |= clampD(g.cropW, 0.0001, 1.0, 1.0);
        geomFixed |= clampD(g.cropH, 0.0001, 1.0, 1.0);
        geomFixed |= clampD(g.straighten, -45.0, 45.0, 0.0);
        if (g.cropX + g.cropW > 1.0) { g.cropX = 0.0; g.cropW = 1.0; geomFixed = true; }
        if (g.cropY + g.cropH > 1.0) { g.cropY = 0.0; g.cropH = 1.0; geomFixed = true; }
        if (g.hasWarp) {
            /* geometryFromJson only fills quad when the array is exactly 8 long, so a
               truncated quad would otherwise warp through the DEFAULT corners. */
            bool quadBad = false;
            for (double &q : g.quad) quadBad |= clampD(q, -4.0, 5.0, 0.0);
            if (quadBad) { g.hasWarp = false; geomFixed = true; }
        }
        if (geomFixed && issues)
            issues->append(QStringLiteral("Crop / perspective was out of range"));

        /* Scopes. */
        int droppedMasks = 0;
        for (EditScope &l : s.scopes) {
            if (l.combine < 0 || l.combine > int(MaskCombine::Intersect))
                l.combine = int(MaskCombine::Union);
            clampF(l.opacity, 0.0f, 1.0f, 1.0f);
            for (int i = l.components.size() - 1; i >= 0; --i) {
                MaskComponent &m = l.components[i];
                /* tool / op are serialized as int and both enums are APPEND ONLY, so a
                   value past the end is either corruption or a tool from a newer build.
                   Either way this build cannot render it -- drop it. */
                if (m.tool < 0 || m.tool > int(MaskTool::Object) ||
                    m.op   < 0 || m.op   > int(MaskOp::Intersect)) {
                    l.components.removeAt(i);
                    ++droppedMasks;
                    continue;
                }
                clampF(m.feather, 0.0f, 100.0f, 50.0f);
            }
            sanitizeParams(l.params, l.name.isEmpty() ? QStringLiteral("Scope") : l.name,
                           issues);
        }
        if (droppedMasks && issues)
            issues->append(QStringLiteral("%1 mask%2 used an unknown tool and %3 skipped")
                           .arg(droppedMasks).arg(droppedMasks == 1 ? "" : "s")
                           .arg(droppedMasks == 1 ? "was" : "were"));

        /* Scope 0 is Global by contract -- several call sites index it directly. */
        if (s.scopes.isEmpty()) s.scopes.append(EditScope());
        s.scopes[0].name = QStringLiteral("Global");
    }

    /* Parse + range-check. status/issues may be null when the caller does not care (the
       one-argument form preserves the original behaviour for existing call sites). */
    static EditStack fromBase64(const QString &b64, Status *status = nullptr,
                                QStringList *issues = nullptr) {
        auto report = [status](Status st) { if (status) *status = st; };
        if (b64.isEmpty()) { report(Status::Absent); return EditStack(); }
        /* Qt's tolerant base64 decode silently skips invalid characters, so garbage in
           gives a short QByteArray rather than an error -- the JSON parse below is what
           actually catches it. */
        const QByteArray json = QByteArray::fromBase64(b64.toLatin1());
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            report(Status::Corrupt);
            if (issues)
                issues->append(QStringLiteral("Develop settings could not be read (%1)")
                               .arg(err.error == QJsonParseError::NoError
                                    ? QStringLiteral("not a settings record")
                                    : err.errorString()));
            return EditStack();
        }
        EditStack s = fromJson(doc.object());
        if (s.version > kVersion && issues)
            issues->append(QStringLiteral("Written by a newer version of Winnow "
                                          "(format %1, this build reads %2) -- unknown "
                                          "settings were ignored")
                           .arg(s.version).arg(kVersion));
        sanitize(s, issues);
        report(Status::Ok);
        return s;
    }
};

#endif // EDITSTACK_H
