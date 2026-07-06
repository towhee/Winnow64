#ifndef DEVELOPPROPERTIES_H
#define DEVELOPPROPERTIES_H

#include <QtWidgets>
#include "PropertyEditor/propertyeditor.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"

class MW;
class ToneRegionSlider;
class LayerHeader;

/*
    Develop dock property tree (Lightroom-style parametric edits). It mirrors the
    EmbelProperties pattern: a PropertyEditor subclass that builds a tree of section
    headers, sliders, checkboxes and a combo. All values persist to QSettings under
    the "Develop/" branch.

    Layers (header with minus/plus, the same idiom as Embellish templates) let several
    independent adjustment sets be stored. Switching the "Select layer" combo rebuilds
    the Basic / Effects sections from that layer's saved values. Each layer maps onto a
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

    QStringList layerList;
    QString layerName;
    int layerId = 0;

    /* The params the renderer should apply for the current image (the active layer's params).
       Identity when no image is current. */
    EditParams editParams();
    QString diagnostics();

    /* The full layer stack the renderer composites, independent of which layer is active for
       editing: Base (layer 0) params applied globally, then each enabled non-Base layer developed
       from the running accumulator (so overlapping masks COMPOUND) and blended over it by its mask
       (empty mask => global). Captured as plain values so the off-thread full-res render can use it. */
    struct StackRenderJob {
        struct Layer {
            EditParams             params;
            QVector<MaskComponent> masks;       // empty => layer applies globally
            int                    combine = 0; // MaskCombine across this layer's components
        };
        EditParams     base;                    // layer 0 params (applied globally)
        QVector<Layer> layers;                  // non-Base, in order, enabled only
        Geometry       geometry;                // per-image crop/straighten/warp (applied last)
    };
    StackRenderJob stackJob();

    /* Whole-layer mask overlay: true when a mask tool is expanded on a non-Base layer (so MW should
       show the composited layer mask), plus the active layer's ordered mask tools to composite. */
    bool maskOverlayActive() const;
    QVector<MaskComponent> activeLayerMasks() const;

    /* The current image's stored geometry (for loading the crop overlay), and a setter the crop
       tool calls on commit (writes it into the image's EditStack + marks the sidecar dirty). */
    Geometry currentGeometry() const;
    void     setCurrentGeometry(const Geometry &g);

    /* Per-image edit state (Increment 1). The dock now reflects the CURRENT IMAGE's EditStack
       (loaded from / saved to its XMP sidecar) instead of app-global QSettings. */
    void setCurrentImage(const QString &fPath);   // flush previous, load+show this image's stack
    bool currentIsIdentity() const;               // true if the current image has no edits

    /* Bind the histogram's tone-region slider (created with the scopes, owned by MW): connect its
       drags to the active layer's tone-split params and keep a pointer so image switches push the
       saved positions back into it. */
    void bindToneSlider(ToneRegionSlider *slider);
    void flushImage(const QString &fPath);        // write one image's dirty stack to its sidecar
    void flushAll();                              // write all dirty stacks (quit / pre-op)

    /* The layer dropdown (layers + layer actions) and the preview eye live in a gradient header
       widget ABOVE this tree (see LayerHeader). Bind it once; this class drives its combo/eye and
       handles its signals. */
    void bindLayerHeader(LayerHeader *header);

    /* Enable/disable the WHOLE Develop panel so it "looks" disabled, not just the dock
       frame. Greys the property tree (caption text, which the delegate paints from the
       per-item UR_isEnabled role, plus every persistent editor and user interaction) and
       the LayerHeader band above the tree. MW pairs this with developDock->setEnabled()
       so the dock frame and scopes grey out as well. */
    void setPanelEnabled(bool enabled);

    /* Sync the raw "Edit: Raw / Embedded Preview" selector (added under the layer header for raw
       files only) with G::useRaw. Called by MW::toggleUseRaw so the status-bar useRaw button and
       this selector always agree; also toggles the visibility of the Demosaic / Denoise raw rows. */
    void syncEditRaw(bool useRaw);

protected:
    /* PropertyEditor::mousePressEvent does not select rows (it only handles expand/collapse), so we
       toggle the clicked mask tool ourselves (reveal/hide its settings children) and return. */
    void mousePressEvent(QMouseEvent *event) override;

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

    /* ---- LayerHeader widget handlers (the layer dropdown + buttons above the tree) ---- */
    void onLayerSelected(const QString &name);    // dropdown picked a different layer
    void renameActiveLayer();                     // [R] rename (dialog); Base cannot be renamed
    void resetActiveLayer();                      // header reset: restore the whole layer's defaults
    void newLayer();                              // [+] add a layer (name dialog, default "Layer n")
    void deleteLayer();                           // [-] remove the selected layer (not Base)
    void showMaskMenu();                          // pop the Add/Subtract mask-tool menu (on new layer)
    void onLayerPreviewToggled(bool shown);       // [E] show/ignore the whole layer
    void setTreeCollapsed(bool collapsed);        // > hide/show this tree (the layer's items)

signals:
    void paramsChanged();           // a develop value changed (decode hook; deferred)
    void centralMsg(QString msg);
    /* The "Edit: Raw / Embedded Preview" selector was changed; MW drives G::useRaw (toggleUseRaw)
       -- a private slot, so we route through this signal rather than calling it directly. */
    void useRawRequested(bool useRaw);
    /* The "Demosaic" combo selects the RAW decode engine (Apple Core Image vs in-house Winnow).
       MW sets G::decodeRawEngine and re-decodes the current image. */
    void demosaicEngineChanged(bool useApple);
    /* Mask editing handshake with ImageView. Begin when a spatial mask tool becomes the active
       (selected) edit target; End when none is selected. ImageView draws the overlay + handles and
       sends geometry back via setActiveMaskParams. */
    void maskEditBegin(int tool, int op, bool inverted, const QString &paramsJson, double feather);
    void maskEditEnd();
    void maskFeatherChanged(double feather);    // Feather slider -> live overlay ramp update
    void maskInvertChanged(bool inverted);      // Invert checkbox -> live overlay flip
    /* Content-range tool params (Luminance lo/hi, Color refine + samples) changed in the dock ->
       ImageView rebuilds its coverage tint. */
    void maskRangeChanged(const QString &paramsJson);
    /* Brush current settings (for the cursor + the next stroke). size/flow 0..100. */
    void maskBrushSettingsChanged(double size, double feather, double flow, bool autoMask,
                                  const QString &autoMaskMode);
    /* Brush "AI edge (SAM)" was just enabled in the dock -> MW pre-warms the SAM encoder. */
    void maskBrushAiEnabled();

private:
    void initialize();
    void readLayerList();
    void setCurrentLayer(QString name);

    /* Build/rebuild the whole tree for the ACTIVE layer: the layer's top items (Core rows for Base,
       else mask tool rows) followed by the Basic / Color / Effects sections. Called on image change,
       layer switch and mask add/remove/select; section expand-state is preserved across the rebuild. */
    void buildTree();
    void addCoreItems();            // Base only: Demosaic + Denoise rows at the top of the tree
    void addMaskItems();            // non-Base: the layer's mask tool rows at the top of the tree
    void addAddMaskRow();           // non-Base with no mask: an "Add mask" [+] placeholder row
    void applyLayerItemsCollapsed();// hide/show just the layer's top items (not the sections)
    void addBasic();
    void addColor();
    void addEffects();
    void updateSectionHeaderCaptions();   // append the active layer name to Basic/Color/Effects

    /* Section (Basic/Color/Effects) expand state persists across sessions in QSettings.
       sectionExpanded reads the saved state (with a first-run default); persistSectionExpanded
       writes it when the user toggles a section header. */
    bool sectionExpanded(const QString &name, bool def) const;
    void persistSectionExpanded(const QModelIndex &idx, bool expanded);

    /* ---- Mask (one mask per non-Base layer, built from a list of Add/Subtract tools) ----------
       Self-contained so the whole mask UI can be redesigned by rewriting just these functions and
       the MaskComponent model. The layer's single mask is an ordered list of tools (each Adds or
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
    EditLayer *activeLayer();                  // active layer of the current image, or nullptr
    static QString maskToolName(int tool);
    static int maskToolFromName(const QString &name);
    static QString opName(int op);             // "Add" / "Subtract"

    /* ---- Preview (show/ignore) + Reset per group ----------------------------------------------
       Each section header (Basic/Color/Effects) and the Layers header carry an eye BarBtn that
       toggles that group's Preview flag on the active layer (non-destructive: values are kept, the
       group is folded to identity at render by effectiveLayerParams). Right-clicking a header pops
       a menu to toggle Preview or Reset (restore defaults, destructive) for that group. Transform's
       preview/reset live in TransformPanel (separate widget), wired via MW. Group codes: PV_Layer =
       whole active layer, PV_Basic/PV_Color/PV_Effects = a section. */
    enum PreviewGroup { PV_Layer = -1, PV_Basic = 0, PV_Color = 1, PV_Effects = 2 };
    BarBtn *makeEyeBtn(const QString &tooltip, int group);   // queue an eye toggle into `btns`
    void togglePreviewSection(int group);   // flip the flag, refresh icon, re-render (no value change)
    void resetSection(int group);           // restore the group's defaults, repopulate, re-render
    void refreshPreviewButtons();           // sync every eye icon from the active layer's flags
    static EditParams::Group paramsGroup(int group);   // PV_* -> EditParams::Group (Basic for PV_Layer)
    bool *previewFlag(EditLayer *l, int group);        // the bool a PV_* code maps to on a layer
    BarBtn *basicEyeBtn = nullptr,
           *colorEyeBtn = nullptr, *effectsEyeBtn = nullptr;

    void contextMenuEvent(QContextMenuEvent *event) override;   // header right-click: Preview / Reset

    /* Item builders. div converts the integer slider amount to a double (eg /100), and
       defaults to identity (0) so an absent value is a no-op edit. */
    void addHeader(const QString &name, const QString &parent,
                   const QString &caption, const QString &tooltip, int previewGroup = -1);
    void addSlider(const QString &key, const QString &caption, const QString &tooltip,
                   QModelIndex parIdx, const QString &parentName,
                   int min, int max, int div, QString color, QString color1,
                   double defaultValue = 0);
    void addCheckbox(const QString &key, const QString &caption, const QString &tooltip,
                     QModelIndex parIdx, const QString &parentName, bool defaultValue = false);

    QString layerRootPath() const;  // "Develop/Layers/<layerName>/" (legacy QSettings; unused now)
    double layerValue(const QString &key, double defaultValue = 0) const;
    QString uniqueLayerName(const QString &name) const;   // unique within the current image's layers

    /* Per-image stack helpers. The Layers combo + (+/-) act on the CURRENT IMAGE's EditStack;
       activeLayerIndex is the layer the dock edits and the renderer shows (no mask/opacity
       compositing yet). */
    void populateSlidersFromStack();              // push the active layer's params into the editors
    void setSliderReal(const QString &key, double real);   // set a slider's displayed value (un-scaled)
    void setCheckboxValue(const QString &key, bool on);
    /* A tone-region slider drag: write the three split positions into the active layer's params
       and drive the live preview (no-op while populating). */
    void onToneSplitsChanged(double shadow, double crossover, double highlight);
    static void applyKeyToParams(const QString &key, const QVariant &v, EditParams &p);
    QStringList currentLayerNames() const;        // names of the current image's layers (>=1)
    void refreshLayerCombo();                     // rebuild the combo's list/value from the stack
    void updateMaskMenuBtn();                     // tell the header whether Base is active (per-layer actions)
    void updateMaskEdit();                        // emit maskEditBegin/End for the active mask tool
    static QString defaultMaskParams(int tool);   // initial paramsJson geometry for a new tool
    int  activeMaskTool() const;                  // active component's tool, or -1
    /* Brush current-settings accessors over paramsJson (size/flow are 0..100, autoMask bool). */
    static double  brushNum(const QString &paramsJson, const QString &key, double def);
    static bool    brushBool(const QString &paramsJson, const QString &key, bool def);
    static QString brushStr(const QString &paramsJson, const QString &key, const QString &def);
    static QString brushWith(const QString &paramsJson, const QString &key, const QJsonValue &v);
    void emitBrushSettings(const MaskComponent &m); // maskBrushSettingsChanged from current settings
    EditParams &activeParams();                   // the active layer's params (creates a layer if none)

    /* The per-image edit state. stackCache holds loaded/edited stacks keyed by file path; dirty
       marks those needing a sidecar write; currentImagePath is the image the dock currently
       shows; activeLayerIndex is the selected layer within that image. isPopulating suppresses
       itemChange while we push values into the editors. */
    QHash<QString, EditStack> stackCache;
    QSet<QString> dirty;
    QString currentImagePath;
    int activeLayerIndex = 0;
    bool isPopulating = false;
    bool layerItemsCollapsed = false;   // the '>' header arrow: hide the layer's top items

    /* Mask UI state. selectedMaskIndex is the component shown in the shared Mask Tool panel (-1 =
       none). isRebuildingMasks guards the tree-selection handler while we add/remove mask rows.
       maskMenu is the "+ add mask" type chooser (popped by the header's [M]). UR_MaskIndex tags a
       mask row's caption with its component index so selection can find it. */
    int selectedMaskIndex = -1;
    bool isRebuildingMasks = false;
    QMenu *maskMenu = nullptr;
    LayerHeader *layerHeader = nullptr; // layer dropdown + buttons, in a gradient band above the tree
    static constexpr int UR_MaskIndex = Qt::UserRole + 100;
    QTimer *debounceWriteTimer = nullptr;
    static constexpr int kDebounceWriteMs = 2000;  // flush this long after edits settle (gated)

    /* Whole-panel enable state (Develop menu action). When false the tree is disabled and
       every caption is greyed; buildTree() re-applies it so a rebuild can't un-grey it. */
    bool panelEnabled = true;
    void applyItemsEnabled(bool enabled);   // set UR_isEnabled on every row (recursively)

    /* Raw "Edit source" selector (Base layer, raw files only). editRawRadio is the "Raw" button
       of the A/B pair; the widget lives in the Edit row's value cell (recreated each buildTree).
       currentIsRaw() gates the raw-only rows; onEditSourceChanged() drives G::useRaw via MW;
       applyCoreVisibility() shows/hides the Demosaic + Denoise raw rows per G::useRaw. */
    bool currentIsRaw() const;
    void onEditSourceChanged(bool raw);
    void applyCoreVisibility();
    QPointer<QRadioButton> editRawRadio;

    MW *mw;
    QSettings *setting;
    ItemInfo i;

    QModelIndex root;
    ToneRegionSlider *toneSlider = nullptr;       // histogram region slider (owned by ScopesView)
};

#endif // DEVELOPPROPERTIES_H
