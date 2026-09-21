#ifndef DEVELOPPROPERTIES_H
#define DEVELOPPROPERTIES_H

#include <functional>

#include <QtWidgets>
#include "PropertyEditor/propertyeditor.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"
#include "Develop/History/develophistory.h"
#include "Develop/Presets/developpresets.h"
#include "Dialogs/savedeveloppresetdlg.h"    // PresetGroup: the checklist model
#include "Develop/workingimage.h"
#include "Develop/Properties/colorgradewheel.h"
#include "Develop/Properties/primarywheel.h"
#include "Develop/Properties/colorrangewheel.h"
#include "Develop/Properties/curveeditor.h"
#include "Develop/Properties/detailpreview.h"

class MW;
class ToneRegionSlider;
class ScopeHeader;
class RawPanel;
class MaskPanel;
class TransformPanel;
class ReplacePanel;
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
            float                  maskEdge = 0; // grow/shrink the FOLDED mask, full-res px
            float                  maskHalo = 0; // halo suppression on the FOLDED mask
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
       on a worker and nothing else in the suite goes near it.

       IDEMPOTENT despite the name: if the image already has a mask scope it re-activates
       it instead of appending another. The driver re-arms on every image switch, so
       appending compounded the stack and collapsed the render rate on long runs.
       Returns false if there is no current image. */
    bool selfTestAddMaskScope(int submasks);

    /* TEST HOOK. Nudge one adjustment on the GLOBAL scope (scope 0) or on the last mask
       scope, then emit paramsChanged -- i.e. exactly what an Exposure drag does, minus
       the property editor. MW::runDevelopStressTest alternates the two under
       ThreadSanitizer.

       WHY BOTH SCOPES: they drive DIFFERENT halves of the interactive cache and each has
       its own worker/GUI-thread interaction. A GLOBAL nudge changes the base signature,
       so the hot prefix is rejected and recaptured every tick (and, before this was
       narrowed, wiped every mask); a MASK-scope nudge leaves the base alone, so the
       prefix survives and the scope's layer is what churns. The brush drag the driver
       already simulates exercises neither -- it moves the MASK, not any params -- so
       without this the whole adjustment path, including the render scratch buffers the
       worker writes, went unraced.

       Writes the param and emits paramsChanged ONLY -- no noteScopeEdit. Marking the
       stack dirty persists it, which makes the driver's scopes compound across its
       folder churn and starves the render rate to nothing; see the implementation.
       Returns false if there is no current image. */
    bool selfTestNudgeAdjustment(bool globalScope, float exposure);

    /* Whole-mask overlay: true when a mask tool is expanded on a mask (so MW should
       show the composited mask), plus the active scope's ordered mask tools to composite. */
    bool maskOverlayActive() const;
    /* ---- Veil ENGAGEMENT -------------------------------------------------------------
       Whether the mask coverage veil should be on its own account: the mask panel has
       the user's attention, either because the cursor is in it or because the last
       thing they did was a mask edit (a canvas stroke or handle drag counts -- that is
       what lets them move off the panel onto the photo to paint without the veil
       dropping). Anything else they touch -- an Exposure slider, Transform, History --
       disengages, and the veil gets out of the way so the edit can be seen.

       This replaced ten hand-written `if (maskOverlayActive()) emit
       maskTintHideRequested()` calls scattered through the adjustment handlers, which
       could only ever hide it: nothing brought the veil back except "O", a scope switch
       or adding a submask. */
    bool maskVeilEngaged() const;
    void setMaskPanelHovered(bool hovered);   // MaskPanel::hoverChanged
    void noteMaskInteraction();               // a mask edit: engage
    void noteNonMaskInteraction();            // anything else: disengage
    /* "O" / the action-row tint button override the rule. The override stands until the
       user's attention actually moves, so it covers the moment it was asked for without
       becoming a sticky mode they have to remember they are in. MW calls it after
       toggling, with the veil's resulting state. */
    void setMaskVeilOverride(bool shown);
    /* Esc from the Develop arbiter: if a mask tool is expanded, collapse it (hide its
       settings, like clicking its caption again) and return true; else false. */
    bool escapeMaskTool();
    QVector<MaskComponent> activeScopeComponents() const;
    /* The active scope's mask-level Edge (EditScope::maskEdge), so the loupe veil grows
       and shrinks the composited mask exactly as the render does. 0 on the Global scope
       and when there is no image. */
    float activeScopeMaskEdge() const;
    /* The active scope's mask-level Halo, so the loupe veil suppresses the halo on the
       composited mask exactly as the render does. 0 on Global and with no image. */
    float activeScopeMaskHalo() const;
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

    /* The image's histogram, drawn behind the Curves panel's plot. Fed from the SAME
       ScopeData MW::updateDevelopScopes builds for the scopes strip, so it costs no extra
       sampling -- but the strip can be hidden while the Curves panel is open, which is
       what wantsScopeData() tells MW so it still takes the sample. */
    void setScopeData(const ScopeData &d);
    void clearScopeData();
    bool wantsScopeData() const;
    void flushImage(const QString &fPath);  // write one image's dirty stack to sidecar
    void flushAll();                        // write every dirty stack (quit/pre-op)

    /* Bring BOTH preview tiers up to date for fPath when the render has already produced
       a faithful frame. Costs a JPEG encode of pixels that were rendered anyway -- it
       never causes a decode or a render.

       This covers an image that was edited in an EARLIER session and merely visited in
       this one: nothing is dirty, so flushImage does not run, and without this such an
       image would never get a preview no matter how often it was opened. Every image
       edited before cached previews existed is in exactly that state.

       The two tiers are updated TOGETHER and the gate tests BOTH, so they cannot drift
       apart -- a thumbnail showing one thing while the loupe placeholder shows another
       would be worse than neither. That does mean visiting an edited image can rewrite
       its sidecar (preview attributes and modifydate only; the recipe is untouched). */
    void topUpDevPreviews(const QString &fPath);

    /* Supplies the cached develop previews written alongside the recipe in flushImage.
       MW registers this at startup and answers from the screen-resolution developProxy it
       already holds, so a preview costs a downscale plus a JPEG encode rather than a
       render. Returns false when fPath is not the image currently proxied -- multi-image
       propagation targets have no proxy, and a stale preview must be cleared rather than
       kept. See Cache/devpreviewcache.h and notes/Documentation.txt.

       thumbJpg  256px JPEG, base64-encoded into the sidecar (winnow:DevelopPreview)
       loupeJpg  screen-resolution JPEG for the out-of-band loupe cache */
    /* Does what the develop render is currently producing actually correspond to the
       STORED recipe? It does not when a History row is being hovered (previewActive
       renders a different stack entirely), when the Transform preview eye is off
       (geometry bypassed, so the frame is uncropped and unstraightened), or when the
       Replace eye is off (heals bypassed). A cached preview taken from such a frame
       would be keyed to a recipe it does not depict. See flushImage. */
    bool renderMatchesStoredRecipe() const;

    /* The base64 recipe currently in effect for fPath, or "" when it is identity. Reads
       the in-memory stack, which during the write debounce may be AHEAD of the sidecar --
       so a preview keyed on this is matched against what the user actually sees. */
    QString developBlobFor(const QString &fPath);

    using DevPreviewProvider =
        std::function<bool(const QString &fPath, QByteArray &thumbJpg, QByteArray &loupeJpg)>;
    void setDevPreviewProvider(DevPreviewProvider provider);

    /* ---- Multi-image editing (edits apply to the whole selection) -------------------
       When more than one image is selected, a Global adjustment made on the current
       image is copied to every other selected image (Lightroom's Auto Sync). See the
       "Multi-image editing" block in the private section for the rules.

       flushPropagation applies whatever is queued RIGHT NOW instead of waiting for the
       debounce. MW calls it whenever the selection changes (the queued batch belongs to
       the selection that was live when editing started, so it must land first) and
       before any operation that reads the sidecars.

       selectedEditCount is what the Develop dock's amber warning row reports: how many
       the next edit will touch (1 = just the current image). It is 0 when there is no
       current image to edit from -- notably a VIDEO current item, where MW sets the
       current path empty.

       selectedStillCount ignores the current item entirely: how many developable stills
       are in the selection, whatever the current item is. The alert rows need it for the
       mixed case (video current + stills selected), where selectedEditCount is 0 and
       would otherwise report nothing at all. */
    void flushPropagation();
    int  selectedEditCount() const;
    int  selectedStillCount() const;

    /* The scope combo (scopes + scope actions) and its eye live in a gradient header
       widget ABOVE this tree (see ScopeHeader). Bind it once; this class drives its
       combo/eye and handles its signals. */
    void bindScopeHeader(ScopeHeader *header);

    /* The raw-decode controls (Edit source / Demosaic / Denoise) live in a RawPanel above
       the scope bar, not in the tree. Bind it once; this class handles its signals and
       pushes state back via syncRawPanel(). */
    void bindRawPanel(RawPanel *panel);

    /* The mask build-up panel, below the scope bar. Bind it once; this class drives it
       and owns the mask model. */
    void bindMaskPanel(MaskPanel *panel);

    /* Right-click ANYWHERE in the Develop dock, not just in this tree. The dock container
       is watched for the ContextMenu events its children let through (only the scopes
       strip and this tree answer their own), and the menu is built for the section the
       click landed in -- see showPanelContextMenu. Transform and Replace are bound here
       because their reset/tips live on those panels; both may be null. */
    void bindPanelContextMenu(QWidget *container, TransformPanel *transform,
                              ReplacePanel *replace);

    /* Size the tree to its CONTENT instead of scrolling internally: it is a detail of the
       scope bar (ScopeHeader::EditsDetail) and the dock's own QScrollArea does the
       scrolling. Off (the default) leaves it a stretch widget with its own scrollbars. */
    void setFitToContentHeight(bool on);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

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
    /* Index (into activeScopeComponents()) of the submask the user is WORKING ON -- the
       pending one if there is one, else the one re-opened from the list. The veil
       distinguishes it from the rest of the mask (denser where the two overlap, a ghost
       where the submask falls outside the mask, a contour along its boundary), which is
       the only way a Subtract submask -- contributing nothing to the folded mask -- can
       be seen at all. -1 when nothing is open. */
    int focusSubmaskIndex() const { return pendingIdx >= 0 ? pendingIdx : selectedMaskIndex; }
    /* The op the overlay AND the render are currently previewing for the pending submask
       (MaskOp: 0 Add, 1 Subtract, 2 Intersect). Not written into the component until
       commit. */
    int pendingMaskOp() const { return pendingOp; }
    /* Set by MW's modifier arbiter (Opt = Subtract, Shift+Opt = Intersect) while a
       submask is being defined: re-previews the veil, the render and the on-canvas op
       chip. Releasing it falls back to the LATCHED op, not to Add (see latchMaskOp). */
    void setPendingMaskOp(int op);
    /* The user began SHAPING the submask (brush/object stroke, handle drag): the modifier
       held at that instant becomes what this submask DOES, and it survives the key
       being released. Without this, letting go of Opt turned the subtract just painted
       back into an add -- and the add is what got committed, changing nothing on
       screen. */
    void latchMaskOp();
    /* The combine op the LIVE keyboard state means: Opt = Subtract, Shift+Opt =
       Intersect, neither = Add. The ONE place that mapping exists -- the button label,
       the veil preview and the commit all read it, so they cannot disagree. */
    static int maskOpFromModifiers();
    /* SHARPENING MASK PREVIEW (Detail panel), as Lightroom's Alt-drag does it. Sharpening's
       Masking slider gates the effect by local edge strength, and which pixels survive
       that gate is invisible in the result until you are at 1:1 on the right part of the
       frame. Holding Opt while DRAGGING Masking replaces the photo with the gate in
       grayscale -- white sharpened, black protected.

       Momentary and read from the LIVE state, like the mask combine modifiers: active
       means Opt is down AND the Masking slider's handle is held. MW polls it on every Opt
       edge and mouse press/release (syncSharpenMaskPreview) and builds the image. */
    bool sharpenMaskPreviewActive() const;
    /* True while the mask panel is up (a submask is being defined). */
    bool isMaskPanelOpen() const { return maskPanelOpen; }
    /* True while ANY submask is open in the panel -- a pending one OR one re-opened from
       the submask list. Wider than isMaskPanelOpen: Return/Esc and the Shift-scope
       readout apply to both, only the discard/commit semantics differ. */
    bool isSubmaskOpen() const { return selectedMaskIndex >= 0; }
    /* Re-caption the panel's attribute header ("Size / Feather / Flow apply to ..."). MW
       calls it on every Shift press/release, since Shift retargets those at the last
       stroke and the header has to say so while it is held. */
    void syncAttributeScopeLabel();
    /* Fold the pending submask into the mask with the op currently being previewed.
       keepVeil leaves the red coverage overlay up, which is what the automatic commit
       wants: the submask stays open and the user is still editing it, so the veil must
       not vanish under them. The veil drops when the editing SESSION ends, in
       closeSubmaskEditing(). Reached from Return and from the settle timer. */
    void commitPendingMask(bool keepVeil = false);
    /* Land the pending submask NOW instead of waiting out the settle timer: called by
       anything that leaves the scope or the image it belongs to. Commits it if there is
       anything worth keeping, drops it silently if not (pendingMaskIsUntouched). No
       prompt -- committing is automatic, so there is no third answer to ask for. */
    void flushMaskAutoCommit();
    /* Return: the user says they are done with the open submask. Lands it if it is still
       inside its settle window, then ends the editing session (handles off the canvas,
       veil down). Nothing open -> nothing to do. */
    void finishSubmaskEditing();
    /* A brush/object stroke started (true) or finished (false). The settle timer must
       not fire under the user's hand mid-stroke, so a stroke stops it and the stroke's
       end re-arms it. Driven by ImageView::maskStrokeStateChanged via MW. */
    void setMaskStrokeActive(bool painting);
    /* True when committing the pending submask would achieve nothing -- it covers no
       pixels (an unpainted Brush/Object, an unsampled Color Range) or is still exactly
       what beginMaskTool built. The settle timer and flushMaskAutoCommit drop those
       instead of committing them, so a tool picked and then ignored leaves nothing. */
    bool pendingMaskIsUntouched(const MaskComponent &m) const;

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
       winnow arrow via the delegate, so the native triangle is redundant.
       rootIsDecorated stays true. */
    void drawBranches(QPainter *painter, const QRect &rect,
                      const QModelIndex &index) const override;
    /* Close the block with the rule every Develop panel carries along its bottom edge,
       just under the last row. Drawn over the rows, after the base paint. */
    void paintEvent(QPaintEvent *event) override;
    /* Fit mode only: the tree never scrolls itself, so pass the wheel out to the dock's
       scroll area instead of letting QAbstractScrollArea swallow it. */
    void wheelEvent(QWheelEvent *event) override;
    /* A width change can re-wrap delegate rows, changing the content height. */
    void resizeEvent(QResizeEvent *event) override;
    /* Centre the Curves plot (and the split handles under it) in the PANEL rather than in
       their tree cell -- see the definition. */
    void updateEditorGeometries() override;
    /* The Curves plot's visibility is one of the conditions gating the canvas point
       picker, and hiding/showing the dock moves it without touching any of our own
       state -- so re-evaluate it here (see updateCurvePickState). */
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

public slots:
    void itemChange(QModelIndex idx) override;
    /*
        The image's scene-linear WorkingImage (and with it its colour characterisation,
        WorkingImage::cam) has landed in WorkingImageCache.

        WHY THIS EXISTS. The Temp/Tint row resolves through cam, but the dock is pointed
        at a new image from MW::fileSelectionChange -- at SELECTION, a second or so before
        the raw decode finishes. currentCam() is a cache PEEK, so at that moment it
        returns a default (invalid) CameraColor and WhiteBalance::resolve falls back to
        its literal 6500 K / 0. Nothing re-read it when the decode landed, so the row then
        stayed at 6500/0 for that image no matter what the camera had metered.

        EDGE-TRIGGERED on cam becoming valid, and a no-op for any other path, because
        MW::renderDevelopPreview calls this on EVERY render -- including mid-drag, where
        re-pushing slider values would fight the user's hand.
    */
    void onWorkingImageReady(const QString &fPath);
    /* ImageView reports new mask geometry (dragged overlay) as the active tool's paramsJson. */
    void setActiveMaskParams(const QString &paramsJson);
    /* Re-assert the overlay for the active tool (e.g. when the Develop dock becomes visible). */
    void refreshMaskEdit() { updateMaskEdit(); }
    /* ImageView changed the brush size via keyboard ([ ]) or a two-finger drag; sync the
       dock. */
    void setActiveBrushSize(double size);
    /* ImageView changed the feather of a gradient / brush mask (Shift + wheel); sync the
       dock slider, persist and re-composite. */
    void setActiveMaskFeather(double feather);
    /* ImageView toggled auto-mask ("A"); sync the dock checkbox. */
    void setActiveBrushAutoMask(bool on);
    /* Re-read the sharpening-mask-preview state (Opt edge, mouse press/release, app
       activation) and emit sharpenMaskPreviewChanged when it flips, so MW builds or drops
       the preview. */
    void syncSharpenMaskPreview();
    /* ImageView showed/hid the mask overlay tint; sync the scope menu's check state. */
    void setMaskOverlayShown(bool shown);
    /* Set the overlay colour / grayscale-under-the-veil flag (the action-row tint
       button's context menu): updates G::, persists and asks for the matching redraw --
       a colour needs the veil rebuilt, grayscale is view-only. */
    void setMaskOverlayColour(const QColor &c);
    void setMaskOverlayGrayscale(bool on);

    /* Regenerative spot fill. onSpotToolToggled arms/disarms spot-brush mode (from the
       Develop action-row button); onSpotStrokeCommitted takes one finished stroke and
       appends it as a FillSpot to the current stack. */
    void onSpotToolToggled(bool active);
    void onSpotStrokeCommitted(const QString &paramsJson);
    void onSpotRemoveRequested(int index);      // a pin was clicked -> drop that spot
    bool isSpotActive() const { return spotMode; }   // for the action-row spot button
    /* Replace panel preview eye: false renders WITHOUT the spot heals (non-destructive
       bypass, like the Transform eye); the spots stay in the stack. Session-wide. */
    void setSpotsShown(bool shown) { spotsShown = shown; }
    bool isSpotsShown() const { return spotsShown; }

    /* ---- ScopeHeader widget handlers (the scope dropdown + buttons above the tree) ---- */
    void onScopeSelected(const QString &name);    // dropdown picked a different scope
    void renameActiveScope();                     // [R] rename (dialog); Global cannot be renamed
    void resetActiveScope();                      // header reset: scope back to defaults
    /* Menu "Reset all edits": wipe the WHOLE recipe -- every scope and mask, the
       crop/warp geometry and the spot heals -- then clear the history and the sidecar
       record, for EVERY selected image. Destructive and confirmed. */
    void resetAllEdits();
    void newScope();                              // [+] add a scope (name dialog, default "Scope n")
    void deleteScope();                           // [-] remove the selected scope (not Global)
    void showMaskMenu();                          // pop SubmaskDialog (op + type) and begin one
    void onScopePreviewToggled(bool shown);       // [E] show/ignore the whole scope
    void onScopeEnabledToggled(int index, bool on); // the scope bar's show/hide eye
    void setTreeCollapsed(bool collapsed);        // > hide/show this tree (the scope's items)
    void howThisWorks();                          // Develop help
    /* Section help: every band [:] menu ends with "<section> help", opening that
       section's own page (Docs/develop<section>help.html). sectionHelp takes a PV_*
       group; editsHelp / submasksHelp serve the two bands that are not tree sections. */
    void sectionHelp(int group);                  // Basic / Curves / ... section help
    void editsHelp();                             // Edits (scope list) band help
    void submasksHelp();                          // Submasks band help
    void rawHelp();                               // Raw panel help ([?] and its context menu)
    void transformHelp();                         // Transform panel help ([?] and its context menu)

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

    /* ---- Curves panel pointer sample ---------------------------------------------
       The Curves counterpart of the WB dropper: a toggle in the Curves panel that, while
       armed, marks the pixel under the mouse pointer on the tone curve. Unlike the
       dropper it takes nothing and changes nothing -- it is a readout, so it does NOT
       arm a mode in ImageView, does not own the click, and stays armed until it is
       turned off. MW already reports the hovered pixel for the scopes' readout marker
       (MW::onImageCursorPos, from ImageView::cursorImagePos); setCurveSample is fed from
       that same sample, and clearCurveSample from cursorLeftImage.

       wantsCurveSample gates it: armed AND the plot actually on screen (the panel can be
       collapsed or the Edits tree scrolled elsewhere), so a hover costs nothing when
       there is nothing to mark. */
    void toggleCurveSampler();      // the Curves panel's pointer button
    bool isCurveSamplerActive() const { return curveSamplerActive; }
    bool wantsCurveSample() const;
    void setCurveSample(int r, int g, int b);
    void clearCurveSample();

    /* ---- Curves panel point picking (the canvas half of the sampler) ---------------
       While the sampler is armed AND the plot is on screen AND the mode is Point, a
       click on the image adds a control point at the clicked pixel's tone. That makes
       the toggle a Lightroom targeted-adjustment tool rather than a readout, so unlike
       setCurveSample it DOES own the canvas click: curvePickBegin/End arm ImageView's
       pick mode, which shows a selector cursor and suspends the loupe's
       click-to-toggle-zoom for as long as it is armed.

       The three conditions are all live -- the sampler is toggled, the mode combo
       switches Point/Parametric, and the panel collapses or the dock hides -- so
       updateCurvePickState() re-evaluates them and emits only on a CHANGE. It is called
       from every place any of the three can move (see the definition). */
    bool wantsCurvePointPick() const;
    void updateCurvePickState();
    void addCurvePointFromPixel(int r, int g, int b);

    /* ---- Detail panel 1:1 preview -------------------------------------------------
       The square window at the head of the Detail section showing one patch of the image
       at full resolution, so sharpening and noise reduction can be judged without zooming
       the loupe (see Develop/Properties/detailpreview.h for why that is necessary at
       all). The panel owns the sample point and the armed state; MW renders the patch
       (MW::renderDetailRoi) and hands it back through setDetailRoiImage.

       The point is a VIEW setting, not an edit: it is not in EditParams, not in the
       sidecar, and not in a preset. It resets to the image centre on image change. */
    bool detailPreviewWanted() const;      // on screen, so a render is worth doing
    QPointF detailPoint() const { return detailPt; }
    bool detailHasPoint() const { return detailPtSet; }
    void clearDetailPoint();               // back to "not picked yet"
    int  detailRoiSize() const;            // ROI side in image px, 0 when not wanted
    void setDetailRoiImage(const QImage &img);
    void setDetailMessage(const QString &text);
    void setDetailPoint(QPointF n);        // from a loupe pick or a preview drag
    void nudgeDetailPoint(int dx, int dy); // preview drag, in image px
    void cancelDetailPick();               // Esc / image change / another tool
    void toggleDetailPick();               // the target button
    bool isDetailPickActive() const { return detailPickActive; }

    /* MW-driven raw-denoise completion state for the "Denoise"/"Denoised" checkbox:
       checked + "Denoised" when a denoised base is ready for the current image, else
       unchecked + "Denoise". Signal-blocked so it never re-triggers a run. */
    void updateDenoiseRunState(bool denoised);
    /* Push "can the raw denoise run here" into the dock: greys the whole denoise group and
       shows the reason in its place when it cannot (MW::rawDenoiseAvailable). Called on
       every raw sync and whenever MW learns a denoise came back a no-op. */
    void updateDenoiseAvailability();

signals:
    void paramsChanged();           // a develop value changed (decode hook; deferred)
    /* An image's edits were just written to its sidecar, together with a new cached
       thumbnail preview (or, when thumb is null, with the old one cleared because no
       preview could be made for the new recipe). MW updates the grid from this. */
    void devPreviewUpdated(const QString &fPath, const QImage &thumb);
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
       Develop action-row spot tool is toggled. */
    void spotEditBegin();
    void spotEditEnd();
    /* White-balance dropper: arm/disarm ImageView's sample-a-neutral mode. */
    void wbDropperBegin();
    void wbDropperEnd();
    /* Detail 1:1 preview: arm/disarm ImageView's pick-a-location mode, and ask MW to
       re-render the patch (the point moved, or the panel just became visible). */
    void detailPickBegin();
    void detailPickEnd();
    void detailRoiNeeded();
    /* Curves point picking: arm/disarm ImageView's "click the image to add a curve
       point" mode (see wantsCurvePointPick). Unlike the dropper and the Detail picker
       this mode is NOT one-shot -- it stays armed until one of its conditions goes. */
    void curvePickBegin();
    void curvePickEnd();
    /* The user dragged inside the preview: move the sample point by this many image
       pixels. Resolved by MW, which owns the image's orientation. */
    void detailPointNudged(int dx, int dy);
    /* The pre-develop WorkingImage for fPath is needed NOW (a WB dropper sample / Auto
       white balance) and is not in WorkingImageCache. MW builds it -- synchronously for
       a display-referred file, or by starting the async scene-linear decode for raw --
       see ensureWorkingImage(). */
    void workingImageNeeded(const QString &fPath);
    /* Spot tool armed/disarmed: drives the action-row spot button's on/off icon. */
    void spotActiveChanged(bool active);
    /* The current image's spot centres (normalized), for ImageView's on-canvas pins. */
    void spotPinsChanged(const QVector<QPointF> &pins);
    void maskFeatherChanged(double feather);    // Feather slider -> live overlay ramp update
    void maskInvertChanged(bool inverted);      // Invert checkbox -> live overlay flip
    /* Content-range tool params (Luminance lo/hi, Color refine + samples) changed in the dock ->
       ImageView rebuilds its coverage tint. */
    void maskRangeChanged(const QString &paramsJson);
    /* Brush current settings (for the cursor + the next stroke). size/flow 0..100. */
    void maskBrushSettingsChanged(double size, double feather, double flow, bool autoMask);
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
    /* Repaint the view alone: neither the image nor the veil changed, only how the view
       draws them (the overlay grayscale toggle). Cheaper again than a veil rebuild. */
    void maskOverlayRepaintRequested();
    /* The sharpening mask preview turned on or off (Opt pressed/released during a Masking
       drag, or the drag ended) -> MW builds or drops the grayscale mask image. */
    void sharpenMaskPreviewChanged(bool active);

protected:
    /* The Profile dropdown's popup closing. QComboBox reports a SELECTION but not a
       dismissal, and a hover preview left standing after the user pressed Esc would show
       a profile the image does not have. */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void initialize();

    /* Last state sharpenMaskPreviewChanged reported, so syncSharpenMaskPreview only
       emits on an edge (it is called from every focus change in the app). */
    bool sharpenMaskPreviewOn = false;

    /* Build/rebuild the whole tree for the ACTIVE scope: the Basic / Curves / Color /
       Calibrate / Color Grade / Detail / Effects sections. (The raw-decode rows live in
       the RawPanel above the scope bar and the mask in the MaskPanel below it, so neither
       is in the tree.) Called on image change, scope switch and mask add/remove/select;
       section expand-state is preserved across the rebuild. */
    void buildTree();
    void addMaskItems();            // non-Global: keep selectedMaskIndex honest
    void applyScopeItemsCollapsed();// hide/show the scope's top items (not the sections)

    /* ---- Content-height fitting (nested under a scope row) ------------------------
       In fit mode the tree has no scrollbars of its own: its sizeHint is the bottom of
       the last VISIBLE row, so the enclosing QScrollArea scrolls the whole scope block.
       Anything that changes which rows are visible (rebuild, expand/collapse, row hide)
       must schedule a re-fit; the call is deferred + coalesced so it is never made from
       inside a paint, a sizeHint or a mid-rebuild signal. */
    void scheduleContentFit();
    bool fitToContents = false;
    bool fitPending    = false;
    int  fittedHeight  = -1;
    /* Gap between the last row and the block's closing rule (paintEvent), reserved by
       sizeHint in fit mode so the rule always has room. 0 = the rule sits directly on the
       last row, closing the bracket tight against the section it ends. */
    static constexpr int kBlockCloseGap = 0;
    /* Clear space BELOW the closing rule, so the next scope row does not butt onto the
       bracket's foot. Also reserved by sizeHint in fit mode. */
    static constexpr int kBlockBottomGap = 2;
    void addBasic();
    void addCurves();
    void addColor();
    void addCalibrate();
    void addColorGrade();
    void addDetail();
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
    /* The as-shot Kelvin the row was last resolved against, or -1 for "no valid cam yet".
       Keyed on the VALUE rather than a seen/not-seen flag so that a re-characterisation
       of the same image also refreshes -- switching the Demosaic combo clears
       WorkingImageCache and re-decodes, and the two engines need not agree to the kelvin.
       Never changes during a slider drag, so it cannot fight the user's hand. See
       onWorkingImageReady; reset per image in setCurrentImage. */
    float wbRowAsShotK = -1.0f;

    /* Tone mapping -- the view transform, the second row of Basic, directly under the
       camera profile: both say what rendering the sliders below sit on. Always
       written to scope 0: it maps the WHOLE image for display, so it is not a per-mask
       adjustment. Consumed by OutputTransform, not by Develop::Apply. The identifiers
       keep the name `viewTransform` because that is the published sidecar key; only the
       LABEL is "Tone mapping". See EditParams::viewTransform. */
    /* THE CAMERA PROFILE -- the first row of Basic, Lightroom's Profile slot. It
       characterises the SENSOR and feeds white balance, which is why it heads the panel,
       with Tone mapping immediately below it in the same group.
       Always written to scope 0 (a profile describes the camera; one part of a picture
       cannot have been taken by a different one). See EditParams::cameraProfile. */
    void addCameraProfileRow(const QModelIndex &parIdx);
    void setCameraProfile(const QString &name);

    /*
        HOVERING AN ITEM IN THE OPEN PROFILE DROPDOWN PREVIEWS IT ON THE LOUPE.

        A profile is the one Basic control whose effect cannot be guessed from its name --
        "Camera VV2 v2" means nothing until you see it -- and the list is long, so trying
        them one click at a time is the wrong interaction. Uses the SAME preview override
        the Presets list and History use (previewStack / previewActive /
        historyPreviewChanged): the stored stack, the sliders and the history are never
        touched, and the render is proxy-only with no full-res settle.

        DEBOUNCED at the same 120 ms the Presets list uses, so sweeping the cursor down
        the list does not queue one develop render per row -- a look profile costs ~13 ms
        of pixel work per tick on top of the rest of the pipeline.
    */
    void previewCameraProfile(const QString &name);
    void endCameraProfilePreview();
    QPointer<QTimer> cameraProfileHoverTimer;
    QString cameraProfileHoverName;
    /* Whether WE started the live preview, so closing the popup restores only what this
       row put up -- and costs no render when nothing was previewed. */
    bool cameraProfileHoverActive = false;
    static constexpr int kProfileHoverDelayMs = 120;
    void refreshCameraProfileRow();
    /* Fill the combo with the built-in entry plus every profile installed for THIS
       camera, and select the stored one. Separate from refresh because the list itself
       changes -- on an image change (a different camera) and when the background index
       lands. */
    void repopulateCameraProfileCombo();
    void addViewTransformRow(const QModelIndex &parIdx);
    void setViewTransform(int vt);
    void refreshViewTransformRow();
    bool profileSuppliesToneMapping() const;
    static QString viewTransformName(int vt);
    void setWbDropperActive(bool on);
    void setDetailPickActive(bool on);
    /* Double-click reset for the Temp / Tint rows: back to AS SHOT, not to the slider's
       0 default (see mouseDoubleClickEvent). */
    void resetWbAxisToAsShot(bool isTemp);
    /* The current image's colour characterisation, from the cached pre-develop
       WorkingImage. Invalid when the image is not (yet) in the cache, which resolves
       temperatures to a D65 fallback and disables the dropper. */
    CameraColor currentCam() const;
    /* The current image's pre-develop WorkingImage, BUILDING it on demand (via
       workingImageNeeded) when the cache has no entry. Null if it could not be made
       ready in this call -- a raw decode is asynchronous. */
    std::shared_ptr<const WorkingImage> ensureWorkingImage();
    static QIcon dropperIcon(bool armed);   // drawn, not a resource
    /* The Curves pointer toggle's glyph, drawn for the same reasons dropperIcon is: a
       curve with a crosshair sitting on it. */
    static QIcon curveSamplerIcon(bool armed);
    void setCurveSamplerActive(bool on);
    QPointer<QComboBox> wbCombo;
    QPointer<QComboBox> viewTransformCombo;
    /* Shown IN PLACE of the combo when the selected camera profile brings its own tone
       curve: greyed control plus a brief inline reason, never a popup after the fact. It
       has to be a widget rather than a build-time choice because the reason appears and
       disappears with the PROFILE, without the tree being rebuilt. */
    QPointer<QLabel> viewTransformReason;
    QPointer<QComboBox> cameraProfileCombo;
    QPointer<BarBtn> wbDropperBtn;
    bool wbDropperActive = false;
    /* Detail 1:1 preview. detailPt is normalized over the ORIENTED full-res frame (the
       space ImageView::maskViewportToNorm reports and Develop/detailroi.h maps back to
       source pixels). "Not picked yet" is its own FLAG rather than a null QPointF: a
       point of exactly (0,0) is a legitimate pick (the image's top-left corner) and a
       null-point sentinel would silently read it as unset. */
    static QIcon detailTargetIcon(bool armed);   // drawn, like dropperIcon
    QPointer<DetailPreview> detailPreview;
    QPointer<BarBtn> detailPickBtn;
    QPointF detailPt;
    bool detailPtSet = false;
    bool detailPickActive = false;
    void addDetailPreviewRow(QModelIndex parIdx);
    void updateSectionHeaderCaptions();   // section names + the edited " *" marker
    /* Current " *" state of each section header: 1 = starred, 0 = plain, -1 = unknown
       (after a tree rebuild). Indexed BY POSITION in updateSectionHeaderCaptions's hdrs[]
       table, so this array must be at least as long as that table -- grow both together.
       Lets updateSectionHeaderCaptions skip the model search when nothing flipped -- it
       runs on every params change. */
    int hdrStar[7] = {-1, -1, -1, -1, -1, -1, -1};

    /* ---- Calibrate (RGB primaries) ---------------------------------------------------
       Same select-then-adjust grammar as Color Grade: R/G/B checkboxes pick which
       primaries the wheel writes (calActiveMask bits 0x1/0x2/0x4). The wheel is an
       embedded index widget, and each dot is a DELTA from its primary's home angle --
       see primarywheel.h. Recreated on every tree rebuild. */
    QPointer<PrimaryWheel> primaryWheel;
    int  calActiveMask = 0x1;                 // red checked by default
    void onPrimaryWheelChanged(bool commit);  // wheel drag -> active-scope cal params
    void refreshCalibrateRow();               // push stored primaries to wheel + sliders
    void setCalAxis(bool isHue, float v);     // nudge one axis of every checked primary
    int  firstActivePrimary() const;          // lowest checked primary (the sliders' reference)

    /* ---- Curves (tone curve) ----------------------------------------------------------
       An embedded index widget like the wheels, re-created on every tree rebuild. The
       panel is a SECOND VIEW of tone: its Point curves are the only params it owns
       (EditParams::curveN/curveX/curveY), while Parametric mode drives Basic's existing
       Highlights / Shadows / Whites / Blacks. curveToneSlider is a second
       ToneRegionSlider
       shown under the plot in Parametric mode; it edits the SAME tone-split params as the
       one under the histogram scope (see onToneSplitsChanged, which cross-pushes between
       the two so they can never disagree). The scopes strip can be hidden entirely, which
       is why the splits are surfaced in both places rather than only there. */
    QPointer<CurveEditor> curveEditor;
    QPointer<ToneRegionSlider> curveToneSlider;
    QPointer<QComboBox> curveChannelCombo;
    QPointer<QWidget> curveChannelRow;      // hidden in Parametric mode
    /* The row widget holding curveToneSlider, centred in the panel alongside the plot
       (updateEditorGeometries) so the handles stay under the curve's x-axis. */
    QPointer<QWidget> curveSplitsRow;
    /* The pointer-sample toggle (see toggleCurveSampler). The BUTTON is rebuilt with the
       tree, the armed STATE outlives it -- addCurves restores the new button from it. */
    QPointer<BarBtn> curveSamplerBtn;
    bool curveSamplerActive = false;
    /* Last state pushed to ImageView through curvePickBegin/End, so the signals fire
       only on a transition (updateCurvePickState is called from hot paths). */
    bool curvePickArmed = false;
    /* 0 = Parametric, 1 = Point. UI state, not a param -- it is not persisted, so this
       initial value IS what the panel shows the first time Curves is expanded in a
       session; after that it holds whatever the user last picked (it outlives a tree
       rebuild, like curveSamplerActive). POINT is the default: it is the mode with
       controls of its own, and the one the canvas point picker needs. */
    int  curveModeIndex = 1;
    /* The band whose Basic caption was last flashed, so a drag flashes ONCE rather than
       on every mouse-move (which would restart a 450ms animation, and storm the model
       with setData, dozens of times a second). -1 = no drag in progress. */
    int  lastFlashBand = -1;
    void onCurveChanged(bool commit);       // point drag -> active-scope curve params
    void onParametricChanged(int band, double value, bool commit);
    void refreshCurveRow();                 // push stored params + splits into the editor
    void applyCurveMode();                  // show/hide the channel combo + split slider
    static QString curveBandKey(int band);  // band -> the Basic dock slider key it writes

    /* ---- Color Grade (colour grading) ------------------------------------------------
       The wheel is a directly-embedded index widget (setIndexWidget), NOT a delegate
       editor; it edits whichever range(s) the Dark/Mid/Light checkboxes select
       (gradeActiveMask bits: 0x1 shadow, 0x2 mid, 0x4 high). The Luminance slider writes
       the same active range(s). Recreated on every tree rebuild. */
    QPointer<ColorGradeWheel> colorGradeWheel;
    int  gradeActiveMask = 0x2;             // midtones checked by default
    void onGradeWheelChanged(bool commit);  // wheel drag -> active-scope grade params
    void refreshColorGradeRow();            // push stored grade to the wheel + Lum slider
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
    /* HUE PRESET CHIPS. One click seeds the active Color Range mask with a synthetic
       sample at a named band's centre hue plus a matching hue window -- the fast route
       to "the greens" without hunting for a pixel to pipette. This is what Winnow has
       INSTEAD of a Lightroom-style 8-band HSL mixer: the mask already selects by hue
       with more freedom, it only lacked a one-click start. See the Color Grade panel
       notes in Documentation.txt for why the mixer was dropped. */
    void applyColorRangeHuePreset(int band);      // 0..7, see kHueBands
    static int  hueBandCount();
    static QString hueBandName(int band);
    static float   hueBandCentre(int band);       // degrees
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
    void deleteMask(int index);
    void setSelectedMask(int index);           // make a tool active (-1 = none, e.g. Done)
    void onMaskSelectionChanged();             // (programmatic selection only; clicks go via mousePressEvent)
    EditScope *activeScope();                  // active scope of image, or nullptr
    static QString opName(int op);             // "Add" / "Subtract"

    /* ---- Preview (show/ignore) + Reset per group ----------------------------------------------
       Each section header (Basic/Color/Effects) and the Scopes header carry an eye BarBtn that
       toggles that group's Preview flag on the active scope (non-destructive: values are kept, the
       group is folded to identity at render by effectiveScopeParams). Right-clicking a header pops
       a menu to toggle Preview or Reset (restore defaults, destructive) for that group. Transform's
       preview/reset live in TransformPanel (separate widget), wired via MW. Group codes: PV_Scope =
       whole active scope, PV_Basic/PV_Color/PV_ColorGrade/PV_Effects = a section.
       Codes are append-only: hdrStar is indexed by them. */
    enum PreviewGroup { PV_Scope = -1, PV_Basic = 0, PV_Color = 1,
                        PV_ColorGrade = 2, PV_Effects = 3, PV_Calibrate = 4,
                        PV_Curves = 5, PV_Detail = 6 };
    BarBtn *makeEyeBtn(const QString &tooltip, int group);   // queue an eye toggle into `btns`
    /* The section header's trailing menu button (the vertical ellipsis), built right after
       its eye so every header in the dock ends with the same pair -- eye, then menu. It
       pops the SAME Preview / Reset items the header's right-click menu carries, for the
       users who never think to right-click. */
    BarBtn *makeSectionMenuBtn(int group);
    void showSectionMenu(int group);
    void togglePreviewSection(int group);   // flip the flag, refresh icon, re-render (no value change)
    void resetSection(int group);           // restore the group's defaults, repopulate, re-render
    void refreshPreviewButtons();           // sync every eye icon from the active scope's flags
    void showRawDemosaic();                 // Global + expand: reveal raw Core rows
    /* PV_* -> EditParams::Group (Basic for PV_Scope). */
    static EditParams::Group paramsGroup(int group);
    /* PV_* -> "Basic" / "Effects" / ... , for history captions. */
    static QString groupLabel(int group);
    bool *previewFlag(EditScope *l, int group);        // the bool a PV_* code maps to on a scope
    BarBtn *basicEyeBtn = nullptr, *curvesEyeBtn = nullptr,
           *colorEyeBtn = nullptr, *colorGradeEyeBtn = nullptr,
           *calibrateEyeBtn = nullptr, *effectsEyeBtn = nullptr,
           *detailEyeBtn = nullptr;

    void contextMenuEvent(QContextMenuEvent *event) override;   // right-click menu

    /* Which Develop dock section a right-click came from. The menu carries only what that
       section can do (its reset, its help), plus the panel-wide copy/paste/preset items. */
    enum PanelSection { SectionEdits, SectionScopeBar, SectionMask, SectionRaw,
                        SectionTransform, SectionReplace, SectionOther };
    PanelSection sectionAt(QWidget *w);         // walk up from the clicked widget
    /* Build + run the dock's context menu. treeIdx is valid only for SectionEdits (the
       row the click landed on, so a section header can offer its own Reset). */
    void showPanelContextMenu(PanelSection section, const QPoint &globalPos,
                              const QModelIndex &treeIdx = QModelIndex());
    QWidget *panelContainer = nullptr;      // the dock's container, watched for right-clicks
    TransformPanel *transformPanel = nullptr;
    ReplacePanel *replacePanel = nullptr;

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
    /* As brushWith, but ALSO writes the key onto every stroke already painted, so the
       slider changes what is on screen instead of only arming the next stroke. Used when
       a COMMITTED submask is re-opened (see onMaskEditorSetting). */
    static QString brushStrokesWith(const QString &paramsJson, const QString &key,
                                    const QJsonValue &v);
    /* As brushStrokesWith, but only the LAST stroke -- "I just painted that, make it
       softer". Shift + any brush attribute in the panel. The component's own next-stroke
       default is deliberately left alone: this edits history, it does not re-arm. */
    static QString brushLastStrokeWith(const QString &paramsJson, const QString &key,
                                       const QJsonValue &v);
    /* Caption for the MaskPanel's attribute header: what a change to the settings below
       will actually affect, given the tool, whether the submask is still pending, and
       whether Shift is held right now. */
    QString maskAttributeScopeText();
    /* maskBrushSettingsChanged from the current settings. */
    void emitBrushSettings(const MaskComponent &m);
    /* A canvas gesture changed a mask setting: seed the row wherever it is showing --
       the MaskPanel's editor (Lab UI) and/or the main tree (legacy) -- without echoing
       back. */
    void syncMaskSlider(const QString &key, double value);
    EditParams &activeParams();                   // the active scope's params (creates a scope if none)

    /* The per-image edit state. stackCache holds loaded/edited stacks keyed by file path; dirty
       marks those needing a sidecar write; currentImagePath is the image the dock currently
       shows; activeScopeIndex is the selected scope within that image. isPopulating suppresses
       itemChange while we push values into the editors. */
    QHash<QString, EditStack> stackCache;
    QSet<QString> dirty;
    QString currentImagePath;
    DevPreviewProvider devPreviewProvider;   // set by MW; see setDevPreviewProvider
    /* The last (path, recipe) topUpDevPreviews brought up to date, so re-entry is free.
       flushAll runs before EVERY file operation (FileOps::flushPendingEdits), and a
       multi-image paste or ingest would otherwise re-parse the same sidecar once per
       file just to discover there is nothing to do. */
    QString toppedUpPath;
    QByteArray toppedUpKey;
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
    /* One image's share of resetAllEdits: identity stack + history dropped + sidecar
       cleared. No dock work, so it serves the current image and the others alike. */
    void resetImageEdits(const QString &fPath);
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

    int dividerHeight = 5;      // seeded here too: buildTree runs before the ctor sets it
    QColor divColor;


    /* Mask UI state. selectedMaskIndex is the component shown in the shared Mask Tool panel (-1 =
       none). isRebuildingMasks guards the tree-selection handler while we add/remove mask rows.
       Adding one pops SubmaskDialog (op + type) rather than a menu. UR_MaskIndex tags a
       mask row's caption with its component index so selection can find it. */
    int selectedMaskIndex = -1;
    bool isRebuildingMasks = false;

    /* ---- Mask build-up (lab UI): append-only "flatten" via the MaskPanel ----------
       A picked submask is APPENDED to the scope's mask and edited in the panel's embedded
       MaskEditor. EVERY submask (including the first) is PENDING -- pendingIdx = its
       index -- until it SETTLES: kMaskAutoCommitMs after the last edit the commit
       happens by itself and folds it in with the op being previewed. There is no commit
       button, because there was never anything buffered for one to flush -- every edit
       is already written into the component and rendered live, and the only thing commit
       decides is the combine op, which the submask's row can still change afterwards.
       Return commits and closes early, Esc discards while still pending. The overlay
       composites the pending submask with pendingOp, so the veil shows the OUTCOME
       rather than colouring the operand. maskPanelOpen tracks whether the strip is up,
       so Esc cancels the panel edit rather than collapsing a legacy tree tool. */
    MaskPanel *maskPanel = nullptr;
    int  pendingIdx = -1;                     // uncommitted submask's index, else -1
    int  pendingOp = 0;                       // op the overlay + render preview
    int  latchedMaskOp = 0;                   // op the last shaping action set (survives
                                              // the key release; see latchMaskOp)
    /* What the user picked in SubmaskDialog when this submask was added. The RESTING op:
       a held modifier overrides it while shaping, and releasing the modifier comes back
       here rather than to Add, because the dialog is where the op was decided. */
    int  chosenMaskOp = 0;
    /* The pending submask AS BUILT by beginMaskTool, before the user touched it. The
       baseline pendingMaskIsUntouched compares against, so leaving a scope with a tool
       that was picked and then ignored asks nothing. */
    MaskComponent pendingPristine;
    bool maskPanelOpen = false;
    /* Keep the combined-mask (red) overlay showing after a tool is committed/finished
       (the panel closes but the result stays visible + 'O'-toggleable). Cleared on scope/
       image switch or cancel. */
    bool maskLatched = false;
    /* ---- Veil engagement state (see maskVeilEngaged) -------------------------------- */
    bool maskPanelHovered = false;
    bool lastInteractionWasMask = false;
    /* The veil's state as far as engagement is concerned -- what was last pushed at it,
       or what "O" last made it. Only a DISAGREEMENT with maskVeilEngaged() emits, which
       is both what keeps a drag from re-asserting the veil on every tick and what lets
       an "O" override stand until the user's attention actually moves. */
    bool maskVeilEngagedNow = false;
    void syncMaskVeilEngagement();       // emit show/hide iff engagement flipped
    /* Append a new pending submask of this tool, combining with the mask by `op`. The
       op arrives from SubmaskDialog rather than from a held modifier, and is forced to
       Add on an empty mask. It is NOT written into the component here -- the component
       carries Add until the submask lands -- it seeds pendingOp/latchedMaskOp/
       chosenMaskOp, which the veil, the render and the canvas chip preview. */
    void beginMaskTool(int tool, int op = int(MaskOp::Add));
    void onMaskToolChosen(int tool, int op = int(MaskOp::Add));  // -> beginMaskTool
    void cancelMaskTool();                   // [x]/Esc: discard the pending submask
    MaskComponent *editingMaskComp();        // the tool being built, or null

    /* ---- Submask list (lab UI) ------------------------------------------------------
       A committed submask is NOT flattened away: it stays in the scope's ordered
       components and stays editable. These are what the MaskPanel's SubmaskList drives.
       Each follows the deleteMask shape: mutate, fix up selectedMaskIndex, noteEdit,
       buildTree, paramsChanged.

       reopenSubmask makes a committed submask the active one again -- its settings load
       into the panel's editor and buildTree's updateMaskEdit re-arms the on-canvas
       overlay (handles, brush buffers, range swatches). Nothing else is needed because
       every mask mutator already writes through selectedMaskIndex. Clicking the row of
       the submask that is ALREADY open closes it instead (closeSubmaskEditing) -- the
       row is the only affordance the list has, so if it opens it must also close, the
       way every other disclosure in the dock does. One that has not settled yet lands
       first rather than refusing: closing it IS the user saying they are done. */
    void reopenSubmask(int index);
    /* The Submasks section collapsed: close the open submask with it (handles off the
       canvas, settings gone), so expanding is pure disclosure with nothing selected. */
    void onSubmasksCollapsed(bool collapsed);
    /* End the editing session on a COMMITTED submask: deselect it, take its handles off
       the canvas and hide its settings, leaving the submask and the mask untouched. What
       the panel's "Done" button does, and what a click on the open row does. */
    void closeSubmaskEditing();
    void setSubmaskEnabled(int index, bool on);
    void setSubmaskOp(int index, int op);
    void toggleSubmaskInverted(int index);
    void moveSubmask(int from, int to);
    void duplicateSubmask(int index);
    /* Push the active scope's components into the panel's list and set the panel's
       visibility/state. Called at the end of every buildTree, so the list tracks image,
       scope and mask changes without any caller having to remember. */
    void syncMaskPanel();
    /* Caption split for the MaskPanel's embedded editor. The panel starts at the panel
       edge, like the tree, so this is simply the tree's split -- it stays a function so
       the panel and the tree cannot drift apart. */
    int maskPanelCaptionWidth() const;
    /* MaskEditor (panel) change routing -- mirrors the tree's itemChange mask blocks. */
    void onMaskEditorSetting(const QString &key, const QVariant &value);
    /* MaskPanel's MASK-level editor (currently just Edge). Separate from the per-submask
       handler because it must work with NO submask selected -- it edits the scope, not a
       component. */
    void onMaskLevelSetting(const QString &key, const QVariant &value);
    /* Mask band [:] "Reset mask Edge and Halo": both back to 0 on the active scope. The
       submasks' own Edge values are untouched -- this row is the mask-level pair, and
       silently flattening the parts would be a much bigger edit than the menu says. */
    void resetMaskLevel();
    void onMaskEditorWheel(int hueLo, int hueHi, int satLo, int satHi, bool commit);
    bool spotMode = false;              // regenerative spot-fill brushing is armed
    bool spotsShown = true;             // Replace preview eye: heals rendered or bypassed
    QVector<QPointF> spotPinCenters() const;   // current image's spot centres (norm)
    void emitSpotPins();                       // push spotPinCenters() to ImageView
    ScopeHeader *scopeHeader = nullptr;     // scope combo + buttons, band above tree
    static constexpr int UR_MaskIndex = Qt::UserRole + 100;
    QTimer *debounceWriteTimer = nullptr;
    static constexpr int kDebounceWriteMs = 2000;  // flush this long after edits settle (gated)

    /* ---- Pending submask settle timer -----------------------------------------------
       Every mask edit (shape drag, feather, edge, invert, brush settings) restarts this;
       when it expires the pending submask commits itself. Same shape as
       debounceWriteTimer above and MW's developFullResTimer: the work is coalesced to
       the moment the user's hand stops, not asked for with a button. */
    QTimer *maskCommitTimer = nullptr;
    static constexpr int kMaskAutoCommitMs = 2000;   // quiet time before a submask lands
    bool maskStrokeActive = false;           // a brush/object stroke is in progress
    void armMaskAutoCommit();                // (re)start the settle timer for an edit
    void autoCommitPendingMask();            // the timer expired: land it

    /* Whole-panel enable state (Develop menu action). When false the tree is disabled and
       every caption is greyed; buildTree() re-applies it so a rebuild can't un-grey it. */
    bool panelEnabled = true;
    void applyItemsEnabled(bool enabled);   // set UR_isEnabled on every row (recursively)

    /* Raw "Edit source" (Global scope, raw files only): the A/B radio pair lives in the
       RawPanel. currentIsRaw() gates the raw-only controls; onEditSourceChanged() drives
       G::useRaw via MW, and syncEditRaw() pushes MW's state back into the panel. */
    bool currentIsRaw() const;
    void onEditSourceChanged(bool raw);

    /* RawPanel (lab UI) sync + handlers. syncRawPanel pushes the current raw state (edit
       source, engine, denoise run state, Global denoise amounts) into the panel and toggles
       its visibility (raw files only); setGlobalDenoise writes a slider change into the
       Global scope's params. */
    RawPanel *rawPanel = nullptr;
    void syncRawPanel();
    void setGlobalDenoise(bool luma, int value0to100);
    /* The "Denoise" checkbox writes its state into the Global scope's
       EditParams::denoiseRaw (explicit on/off), so the intent is part of the saved recipe
       rather than of the session -- the render, the export and the devPreview builder all
       read it from there. */
    void setDenoiseRawFlag(bool on);
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
