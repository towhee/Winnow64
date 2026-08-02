#ifndef DEVELOPPROPERTIES_H
#define DEVELOPPROPERTIES_H

#include <QtWidgets>
#include "PropertyEditor/propertyeditor.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"
#include "Develop/History/develophistory.h"
#include "Develop/workingimage.h"
#include "Develop/Properties/colorgradewheel.h"
#include "Develop/Properties/colorrangewheel.h"

class MW;
class ToneRegionSlider;
class ScopeHeaderBase;
class RawPanel;
class MaskPanel;
class HistoryView;
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
    /* Display name of a MaskTool (public so MW can label the overlay legend). */
    static QString maskToolName(int tool);
    /* Scope menu's "Show mask breakdown" check state, mirrored from MW's session flag. */
    void setMaskBreakdownShown(bool shown);

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
    void flushImage(const QString &fPath);        // write one image's dirty stack to its sidecar
    void flushAll();                              // write all dirty stacks (quit / pre-op)

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

    /* History hover: show entry i's state in the loupe WITHOUT touching the stored stack
       or the sliders (previewStack overrides what stackJob/editParams report). The caller
       renders the PROXY only -- hovering must not spin up full-res settle renders. */
    void previewHistoryEntry(int index);
    void endHistoryPreview();
    /* History click: make entry i the current state -- restore its EditStack, rebuild the
       panel, mark the sidecar dirty and re-render. Steps after it are discarded by the
       NEXT edit (see DevelopHistory::record). */
    void applyHistoryEntry(int index);
    /* Index (into the active scope's masks) of the in-progress, uncommitted tool -- MW
       draws it BLUE over the red committed veil while a later mask tool is previewed. -1
       when nothing is pending (the first tool is committed immediately, drawn red). */
    int pendingMaskIndex() const { return pendingIdx; }

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

    /* Snapshot the current image's develop state into a named preset (QSettings). Opens
       the Save Develop Preset checklist dialog; no-op (with a message) when there is no
       current image or it has no edits. Reached via Cmd+Shift+N and the dock context
       menu. Applying a preset is a separate, later task. */
    void saveDevelopPreset();

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
    /* The scope menu's "Show mask breakdown" row -> MW flips Result/Breakdown view. */
    void maskBreakdownToggleRequested();

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

    /* ---- Develop presets (save) --------------------------------------------------
       presetNames lists the existing preset groups; writePreset persists the dialog's
       selection under "Develop Presets/<name>/Global|Global scope|Scope N/";
       writeScopeLeaves writes one scope group's checked adjustment values (raw
       EditParams field values under their JSON key names, so a future apply can
       round-trip them). */
    QStringList presetNames() const;
    void writePreset(const QString &name, const QHash<QString, QSet<QString>> &selected,
                     const EditStack &stack);
    void writeScopeLeaves(const EditParams &p, const QSet<QString> &keys);

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
       A picked tool is APPENDED to the scope's masks and edited via the property tree
       (addToolRow -- so its settings look exactly like the tree). The FIRST tool on an
       empty mask is committed immediately and drawn RED; a LATER tool is a preview drawn
       BLUE (pendingIdx = its index) until [Add]/[Subtract]/[Intersect] sets its op (fold)
       or Cancel/Esc removes it. maskPanelOpen tracks whether the strip is up, so Esc
       cancels the panel edit rather than collapsing a legacy tree tool. Gradient + brush
       use this flow; range/AI still edit in the tree. */
    MaskPanel *maskPanel = nullptr;
    int  pendingIdx = -1;                     // uncommitted (blue) tool's index, else -1
    bool maskPanelOpen = false;
    /* Keep the combined-mask (red) overlay showing after a tool is committed/finished
       (the panel closes but the result stays visible + 'O'-toggleable). Cleared on scope/
       image switch or cancel. */
    bool maskLatched = false;
    void beginMaskTool(int tool);            // pick -> append; commit first, else pending
    void onMaskToolChosen(int tool);         // a tool was chosen -> beginMaskTool
    void commitPendingMask(int op);          // [Add]/[Subtract]/[Intersect]
    void finishFirstMask();                  // [Done] on the first tool (keep it)
    void cancelMaskTool();                   // [x]/Esc: discard the preview / first tool
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
