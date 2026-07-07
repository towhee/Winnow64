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

    An EditStack is an ordered list of EditLayers; each layer carries a mask (reserved -- mask
    editing and the compositor land in a later increment, so today the mask is always empty =
    global) and an EditParams (the slider values). This is the on-disk shape the docs call the
    "EditStack / Layer & masking model": it stores layer = mask + settings so the format does not
    change when masks arrive.

    INCREMENT 1: the dock edits a single (global) layer; the renderer reads effectiveParams()
    which today returns the first enabled layer's params. The multi-layer compositor (opacity /
    mask blending over N layers) is deferred (roadmap item 2), but the format already supports it.

    Serialization is JSON, then base64 for the sidecar attribute (raw JSON quotes are awkward in
    an XML attribute value). It is VERSIONED and FORWARD-TOLERANT: unknown keys are ignored and
    missing keys fall back to identity, so older builds tolerate newer sidecars and vice versa.
    This header is intentionally free of any dependency on the render hot path (develop.cpp does
    NOT include it), so pulling in Qt's JSON types here costs nothing there.
*/

/*
    Masking model (UI increment -- compositor + canvas overlay deferred). A non-Base layer's
    effect area is the combination of an ordered list of MaskComponents. Each component is created
    with a tool (gradient/brush/range/AI-select), contributes (Add) or removes (Subtract) area,
    and may be inverted/feathered. The whole layer combines its Add components by MaskCombine
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
enum class MaskOp      { Add, Subtract };
enum class MaskCombine { Union, Intersect };

struct MaskComponent {
    int     tool     = int(MaskTool::LinearGradient);
    int     op       = int(MaskOp::Add);
    bool    enabled  = true;
    bool    inverted = false;
    float   feather  = 50.0f;
    QString paramsJson;                 // opaque per-tool geometry/sample blob (forward-tolerant)
};

struct EditLayer {
    QString    name    = "Base";        // index 0 is always the Base layer; extras are "Layer 1", ...
    QString    mask;                    // empty = global; reserved (legacy single-mask slot)
    float      opacity = 1.0f;          // reserved (compositor deferred)
    bool       enabled = true;
    int        combine = int(MaskCombine::Union);   // how this layer's masks combine
    QVector<MaskComponent> masks;       // empty (Base or unmasked) = global
    EditParams params;

    /* Preview (show/ignore) flags for the panel's per-section eye toggles. NON-DESTRUCTIVE: false
       bypasses that group at render (its fields fold to identity in effectiveLayerParams) while the
       stored values in `params` are kept, so re-showing restores them. showLayer bypasses the whole
       layer's params (Basic+Color+Effects together) and is distinct from `enabled` -- `enabled`
       drives the compositor's layer on/off, showLayer is the editing-side preview and leaves the
       compositor semantics untouched. All default true so older sidecars are unaffected. */
    bool showLayer   = true;
    bool showBasic   = true;
    bool showColor   = true;
    bool showEffects = true;
};

/* The params the renderer should apply for one layer: a COPY of the stored params with any
   previewed-off group folded back to identity. Never mutates the stored layer. */
inline EditParams effectiveLayerParams(const EditLayer &l) {
    EditParams p = l.params;
    if (!l.showLayer) {
        EditParams::resetGroup(p, EditParams::Group::Basic);
        EditParams::resetGroup(p, EditParams::Group::Color);
        EditParams::resetGroup(p, EditParams::Group::Effects);
        return p;
    }
    if (!l.showBasic)   EditParams::resetGroup(p, EditParams::Group::Basic);
    if (!l.showColor)   EditParams::resetGroup(p, EditParams::Group::Color);
    if (!l.showEffects) EditParams::resetGroup(p, EditParams::Group::Effects);
    return p;
}

/*
    Geometry (Transform tool): crop + straighten + 4-point perspective warp. PER-IMAGE (not per
    layer) and applied LAST in the render -- after the develop ops + EXIF orientation -- so it is
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

struct EditStack {
    int                 version = 1;
    QVector<EditLayer>  layers;
    Geometry            geometry;       // per-image crop / straighten / warp (applied last)

    /* The params the renderer should apply. Today: the first enabled layer's params (one layer
       in Increment 1). When the compositor lands this becomes a true multi-layer composite. */
    EditParams effectiveParams() const {
        for (const EditLayer &l : layers)
            if (l.enabled) return effectiveLayerParams(l);
        return EditParams();
    }

    /* True when nothing would change a pixel -- lets callers skip Develop entirely and avoids
       writing a sidecar for an untouched image. */
    bool isIdentity() const {
        if (!geometry.isIdentity() || !geometry.show) return false;
        for (const EditLayer &l : layers)
            if (l.enabled && (!l.params.isIdentity() || !l.masks.isEmpty() ||
                              !l.showLayer || !l.showBasic || !l.showColor || !l.showEffects))
                return false;
        return true;
    }

    /* The layer the dock edits (Increment 1: layer 0), created from identity if the stack is
       empty. */
    EditLayer &activeLayer() {
        if (layers.isEmpty()) layers.append(EditLayer());
        return layers.first();
    }

    /* ---- Serialization ------------------------------------------------------------------- */

    static QJsonObject paramsToJson(const EditParams &p) {
        QJsonObject o;
        o["temp"]            = p.temp;
        o["tint"]            = p.tint;
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
        o["luminance"]       = p.luminance;
        o["denoiseLuma"]     = p.denoiseLuma;
        o["denoiseChroma"]   = p.denoiseChroma;
        o["localDenoiseLuma"]= p.localDenoiseLuma;
        o["localDenoiseChroma"]= p.localDenoiseChroma;
        o["version"]         = p.version;
        return o;
    }

    /* Forward-tolerant: any field absent from o keeps its identity default. */
    static EditParams paramsFromJson(const QJsonObject &o) {
        EditParams p;
        p.temp            = static_cast<float>(o.value("temp").toDouble(p.temp));
        p.tint            = static_cast<float>(o.value("tint").toDouble(p.tint));
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
        p.luminance       = static_cast<float>(o.value("luminance").toDouble(p.luminance));
        p.denoiseLuma     = static_cast<float>(o.value("denoiseLuma").toDouble(p.denoiseLuma));
        p.denoiseChroma   = static_cast<float>(o.value("denoiseChroma").toDouble(p.denoiseChroma));
        p.localDenoiseLuma= static_cast<float>(o.value("localDenoiseLuma").toDouble(p.localDenoiseLuma));
        p.localDenoiseChroma= static_cast<float>(o.value("localDenoiseChroma").toDouble(p.localDenoiseChroma));
        p.version         = o.value("version").toInt(p.version);
        return p;
    }

    QJsonObject toJson() const {
        QJsonArray arr;
        for (const EditLayer &l : layers) {
            QJsonObject lo;
            lo["name"]    = l.name;
            lo["mask"]    = l.mask;
            lo["opacity"] = l.opacity;
            lo["enabled"] = l.enabled;
            lo["combine"] = l.combine;
            /* Preview flags: only emit the non-default (false = previewed off) ones, so a normal
               untouched layer serializes exactly as before (forward/backward tolerant). */
            if (!l.showLayer)   lo["showLayer"]   = false;
            if (!l.showBasic)   lo["showBasic"]   = false;
            if (!l.showColor)   lo["showColor"]   = false;
            if (!l.showEffects) lo["showEffects"] = false;
            QJsonArray marr;
            for (const MaskComponent &m : l.masks) {
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
        o["layers"]  = arr;
        if (!geometry.isIdentity() || !geometry.show) o["geometry"] = geometryToJson(geometry);
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
        const QJsonArray arr = o.value("layers").toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject lo = v.toObject();
            EditLayer l;
            l.name    = lo.value("name").toString(l.name);
            l.mask    = lo.value("mask").toString(l.mask);
            l.opacity = static_cast<float>(lo.value("opacity").toDouble(l.opacity));
            l.enabled = lo.value("enabled").toBool(l.enabled);
            l.combine = lo.value("combine").toInt(l.combine);
            l.showLayer   = lo.value("showLayer").toBool(l.showLayer);
            l.showBasic   = lo.value("showBasic").toBool(l.showBasic);
            l.showColor   = lo.value("showColor").toBool(l.showColor);
            l.showEffects = lo.value("showEffects").toBool(l.showEffects);
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
                l.masks.append(m);
            }
            l.params  = paramsFromJson(lo.value("params").toObject());
            s.layers.append(l);
        }
        return s;
    }

    /* Base64(compact JSON) -- the value stored in the winnow:Develop sidecar attribute. */
    QString toBase64() const {
        const QByteArray json = QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
        return QString::fromLatin1(json.toBase64());
    }

    static EditStack fromBase64(const QString &b64) {
        if (b64.isEmpty()) return EditStack();
        const QByteArray json = QByteArray::fromBase64(b64.toLatin1());
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) return EditStack();
        return fromJson(doc.object());
    }
};

#endif // EDITSTACK_H
