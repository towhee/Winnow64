#ifndef DEVELOPPROPERTIES_H
#define DEVELOPPROPERTIES_H

#include <QtWidgets>
#include "PropertyEditor/propertyeditor.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"

class MW;
class ToneRegionSlider;

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

signals:
    void paramsChanged();           // a develop value changed (decode hook; deferred)
    void centralMsg(QString msg);
    /* Mask editing handshake with ImageView. Begin when a spatial mask tool becomes the active
       (selected) edit target; End when none is selected. ImageView draws the overlay + handles and
       sends geometry back via setActiveMaskParams. */
    void maskEditBegin(int tool, const QString &paramsJson, double feather);
    void maskEditEnd();
    void maskFeatherChanged(double feather);    // Feather slider -> live overlay ramp update

private:
    void initialize();
    void readLayerList();
    void setCurrentLayer(QString name);

    void addCoreHeader();
    void addLayersHeader();
    void addLayerItems();           // Basic + Effects for the current layer
    void addBasic();
    void addColor();
    void addEffects();

    void newLayer();
    void deleteLayer();

    /* ---- Mask (one mask per non-Base layer, built from a list of Add/Subtract tools) ----------
       Self-contained so the whole mask UI can be redesigned by rewriting just these functions and
       the MaskComponent model. The layer's single mask is an ordered list of tools (each Adds or
       Subtracts area); the [M] button beside "Select layer" appends one, each tool is a row with a
       [-] remove button, and clicking a tool reveals its settings (+ a Done button) below the
       list. Spatial editing (drag/paint/AI-select on the image) is a later increment on the
       canvas; rendering does not yet composite the mask. */
    void showMaskMenu();                       // [M] button: pop the Add/Subtract tool-type menu
    void rebuildMaskTools();                   // rebuild the tool rows under the Layers header
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

    /* Item builders. div converts the integer slider amount to a double (eg /100), and
       defaults to identity (0) so an absent value is a no-op edit. */
    void addHeader(const QString &name, const QString &caption, const QString &tooltip);
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
    void updateMaskMenuBtn();                     // show [M] only when a non-Base layer is active
    void updateMaskEdit();                        // emit maskEditBegin/End for the active mask tool
    static QString defaultMaskParams(int tool);   // initial paramsJson geometry for a new tool
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

    /* Mask UI state. selectedMaskIndex is the component shown in the shared Mask Tool panel (-1 =
       none). isRebuildingMasks guards the tree-selection handler while we add/remove mask rows.
       maskMenu is the "+ add mask" type chooser. UR_MaskIndex tags a mask row's caption with its
       component index so selection can find it. */
    int selectedMaskIndex = -1;
    bool isRebuildingMasks = false;
    QMenu *maskMenu = nullptr;
    BarBtn *maskMenuBtn = nullptr;      // [M] add-mask button; hidden on the Base layer (no mask)
    static constexpr int UR_MaskIndex = Qt::UserRole + 100;
    QTimer *debounceWriteTimer = nullptr;
    static constexpr int kDebounceWriteMs = 2000;  // flush this long after edits settle (gated)

    MW *mw;
    QSettings *setting;
    ItemInfo i;

    QModelIndex root;
    QModelIndex layersIdx;
    ComboBoxEditor *layerListEditor = nullptr;
    ToneRegionSlider *toneSlider = nullptr;       // histogram region slider (owned by ScopesView)

    /* Order of root rows in the tree. */
    enum roots { _layers, _basic, _effects };
};

#endif // DEVELOPPROPERTIES_H
