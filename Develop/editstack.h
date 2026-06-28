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

struct EditLayer {
    QString    name    = "Layer 1";
    QString    mask;                    // empty = global; reserved (mask editing deferred)
    float      opacity = 1.0f;          // reserved (compositor deferred)
    bool       enabled = true;
    EditParams params;
};

struct EditStack {
    int                 version = 1;
    QVector<EditLayer>  layers;

    /* The params the renderer should apply. Today: the first enabled layer's params (one layer
       in Increment 1). When the compositor lands this becomes a true multi-layer composite. */
    EditParams effectiveParams() const {
        for (const EditLayer &l : layers)
            if (l.enabled) return l.params;
        return EditParams();
    }

    /* True when nothing would change a pixel -- lets callers skip Develop entirely and avoids
       writing a sidecar for an untouched image. */
    bool isIdentity() const {
        for (const EditLayer &l : layers)
            if (l.enabled && !l.params.isIdentity()) return false;
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
        o["denoiseLuma"]     = p.denoiseLuma;
        o["denoiseChroma"]   = p.denoiseChroma;
        o["localDenoiseLuma"]= p.localDenoiseLuma;
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
        p.denoiseLuma     = static_cast<float>(o.value("denoiseLuma").toDouble(p.denoiseLuma));
        p.denoiseChroma   = static_cast<float>(o.value("denoiseChroma").toDouble(p.denoiseChroma));
        p.localDenoiseLuma= static_cast<float>(o.value("localDenoiseLuma").toDouble(p.localDenoiseLuma));
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
            lo["params"]  = paramsToJson(l.params);
            arr.append(lo);
        }
        QJsonObject o;
        o["version"] = version;
        o["layers"]  = arr;
        return o;
    }

    static EditStack fromJson(const QJsonObject &o) {
        EditStack s;
        s.version = o.value("version").toInt(s.version);
        const QJsonArray arr = o.value("layers").toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject lo = v.toObject();
            EditLayer l;
            l.name    = lo.value("name").toString(l.name);
            l.mask    = lo.value("mask").toString(l.mask);
            l.opacity = static_cast<float>(lo.value("opacity").toDouble(l.opacity));
            l.enabled = lo.value("enabled").toBool(l.enabled);
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
