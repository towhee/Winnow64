#ifndef DEVELOPPRESETS_H
#define DEVELOPPRESETS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariantHash>
#include <QVector>
#include "Develop/editstack.h"

/*
    Develop presets (Lightroom's Presets panel) -- the persistent store.

    A preset is a NAMED, PARTIAL develop recipe: the subset of settings the user ticked
    in the New Preset dialog, captured from ONE scope of one image. It is deliberately
    partial, and that is the whole design: applying a preset MERGES it, writing only the
    keys it actually holds and leaving every other setting on the target image alone (a
    Temp-only preset moves Temp and nothing else). An absent key means "don't touch", so
    read() only populates what QSettings really contains.

    Values are stored RAW -- the EditParams field values under their EditStack JSON key
    names (denoise 0..1, not the slider's 0..100) -- so applying is a straight assignment
    back into an EditParams, mirroring EditStack::paramsFromJson.

    QSettings layout:

        Develop Presets/<name>/version      = schema version of the keys below
        Develop Presets/<name>/sourceScope  = "Global" | "Sky" | ...   (display only)
        Develop Presets/<name>/Global/<key> = the per-IMAGE settings
        Develop Presets/<name>/Edits/<key>  = the captured scope's EditParams values
        Develop Presets/<name>/Global/spots = array of FillSpot paramsJson

    VERSION / MIGRATION. A preset lives in QSettings indefinitely and is applied by a
    straight assignment of its stored value into an EditParams field, so a preset key is
    effectively a PUBLISHED FORMAT. Adding keys is always safe (an absent key is a no-op:
    read() reports only what QSettings holds), but renaming one silently drops that
    adjustment from every existing preset, and RESCALING or redefining one silently
    corrupts them -- the old number is written into the new field unchanged. So: add
    freely, never rename or rescale in place. When that is genuinely unavoidable, bump
    kVersion and convert in migrate(), which runs inside read() before any value reaches
    assignParam. Presets written before the version key existed read back as kVersion1
    (see read()) -- they are v1 by definition, since v1 is the schema they were written
    against. See notes/Documentation.txt "Stale presets and history (schema drift)".

    "Global" holds the items that belong to no scope: the raw decode engine, the raw NR
    pair and the tone splits (all scope 0 by definition), plus the per-image geometry
    (crop / straighten / warp) and the spot heals. "Edits" holds the adjustments, which
    may have come from any scope -- sourceScope records which, for display only. The
    scope that RECEIVES them is chosen at apply time (the active scope), not here.

    Mask geometry is never stored: it is specific to the source image's content, so a
    mask-scope preset carries its adjustments but not its mask.
*/

struct DevelopPreset {
    QString           name;
    int               version = 1;   // schema of the stored keys; see kVersion
    QString           sourceScope;   // scope it was captured from (display only)
    QVariantHash      globals;       // demosaicEngine, raw NR, tone splits, geometry
    QVariantHash      params;        // EditParams JSON key -> raw value
    QVector<FillSpot> spots;
};

class DevelopPresets : public QObject
{
    Q_OBJECT
public:
    explicit DevelopPresets(QSettings *setting, QObject *parent = nullptr);

    QStringList   names() const;                  // sorted, case-insensitive
    bool          contains(const QString &name) const;
    DevelopPreset read(const QString &name) const;
    void          write(const DevelopPreset &p);  // overwrites any preset of that name
    bool          rename(const QString &from, const QString &to);
    void          remove(const QString &name);

    /* Assign one RAW preset value onto an EditParams, keyed by its EditStack JSON field
       name. Unknown keys are ignored, so a preset written by a later build loads here
       without harm (the same forward tolerance as EditStack::paramsFromJson). */
    static void assignParam(const QString &key, const QVariant &v, EditParams &p);

    /* Current preset schema version, written by write() and checked by read(). Bump ONLY
       when an existing key changes meaning, scale or name -- never for a new key, which
       old presets simply lack. Every bump needs a matching step in migrate(). */
    static constexpr int kVersion = 1;

signals:
    void changed();                               // any write / rename / remove

private:
    static constexpr const char *kRoot = "Develop Presets";
    /* The adjustments group. kEditsLegacy is what the first (save-only) build wrote,
       when a preset could only come from the Global scope; read() still accepts it. */
    static constexpr const char *kEdits       = "Edits";
    static constexpr const char *kEditsLegacy = "Global edits";

    /* Bring a preset read from disk up to kVersion, in place, then stamp p.version.
       No-op while kVersion is 1: nothing has changed meaning yet. Each future bump adds
       one `if (p.version < N) { ...convert p.params / p.globals...; }` step, in order,
       so a v1 preset migrates through every intervening version. */
    static void migrate(DevelopPreset &p);

    QSettings *setting;
};

#endif // DEVELOPPRESETS_H
