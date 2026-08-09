#ifndef DEVELOPPROPERTIES_H
#define DEVELOPPROPERTIES_H

#include <QtWidgets>
#include "PropertyEditor/propertyeditor.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"
#include "Develop/History/develophistory.h"
#include "Develop/Presets/developpresets.h"
#include "Dialogs/savedeveloppresetdlg.h"    // PresetGroup: the checklist model
#include "Develop/workingimage.h"
#include "Develop/Properties/colorgradewheel.h"
#include "Develop/Properties/colorrangewheel.h"

class MW;
class ToneRegionSlider;
class ScopeHeaderBase;
class RawPanel;
class MaskPanel;
class HistoryView;
class PresetsView;
class QVariantAnimation;

/*
    Develop dock property tree (Lightroom-style parametric edits). It mirrors the
    EmbelProperties pattern: a PropertyEditor subclass that builds a tree of section
    headers, sliders, checkboxes and a combo. All values persist to QSettings under
    the "Develop/" branch.

    Scopes (header with minus/plus, the same idiom as Embellish templates) let several
    independent adjustment sets be stored. Switching the "Select scope" combo rebuilds
    the Basic / Effects sections from that scope's saved values. Each scope maps onto a
    single EditParams -- the one source of truth read by the Develop processor (and, for
    RAW, the white-balance / denoise steps inside RawFormat).

    Binding the UI to the decode pipeline (re-decode on change) is deferred; for now a
    change persists to settings and emits paramsChanged() so the hook is ready.
*/
class DevelopProperties : public PropertyEditor
{
    Q_OBJECT
public:
    DevelopProperties(QWidget *parent, QSettings *setting);

    QStringList scopeList;      // this image's scopes: "Global", then each mask

    /* The params the renderer should apply for the current image (the active scope's params).
       Identity when no image is current. */
    EditParams editParams();
    QString diagnostics();

    /* The full scope stack the renderer composites, independent of which scope is active for
       editing: Global (scope 0) params applied globally, then each enabled mask developed
       from the running accumulator (so overlapping masks COMPOUND) and blended over it by its mask
       (empty mask => global). Captured as plain values so the off-thread full-res render can use it. */
    struct StackRenderJob {
        struct Scope {
            EditParams             params;
            QVector<MaskComponent> components;   // empty => applies to the whole image
            int                    combine = 0;  // MaskCombine across the components
        };
        EditParams     global;                  // scope 0 params (whole image)
        QVector<Scope> scopes;                  // masks, in order, enabled only
        Geometry       geometry;       // crop/straighten/warp, applied last
        QVector<FillSpot> spots;       // spot heals, applied before geometry
    };
    StackRenderJob stackJob();

    /* stackJob for ANY image, for the non-interactive EXPORT path. It differs from
       stackJob() in exactly the ways a render-to-file must differ from a preview:

         o It reads the image's OWN stored stack (stackFor loads the sidecar on first
           touch), so a batch export gives every selected image its own recipe.
         o The History hover override (previewStack) is ignored -- a hovered row must
           never leak into an exported file.
         o The Transform / Replace preview eyes (geometry.show, spotsShown) are ignored.
           Those are VIEW toggles: an export always renders the stored recipe in full.

       Not const: stackFor() caches the sidecar it just read. It does not otherwise touch
       the edit state -- nothing here marks an image dirty or records history. */
    StackRenderJob stackJobFor(const QString &fPath);

    /* ---- TEST HOOK, no production caller ----
       Build a scope carrying `submasks` committed brush submasks plus one pending, on the
       current image. The UI route to this is newScope() -> showMaskMenu() -> beginMaskTool,
       and both of those are MODAL (a name dialog and a popup menu), so a headless driver
       cannot use them. MW::runDevelopStressTest calls this to exercise the Develop render
       path under ThreadSanitizer (tests/tsan/run_tsan_develop.sh) -- the proxy render runs
       on a worker and nothing else in the suite goes near it. Returns false if there is no
       current image. */
    bool selfTestAddMaskScope(int submasks);

    /* Whole-mask overlay: true when a mask tool is expanded on a mask (so MW should
       show the composited mask), plus the active scope's ordered mask tools to composite. */
    bool maskOverlayActive() const;
    /* Esc from the Develop arbiter: if a mask tool is expanded, collapse it (hide its
       settings, like clicking its caption again) and return true; else false. */
    bool escapeMaskTool();
    QVector<MaskComponent> activeScopeComponents() const;
    /* Index into activeScopeComponents() of the tool whose settings are expanded (the one
       the user clicked in the scope panel), or -1. MW tints this component in its own
       colour so the user can see the selected tool's share of the mask. */
    int  activeMaskIndex() const;
    /* Display name of a MaskTool (public so MW can label the on-canvas op chip). */
    static QString maskToolName(int tool);

    /* The current image's stored geometry (for loading the crop overlay), and a setter the crop
       tool calls on commit (writes it into the image's EditStack + marks the sidecar dirty). */
    Geometry currentGeometry() const;
    void     setCurrentGeometry(const Geometry &g);

    /* Per-image edit state (Increment 1). The dock now reflects the CURRENT IMAGE's EditStack
       (loaded from / saved to its XMP sidecar) instead of app-global QSettings. */
    void setCurrentImage(const QString &fPath);   // flush previous, load+show this image's stack
    bool currentIsIdentity() const;               // true if the current image has no edits

    /* Bind the histogram's tone-region slider (created with the scopes, owned by MW): connect its
       drags to the active scope's tone-split params and keep a pointer so image switches push the
       saved positions back into it. */
    void bindToneSlider(ToneRegionSlider *slider);
    void flushImage(const QString &fPath);  // write one image's dirty stack to sidecar
    void flushAll();                        // write every dirty stack (quit/pre-op)

    /* ---- Multi-image editing (edits apply to the whole selection) -------------------
       When more than one image is selected, a Global adjustment made on the current
       image is copied to every other selected image (Lightroom's Auto Sync). See the
       "Multi-image editing" block in the private section for the rules.

       flushPropagation applies whatever is queued RIGHT NOW instead of waiting for the
       debounce. MW calls it whenever the selection changes (the queued batch belongs to
       the selection that was live when editing started, so it must land first) and
       before any operation that reads the sidecars.

       selectedEditCount is what the Develop dock's red banner reports: how many images
       the next edit will touch (1 = just the current image). */
    void flushPropagation();
    int  selectedEditCount() const;

    /* The scope dropdown (scopes + scope actions) and the preview eye live in a gradient header
       widget ABOVE this tree (see ScopeHeaderBase). Bind it once; this class drives its
       combo/eye and handles its signals. The concrete widget (ScopeHeader or the
       experimental ScopeHeaderLab) is chosen in MW init behind G::useScopeHeaderLab. */
    void bindScopeHeader(ScopeHeaderBase *header);

    /* The raw-decode controls (Edit source / Demosaic / Denoise) live in a RawPanel above
       the Scopes list (lab UI), lifted out of the Global "Core" tree rows. Bind it once;
       this class handles its signals (reusing the existing raw signal contract) and
       pushes state back via syncRawPanel(). Constructed only when G::useScopeHeaderLab. */
    void bindRawPanel(RawPanel *panel);

    /* The transient mask build-up strip (lab UI) above the Scopes list. Bind it once;
       this class drives it and owns the mask model. */
    void bindMaskPanel(MaskPanel *panel);

    /* The History dock's list (owned by MW, lives in its own dock). Bind it once; this
       class owns the DevelopHistory model it views and answers its hover/click. */
    void bindHistoryView(HistoryView *view);

    /* The Presets dock's list (owned by MW, lives in its own dock). Bind it once; this
       class owns the DevelopPresets store it views and answers its hover/click and its
       New / Update / Rename / Delete requests. */
    void bindPresetsView(PresetsView *view);

    /* History hover: show entry i's state in the loupe WITHOUT touching the stored stack
       or the sliders (previewStack overrides what stackJob/editParams report). The caller
       renders the PROXY only -- hovering must not spin up full-res settle renders. */
    void previewHistoryEntry(int index);
    void endHistoryPreview();
    /* History click: make entry i the current state -- restore its EditStack, rebuild the
       panel, mark the sidecar dirty and re-render. Steps after it are discarded by the
       NEXT edit (see DevelopHistory::record). */
    void applyHistoryEntry(int index);
    /* Index (into the active scope's submasks) of the in-progress, uncommitted submask.
       MW composites it into the veil with the PREVIEWED op (pendingMaskOp), so the
       overlay shows the outcome. -1 when nothing is being defined. */
    int pendingMaskIndex() const { return pendingIdx; }
    /* The op the overlay AND the render are currently previewing for the pending submask
       (MaskOp: 0 Add, 1 Subtract, 2 Intersect). Not written into the component until
       commit. */
    int pendingMaskOp() const { return pendingOp; }
    /* Set by MW's modifier arbiter (Opt = Subtract, Shift+Opt = Intersect) while a
       submask is being defined: relabels the commit button and re-previews the veil.
       Releasing it falls back to the LATCHED op, not to Add (see latchMaskOp). */
    void setPendingMaskOp(int op);
    /* The user began SHAPING the submask (brush/object stroke, handle drag): the modifier
       held at that instant becomes what this submask DOES, and it survives the key
       being released. Without this, letting go of Opt turned the subtract just painted
       back into an add -- and [Update] committed the add, changing nothing on screen. */
    void latchMaskOp();
    /* The combine op the LIVE keyboard state means: Opt = Subtract, Shift+Opt =
       Intersect, neither = Add. The ONE place that mapping exists -- the button label,
       the veil preview and the commit all read it, so they cannot disagree. */
    static int maskOpFromModifiers();
    /* True while the mask panel is up (a submask is being defined). */
    bool isMaskPanelOpen() const { return maskPanelOpen; }
    /* [Update] / Return: fold the pending submask in with the op held right now. */
    void commitPendingMask();

    /* Enable/disable the WHOLE Develop panel so it "looks" disabled, not just the dock
       frame. Greys the property tree (caption text, which the delegate paints from the
       per-item UR_isEnabled role, plus every persistent editor and user interaction) and
       the ScopeHeader band above the tree. MW pairs this with developDock->setEnabled()
       so the dock frame and scopes grey out as well. */
    void setPanelEnabled(bool enabled);

    /* Sync the raw "Edit: Raw / Embedded Preview" selector (added under the scope header for raw
       files only) with G::useRaw. Called by MW::toggleUseRaw so the status-bar useRaw button and
       this selector always agree; also toggles the visibility of the Demosaic / Denoise raw rows. */
    void syncEditRaw(bool useRaw);

protected:
    /* PropertyEditor::mousePressEvent does not select rows (it only handles expand/collapse), so we
       toggle the clicked mask tool ourselves (reveal/hide its settings children) and return. */
    void mousePressEvent(QMouseEvent *event) override;
    /* A double-click resets the slider to default (base class) but Qt then moves focus to
       the tree; re-focus the slider so the caption double-click keeps the row lit. */
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    /* Suppress the native branch indicator; every expandable Develop row draws its own
       winnow arrow via the delegate, so the native triangle is redundant (and shows
       through beside the Demosaic value row's arrow). rootIsDecorated stays true. */
    void drawBranches(QPainter *painter, const QRect &rect,
                      const QModelIndex &index) const override;
    /* Carry the Scope band's containment rail (G::scopeRailX/W) down the tree's left
       edge, so the scope rows above and the sections below read as one block. Drawn
       over the rows, after the base paint. */
    void paintEvent(QPaintEvent *event) override;

public slots:
    void itemChange(QModelIndex idx) override;
    /* ImageView reports new mask geometry (dragged overlay) as the active tool's paramsJson. */
    void setActiveMaskParams(const QString &paramsJson);
    /* Re-assert the overlay for the active tool (e.g. when the Develop dock becomes visible). */
    void refreshMaskEdit() { updateMaskEdit(); }
    /* ImageView changed the brush size via keyboard ([ ]) or a two-finger drag; sync the dock. */
    void setActiveBrushSize(double size);
    /* ImageView toggled auto-mask ("A"); sync the dock checkbox. */
    void setActiveBrushAutoMask(bool on);
    /* ImageView showed/hid the mask overlay tint; sync the scope menu's check state. */
    void setMaskOverlayShown(bool shown);

    /* Regenerative spot fill. onSpotToolToggled arms/disarms spot-brush mode (from the
       Develop title-bar button); onSpotStrokeCommitted takes one finished stroke and
       appends it as a FillSpot to the current stack. */
    void onSpotToolToggled(bool active);
    void onSpotStrokeCommitted(const QString &paramsJson);
    void onSpotRemoveRequested(int index);      // a pin was clicked -> drop that spot
    bool isSpotActive() const { return spotMode; }   // for the title-bar spot button
    /* Replace panel preview eye: false renders WITHOUT the spot heals (non-destructive
       bypass, like the Transform eye); the spots stay in the stack. Session-wide. */
    void setSpotsShown(bool shown) { spotsShown = shown; }
    bool isSpotsShown() const { return spotsShown; }

    /* ---- ScopeHeader widget handlers (the scope dropdown + buttons above the tree) ---- */
    void onScopeSelected(const QString &name);    // dropdown picked a different scope
    void renameActiveScope();                     // [R] rename (dialog); Global cannot be renamed
    void resetActiveScope();                      // header reset: restore the whole scope's defaults
    void newScope();                              // [+] add a scope (name dialog, default "Scope n")
    void deleteScope();                           // [-] remove the selected scope (not Global)
    void showMaskMenu();                          // pop the Add/Subtract mask-tool menu (on new scope)
    void onScopePreviewToggled(bool shown);       // [E] show/ignore the whole scope
    void onScopeEnabledToggled(int index, bool on); // list row show/hide checkbox
    void setTreeCollapsed(bool collapsed);        // > hide/show this tree (the scope's items)

    void howThisWorks();                          // Develop help

    /* ---- Develop presets (the Presets dock) --------------------------------------
       saveDevelopPreset snapshots the current image's develop state into a named preset:
       it opens the Save Develop Preset checklist dialog (name, source scope, the ticked
       settings) and hands the result to the DevelopPresets store. No-op with a message
       when there is no current image or it has no edits. Reached via Cmd+Shift+N, the
       Develop dock context menu and the Presets dock's [+].

       previewPreset / endPresetPreview are the Presets list's HOVER: show the preset
       applied on the loupe without touching the stored stack or the sliders (the same
       previewStack override History uses). applyPreset is the CLICK: merge the preset
       into the ACTIVE scope for real, rebuild the panel and record one history step.
       updatePresetFromCurrent / renamePreset / deletePreset serve the list's context
       menu. */
    void saveDevelopPreset();
    void previewPreset(const QString &name);
    void endPresetPreview();
    void applyPreset(const QString &name);
    void updatePresetFromCurrent(const QString &name);
    void renamePreset(const QString &from, const QString &to);
    void deletePreset(const QString &name);

    /* ---- Copy / Paste develop settings (Lightroom's Copy Settings / Paste Settings) --
       An UNNAMED preset, and deliberately the same machinery: copyDevelopSettings opens
       the same checklist (in Copy mode -- no name field), captures the ticked settings
       from the chosen scope with the same buildPreset, and stores them in the develop
       clipboard; pasteDevelopSettings merges that buffer into the ACTIVE scope of the
       current image with the same mergePreset + one history step, exactly as applying a
       preset does. So copy/paste inherits every preset rule: only what you ticked is
       written, the target is the active scope, per-image items go to scope 0 / the
       geometry, spots append, and the mask does not travel.

       hasCopiedSettings / copiedSettingsSource drive the menu's enabled state and its
       tooltip ("Paste 'Basic' settings from IMG_1234.NEF"). */
    void copyDevelopSettings();
    void pasteDevelopSettings();
    bool hasCopiedSettings() const;
    QString copiedSettingsSource() const;   // file name copied from, or empty

    /* ---- White balance (Basic panel, above Temp) ---------------------------------
       The dropper: ImageView reports the normalized point the user clicked, and
       onWbSampled reads that pixel out of the pre-develop WorkingImage and solves the
       (Kelvin, tint) that makes it neutral -- Lightroom's behaviour. cancelWbDropper
       disarms without sampling (Esc, image change, another tool). */
    void onWbSampled(double nx, double ny, bool skin);
    void cancelWbDropper();
    void toggleWbDropper();         // "W" in Develop mode, and the row's dropper button
    bool isWbDropperActive() const { return wbDropperActive; }

    /* MW-driven raw-denoise completion state for the "Denoise"/"Denoised" checkbox:
       checked + "Denoised" when a denoised base is ready for the current image, else
       unchecked + "Denoise". Signal-blocked so it never re-triggers a run. */
    void updateDenoiseRunState(bool denoised);

signals:
    void paramsChanged();           // a develop value changed (decode hook; deferred)
    /* A History row is being hovered (or the hover ended): re-render the PROXY preview
       only. Deliberately not paramsChanged -- that also arms the full-res settle render,
       which a passing cursor must not trigger. */
    void historyPreviewChanged();
    /* The "Edit: Raw / Embedded Preview" selector was changed; MW drives G::useRaw (toggleUseRaw)
       -- a private slot, so we route through this signal rather than calling it directly. */
    void useRawRequested(bool useRaw);
    /* The "Demosaic" combo selects the RAW decode engine (Apple Core Image vs in-house
       Winnow). MW sets G::decodeRawEngine and re-decodes the current image. */
    void demosaicEngineChanged(bool useApple);
    /* Global raw-denoise (PMRID) run mode. autoRunDenoiseToggled: the "auto run denoise"
       checkbox flipped -- MW gates its automatic PMRID runs on it.
       runRawDenoiseRequested: the "Run Denoise" button was clicked -- run now regardless
       of the flag. */
    void autoRunDenoiseToggled(bool on);
    void runRawDenoiseRequested();
    void clearRawDenoiseRequested();    // "Denoise" unchecked -> drop the denoised base
    /* Mask editing handshake with ImageView. Begin when a spatial mask tool becomes the active
       (selected) edit target; End when none is selected. ImageView draws the overlay + handles and
       sends geometry back via setActiveMaskParams. */
    void maskEditBegin(int tool, int op, bool inverted, const QString &paramsJson, double feather);
    void maskEditEnd();
    /* Regenerative spot fill: arm/disarm ImageView spot-brush capture. Emitted when the
       Develop title-bar spot tool is toggled. */
    void spotEditBegin();
    void spotEditEnd();
    /* White-balance dropper: arm/disarm ImageView's sample-a-neutral mode. */
    void wbDropperBegin();
    void wbDropperEnd();
    /* Spot tool armed/disarmed: drives the title-bar spot button's on/off icon. */
    void spotActiveChanged(bool active);
    /* The current image's spot centres (normalized), for ImageView's on-canvas pins. */
    void spotPinsChanged(const QVector<QPointF> &pins);
    void maskFeatherChanged(double feather);    // Feather slider -> live overlay ramp update
    void maskInvertChanged(bool inverted);      // Invert checkbox -> live overlay flip
    /* Content-range tool params (Luminance lo/hi, Color refine + samples) changed in the dock ->
       ImageView rebuilds its coverage tint. */
    void maskRangeChanged(const QString &paramsJson);
    /* Brush current settings (for the cursor + the next stroke). size/flow 0..100. */
    void maskBrushSettingsChanged(double size, double feather, double flow, bool autoMask,
                                  const QString &autoMaskMode);
    /* Brush "AI edge (SAM)" was just enabled in the dock -> MW pre-warms the SAM
       encoder. */
    void maskBrushAiEnabled();
    /* An adjustment slider (Basic/Color/Effects) was changed while a mask overlay is
       shown -> ImageView hides the red coverage tint so the effect on the masked pixels
       is visible. */
    void maskTintHideRequested();
    /* Another scope was selected -> ImageView un-hides the tint so that scope's combined
       mask is visible (the hidden flag is sticky). */
    void maskTintShowRequested();
    /* The scope menu's "Show mask overlay" row was clicked -> MW flips the tint. */
    void maskOverlayToggleRequested();
    /* Rebuild the overlay tint ALONE (previewed op changed, overlay colour changed).
       Deliberately NOT paramsChanged: nothing about the developed image has changed, and
       paramsChanged would pay a proxy + full-res re-render per modifier press. */
    void maskOverlayRefreshRequested();

private:
    void initialize();

    /* Build/rebuild the whole tree for the ACTIVE scope: the scope's top items (Core rows for Global,
       else mask tool rows) followed by the Basic / Color / Effects sections. Called on image change,
       scope switch and mask add/remove/select; section expand-state is preserved across the rebuild. */
    void buildTree();
    void addCoreItems();            // Global only: Demosaic + Denoise rows at the top of the tree
    void addMaskItems();            // non-Global: the scope's mask tool rows at the top of the tree
    void addAddMaskRow();           // non-Global with no mask: an "Add mask" [+] placeholder row
    void applyScopeItemsCollapsed();// hide/show just the scope's top items (not the sections)
    void addBasic();
    void addColor();
    void addColorMix();
    void addEffects();

    /* ---- White balance row (Basic, above Temp) -----------------------------------
       A dropper toggle in the caption cell and the preset dropdown in the value cell,
       built with the same DT_None + setIndexWidget idiom as the Core rows. The named
       illuminants are offered for RAW only: a rendered JPEG has no camera-neutral
       reference, so (like Lightroom) it gets As Shot / Auto / Custom. Full colour
       science lives in Develop/whitebalance.h -- this is only the UI. */
    void addWhiteBalanceRow(QModelIndex parIdx);
    void setWbPreset(int preset);      // apply a dropdown pick to the active scope
    void refreshWbRow();               // sync the combo + Temp/Tint display
    void setWbDropperActive(bool on);
    /* The current image's colour characterisation, from the cached pre-develop
       WorkingImage. Invalid when the image is not (yet) in the cache, which resolves
       temperatures to a D65 fallback and disables the dropper. */
    CameraColor currentCam() const;
    static QIcon dropperIcon(bool armed);   // drawn, not a resource
    QPointer<QComboBox> wbCombo;
    QPointer<BarBtn> wbDropperBtn;
    bool wbDropperActive = false;
    void updateSectionHeaderCaptions();   // append active scope name to section headers

    /* ---- Color Mix (colour grading) --------------------------------------------------
       The wheel is a directly-embedded index widget (setIndexWidget), NOT a delegate
       editor; it edits whichever range(s) the Dark/Mid/Light checkboxes select
       (gradeActiveMask bits: 0x1 shadow, 0x2 mid, 0x4 high). The Luminance slider writes
       the same active range(s). Recreated on every tree rebuild. */
    QPointer<ColorGradeWheel> colorGradeWheel;
    int  gradeActiveMask = 0x2;             // midtones checked by default
    void onGradeWheelChanged(bool commit);  // wheel drag -> active-scope grade params
    void refreshColorMixRow();              // push stored grade to the wheel + Lum slider
    void setGradeLum(float lum);            // write Lum to every active range
    int  firstActiveGradeRange() const;     // lowest checked range (drives Lum slider)

    /* ---- Color Range mask wheel ------------------------------------------------------
       Embedded index widget (like colorGradeWheel) shown above the Color Range mask's
       sliders. Shows the sampled colours + their hue/sat selection band and lets the user
       drag the hue/sat bounds. Recreated on every tree rebuild that shows a Color Range
       tool; null otherwise. */
    QPointer<ColorRangeWheel> colorRangeWheel;
    void onColorRangeWheelChanged(bool commit);   // wheel drag -> mask hue/sat bounds
    void refreshColorRangeWheel();                // push samples + bounds into the wheel
    static QVector<QPointF> colorRangeSamplesHS(const QString &paramsJson);

    /* Clicking a slider row's caption flashes that caption white (fading to 0 via
       UR_FlashLevel) as feedback, in addition to focusing the slider + hiding the mask
       overlay (see mousePressEvent). QPersistentModelIndex so a tree rebuild mid-flash
       cannot leave a stale row lit. */
    void flashCaption(const QModelIndex &capIdx);
    QPersistentModelIndex flashCaptionIdx;
    QPointer<QVariantAnimation> captionFlashAnim;

    /* Section (Basic/Color/Effects) expand state persists across sessions in QSettings.
       sectionExpanded reads the saved state (with a first-run default); persistSectionExpanded
       writes it when the user toggles a section header. */
    bool sectionExpanded(const QString &name, bool def) const;
    void persistSectionExpanded(const QModelIndex &idx, bool expanded);

    /* ---- Mask (one mask per mask, built from a list of Add/Subtract tools) ----------
       Self-contained so the whole mask UI can be redesigned by rewriting just these functions and
       the MaskComponent model. The scope's single mask is an ordered list of tools (each Adds or
       Subtracts area); each tool is a row with a [+] add and a [-] remove button ([+] appends
       another tool via showMaskMenu), and clicking a tool reveals its settings (Feather, Invert)
       below the list (click the tool again to collapse). Spatial editing (drag/rotate the gradient on the image)
       composites the mask into the render; see notes/Documentation.txt. */
    /* One row per tool; the SELECTED tool also gets its settings (Feather/Invert/Done) as children. */
    void addToolRow(QModelIndex parIdx, int index, const MaskComponent &m, bool selected);
    void newMask();                            // QAction handler: append the chosen Add/Subtract tool
    void deleteMask(int index);
    void setSelectedMask(int index);           // make a tool active (-1 = none, e.g. Done)
    void onMaskSelectionChanged();             // (programmatic selection only; clicks go via mousePressEvent)
    EditScope *activeScope();                  // active scope of image, or nullptr
    static int maskToolFromName(const QString &name);
    static QString opName(int op);             // "Add" / "Subtract"

    /* ---- Preview (show/ignore) + Reset per group ----------------------------------------------
       Each section header (Basic/Color/Effects) and the Scopes header carry an eye BarBtn that
       toggles that group's Preview flag on the active scope (non-destructive: values are kept, the
       group is folded to identity at render by effectiveScopeParams). Right-clicking a header pops
       a menu to toggle Preview or Reset (restore defaults, destructive) for that group. Transform's
       preview/reset live in TransformPanel (separate widget), wired via MW. Group codes: PV_Scope =
       whole active scope, PV_Basic/PV_Color/PV_ColorMix/PV_Effects = a section. */
    enum PreviewGroup { PV_Scope = -1, PV_Basic = 0, PV_Color = 1,
                        PV_ColorMix = 2, PV_Effects = 3 };
    BarBtn *makeEyeBtn(const QString &tooltip, int group);   // queue an eye toggle into `btns`
    void togglePreviewSection(int group);   // flip the flag, refresh icon, re-render (no value change)
    void resetSection(int group);           // restore the group's defaults, repopulate, re-render
    void refreshPreviewButtons();           // sync every eye icon from the active scope's flags
    void showRawDemosaic();                 // Global + expand: reveal raw Core rows
    /* PV_* -> EditParams::Group (Basic for PV_Scope). */
    static EditParams::Group paramsGroup(int group);
    /* PV_* -> "Basic" / "Effects" / ... , for history captions. */
    static QString groupLabel(int group);
    bool *previewFlag(EditScope *l, int group);        // the bool a PV_* code maps to on a scope
    BarBtn *basicEyeBtn = nullptr,
           *colorEyeBtn = nullptr, *colorMixEyeBtn = nullptr, *effectsEyeBtn = nullptr;

    void contextMenuEvent(QContextMenuEvent *event) override;   // right-click menu

    /* Expand all / Collapse all, extended to drive the Scope row (its collapse arrow
       lives in the ScopeHeader band, not the tree). onSectionExpanded folds the Scope
       row into Solo mode: expanding an adjustment section collapses the scope, and
       vice versa. */
    void setAllSectionsExpanded(bool expand);
    void onSectionExpanded(const QModelIndex &idx);

    /* Solo mode peers: the Raw panel and the Scope row are widgets outside the tree, so
       the base PropertyEditor's sibling-collapse cannot reach them. soloCollapseOthers
       folds every peer except the one just opened; owner says which that is (for a
       section, keepSection names it). No-op unless Solo is on, and skipped during the
       Expand-all / Collapse-all sweep. */
    enum class SoloOwner { RawPanel, ScopeRow, Section };
    void soloCollapseOthers(SoloOwner owner, const QString &keepSection = QString());

    /* Item builders. div converts the integer slider amount to a double (eg /100), and
       defaults to identity (0) so an absent value is a no-op edit. */
    void addHeader(const QString &name, const QString &parent,
                   const QString &caption, const QString &tooltip, int previewGroup = -1);
    void addSlider(const QString &key, const QString &caption, const QString &tooltip,
                   QModelIndex parIdx, const QString &parentName,
                   int min, int max, int div, QString color, QString color1,
                   double defaultValue = 0, bool logScale = false);
    void addCheckbox(const QString &key, const QString &caption, const QString &tooltip,
                     QModelIndex parIdx, const QString &parentName, bool defaultValue = false);

    /* ---- Develop presets ----------------------------------------------------------
       buildPreset turns the dialog's ticked keys into a DevelopPreset, reading the
       adjustments out of scope `srcScope` and the per-image items out of the stack;
       collectScopeLeaves fills the adjustment values for one set of checked keys.
       changedLeavesForScope reports which adjustment leaves differ from their default
       in a scope -- the dialog's pre-checked state, recomputed as its combo moves.

       mergePreset is the SINGLE apply point, shared by hover-preview and click-apply: it
       returns a copy of `s` with the preset's stored keys written into scope `target`
       (the per-image items always going to scope 0 / the geometry). Absent keys are left
       alone -- that is what makes a preset partial. */
    DevelopPreset buildPreset(const QString &name, int srcScope,
                              const QHash<QString, QSet<QString>> &selected,
                              const EditStack &stack) const;
    static void   collectScopeLeaves(const EditParams &p, const QSet<QString> &keys,
                                     QVariantHash &out);
    static QSet<QString> changedLeavesForScope(const EditParams &p);
    EditStack     mergePreset(const DevelopPreset &preset, EditStack s, int target) const;
    int           targetScopeIndex(const EditStack &s) const;   // active, clamped valid

    /* The checklist model (Global settings + one group per Develop section), pre-checked
       from what differs from default in `s`. Shared by Save Preset and Copy Settings --
       they ask the user the same question. */
    QVector<PresetGroup> buildChecklistGroups(const EditStack &s) const;

    /* Apply an already-read preset to the current image: the shared body of applyPreset
       and pasteDevelopSettings (mask-tool teardown, merge, rebuild, one history step,
       decode-engine switch). `label` is the history text ("Preset: Warm" / "Paste
       settings"); `source` names the preset or the clipboard in any warning.
       With a multi-image selection it also merges into every other selected image
       (propagatePreset). Returns the number of images written, so the caller's
       confirmation can say so. */
    int  applyPresetObject(const DevelopPreset &preset, const QString &label,
                           const QString &source);

    QString uniqueScopeName(const QString &name) const;  // unique within this image

    /* Per-image stack helpers. The Scopes combo + (+/-) act on the CURRENT IMAGE's EditStack;
       activeScopeIndex is the scope the dock edits and the renderer shows (no mask/opacity
       compositing yet). */
    void populateSlidersFromStack();              // push the active scope's params into the editors
    void setSliderReal(const QString &key, double real);   // set a slider's displayed value (un-scaled)
    void setCheckboxValue(const QString &key, bool on);
    /* A tone-region slider drag: write the three split positions into the active scope's params
       and drive the live preview (no-op while populating). */
    void onToneSplitsChanged(double shadow, double crossover, double highlight);
    static void applyKeyToParams(const QString &key, const QVariant &v, EditParams &p);
    QStringList currentScopeNames() const;        // names of the current image's scopes (>=1)
    void refreshScopeList();                     // rebuild the combo's list/value from the stack
    void updateMaskMenuBtn();                     // tell the header whether Global is active (per-scope actions)
    void updateMaskEdit();                        // emit maskEditBegin/End for the active mask tool
    static QString defaultMaskParams(int tool);   // initial paramsJson geometry for a new tool
    int  activeMaskTool() const;                  // active component's tool, or -1
    /* Brush current-settings accessors over paramsJson (size/flow are 0..100, autoMask bool). */
    static double  brushNum(const QString &paramsJson, const QString &key, double def);
    static bool    brushBool(const QString &paramsJson, const QString &key, bool def);
    static QString brushStr(const QString &paramsJson, const QString &key, const QString &def);
    static QString brushWith(const QString &paramsJson, const QString &key, const QJsonValue &v);
    void emitBrushSettings(const MaskComponent &m); // maskBrushSettingsChanged from current settings
    EditParams &activeParams();                   // the active scope's params (creates a scope if none)

    /* The per-image edit state. stackCache holds loaded/edited stacks keyed by file path; dirty
       marks those needing a sidecar write; currentImagePath is the image the dock currently
       shows; activeScopeIndex is the selected scope within that image. isPopulating suppresses
       itemChange while we push values into the editors. */
    QHash<QString, EditStack> stackCache;
    QSet<QString> dirty;
    QString currentImagePath;
    /* stackCache entry for any image, loading it from the sidecar on first touch. The
       propagation path needs stacks for images the user has never visited. */
    EditStack &stackFor(const QString &fPath);

    /* The compositing rules shared by stackJob() and stackJobFor(): which scopes and mask
       components survive into a render job. The callers differ only in which EditStack
       they supply and whether the geometry / spots view toggles apply. */
    StackRenderJob buildStackJob(const EditStack &s, bool useGeometry, bool useSpots) const;

    /* ---- Multi-image editing (Lightroom's Auto Sync) -------------------------------
       With more than one image selected, an adjustment made on the current image is
       copied to every other selected image. The rules:

       o ABSOLUTE, not relative. The target gets the same VALUE, not the same delta --
         so Temp lands as the same Kelvin on every image (temp/tint are absolute; see
         Develop/whitebalance.h), and Exposure +0.5 means +0.5 EV everywhere. This is
         what Auto Sync does and it is the only rule that makes a mixed selection
         (some images already edited) predictable.
       o GLOBAL SCOPE ONLY. Only scope 0's EditParams travel. A mask scope's params
         describe pixels chosen from THIS image's content, so an edit made inside a
         mask scope stays on the current image -- the same rule presets follow (the
         mask never travels). Mask geometry, spot heals and crop/straighten/warp are
         per-image for the same reason and are never propagated by an adjustment.
       o WHAT CHANGED, not the whole recipe. Each commit is diffed against
         propagateBase (the current image's scope-0 params as of the previous commit)
         and only the fields that actually moved are written, so propagation never
         overwrites settings on the other images that the user did not just touch.

       Paste Settings / Apply Preset do NOT go through the diff: they merge the whole
       preset object into every selected image (propagatePreset), so the copied crop,
       spots and per-image items travel exactly as they do on the current image.

       BATCHING. A slider drag commits continuously and each target may need a sidecar
       read, so commits accumulate into pendingFields and land once edits settle
       (kPropagateMs). propagateTargets is captured when a batch OPENS, so changing the
       selection mid-drag cannot redirect edits already made. */
    void queuePropagation(const QString &action, const QString &value);
    void syncPropagateBase();                  // adopt the current stack as the diff base
    /* Whole-object merge into the other selected images; returns how many it wrote. */
    int  propagatePreset(const DevelopPreset &preset, const QString &label);
    QStringList otherSelectedPaths() const;    // selected images minus the current one
    /* The names of the EditParams fields that differ, and the copy of those fields --
       one field table drives both (see kFloatFields / kIntFields in the .cpp). */
    static QSet<QString> diffParamFields(const EditParams &a, const EditParams &b);
    static void copyParamFields(const EditParams &src, EditParams &dst,
                                const QSet<QString> &fields);
    EditParams propagateBase;              // current image scope-0 params at last commit
    QSet<QString> pendingFields;           // fields waiting to be copied to the targets
    QStringList propagateTargets;          // captured when the batch opened
    QString propagateAction, propagateValue;    // history label for the batch
    bool propagateSuspended = false;       // whole-object paths do their own propagation
    QTimer *propagateTimer = nullptr;
    static constexpr int kPropagateMs = 400;    // apply this long after edits settle

    /* ---- Corrupt / unreadable develop settings -------------------------------------
       Sidecars are read on every image the user visits, so a damaged folder must not pop
       one toast per file: warnOnce reports each KIND of problem at most once a session
       (warnedIssues holds the kinds already shown) while still logging every occurrence.
       reportStackIssues maps what EditStack::fromBase64 found onto those kinds; status is
       an EditStack::Status, passed as int so this header need not order its includes
       around it. See notes/Documentation.txt "Corrupted or changed develop settings". */
    void warnOnce(const QString &kind, const QString &msg);
    void reportStackIssues(const QString &fPath, int status, const QStringList &issues);
    QSet<QString> warnedIssues;

    /* ---- Edit history (the History dock) ------------------------------------------
       noteEdit() is the SINGLE commit point for every develop action: it marks the
       sidecar dirty (what the old bare dirty.insert did), pushes a labelled snapshot of
       the image's EditStack onto its history, and arms the debounced write. A non-empty
       mergeKey coalesces a continuous gesture (a slider drag, a wheel drag) into one
       history entry instead of one per tick.

       previewStack/previewActive is the hover override read by stackJob/editParams;
       isRestoringHistory suppresses recording while a restore repopulates the panel
       (otherwise reverting would itself append history entries). */
    void noteEdit(const QString &action, const QString &value = QString(),
                  const QString &mergeKey = QString());
    /* noteEdit with an explicit prefix: "Global" for the raw rows (which always write
       scope 0), or "" for whole-image actions (crop, spots) that belong to no scope. */
    void noteScopeEdit(const QString &scope, const QString &action,
                       const QString &value = QString(),
                       const QString &mergeKey = QString());
    /* Display prefix for a history entry: the active scope's name, except an unrenamed
       "Mask N" with tools reads as its tool ("Subject Mask"), matching the dock. */
    QString historyScopeLabel() const;
    /* A slider's committed value, formatted the way the slider itself shows it (int when
       div == 0, else 2 dp) with a + on positives. */
    static QString historyValueText(const QModelIndex &valIdx, const QVariant &v);

    DevelopHistory *history = nullptr;
    HistoryView *historyView = nullptr;
    DevelopPresets *presets = nullptr;
    PresetsView *presetsView = nullptr;
    EditStack previewStack;
    bool previewActive = false;
    bool isRestoringHistory = false;
    int activeScopeIndex = 0;
    bool isPopulating = false;
    bool scopeItemsCollapsed = false;   // the '>' arrow: hide the scope's top items
    bool isBulkExpandCollapse = false;  // guard: Expand/Collapse all vs Solo handler

    int dividerHeight;
    QColor divColor;


    /* Mask UI state. selectedMaskIndex is the component shown in the shared Mask Tool panel (-1 =
       none). isRebuildingMasks guards the tree-selection handler while we add/remove mask rows.
       maskMenu is the "+ add mask" type chooser (popped by the header's [M]). UR_MaskIndex tags a
       mask row's caption with its component index so selection can find it. */
    int selectedMaskIndex = -1;
    bool isRebuildingMasks = false;

    /* ---- Mask build-up (lab UI): append-only "flatten" via the MaskPanel ----------
       A picked submask is APPENDED to the scope's mask and edited in the panel's embedded
       MaskEditor. EVERY submask (including the first) is PENDING -- pendingIdx = its
       index -- until the single [Update] button (or Return) folds it in with the op held
       at that moment, or Cancel/Esc removes it. The overlay composites the pending
       submask with pendingOp, so the veil shows the OUTCOME rather than colouring the
       operand. maskPanelOpen tracks whether the strip is up, so Esc cancels the panel
       edit rather than collapsing a legacy tree tool. */
    MaskPanel *maskPanel = nullptr;
    int  pendingIdx = -1;                     // uncommitted submask's index, else -1
    int  pendingOp = 0;                       // op the overlay + render preview
    int  latchedMaskOp = 0;                   // op the last shaping action set (survives
                                              // the key release; see latchMaskOp)
    bool maskPanelOpen = false;
    /* Keep the combined-mask (red) overlay showing after a tool is committed/finished
       (the panel closes but the result stays visible + 'O'-toggleable). Cleared on scope/
       image switch or cancel. */
    bool maskLatched = false;
    void beginMaskTool(int tool);            // pick -> append as the pending submask
    void onMaskToolChosen(int tool);         // a submask type was chosen -> beginMaskTool
    void cancelMaskTool();                   // [x]/Esc: discard the pending submask
    MaskComponent *editingMaskComp();        // the tool being built, or null
    /* MaskEditor (panel) change routing -- mirrors the tree's itemChange mask blocks. */
    void onMaskEditorSetting(const QString &key, const QVariant &value);
    void onMaskEditorWheel(int hueLo, int hueHi, int satLo, int satHi, bool commit);
    bool spotMode = false;              // regenerative spot-fill brushing is armed
    bool spotsShown = true;             // Replace preview eye: heals rendered or bypassed
    QVector<QPointF> spotPinCenters() const;   // current image's spot centres (norm)
    void emitSpotPins();                       // push spotPinCenters() to ImageView
    QMenu *maskMenu = nullptr;
    ScopeHeaderBase *scopeHeader = nullptr; // scope dropdown + buttons, band above tree
    static constexpr int UR_MaskIndex = Qt::UserRole + 100;
    QTimer *debounceWriteTimer = nullptr;
    static constexpr int kDebounceWriteMs = 2000;  // flush this long after edits settle (gated)

    /* Whole-panel enable state (Develop menu action). When false the tree is disabled and
       every caption is greyed; buildTree() re-applies it so a rebuild can't un-grey it. */
    bool panelEnabled = true;
    void applyItemsEnabled(bool enabled);   // set UR_isEnabled on every row (recursively)

    /* Raw "Edit source" selector (Global scope, raw files only). editRawRadio is the "Raw" button
       of the A/B pair; the widget lives in the Edit row's value cell (recreated each buildTree).
       currentIsRaw() gates the raw-only rows; onEditSourceChanged() drives G::useRaw via MW;
       applyCoreVisibility() shows/hides the Demosaic + Denoise raw rows per G::useRaw. */
    bool currentIsRaw() const;
    void onEditSourceChanged(bool raw);
    void applyCoreVisibility();
    QPointer<QRadioButton> editRawRadio;

    /* RawPanel (lab UI) sync + handlers. syncRawPanel pushes the current raw state (edit
       source, engine, denoise run state, Global denoise amounts) into the panel and toggles
       its visibility (raw files only); setGlobalDenoise writes a slider change into the
       Global scope's params. rawPanel is null in the legacy UI -- all guards no-op. */
    RawPanel *rawPanel = nullptr;
    void syncRawPanel();
    void setGlobalDenoise(bool luma, int value0to100);
    /* "Denoise" run/state checkbox in the Core (raw) section. MW calls
       updateDenoiseRunState to reflect completion: checked + "Denoised" when a denoised
       base is ready for the current image, else unchecked + "Denoise". QPointer --
       recreated each buildTree. */
    QPointer<QCheckBox> denoiseRunCheck;

    MW *mw;
    QSettings *setting;
    ItemInfo i;

    QModelIndex root;
    ToneRegionSlider *toneSlider = nullptr;       // histogram region slider (owned by ScopesView)
};

#endif // DEVELOPPROPERTIES_H
