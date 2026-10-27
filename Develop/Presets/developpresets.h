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
        Develop Presets/<name>/Global/autoWhiteBalance, autoTone = true when ticked

    CATEGORIES. The panel groups presets under user-named header categories, in a
    user-chosen order (drag to reorder). That organisation lives under its OWN root,
    so the preset data above -- a published format -- is untouched by it:

        Develop Preset Layout/categories    = array of { name, presets, collapsed }

    "Winnow" (kWinnowCategory) is PERMANENT: layout() always contains it, it cannot be
    renamed or deleted, and it is where every preset not placed anywhere else lands --
    presets saved before categories existed, a deleted category's presets, a preset
    written without a category. layout() reconciles the stored list against the
    presets that really exist on every call, so the two can never disagree for long.

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
    QString           sourceImage;   // file name it was captured from (display only)
    QVariantHash      globals;       // demosaicEngine, raw NR, tone splits, geometry
    QVariantHash      params;        // EditParams JSON key -> raw value
    QVector<FillSpot> spots;

    /* Nothing was ticked. Applying it is a no-op, so the callers use this to say
       "nothing copied yet" rather than silently pasting nothing. */
    bool isEmpty() const { return globals.isEmpty() && params.isEmpty() && spots.isEmpty(); }

    /* AUTO SETTINGS (added 2026-10-27): two Global keys that are COMMANDS rather than
       values -- applying the preset runs Auto white balance / Auto tone on each image it
       lands on (AutoTone::ApplyPresetAutos). Stored as bool true; absent = off, so every
       older preset reads as having neither. */
    static constexpr const char *kAutoWhiteBalance = "autoWhiteBalance";
    static constexpr const char *kAutoTone         = "autoTone";
    bool autoWhiteBalance() const { return globals.value(kAutoWhiteBalance).toBool(); }
    bool autoTone() const         { return globals.value(kAutoTone).toBool(); }
    bool hasAutos() const         { return autoWhiteBalance() || autoTone(); }
    static bool isAutoKey(const QString &k)
        { return k == QLatin1String(kAutoWhiteBalance) || k == QLatin1String(kAutoTone); }
};

/* One header category of the Presets panel, in display order. */
struct PresetCategory {
    QString     name;
    QStringList presets;            // preset names, in display order
    bool        collapsed = false;  // header folded in the panel
};

class DevelopPresets : public QObject
{
    Q_OBJECT
public:
    explicit DevelopPresets(QSettings *setting, QObject *parent = nullptr);

    QStringList   names() const;                  // sorted, case-insensitive
    bool          contains(const QString &name) const;
    DevelopPreset read(const QString &name) const;
    /* Overwrites any preset of that name. category empty = leave an existing preset
       where it is (a new one goes to the end of Winnow); otherwise the preset moves to
       the end of that category, which is created if it does not exist yet. */
    void          write(const DevelopPreset &p, const QString &category = QString());
    bool          rename(const QString &from, const QString &to);
    void          remove(const QString &name);

    /* ---- Categories (see the class comment) ---------------------------------------
       All of these keep the preset data alone; only the layout is rewritten. Every
       mutation except setCategoryCollapsed emits changed() -- folding a header is the
       view's own business, so the list does not rebuild for it. */
    static constexpr const char *kWinnowCategory = "Winnow";
    QVector<PresetCategory> layout() const;         // reconciled, display order
    QStringList categoryNames() const;              // display order
    QString     categoryOf(const QString &preset) const;
    bool        hasCategory(const QString &name) const;     // case-insensitive
    bool        addCategory(const QString &name);           // false: empty or exists
    bool        renameCategory(const QString &from, const QString &to);  // not Winnow
    void        removeCategory(const QString &name);        // presets move to Winnow
    /* toIndex is the insertion point counted BEFORE the item is taken out, i.e. the
       gap the user dropped on; the store corrects for the removal. */
    void        moveCategory(const QString &name, int toIndex);
    void        movePreset(const QString &preset, const QString &category, int toIndex);
    void        setCategoryCollapsed(const QString &name, bool collapsed);

    /* ---- The develop CLIPBOARD (Copy / Paste Develop Settings) --------------------
       Copy captures exactly what a preset captures -- the same checklist, the same
       partial recipe -- it just has no name, so it goes to a single slot that every Copy
       overwrites, and Paste merges it with the same mergePreset(). It lives under its own
       QSettings root, so it inherits the version / migrate path and can never turn up in
       names().

       SESSION SCOPED. QSettings is the container, not a promise of persistence: the
       constructor calls clearClipboard(), so the buffer starts every run empty and Paste
       is unavailable until this session's first Copy. (Lightroom's buffer does survive a
       restart; Winnow's deliberately does not -- pasting adjustments copied in an
       earlier session, from an image long out of view, is far more often a slip than
       an intent.) */
    DevelopPreset readClipboard() const;
    void          writeClipboard(const DevelopPreset &p);
    bool          hasClipboard() const;
    void          clearClipboard();     // empty the slot; called on construction

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
    /* The category layout's own root (see the class comment). */
    static constexpr const char *kLayoutRoot = "Develop Preset Layout";
    /* The clipboard's own root and its single fixed slot name. Separate from kRoot so the
       buffer is invisible to names() / contains() and cannot collide with a preset the
       user happens to name "clipboard". */
    static constexpr const char *kClipRoot = "Develop Clipboard";
    static constexpr const char *kClipName = "buffer";
    /* The adjustments group. kEditsLegacy is what the first (save-only) build wrote,
       when a preset could only come from the Global scope; read() still accepts it. */
    static constexpr const char *kEdits       = "Edits";
    static constexpr const char *kEditsLegacy = "Global edits";

    /* Bring a preset read from disk up to kVersion, in place, then stamp p.version.
       No-op while kVersion is 1: nothing has changed meaning yet. Each future bump adds
       one `if (p.version < N) { ...convert p.params / p.globals...; }` step, in order,
       so a v1 preset migrates through every intervening version. */
    static void migrate(DevelopPreset &p);

    /* The store, parameterised by root: presets and the clipboard are the same object in
       the same layout, differing only in where they live and who may enumerate them. */
    DevelopPreset readGroup(const char *root, const QString &name) const;
    void          writeGroup(const char *root, const DevelopPreset &p);

    /* Persist a layout as given (no reconcile, no changed()). */
    void          saveLayout(const QVector<PresetCategory> &cats);
    static int    indexOfCategory(const QVector<PresetCategory> &cats, const QString &name);

    QSettings *setting;
};

#endif // DEVELOPPRESETS_H
