#include "Develop/Properties/developproperties.h"
#include "Develop/Properties/layerheader.h"
#include "Develop/fillspot.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include "Develop/Scopes/toneregionslider.h"
#include <functional>
/*
    See developproperties.h for an overview. Construction mirrors EmbelProperties:
    initialize tree behaviour, read the saved layer list, add the persistent Layers
    header (tree row 0), then build the Basic / Effects sections for the current layer.
*/

DevelopProperties::DevelopProperties(QWidget *parent, QSettings *setting) : PropertyEditor(parent)
{
    if (G::isLogger) G::log("DevelopProperties::DevelopProperties");
    mw = qobject_cast<MW*>(parent);
    this->setting = setting;

    initialize();
    /* The add-mask type chooser, popped automatically when a new layer is created (newLayer). */
    maskMenu = new QMenu(this);
    /* The Layers combo (now in the LayerHeader widget) lists the CURRENT IMAGE's layers (per-image
       EditStack), not app-global QSettings presets. Seed one name so it is valid before any image. */
    layerList = QStringList() << "Base";
    layerName = "Base";
    buildTree();        // active layer's top items (Core for Base) + Basic / Color / Effects

    updateHiddenRows(QModelIndex());
    setMouseTracking(true);

    dividerHeight = 5;
    const int c = G::backgroundShade + 20;
    divColor = QColor(c,c,c);

    /* Optional debounce write: flush the in-memory stack to the sidecar a short time after edits
       settle, so a crash loses less. Gated by G::isDevelopDebounceWrite; navigate-away / quit /
       pre-op flushes always run regardless. */
    debounceWriteTimer = new QTimer(this);
    debounceWriteTimer->setSingleShot(true);
    connect(debounceWriteTimer, &QTimer::timeout, this, [this]{ flushImage(currentImagePath); });

    /* Persist the Basic/Color/Effects section expand-state across sessions (QSettings). Fires for
       every expand/collapse; persistSectionExpanded filters to the three section headers. */
    connect(this, &QTreeView::expanded,  this, [this](const QModelIndex &idx){
        persistSectionExpanded(idx, true);
        onSectionExpanded(idx);                 // Solo: fold the Layer row in
    });
    connect(this, &QTreeView::collapsed, this, [this](const QModelIndex &idx){ persistSectionExpanded(idx, false); });

    /* Programmatic row selection reveals that mask tool's settings (clicks go via mousePressEvent,
       since the base PropertyEditor does not select on click). */
    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &, const QModelIndex &){ onMaskSelectionChanged(); });
}

void DevelopProperties::initialize()
{
    if (G::isLogger) G::log("DevelopProperties::initialize");

    setSolo(setting->value("Develop/isSolo", false).toBool());
    setIndentation(10);
    setAlternatingRowColors(false);
    setMouseTracking(false);
    setHeaderHidden(true);
    ignoreFontSizeChangeSignals = false;

    /* Column widths. The dock minimum width is set in MW::createDevelopDock so the
       header - and + buttons are always visible. */
    stringToFitCaptions = "=captions column=";
    stringToFitValues   = "======values column======";
    resizeColumns();

    root = model->invisibleRootItem()->index();
}

/* ----------------------------------------------------------------------------------------
   Layers
   ---------------------------------------------------------------------------------------- */

QString DevelopProperties::layerRootPath() const
{
    return "Develop/Layers/" + layerName + "/";
}

double DevelopProperties::layerValue(const QString &key, double defaultValue) const
{
    QString path = layerRootPath() + key;
    if (setting->contains(path)) return setting->value(path).toDouble();
    return defaultValue;
}

QString DevelopProperties::uniqueLayerName(const QString &name) const
{
    const QStringList names = currentLayerNames();
    QString unique = name;
    int n = 1;
    while (names.contains(unique)) {
        unique = name + " " + QString::number(++n);
    }
    return unique;
}

void DevelopProperties::readLayerList()
{
    if (G::isLogger) G::log("DevelopProperties::readLayerList");
    layerList.clear();

    setting->beginGroup("Develop/Layers");
    layerList = setting->childGroups();
    QString current = setting->value("current").toString();
    setting->endGroup();

    /* Always have at least one layer. */
    if (layerList.isEmpty()) {
        layerName = "Base";
        layerList << layerName;
        setting->setValue("Develop/Layers/current", layerName);
        setting->setValue(layerRootPath() + "created", true);
    }
    else {
        layerName = layerList.contains(current) ? current : layerList.first();
    }
    layerId = layerList.indexOf(layerName);
}

void DevelopProperties::setCurrentLayer(QString name)
{
    if (G::isLogger) G::log("DevelopProperties::setCurrentLayer", name);
    if (name.isEmpty() || !layerList.contains(name)) return;
    layerName = name;
    layerId = layerList.indexOf(name);
    setting->setValue("Develop/Layers/current", layerName);
}

void DevelopProperties::buildTree()
{
    if (G::isLogger) G::log("DevelopProperties::buildTree");

    /* Preserve which sections are expanded across the rebuild. On the first build no header row
       exists yet, so the default comes from the session-persistent QSettings state (first-ever
       run => Basic open only). */
    auto expandedOr = [this](const QString &name, bool def)->bool {
        const QModelIndex idx = findCaptionIndex(name);
        return idx.isValid() ? isExpanded(idx) : def;
    };
    const bool exBasic   = expandedOr("BasicHeader",   sectionExpanded("BasicHeader",   true));
    const bool exColor   = expandedOr("ColorHeader",   sectionExpanded("ColorHeader",   false));
    const bool exEffects = expandedOr("EffectsHeader", sectionExpanded("EffectsHeader", false));
    /* The Demosaic row (Base, raw) also expands/collapses to hide its raw-denoise
       children; default expanded on first run. */
    const bool exDemosaic = expandedOr("demosaic",     sectionExpanded("demosaic",      true));

    isPopulating = true;
    isRebuildingMasks = true;

    /* Delete every row's editor widgets (close), then drop the rows, and rebuild for the ACTIVE
       layer: its top items first, then the adjustment sections. close() before removeRows so the
       setIndexWidget editors are freed rather than leaked on each rebuild. */
    btns.clear();
    basicEyeBtn = colorEyeBtn = effectsEyeBtn = nullptr;
    if (model->rowCount() > 0) {
        close(QModelIndex());
        model->removeRows(0, model->rowCount());
    }
    /* removeRows deletes the items but leaves sourceIdx (key -> value index) holding
       stale entries; addItem repopulates it for every row it re-adds, but a key NOT
       re-added this pass (e.g. denoiseLuma/denoiseChroma when the engine is Apple) keeps
       a dangling index that isValid() still returns true for -- setSliderReal would then
       crash on data(). Clear it so only rows present after the rebuild have entries. */
    sourceIdx.clear();

    if (activeLayerIndex == 0) addCoreItems();     // Base: Demosaic + Denoise at the top
    else                        addMaskItems();     // else: this layer's mask tool rows at the top
    addBasic();
    addColor();
    addEffects();
    updateSectionHeaderCaptions();

    isPopulating = false;
    isRebuildingMasks = false;

    /* Restore section expand-state (leaves the selected mask tool's own expansion, set in
       addToolRow, untouched). */
    auto setExpandOn = [this](const QString &name, bool on){
        const QModelIndex idx = findCaptionIndex(name);
        if (idx.isValid()) setExpanded(idx, on);
    };
    setExpandOn("BasicHeader",   exBasic);
    setExpandOn("ColorHeader",   exColor);
    setExpandOn("EffectsHeader", exEffects);
    setExpandOn("demosaic",      exDemosaic);

    populateSlidersFromStack();
    refreshPreviewButtons();
    updateMaskEdit();
    updateHiddenRows(QModelIndex());
    applyLayerItemsCollapsed();      // re-assert the '>' collapse (a rebuild resets row visibility)
    applyCoreVisibility();           // hide Demosaic/Denoise raw when editing preview (after collapse)
    if (!panelEnabled) applyItemsEnabled(false);   // keep captions greyed if the panel is disabled
}

bool DevelopProperties::sectionExpanded(const QString &name, bool def) const
{
    return setting->value("Develop/SectionExpanded/" + name, def).toBool();
}

void DevelopProperties::persistSectionExpanded(const QModelIndex &idx, bool expanded)
{
    /* The three adjustment section headers and the Demosaic row persist (mask tool
       rows also expand/collapse, but their state is transient). */
    const QString name = idx.data(UR_Name).toString();
    if (name == "BasicHeader" || name == "ColorHeader" || name == "EffectsHeader" ||
        name == "demosaic")
        setting->setValue("Develop/SectionExpanded/" + name, expanded);
}

void DevelopProperties::addCoreItems()
{
    if (G::isLogger) G::log("DevelopProperties::addCoreItems");
    /* Base layer only. These rows apply to raw sensor data, so they are shown for RAW files only:
       a JPG/TIFF/PNG is already the developable image (no demosaic, no raw denoise, no source
       choice). Non-raw Base layers get no Core rows. */
    if (!currentIsRaw()) return;

    /* Edit source selector (directly under the layer header): choose whether Develop edits the raw
       sensor data or the embedded preview jpg. An A/B radio pair kept in sync with G::useRaw and
       the status-bar useRaw button. "Edit:" is left-aligned (isIndent = false); the radios are
       hosted in the value cell (DT_None = no built-in editor, so the cell is ours). */
    clearItemInfo(i);
    i.name = "editSource";
    i.parentName = "";              // root row (no section header)
    i.captionText = "Edit:";
    i.tooltip = "Edit the raw sensor data (demosaic) or the embedded preview jpg.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = "editSource";
    i.delegateType = DT_None;
    addItem(i);

    const QModelIndex editValIdx = findValueIndex("editSource");
    if (editValIdx.isValid()) {
        /* Recreated on every rebuild; the previous widget is freed when removeRows() drops the row
           (setIndexWidget gives the view ownership of the index widget). */
        QWidget *w = new QWidget;
        w->setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(w);
        hb->setContentsMargins(0, 0, 0, 0);
        hb->setSpacing(12);
        QRadioButton *rawBtn  = new QRadioButton("Raw", w);
        QRadioButton *prevBtn = new QRadioButton("Embedded Preview", w);
        QButtonGroup *grp = new QButtonGroup(w);
        grp->setExclusive(true);
        grp->addButton(rawBtn);
        grp->addButton(prevBtn);
        rawBtn->setChecked(G::useRaw);
        prevBtn->setChecked(!G::useRaw);
        hb->addWidget(rawBtn);
        hb->addWidget(prevBtn);
        hb->addStretch(1);
        editRawRadio = rawBtn;      // QPointer: cleared automatically when the row/widget is freed
        connect(rawBtn, &QRadioButton::toggled, this, &DevelopProperties::onEditSourceChanged);
        setIndexWidget(editValIdx, w);
    }

    /* Demosaic engine + raw noise reduction. Indented (isIndent = true) so they align with the
       Basic-section sliders (e.g. "Temp"). Visible only when editing raw (applyCoreVisibility). */
    clearItemInfo(i);
    i.name = "demosaic";
    i.parentName = "";
    i.captionText = "Demosaic";
    i.tooltip = "Select demosaic engine.";
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    /* Reflect the sticky engine (G::decodeRawEngine) rather than a hardcoded default,
       so a tree rebuild on folder/image selection shows the engine actually in effect. */
    i.value = (G::decodeRawEngine == G::DecodeRawEngine::appleDecodeRawEngine) ? "Apple" : "Winnow";
    i.key = "demosaic";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    i.dropList << "Apple" << "Winnow";
    addItem(i);
    const QModelIndex demosaicIdx = capIdx;
    /* No UR_ExtraIndent here: as the raw-denoise group parent the Demosaic row aligns
       with the section headers (Basic/Color/Effects), and its expand arrow sits in the
       gutter over the native indicator (an ExtraIndent offset would push the delegate
       arrow off the native one, showing two). Its denoise children indent one tree
       level, lining up with the sliders. */

    /* "Denoise raw": Base-layer, decode-time raw noise reduction (denoiseLuma/
       denoiseChroma), baked into the pre-develop WorkingImage (global, not maskable) --
       distinct from the Effects "Denoise" (localDenoiseLuma). Two Lightroom-style 0..100
       sliders mapped to 0..1: Luminance = the master AI-denoise amount; Color = extra
       chroma-noise suppression (default 100).

       These are CHILDREN of the Demosaic row so it gains an expand/collapse arrow that
       hides them -- but only on the WINNOW engine (PMRID needs the CFA mosaic). On
       Apple the raw denoise is inert, so add no children: the row then has no arrow. */
    if (G::decodeRawEngine != G::DecodeRawEngine::appleDecodeRawEngine) {
        addSlider("denoiseLuma", "Denoise Lum",
                  "Raw luminance noise reduction (AI, whole image).",
                  demosaicIdx, "", 0, 100, 0, G::darkgray, G::lightgray, 0);
        addSlider("denoiseChroma", "Denoise Color", "Raw colour (chroma) noise reduction.",
                  demosaicIdx, "", 0, 100, 0, G::darkgray, G::lightgray, 100);
        /* Draw the expand/collapse arrow on the Demosaic row. The delegate decorates only
           rows flagged UR_isDecoration (normally headers); Demosaic is a value row, so we
           flag it now it has children. Apple: no children, no flag -> no arrow. */
        model->setData(demosaicIdx, true, UR_isDecoration);
    }
    /* Visibility (per G::useRaw + collapse) applied by buildTree() after
       applyLayerItemsCollapsed. */
}

bool DevelopProperties::currentIsRaw() const
{
    return mw && mw->isFileRaw(currentImagePath);
}

void DevelopProperties::onEditSourceChanged(bool raw)
{
    if (G::isLogger) G::log("DevelopProperties::onEditSourceChanged");
    /* rawBtn->toggled fires for both halves of the exclusive pair; either way `raw` is the new
       state of the "Raw" button. Only ask MW to switch when it actually differs from G::useRaw
       (syncEditRaw sets the button with signals blocked, so this is also the loop guard). */
    if (raw == G::useRaw) return;
    emit useRawRequested(raw);
}

void DevelopProperties::syncEditRaw(bool useRaw)
{
    if (G::isLogger) G::log("DevelopProperties::syncEditRaw");
    if (editRawRadio) {
        /* An exclusive QButtonGroup auto-unchecks the siblings when a button is CHECKED, but does
           NOT auto-check a sibling when the checked button is unchecked. So editRawRadio->setChecked
           (false) would leave both "Raw" and "Embedded Preview" blank. Always check the button that
           should be selected: "Raw" when useRaw, otherwise its group sibling ("Embedded Preview").
           editRawRadio is the only button connected to a slot, so blocking it also guards re-entry. */
        QSignalBlocker block(editRawRadio);
        QAbstractButton *target = editRawRadio;
        if (!useRaw) {
            if (QButtonGroup *grp = editRawRadio->group()) {
                for (QAbstractButton *b : grp->buttons()) {
                    if (b != editRawRadio) { target = b; break; }
                }
            }
        }
        target->setChecked(true);
    }
    applyCoreVisibility();
}

void DevelopProperties::applyCoreVisibility()
{
    /* The Demosaic row (and its raw-denoise children on the Winnow engine) is visible
       only when editing raw AND the layer items are not collapsed ('>'). Scan root rows
       for "demosaic" -- hiding the parent hides the whole subtree -- and run AFTER
       applyLayerItemsCollapsed(), which would otherwise re-show it on a non-collapsed
       rebuild. On the Apple engine the row has no children (addCoreItems), so there is
       nothing extra to hide here. */
    const bool hide = !G::useRaw || layerItemsCollapsed;
    for (int r = 0; r < model->rowCount(); ++r) {
        const QModelIndex cap = model->index(r, CapColumn);
        if (cap.data(UR_Name).toString() != "demosaic") continue;
        model->setData(cap, hide, UR_isHidden);   // remembered for updateHiddenRows()
        setRowHidden(r, QModelIndex(), hide);
        /* Keep the denoise children's UR_isHidden in step with the parent so a later
           updateHiddenRows() can't force-show them; when the parent is shown they still
           obey the expand/collapse arrow (a collapsed parent hides them regardless). */
        for (int c = 0; c < model->rowCount(cap); ++c)
            model->setData(model->index(c, CapColumn, cap), hide, UR_isHidden);
        break;
    }
}

void DevelopProperties::addMaskItems()
{
    if (G::isLogger) G::log("DevelopProperties::addMaskItems");
    /* Non-Base layers: the layer's ordered Add/Subtract mask tools as rows at the top of the tree.
       The selected tool is expanded with its settings (addToolRow). */
    EditLayer *layer = activeLayer();
    if (!layer || activeLayerIndex == 0) { selectedMaskIndex = -1; return; }
    const int n = layer->masks.size();
    /* A layer with no mask yet shows a single "Add mask" placeholder row with a [+] button (the layer
       otherwise has no top rows). It vanishes as soon as a tool is added -- the layer then shows its
       tool rows, and adding a tool selects it (see the "hidden when a mask is selected" invariant). */
    if (n == 0) {
        selectedMaskIndex = -1;
        addAddMaskRow();
        return;
    }
    if (selectedMaskIndex >= n) selectedMaskIndex = n - 1;
    for (int m = 0; m < n; ++m)
        addToolRow(QModelIndex(), m, layer->masks[m], m == selectedMaskIndex);
}

void DevelopProperties::addAddMaskRow()
{
    /* Placeholder row shown only while the active (non-Base) layer has no mask: a header-style,
       full-width single-line "Add mask" caption carrying just a [+] glyph (UR_AddBtn, no [-] and no
       expand arrow). Clicking anywhere on the row pops the Add/Subtract chooser (showMaskMenu). It is
       identified in mousePressEvent by its name ("addMask"), as it has no UR_MaskIndex. */
    clearItemInfo(i);
    i.name = "addMask";
    i.parIdx = QModelIndex();
    i.captionText = "Add mask";
    i.tooltip = "This layer has no mask (it applies globally). Click [+] to add a mask tool.";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = false;
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex rowIdx = capIdx;
    model->setData(rowIdx, true, UR_LeafSingleLine);
    model->setData(rowIdx, true, UR_AddBtn);            // [+] add the first mask tool
    setFirstColumnSpanned(rowIdx.row(), QModelIndex(), true);
}

void DevelopProperties::bindLayerHeader(LayerHeader *header)
{
    if (G::isLogger) G::log("DevelopProperties::bindLayerHeader");
    layerHeader = header;
    if (!layerHeader) return;

    connect(layerHeader, &LayerHeader::layerSelected,       this, &DevelopProperties::onLayerSelected);
    connect(layerHeader, &LayerHeader::renameRequested,     this, &DevelopProperties::renameActiveLayer);
    connect(layerHeader, &LayerHeader::resetLayerRequested, this, &DevelopProperties::resetActiveLayer);
    connect(layerHeader, &LayerHeader::removeLayerRequested,this, &DevelopProperties::deleteLayer);
    connect(layerHeader, &LayerHeader::addLayerRequested,   this, &DevelopProperties::newLayer);
    connect(layerHeader, &LayerHeader::addMaskRequested,    this, &DevelopProperties::showMaskMenu);
    connect(layerHeader, &LayerHeader::previewToggled,      this, &DevelopProperties::onLayerPreviewToggled);
    connect(layerHeader, &LayerHeader::collapseToggled,     this, &DevelopProperties::setTreeCollapsed);

    /* Seed the dropdown + eye from the current stack. */
    refreshLayerCombo();
    refreshPreviewButtons();
}

void DevelopProperties::setPanelEnabled(bool enabled)
{
    if (G::isLogger) G::log("DevelopProperties::setPanelEnabled");
    panelEnabled = enabled;

    /* The tree view: blocks interaction and (because the sliders/checkboxes/combos are
       persistent editors parented under the viewport) inherits a disabled look to every
       control without clobbering their individual enabled flags. */
    setEnabled(enabled);

    /* The LayerHeader band (layer dropdown + buttons + eye) sits ABOVE the tree and is a
       sibling widget, so it must be greyed explicitly. */
    if (layerHeader) layerHeader->setEnabled(enabled);

    applyItemsEnabled(enabled);
}

void DevelopProperties::applyItemsEnabled(bool enabled)
{
    /* Caption text is painted by PropertyDelegate from the per-item UR_isEnabled role, NOT
       the widget's enabled state, so walk every row (recursively) and set the flag or the
       captions stay black over the greyed controls. buildTree() calls this too, because a
       rebuild recreates rows with the default (enabled) flag and would otherwise un-grey a
       panel that is meant to stay disabled. */
    std::function<void(const QModelIndex &)> setRows = [&](const QModelIndex &parent) {
        const int rows = model->rowCount(parent);
        for (int r = 0; r < rows; ++r) {
            QModelIndex capIdx = model->index(r, CapColumn, parent);
            model->setData(capIdx, enabled, UR_isEnabled);
            setRows(capIdx);            // children hang off the caption (column 0) index
        }
        if (rows)
            emit model->dataChanged(model->index(0, CapColumn, parent),
                                    model->index(rows - 1, CapColumn, parent));
    };
    setRows(QModelIndex());
    viewport()->update();
}

void DevelopProperties::onLayerSelected(const QString &name)
{
    if (G::isLogger) G::log("DevelopProperties::onLayerSelected", name);
    if (currentImagePath.isEmpty()) return;
    const int idx = currentLayerNames().indexOf(name);
    if (idx < 0 || idx == activeLayerIndex) return;
    activeLayerIndex = idx;

    /* Selecting a layer reveals ITS OWN section (the mask-tool rows) and shows its mask
       straight away -- select the first mask tool so its coverage overlays the image. The
       Basic/Color/Effects sections keep whatever expand state they had. Base (index 0)
       applies globally, so it has no mask to show. */
    EditLayer *l = activeLayer();
    selectedMaskIndex = (idx > 0 && l && !l->masks.isEmpty()) ? 0 : -1;

    refreshLayerCombo();          // move checkmark + re-caption actions to new layer
    buildTree();                  // swap top items (Core/masks) + repopulate sections

    /* Un-collapse the layer's own items (its mask rows). Bulk-guarded so setTreeCollapsed
       does not fold the adjustment sections under Solo -- those stay as the user left
       them. */
    isBulkExpandCollapse = true;
    setTreeCollapsed(false);
    isBulkExpandCollapse = false;
    if (layerHeader) layerHeader->setCollapsed(false);

    emit paramsChanged();         // the renderer shows the active layer
}

void DevelopProperties::onSpotToolToggled(bool active)
{
    if (G::isLogger) G::log("DevelopProperties::onSpotToolToggled");
    if (spotMode == active) return;
    spotMode = active;
    emit spotActiveChanged(active);            // update the title-bar spot button icon
    if (active) { emit spotEditBegin(); emitSpotPins(); }   // arm + seed the pins
    else        emit spotEditEnd();
}

void DevelopProperties::onSpotStrokeCommitted(const QString &paramsJson)
{
    if (G::isLogger) G::log("DevelopProperties::onSpotStrokeCommitted");
    if (currentImagePath.isEmpty() || paramsJson.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    FillSpot fs;
    fs.paramsJson = paramsJson;
    s.spots.append(fs);
    dirty.insert(currentImagePath);
    emitSpotPins();
    emit paramsChanged();                   // re-render -> MiganFill heals the new spot
}

void DevelopProperties::onSpotRemoveRequested(int index)
{
    if (G::isLogger) G::log("DevelopProperties::onSpotRemoveRequested");
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (index < 0 || index >= s.spots.size()) return;
    s.spots.remove(index);
    dirty.insert(currentImagePath);
    emitSpotPins();
    emit paramsChanged();                   // re-render without the removed spot
}

/* Centre (centroid of the stroke points) of each spot, in output-normalized coords, for
   the pins. Order matches EditStack::spots so a pin index maps to a spot. */
QVector<QPointF> DevelopProperties::spotPinCenters() const
{
    QVector<QPointF> centers;
    if (currentImagePath.isEmpty()) return centers;
    const EditStack s = stackCache.value(currentImagePath);
    for (const FillSpot &sp : s.spots) {
        const FillSpotGeom::Parsed p = FillSpotGeom::parse(sp.paramsJson);
        if (!p.valid()) { centers.append(QPointF(0.5, 0.5)); continue; }
        double sx = 0, sy = 0; const int n = int(p.pts.size() / 2);
        for (int i = 0; i < n; ++i) { sx += p.pts[2 * i]; sy += p.pts[2 * i + 1]; }
        centers.append(QPointF(sx / n, sy / n));
    }
    return centers;
}

void DevelopProperties::emitSpotPins()
{
    emit spotPinsChanged(spotPinCenters());
}

void DevelopProperties::renameActiveLayer()
{
    if (G::isLogger) G::log("DevelopProperties::renameActiveLayer");
    if (currentImagePath.isEmpty()) return;
    if (activeLayerIndex == 0) { emit centralMsg("The Base layer cannot be renamed."); return; }
    EditStack &s = stackCache[currentImagePath];
    if (activeLayerIndex >= s.layers.size()) return;

    const QString oldName = s.layers[activeLayerIndex].name;
    bool ok = false;
    const QString entered = QInputDialog::getText(this, "Rename layer", "Layer name:",
                                                  QLineEdit::Normal, oldName, &ok).trimmed();
    if (!ok || entered.isEmpty() || entered == oldName) return;

    /* uniqueLayerName still sees this layer's own OLD name, so it only suffixes on a clash with a
       DIFFERENT layer. */
    s.layers[activeLayerIndex].name = uniqueLayerName(entered);
    dirty.insert(currentImagePath);
    refreshLayerCombo();
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::resetActiveLayer()
{
    if (G::isLogger) G::log("DevelopProperties::resetActiveLayer");
    if (currentImagePath.isEmpty()) return;
    EditLayer *l = activeLayer();
    if (!l) return;
    /* Restore this layer's adjustments to their defaults and un-hide every group (non-destructive to
       the mask geometry, which defines WHERE the layer applies). */
    l->params = EditParams();
    l->showLayer = l->showBasic = l->showColor = l->showEffects = true;
    dirty.insert(currentImagePath);
    buildTree();                 // repopulate the sliders at their defaults + refresh the eyes
    emit paramsChanged();
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::onLayerPreviewToggled(bool shown)
{
    if (G::isLogger) G::log("DevelopProperties::onLayerPreviewToggled");
    EditLayer *l = activeLayer();
    if (!l) { if (layerHeader) layerHeader->setPreviewShown(true); return; }
    l->showLayer = shown;        // non-destructive: values kept, the whole layer folds off at render
    dirty.insert(currentImagePath);
    refreshPreviewButtons();
    emit paramsChanged();
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::setTreeCollapsed(bool collapsed)
{
    /* Hide only the LAYER items -- the Core rows (Base) or mask-tool rows that sit ABOVE
       the Basic/Color/Effects sections. The sections stay visible. */
    layerItemsCollapsed = collapsed;
    applyLayerItemsCollapsed();
    applyCoreVisibility();           // core rows also depend on collapse state

    /* Solo: showing the layer's items is one of the mutually-exclusive sections, so
       collapse the three adjustment sections (they would otherwise stay open alongside
       the layer). Skipped during the Expand-all bulk sweep, which wants all open. */
    if (!collapsed && !isBulkExpandCollapse &&
        setting->value("Develop/isSolo", false).toBool()) {
        for (const char *h : {"BasicHeader", "ColorHeader", "EffectsHeader"}) {
            const QModelIndex idx = findCaptionIndex(h);
            if (idx.isValid()) collapse(idx);
        }
    }
}

void DevelopProperties::applyLayerItemsCollapsed()
{
    /* The layer's top items are the root rows BEFORE the Basic section (Core rows for Base, else the
       mask-tool rows). Re-applied after each buildTree, since a rebuild resets row visibility. */
    const QModelIndex basic = findCaptionIndex("BasicHeader");
    const int firstSection = basic.isValid() ? basic.row() : model->rowCount();
    for (int r = 0; r < firstSection; ++r)
        setRowHidden(r, QModelIndex(), layerItemsCollapsed);
}

void DevelopProperties::bindToneSlider(ToneRegionSlider *slider)
{
    if (G::isLogger) G::log("DevelopProperties::bindToneSlider");
    toneSlider = slider;
    if (!toneSlider) return;
    connect(toneSlider, &ToneRegionSlider::valueChanged,
            this, &DevelopProperties::onToneSplitsChanged);
    /* Reflect whatever image is already current (if any). */
    populateSlidersFromStack();
}

void DevelopProperties::onToneSplitsChanged(double shadow, double crossover, double highlight)
{
    if (G::isLogger) G::log("DevelopProperties::onToneSplitsChanged");
    if (isPopulating) return;
    if (currentImagePath.isEmpty()) return;

    EditParams &p = activeParams();
    p.toneShadowCenter    = static_cast<float>(shadow);
    p.toneCrossover       = static_cast<float>(crossover);
    p.toneHighlightCenter = static_cast<float>(highlight);

    /* The splits move only the shadows/highlights centres (blacks/whites are pinned), so they
       change pixels only when one of those two is engaged. Skip the re-render otherwise -- the
       new positions still ride in the params for when a tone slider is next moved -- so dragging
       the handles on an untoned image does not spin up needless full-res renders. */
    if (p.shadows == 0.0f && p.highlights == 0.0f) return;
    dirty.insert(currentImagePath);
    emit paramsChanged();
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::newLayer()
{
    if (G::isLogger) G::log("DevelopProperties::newLayer");
    if (currentImagePath.isEmpty()) return;

    EditStack &s = stackCache[currentImagePath];
    if (s.layers.isEmpty()) s.layers.append(EditLayer());     // ensure a base layer exists
    s.layers[0].name = "Base";                                // index 0 is always Base

    /* Prompt for the name, defaulting to "Layer n" (the first above Base is "Layer 1", since Base is
       index 0 and the new layer's index equals the pre-append size). Pop the dialog near the cursor
       (the [+] button just clicked) rather than screen-centre, so the flow stays where the user is
       looking, clamped to the screen. */
    const QString def = uniqueLayerName("Layer " + QString::number(s.layers.size()));
    QInputDialog dlg(this);
    dlg.setWindowTitle("New layer");
    dlg.setLabelText("Layer name:");
    dlg.setTextEchoMode(QLineEdit::Normal);
    dlg.setTextValue(def);
    dlg.adjustSize();
    const QPoint cur = QCursor::pos();
    QPoint tl(cur.x() - dlg.width() / 2, cur.y() - 20);
    if (QScreen *scr = QGuiApplication::screenAt(cur)) {
        const QRect a = scr->availableGeometry();
        tl.setX(qBound(a.left(), tl.x(), a.right()  - dlg.width()));
        tl.setY(qBound(a.top(),  tl.y(), a.bottom() - dlg.height()));
    }
    dlg.move(tl);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString entered = dlg.textValue().trimmed();

    EditLayer l;
    l.name = uniqueLayerName(entered.isEmpty() ? def : entered);
    s.layers.append(l);
    activeLayerIndex = s.layers.size() - 1;                   // edit the new layer
    dirty.insert(currentImagePath);

    /* New layer is identity, so the rendered result is unchanged; just refresh the header combo and
       rebuild the tree for it. The stack only persists once a layer actually changes a pixel. */
    selectedMaskIndex = -1;
    refreshLayerCombo();
    buildTree();
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);

    /* A new layer's whole point is to apply a masked adjustment, so the next step is choosing a mask
       tool -- pop that menu straight away (near the cursor). Escape just leaves an empty layer. */
    showMaskMenu();
}

void DevelopProperties::deleteLayer()
{
    if (G::isLogger) G::log("DevelopProperties::deleteLayer");
    if (currentImagePath.isEmpty()) return;

    EditStack &s = stackCache[currentImagePath];
    if (activeLayerIndex <= 0) {
        emit centralMsg("The Base layer cannot be removed.");
        return;
    }
    if (activeLayerIndex >= s.layers.size()) activeLayerIndex = s.layers.size() - 1;
    s.layers.removeAt(activeLayerIndex);
    if (activeLayerIndex >= s.layers.size()) activeLayerIndex = s.layers.size() - 1;
    dirty.insert(currentImagePath);

    selectedMaskIndex = -1;
    refreshLayerCombo();
    buildTree();
    emit paramsChanged();                                    // removing a layer changes the result
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

/* ----------------------------------------------------------------------------------------
   Mask (one mask per non-Base layer, built from an ordered list of Add/Subtract tools)

   Tree shape (the mask-tool rows at the top of the tree, above the Basic/Color/Effects sections):

     Add Linear Gradient       [+][-]    <- one row per tool (caption = op + tool; [+] adds another, [-] removes)
     Subtract Brush            [+][-]    <- selected tool is expanded; its settings are ITS children
        Feather  [slider]
        Invert   [checkbox]
        Done     [checkbox]              <- ends the build (collapses the tool)

   Tool rows are header-style (single-line full-width caption + expand arrow) but flagged
   UR_LeafSingleLine so they paint in the leaf colour, not category teal. The [+] glyph (UR_AddBtn,
   hit-tested in mousePressEvent) pops showMaskMenu to append a tool (newMask); a new layer's first
   tool is offered automatically (newLayer -> showMaskMenu). Subtract is offered only once a tool
   exists ("the first must be added"). Only the SELECTED tool carries its settings children (no duplicate tool-name
   row); clicking a tool (mousePressEvent) toggles which one is expanded. Spatial editing (drag/
   paint/AI-select on the image) is deferred to the canvas; rendering does not yet composite the
   mask.
   ---------------------------------------------------------------------------------------- */

QString DevelopProperties::maskToolName(int tool)
{
    switch (static_cast<MaskTool>(tool)) {
    case MaskTool::LinearGradient:  return "Linear Gradient Mask";
    case MaskTool::RadialGradient:  return "Radial Gradient Mask";
    case MaskTool::Brush:           return "Brush Mask";
    case MaskTool::ColorRange:      return "Color Range Mask";
    case MaskTool::LuminanceRange:  return "Luminance Range Mask";
    case MaskTool::Subject:         return "Subject Mask";
    case MaskTool::Sky:             return "Sky Mask";
    case MaskTool::Background:      return "Background Mask";
    case MaskTool::Depth:           return "Depth Range Mask";
    case MaskTool::Object:          return "Object Mask";
    }
    return "Mask";
}

int DevelopProperties::maskToolFromName(const QString &name)
{
    for (int t = 0; t <= int(MaskTool::Object); ++t)
        if (maskToolName(t) == name) return t;
    return int(MaskTool::LinearGradient);
}

QString DevelopProperties::opName(int op)
{
    return (op == int(MaskOp::Subtract)) ? "(-)" : "(+)";
}

EditLayer *DevelopProperties::activeLayer()
{
    if (currentImagePath.isEmpty()) return nullptr;
    EditStack &s = stackCache[currentImagePath];
    if (activeLayerIndex < 0 || activeLayerIndex >= s.layers.size()) return nullptr;
    return &s.layers[activeLayerIndex];
}

bool DevelopProperties::maskOverlayActive() const
{
    /* A mask tool is expanded (its settings shown) on a non-Base layer -> show the whole-layer mask. */
    return activeLayerIndex > 0 && selectedMaskIndex >= 0;
}

QVector<MaskComponent> DevelopProperties::activeLayerMasks() const
{
    if (currentImagePath.isEmpty()) return {};
    auto it = stackCache.constFind(currentImagePath);
    if (it == stackCache.constEnd()) return {};
    const EditStack &s = it.value();
    if (activeLayerIndex < 0 || activeLayerIndex >= s.layers.size()) return {};
    return s.layers[activeLayerIndex].masks;
}

/* ----------------------------------------------------------------------------------------
   Preview (show/ignore) + Reset per group
   ---------------------------------------------------------------------------------------- */

EditParams::Group DevelopProperties::paramsGroup(int group)
{
    switch (group) {
    case PV_Color:   return EditParams::Group::Color;
    case PV_Effects: return EditParams::Group::Effects;
    default:         return EditParams::Group::Basic;   // PV_Basic (and PV_Layer, unused here)
    }
}

bool *DevelopProperties::previewFlag(EditLayer *l, int group)
{
    if (!l) return nullptr;
    switch (group) {
    case PV_Layer:   return &l->showLayer;
    case PV_Color:   return &l->showColor;
    case PV_Effects: return &l->showEffects;
    default:         return &l->showBasic;              // PV_Basic
    }
}

BarBtn *DevelopProperties::makeEyeBtn(const QString &tooltip, int group)
{
    BarBtn *b = new BarBtn();
    b->setIcon(":/images/icon16/eye.png", G::iconOpacity);
    b->setToolTip(tooltip);
    connect(b, &BarBtn::clicked, this, [this, group]{ togglePreviewSection(group); });
    return b;
}

void DevelopProperties::refreshPreviewButtons()
{
    EditLayer *l = activeLayer();
    auto set = [](BarBtn *b, bool shown) {
        if (!b) return;
        b->setIcon(shown ? ":/images/icon16/eye.png" : ":/images/icon16/eye_off.png", G::iconOpacity);
    };
    set(basicEyeBtn,   l ? l->showBasic   : true);
    set(colorEyeBtn,   l ? l->showColor   : true);
    set(effectsEyeBtn, l ? l->showEffects : true);
    /* The whole-layer eye lives in the LayerHeader widget, not the tree. */
    if (layerHeader) layerHeader->setPreviewShown(l ? l->showLayer : true);
}

void DevelopProperties::togglePreviewSection(int group)
{
    if (G::isLogger) G::log("DevelopProperties::togglePreviewSection");
    EditLayer *l = activeLayer();
    bool *f = previewFlag(l, group);
    if (!f) return;
    *f = !*f;                               // non-destructive: values are kept, only the fold changes
    dirty.insert(currentImagePath);
    refreshPreviewButtons();
    emit paramsChanged();                   // re-render with the folded params
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::resetSection(int group)
{
    if (G::isLogger) G::log("DevelopProperties::resetSection");
    if (currentImagePath.isEmpty()) return;
    EditParams &p = activeParams();
    if (group == PV_Layer) {
        EditParams::resetGroup(p, EditParams::Group::Basic);
        EditParams::resetGroup(p, EditParams::Group::Color);
        EditParams::resetGroup(p, EditParams::Group::Effects);
    }
    else {
        EditParams::resetGroup(p, paramsGroup(group));
    }
    dirty.insert(currentImagePath);
    populateSlidersFromStack();             // reflect the restored defaults (sliders + tone slider)
    emit paramsChanged();
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::contextMenuEvent(QContextMenuEvent *event)
{
    if (G::isLogger) G::log("DevelopProperties::contextMenuEvent");
    QModelIndex idx = indexAt(event->pos());
    if (idx.isValid()) idx = model->index(idx.row(), CapColumn, idx.parent());
    const QString name = idx.isValid() ? idx.data(UR_Name).toString() : QString();

    /* Map a header row to its Preview/Reset group; other rows only get tree items. */
    int group = -1;
    QString label;
    if      (name == "BasicHeader")   { group = PV_Basic;   label = "Basic"; }
    else if (name == "ColorHeader")   { group = PV_Color;   label = "Color"; }
    else if (name == "EffectsHeader") { group = PV_Effects; label = "Effects"; }

    QMenu menu(this);

    /* Section-specific Preview/Reset (only when the click landed on a section header). */
    QAction *aPreview = nullptr;
    QAction *aReset = nullptr;
    if (group >= 0) {
        EditLayer *l = activeLayer();
        const bool shown = l ? *previewFlag(l, group) : true;
        aPreview = menu.addAction("Preview");
        aPreview->setCheckable(true);
        aPreview->setChecked(shown);
        aPreview->setEnabled(l != nullptr);
        aReset = menu.addAction("Reset " + label);
        aReset->setEnabled(!currentImagePath.isEmpty());
        menu.addSeparator();
    }

    /* Tree-wide items, available anywhere in the dock (mirrors the Embellish dock). */
    QAction *aExpandAll = menu.addAction("Expand all");
    QAction *aCollapseAll = menu.addAction("Collapse all");
    QAction *aSolo = menu.addAction("Solo mode");
    aSolo->setCheckable(true);
    aSolo->setChecked(setting->value("Develop/isSolo", false).toBool());

    QAction *chosen = menu.exec(event->globalPos());
    if      (chosen == nullptr)      return;
    else if (chosen == aPreview)     togglePreviewSection(group);
    else if (chosen == aReset)       resetSection(group);
    else if (chosen == aExpandAll)   setAllSectionsExpanded(true);
    else if (chosen == aCollapseAll) setAllSectionsExpanded(false);
    else if (chosen == aSolo) {
        setSolo(aSolo->isChecked());
        setting->setValue("Develop/isSolo", aSolo->isChecked());
    }
}

void DevelopProperties::setAllSectionsExpanded(bool expand)
{
    if (G::isLogger) G::log("DevelopProperties::setAllSectionsExpanded");
    /* Expand/Collapse all also drives the Layer row, whose collapse arrow lives in the
       LayerHeader band (not the tree). The bulk guard stops onSectionExpanded from
       re-collapsing the layer as the tree sections expand under Solo. */
    isBulkExpandCollapse = true;
    if (expand) expandAll();
    else        collapseAll();
    setTreeCollapsed(!expand);                          // show/hide the layer's own items
    if (layerHeader) layerHeader->setCollapsed(!expand);
    isBulkExpandCollapse = false;
}

void DevelopProperties::onSectionExpanded(const QModelIndex &idx)
{
    /* Solo: one section open at a time. The base PropertyEditor collapses the sibling
       tree sections when a root is expanded; extend that to the Layer row (a separate
       widget above the tree). Only the three adjustment sections are peers of the layer
       -- the Demosaic row is itself a layer item (it hides when the layer collapses), so
       expanding it must NOT collapse the layer. Skipped during the Expand-all sweep. */
    if (isBulkExpandCollapse) return;
    if (!setting->value("Develop/isSolo", false).toBool()) return;
    const QString name = idx.data(UR_Name).toString();
    if (name != "BasicHeader" && name != "ColorHeader" && name != "EffectsHeader") return;
    if (!layerItemsCollapsed) {
        setTreeCollapsed(true);
        if (layerHeader) layerHeader->setCollapsed(true);
    }
}

void DevelopProperties::showMaskMenu()
{
    if (G::isLogger) G::log("DevelopProperties::showMaskMenu");
    EditLayer *layer = activeLayer();
    if (!layer || activeLayerIndex == 0) {
        emit centralMsg("The Base layer applies globally and cannot be masked.");
        return;
    }

    /* Built fresh each click: Subtract is offered only once at least one tool exists (the first
       tool must Add -- there is nothing to subtract from an empty mask). */
    maskMenu->clear();
    for (int t = 0; t <= int(MaskTool::Object); ++t) {
        QAction *a = maskMenu->addAction("Add " + maskToolName(t));
        connect(a, &QAction::triggered, this, &DevelopProperties::newMask);
        if (t == int(MaskTool::Brush) || t == int(MaskTool::LuminanceRange))
            maskMenu->addSeparator();           // group geometric / range / AI tools
    }
    if (!layer->masks.isEmpty()) {
        maskMenu->addSeparator();
        for (int t = 0; t <= int(MaskTool::Object); ++t) {
            QAction *a = maskMenu->addAction("Subtract " + maskToolName(t));
            connect(a, &QAction::triggered, this, &DevelopProperties::newMask);
            if (t == int(MaskTool::Brush) || t == int(MaskTool::LuminanceRange))
                maskMenu->addSeparator();           // group geometric / range / AI tools
        }
    }
    maskMenu->exec(QCursor::pos());
}

void DevelopProperties::newMask()
{
    if (G::isLogger) G::log("DevelopProperties::newMask");
    EditLayer *layer = activeLayer();
    if (!layer || activeLayerIndex == 0) return;            // Base layer has no mask
    QAction *a = qobject_cast<QAction*>(sender());
    if (!a) return;

    /* Action text is "Add <tool>" / "Subtract <tool>"; the first word is the op, the rest the
       tool name (handles multi-word names like "Subject Mask"). */
    const QString txt = a->text();
    MaskComponent m;
    m.op   = txt.startsWith("Subtract") ? int(MaskOp::Subtract) : int(MaskOp::Add);
    m.tool = maskToolFromName(txt.section(' ', 1));
    m.paramsJson = defaultMaskParams(m.tool);
    if (m.tool == int(MaskTool::Brush)) m.feather = 0.0f;   // brush defaults to a crisp edge
    layer->masks.append(m);
    selectedMaskIndex = layer->masks.size() - 1;            // start editing the new tool
    dirty.insert(currentImagePath);

    buildTree();
    emit paramsChanged();       // adding a mask confines the layer's adjustment -> re-composite
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::deleteMask(int index)
{
    EditLayer *layer = activeLayer();
    if (!layer || index < 0 || index >= layer->masks.size()) return;
    layer->masks.removeAt(index);
    if      (selectedMaskIndex == index) selectedMaskIndex = -1;     // its settings close
    else if (selectedMaskIndex >  index) selectedMaskIndex--;        // shift to follow the tool
    dirty.insert(currentImagePath);

    buildTree();
    emit paramsChanged();       // removing a mask changes the layer's coverage -> re-composite
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::addToolRow(QModelIndex parIdx, int index, const MaskComponent &m, bool selected)
{
    /* Header-style row: the "op + tool" caption draws single-line across the full row width (no
       wrap) with an expand arrow; UR_LeafSingleLine paints it in the ordinary leaf colour (not
       category teal). The row is made a single full-width spanned cell (setFirstColumnSpanned)
       and the [-] remove button is drawn by the delegate at the right (UR_DeleteBtn), NOT a
       value-column widget: a column-1 widget would cover and clip the caption overflow. The
       delete click is hit-tested in mousePressEvent. */
    clearItemInfo(i);
    i.name = QString("maskTool%1").arg(index);
    i.parIdx = parIdx;
    i.captionText = opName(m.op) + " " + maskToolName(m.tool);
    i.tooltip = "Mask tool. Click to show/hide its settings; [+] to add another tool, [-] to remove.";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = false;
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex toolIdx = capIdx;
    model->setData(toolIdx, index, UR_MaskIndex);
    model->setData(toolIdx, true, UR_LeafSingleLine);
    model->setData(toolIdx, true, UR_DeleteBtn);
    model->setData(toolIdx, true, UR_AddBtn);           // [+] left of [-]: add another mask tool
    model->setData(toolIdx, true, UR_ShowDecoration);   // always show the expand arrow (settings on click)
    setFirstColumnSpanned(toolIdx.row(), parIdx, true);

    /* The selected tool reveals its settings as its OWN children (no duplicate tool-name row).
       Clicking the tool caption again collapses it (see mousePressEvent), so no separate Done row is
       needed. Gradients: Feather + Invert. Brush: Size, Feather, Flow, Auto mask, Invert. */
    if (selected) {
        if (m.tool == int(MaskTool::Brush)) {
            addSlider("maskSize", "Size", "Brush diameter (% of the long edge).",
                      toolIdx, "", 1, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soft edge added OUTSIDE the brush size (0 = crisp).",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFlow", "Flow", "How much each stroke builds up.",
                      toolIdx, "", 1, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskAutoMask", "Auto mask",
                        "Limit the brush to the edge under the stroke start. Toggle with A.",
                        toolIdx, "", false);
            addCheckbox("maskAutoMaskAi", "AI edge (SAM)",
                        "Auto mask mode: on = SAM object under the stroke start; off = similar-luminance band.",
                        toolIdx, "", false);
            addCheckbox("maskInvert", "Invert", "Invert this mask's contribution.", toolIdx, "", false);
            setSliderReal("maskSize", brushNum(m.paramsJson, "size", 20));
            setSliderReal("maskFeather", m.feather);
            setSliderReal("maskFlow", brushNum(m.paramsJson, "flow", 100));
            setCheckboxValue("maskAutoMask", brushBool(m.paramsJson, "autoMask", false));
            setCheckboxValue("maskAutoMaskAi", brushStr(m.paramsJson, "autoMaskMode", "lum") == "ai");
            setCheckboxValue("maskInvert", m.inverted);
        }
        else if (m.tool == int(MaskTool::LuminanceRange)) {
            /* Selects a luminance band [lo,hi] (stored 0..1, shown 0..100). Feather softens the band
               edges; Invert flips the selection. */
            addSlider("maskRangeLo", "Range Min", "Lower luminance bound of the selected band.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskRangeHi", "Range Max", "Upper luminance bound of the selected band.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soften the band edges.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.", toolIdx, "", false);
            setSliderReal("maskRangeLo", brushNum(m.paramsJson, "lo", 0.25) * 100.0);
            setSliderReal("maskRangeHi", brushNum(m.paramsJson, "hi", 0.75) * 100.0);
            setSliderReal("maskFeather", m.feather);
            setCheckboxValue("maskInvert", m.inverted);
        }
        else if (m.tool == int(MaskTool::Depth)) {
            /* Selects a depth band [Near,Far] over the MiDaS depth field (stored 0=near..1=far,
               shown 0..100). Same mechanics as Luminance Range but over depth. */
            addSlider("maskRangeLo", "Near", "Near bound of the selected depth band (0 = nearest).",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskRangeHi", "Far", "Far bound of the selected depth band (100 = farthest).",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soften the depth-band edges.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.", toolIdx, "", false);
            setSliderReal("maskRangeLo", brushNum(m.paramsJson, "lo", 0.0) * 100.0);
            setSliderReal("maskRangeHi", brushNum(m.paramsJson, "hi", 0.5) * 100.0);
            setSliderReal("maskFeather", m.feather);
            setCheckboxValue("maskInvert", m.inverted);
        }
        else if (m.tool == int(MaskTool::ColorRange)) {
            /* Colours are sampled by clicking the loupe (shift-click adds, and the on-image swatches
               remove); the dock carries only Refine (tolerance), Feather and Invert. */
            addSlider("maskRefine", "Refine",
                      "Colour tolerance: higher selects fewer, closer colours. Click the image to "
                      "sample; shift-click to add another colour.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soften the selection edge.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.", toolIdx, "", false);
            setSliderReal("maskRefine", brushNum(m.paramsJson, "refine", 50));
            setSliderReal("maskFeather", m.feather);
            setCheckboxValue("maskInvert", m.inverted);
        }
        else if (m.tool == int(MaskTool::Object)) {
            /* Perimeter-paint: trace the object boundary with a solid brush (Alt erases;
               [ ] or two-finger drag resize). Size = diameter; Feather softens edge. */
            addSlider("maskSize", "Size", "Perimeter brush diameter (% of the long edge).",
                      toolIdx, "", 1, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soften the refined cutout edge.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskInvert", "Invert", "Invert this mask's contribution.", toolIdx, "", false);
            setSliderReal("maskSize", brushNum(m.paramsJson, "size", 8));
            setSliderReal("maskFeather", m.feather);
            setCheckboxValue("maskInvert", m.inverted);
        }
        else {
            addSlider("maskFeather", "Feather", "Soften the mask edge.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.", toolIdx, "", false);
            setSliderReal("maskFeather", m.feather);
            setCheckboxValue("maskInvert", m.inverted);
        }
        expand(toolIdx);
    }
}

void DevelopProperties::setSelectedMask(int index)
{
    selectedMaskIndex = index;
    buildTree();            // rebuild so the newly-selected tool shows (only it carries settings)
}

QString DevelopProperties::defaultMaskParams(int tool)
{
    /* Geometry is stored normalized (0..1 of the image), so it is resolution-independent and
       survives proxy/full re-renders. Other tools: empty until implemented. */
    if (tool == int(MaskTool::LinearGradient)) {
        /* A vertical line down the middle, ramping 0% (top) -> 100% (bottom) across the middle third. */
        QJsonObject o;
        o["x1"] = 0.5; o["y1"] = 0.34;
        o["x2"] = 0.5; o["y2"] = 0.66;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::RadialGradient)) {
        /* A centred ellipse covering the middle of the frame; mask 100% inside, ramping to 0% at
           the edge. cx/cy normalized by W/H, rx by W, ry by H, angle in degrees. */
        QJsonObject o;
        o["cx"] = 0.5;  o["cy"] = 0.5;
        o["rx"] = 0.25; o["ry"] = 0.30;
        o["angle"] = 0.0;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::Brush)) {
        /* Freehand: starts empty. size/flow are 0..100 (current settings for the NEXT stroke; feather
           lives in MaskComponent.feather). strokes is the accumulated path list -- each stroke
           snapshots the settings it was painted with. Points are normalized output coords. */
        QJsonObject o;
        o["size"] = 20;
        o["flow"] = 100;
        o["autoMask"] = false;
        o["autoMaskMode"] = "lum";      // "lum" (luminance band) | "ai" (SAM object)
        o["strokes"] = QJsonArray();
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::LuminanceRange)) {
        /* Selects a band of display-referred luminance [lo,hi] (0..1). Starts on the mid-tones so the
           mask is visible; the feather (MaskComponent.feather) softens the band edges. */
        QJsonObject o;
        o["lo"] = 0.25;
        o["hi"] = 0.75;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::ColorRange)) {
        /* Selects pixels near one or more sampled colours (added by clicking the loupe). refine 0..100
           sets the tolerance (higher = tighter). samples are [r,g,b] 0..1 in display space. */
        QJsonObject o;
        o["refine"] = 50;
        o["samples"] = QJsonArray();
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::Depth)) {
        /* Selects a depth band [lo,hi] (0=near..1=far) over the model depth field. Starts on the near
           half so the mask is visible; feather (MaskComponent.feather) softens the band edges. */
        QJsonObject o;
        o["lo"] = 0.0;
        o["hi"] = 0.5;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    if (tool == int(MaskTool::Object)) {
        /* SAM 2 object mask, perimeter-paint: the user traces the object BOUNDARY with a
           solid brush (multiple strokes; Alt erases). Brush-style blob (mirrors the Brush
           tool): size + a stroke list of normalized output-coord points. Empty -> no
           coverage until a closed loop is drawn, when MW fills the enclosed region and
           SAM refines it to the object edge. */
        QJsonObject o;
        o["size"] = 8;
        o["strokes"] = QJsonArray();
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }
    return QString();
}

int DevelopProperties::activeMaskTool() const
{
    if (currentImagePath.isEmpty()) return -1;
    auto it = stackCache.find(currentImagePath);
    if (it == stackCache.end()) return -1;
    const EditStack &s = it.value();
    if (activeLayerIndex < 0 || activeLayerIndex >= s.layers.size()) return -1;
    if (selectedMaskIndex < 0 || selectedMaskIndex >= s.layers[activeLayerIndex].masks.size()) return -1;
    return s.layers[activeLayerIndex].masks[selectedMaskIndex].tool;
}

double DevelopProperties::brushNum(const QString &paramsJson, const QString &key, double def)
{
    const QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    return o.contains(key) ? o.value(key).toDouble(def) : def;
}

bool DevelopProperties::brushBool(const QString &paramsJson, const QString &key, bool def)
{
    const QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    return o.contains(key) ? o.value(key).toBool(def) : def;
}

QString DevelopProperties::brushStr(const QString &paramsJson, const QString &key, const QString &def)
{
    const QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    return o.contains(key) ? o.value(key).toString(def) : def;
}

QString DevelopProperties::brushWith(const QString &paramsJson, const QString &key, const QJsonValue &v)
{
    QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    o[key] = v;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void DevelopProperties::setActiveBrushSize(double size)
{
    EditLayer *l = activeLayer();
    if (!l || selectedMaskIndex < 0 || selectedMaskIndex >= l->masks.size()) return;
    MaskComponent &m = l->masks[selectedMaskIndex];
    if (m.tool != int(MaskTool::Brush) && m.tool != int(MaskTool::Object)) return;
    const int s = qBound(1, int(size + 0.5), 100);
    m.paramsJson = brushWith(m.paramsJson, "size", s);
    dirty.insert(currentImagePath);
    isPopulating = true;                  // sync the Size slider without re-entering itemChange
    setSliderReal("maskSize", s);
    isPopulating = false;
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::setActiveBrushAutoMask(bool on)
{
    EditLayer *l = activeLayer();
    if (!l || selectedMaskIndex < 0 || selectedMaskIndex >= l->masks.size()) return;
    MaskComponent &m = l->masks[selectedMaskIndex];
    if (m.tool != int(MaskTool::Brush)) return;
    m.paramsJson = brushWith(m.paramsJson, "autoMask", on);
    dirty.insert(currentImagePath);
    isPopulating = true;                  // sync the checkbox without re-entering itemChange
    setCheckboxValue("maskAutoMask", on);
    isPopulating = false;
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::emitBrushSettings(const MaskComponent &m)
{
    emit maskBrushSettingsChanged(brushNum(m.paramsJson, "size", 20),
                                  m.feather,
                                  brushNum(m.paramsJson, "flow", 100),
                                  brushBool(m.paramsJson, "autoMask", false),
                                  brushStr(m.paramsJson, "autoMaskMode", "lum"));
}

void DevelopProperties::updateMaskEdit()
{
    /* Drive the ImageView mask overlay from the active (selected) mask tool. Begin when a spatial
       tool is selected and has geometry; otherwise End. Called at the end of every rebuild, so it
       tracks image/layer switches, selection toggles, add and delete. */
    EditLayer *layer = activeLayer();
    if (layer && selectedMaskIndex >= 0 && selectedMaskIndex < layer->masks.size()) {
        const MaskComponent &m = layer->masks[selectedMaskIndex];
        if (m.tool == int(MaskTool::LinearGradient) || m.tool == int(MaskTool::RadialGradient) ||
            m.tool == int(MaskTool::Brush) ||
            m.tool == int(MaskTool::ColorRange) || m.tool == int(MaskTool::LuminanceRange) ||
            m.tool == int(MaskTool::Subject) || m.tool == int(MaskTool::Sky) ||
            m.tool == int(MaskTool::Background) || m.tool == int(MaskTool::Depth) ||
            m.tool == int(MaskTool::Object)) {
            emit maskEditBegin(m.tool, m.op, m.inverted, m.paramsJson, m.feather);
            return;
        }
    }
    emit maskEditEnd();
}

void DevelopProperties::setActiveMaskParams(const QString &paramsJson)
{
    /* ImageView dragged the overlay: persist the new geometry into the active mask component. No
       tree rebuild (would re-emit maskEditBegin and fight the live drag); just store + debounce. */
    EditLayer *layer = activeLayer();
    if (!layer || selectedMaskIndex < 0 || selectedMaskIndex >= layer->masks.size()) return;
    if (layer->masks[selectedMaskIndex].paramsJson == paramsJson) return;
    layer->masks[selectedMaskIndex].paramsJson = paramsJson;
    dirty.insert(currentImagePath);
    emit paramsChanged();       // new mask geometry -> re-composite the masked layer
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::onMaskSelectionChanged()
{
    /* Clicks do not select in this tree (see mousePressEvent); this only fires on programmatic
       selection. Keep it in sync so any future selectionModel()->select() drives the settings. */
    if (isRebuildingMasks) return;
    QModelIndex idx = selectionModel()->currentIndex();
    if (!idx.isValid()) return;
    const QVariant v = idx.siblingAtColumn(0).data(UR_MaskIndex);
    if (!v.isValid()) return;
    const int m = v.toInt();
    if (m >= 0 && m != selectedMaskIndex) setSelectedMask(m);
}

void DevelopProperties::mousePressEvent(QMouseEvent *event)
{
    /* Clicking a tool row toggles its settings (the base class does not select on click, so we read
       the row's UR_MaskIndex ourselves and rebuild). We manage the tool's expand state via the
       selection, so we do NOT fall through to the base expand/collapse for these rows. */
    if (event->button() == Qt::LeftButton) {
        const QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) {
            /* The "Add mask" placeholder row (shown while the layer has no mask) has no UR_MaskIndex;
               a click anywhere on it -- including its [+] -- pops the Add/Subtract chooser. */
            if (idx.siblingAtColumn(0).data(UR_Name).toString() == "addMask") {
                showMaskMenu();
                return;
            }
            const QVariant v = idx.siblingAtColumn(0).data(UR_MaskIndex);
            if (v.isValid()) {
                const int m = v.toInt();
                /* The [+] add and [-] remove glyphs are delegate-drawn at the row's right edge (must
                   mirror the geometry in PropertyDelegate::paint for UR_AddBtn/UR_DeleteBtn): [-] at
                   the edge, [+] one slot to its left. A click on [-] removes the tool, on [+] adds
                   another; a click anywhere else on the row toggles its settings. */
                const QRect rowRect = visualRect(idx);
                const int sz = 16;
                const int gy = rowRect.top() + (rowRect.height() - sz)/2;
                const QRect delRect(rowRect.right() - sz - 4, gy, sz, sz);
                const QRect addRect(rowRect.right() - 2*sz - 8, gy, sz, sz);
                if (delRect.contains(event->pos())) {
                    deleteMask(m);
                    return;
                }
                if (addRect.contains(event->pos())) {
                    showMaskMenu();
                    return;
                }
                setSelectedMask(m == selectedMaskIndex ? -1 : m);   // click open tool to collapse
                return;
            }
        }
    }
    PropertyEditor::mousePressEvent(event);
}

void DevelopProperties::drawBranches(QPainter *, const QRect &, const QModelIndex &) const
{
    /* Intentionally empty: draw no native branch indicator. Every expandable Develop row
       draws its own winnow arrow through PropertyDelegate (section headers, mask tools,
       and the Demosaic row), so the native triangle is redundant; on the Demosaic value
       row (whose background fill is skipped when isAlternatingRows) it also showed
       through beside the delegate arrow, giving a doubled, oversized decoration. */
}

/* ----------------------------------------------------------------------------------------
   Item builders
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::addHeader(const QString &name, const QString &parent,
                                  const QString &caption, const QString &tooltip, int previewGroup)
{
    clearItemInfo(i);
    i.name = name;
    i.parentName = parent;  // "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = caption;
    i.tooltip = tooltip;
    i.captionIsEditable = false;
    i.path = "Develop/" + name;

    if (previewGroup >= 0) {
        /* A section header (Basic/Color/Effects): a trailing eye toggle in a BarBtn column, drained
           by BarBtnEditor. hasValue + DT_BarBtns are what create that column. */
        BarBtn *eye = makeEyeBtn("Preview: show or ignore these settings", previewGroup);
        if      (previewGroup == PV_Basic)   basicEyeBtn   = eye;
        else if (previewGroup == PV_Color)   colorEyeBtn   = eye;
        else if (previewGroup == PV_Effects) effectsEyeBtn = eye;
        btns.append(eye);
        i.hasValue = true;
        i.delegateType = DT_BarBtns;
    }
    else {
        i.hasValue = false;
        i.delegateType = DT_None;
    }
    addItem(i);
}

void DevelopProperties::addSlider(const QString &key, const QString &caption,
                                  const QString &tooltip, QModelIndex parIdx,
                                  const QString &parentName, int min, int max,
                                  int div, QString color, QString color1,
                                  double defaultValue)
{
    clearItemInfo(i);
    i.name = key;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.captionText = caption;
    i.tooltip = tooltip;
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = key;
    i.defaultValue = defaultValue;
    /* Per-image now: the editor starts at identity and is populated from the current image's
       EditStack on selection (see populateSlidersFromStack). No app-global QSettings path. */
    i.path = "";
    i.value = i.defaultValue;
    i.delegateType = DT_Slider;
    i.type = (div == 0) ? "int" : "double";     // div 0 = integer slider, else double
    i.min = min;
    i.max = max;
    i.div = div;
    i.step = 1;
    i.color = color;
    i.color1 = color1;
    i.fixedWidth = 50;
    addItem(i);
}

void DevelopProperties::addCheckbox(const QString &key, const QString &caption, const QString &tooltip,
                                    QModelIndex parIdx, const QString &parentName, bool defaultValue)
{
    clearItemInfo(i);
    i.name = key;
    i.parIdx = parIdx;
    i.parentName = parentName;
    i.captionText = caption;
    i.tooltip = tooltip;
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = key;
    i.defaultValue = defaultValue;
    i.path = "";
    i.value = i.defaultValue;
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
}

/* ----------------------------------------------------------------------------------------
   Basic
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::addBasic()
{
    if (G::isLogger) G::log("DevelopProperties::addBasic");
    addHeader("BasicHeader", "???", "Basic", "Core tone, white balance and presence adjustments.", PV_Basic);
    QModelIndex parIdx = capIdx;

    /* Lightroom-like ranges. Most adjustments are integer sliders -100..100 (div 0).
       Exposure is a 2-decimal EV slider (-5.00..5.00, div 100). All default to 0
       (identity), matching EditParams. */
    addSlider("temp",       "Temp",       "White balance temperature.",          parIdx, "BasicHeader", -100, 100, 0,   G::lightblue, G::lightyellow);
    addSlider("tint",       "Tint",       "White balance tint (green/magenta).", parIdx, "BasicHeader", -100, 100, 0,   G::lightgreen, G::lightmagenta);
    addDivider(dividerHeight, 1, divColor, parIdx, "BasicHeader", "WBDevider");
    addSlider("exposure",   "Exposure",   "Overall exposure in stops (EV).",     parIdx, "BasicHeader", -500, 500, 100, G::darkgray, G::lightgray);
    addSlider("contrast",   "Contrast",   "Global contrast.",                    parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("highlights", "Highlights", "Recover or lift the highlights.",     parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("shadows",    "Shadows",    "Recover or deepen the shadows.",      parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("whites",     "Whites",     "Set the white point.",                parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("blacks",     "Blacks",     "Set the black point.",                parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addDivider(dividerHeight, 1, divColor, parIdx, "BasicHeader", "ToneDevider");
    addSlider("texture",    "Texture",    "Enhance or smooth fine detail.",      parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    addSlider("dehaze",     "Dehaze",     "Remove or add atmospheric haze.",     parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    // demo colors
    // addSlider("blue", "Blue", "Blue.", parIdx, "BasicHeader", -100, 100, 0, G::darkblue, G::lightblue);
    // addSlider("yellow", "Yellow", "Yellow.", parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    // addSlider("orange", "orange", "orange.", parIdx, "BasicHeader", -100, 100, 0, G::darkorange, G::lightorange);
    // addSlider("cyan", "cyan", "cyan.", parIdx, "BasicHeader", -100, 100, 0, G::darkcyan, G::lightcyan);
    // addSlider("green", "green", "green.", parIdx, "BasicHeader", -100, 100, 0, G::darkgreen, G::lightgreen);
    // addSlider("teal", "teal", "teal.", parIdx, "BasicHeader", -100, 100, 0, G::darkteal, G::lightteal);
    // addSlider("maroon", "maroon", "maroon.", parIdx, "BasicHeader", -100, 100, 0, G::darkmaroon, G::lightmaroon);
    // addSlider("pink", "pink", "pink.", parIdx, "BasicHeader", -100, 100, 0, G::darkpink, G::lightpink);
    // addSlider("purple", "purple", "purple.", parIdx, "BasicHeader", -100, 100, 0, G::darkpurple, G::lightpurple);
    // addSlider("red", "red", "red.", parIdx, "BasicHeader", -100, 100, 0, G::darkred, G::lightred);
}

void DevelopProperties::addColor()
{
    if (G::isLogger) G::log("DevelopProperties::addColor");
    addHeader("ColorHeader", "???", "Color", "RGB and HSL adjustments.", PV_Color);
    QModelIndex parIdx = capIdx;

    /* All integer sliders -100..100 (div 0), default 0 (identity), matching EditParams.
       RGB = per-channel gain; HSL = global hue rotation / saturation / luminance. */
    addSlider("red",        "Red",        "Per-channel red gain.",                 parIdx, "ColorHeader", -100, 100, 0, G::darkred,   G::lightred);
    addSlider("green",      "Green",      "Per-channel green gain.",               parIdx, "ColorHeader", -100, 100, 0, G::darkgreen, G::lightgreen);
    addSlider("blue",       "Blue",       "Per-channel blue gain.",                parIdx, "ColorHeader", -100, 100, 0, G::darkblue,  G::lightblue);
    addDivider(dividerHeight, 1, divColor, parIdx, "ColorHeader", "RGBDevider");
    addSlider("hue",        "Hue",        "Rotate all hues.",                      parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addSlider("saturation", "Saturation", "Global saturation (grey at -100).",     parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addSlider("luminance",  "Luminance",  "Global luminance (brightness).",        parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addDivider(dividerHeight, 1, divColor, parIdx, "ColorHeader", "HSLDevider");
    addSlider("vibrance",   "Vibrance",   "Saturation weighted toward muted colours.", parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
}

/* ----------------------------------------------------------------------------------------
   Effects
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::addEffects()
{
    if (G::isLogger) G::log("DevelopProperties::addEffects");
    addHeader("EffectsHeader", "???", "Effects", "Local (post-demosaic) effects.", PV_Effects);
    QModelIndex parIdx = capIdx;

    /* Local noise reduction: per-layer, maskable Develop ops on the decoded image
       (localDenoiseLuma / localDenoiseChroma). Distinct from the Base layer's "Denoise raw"
       (decode-time global raw NR, denoiseLuma/denoiseChroma) -- different function, different keys.
       Two 0..100 strength sliders (mapped to 0..1): Denoise = luminance NR (ratio-preserving),
       Denoise Color = chroma NR (opponent-chroma blur, luma kept exact) -- see Develop::Denoise. */
    addSlider("localDenoise", "Denoise:", "Local luminance noise reduction on the rendered image.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray);
    addSlider("localDenoiseChroma", "   Color", "Local colour (chroma) noise reduction.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray);

    addDivider(dividerHeight, 1, divColor, parIdx, "EffectsHeader", "VignetteDevider");

    /* Vignette: a global radial exposure falloff about the image centre (see
       Develop::Vignette). Two sliders: Exposure (a 2-decimal EV slider like Basic
       Exposure, the corner adjustment -- negative darkens, positive brightens) and
       Feather (0..100, how far it reaches inward). Off-centre / shaped vignettes use
       radial masks instead. */
    addSlider("vignetteExposure", "Vignette:", "Corner exposure (EV): - darkens, + brightens.",
              parIdx, "EffectsHeader", -500, 500, 100, G::darkgray, G::lightgray);
    addSlider("vignetteFeather", "   Feather", "How far the vignette reaches inward.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray, 50);

    addDivider(dividerHeight, 1, divColor, parIdx, "EffectsHeader", "GrainDivider");

    /* Grain: monochromatic film grain added to luminance (see Develop::Grain). Amount is
       the strength, size the particle size (scaled to the image so the proxy matches full
       res), roughness how patchy / irregular the grain is. Size and roughness are
       sub-controls of Amount (indented "  " captions, like the vignette feather) and are
       inert until Amount is non-zero. All three 0..100 sliders mapped to 0..1. */

    addSlider("grainAmount", "Grain:", "Add monochromatic film grain to the image.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray);
    addSlider("grainSize", "   Size", "Grain particle size (fine to coarse).",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray, 25);
    addSlider("grainRoughness", "   Roughness", "How patchy / irregular the grain is.",
              parIdx, "EffectsHeader", 0, 100, 0, G::darkgray, G::lightgray, 50);

    addDivider(dividerHeight, 1, divColor, parIdx, "EffectsHeader", "EndDivider");
}

void DevelopProperties::updateSectionHeaderCaptions()
{
    if (G::isLogger) G::log("DevelopProperties::updateSectionHeaderCaptions");
    /* The Basic/Color/Effects section headers carry the active layer's name so it is always clear
       which layer the sliders below are editing, e.g. "Basic Base", "Color Layer 1". */
    const QString layerName = currentLayerNames().value(activeLayerIndex);
    const QPair<QString, QString> hdrs[] = {
        {"BasicHeader",   "Basic"},
        {"ColorHeader",   "Color"},
        {"EffectsHeader", "Effects"},
    };
    for (const auto &h : hdrs) {
        const QModelIndex idx = findCaptionIndex(h.first);
        if (idx.isValid())
            model->setData(idx, layerName + ": " + h.second);
            // model->setData(idx, h.second + " " + layerName);
    }
}

/* ----------------------------------------------------------------------------------------
   Value change handling
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::itemChange(QModelIndex idx)
{
    if (G::isLogger) G::log("DevelopProperties::itemChange");

    /* Ignore changes we drove ourselves while loading an image's saved values into the editors
       (setValue emits editorValueChanged -> itemChanged). */
    if (isPopulating) return;

    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();

    /* Brush current settings (size/feather/flow/autoMask) set what the NEXT stroke uses; existing
       strokes keep their own snapshot, so these do NOT re-composite -- they just refresh the cursor
       and brush state in ImageView. (Invert still flips the whole mask -- handled below.) */
    if (source == "maskSize" || source == "maskFlow" || source == "maskAutoMask" ||
        source == "maskAutoMaskAi" ||
        (source == "maskFeather" && activeMaskTool() == int(MaskTool::Brush))) {
        EditLayer *l = activeLayer();
        if (l && selectedMaskIndex >= 0 && selectedMaskIndex < l->masks.size()) {
            MaskComponent &mm = l->masks[selectedMaskIndex];
            if      (source == "maskFeather")  mm.feather = v.toFloat();
            else if (source == "maskSize")     mm.paramsJson = brushWith(mm.paramsJson, "size", v.toInt());
            else if (source == "maskFlow")     mm.paramsJson = brushWith(mm.paramsJson, "flow", v.toInt());
            else if (source == "maskAutoMask") mm.paramsJson = brushWith(mm.paramsJson, "autoMask", v.toBool());
            else if (source == "maskAutoMaskAi") {
                mm.paramsJson = brushWith(mm.paramsJson, "autoMaskMode", v.toBool() ? "ai" : "lum");
                if (v.toBool()) emit maskBrushAiEnabled();   // pre-warm the SAM encoder
            }
            dirty.insert(currentImagePath);
            emitBrushSettings(mm);
            if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
        }
        return;
    }

    /* Content-range tool settings (Luminance Range lo/hi, Color Range refine) live in the active
       component's paramsJson. They change coverage, so re-composite AND live-update the loupe tint. */
    if (source == "maskRangeLo" || source == "maskRangeHi" || source == "maskRefine") {
        EditLayer *l = activeLayer();
        if (l && selectedMaskIndex >= 0 && selectedMaskIndex < l->masks.size()) {
            MaskComponent &mm = l->masks[selectedMaskIndex];
            if      (source == "maskRangeLo") mm.paramsJson = brushWith(mm.paramsJson, "lo", v.toDouble() / 100.0);
            else if (source == "maskRangeHi") mm.paramsJson = brushWith(mm.paramsJson, "hi", v.toDouble() / 100.0);
            else                              mm.paramsJson = brushWith(mm.paramsJson, "refine", v.toInt());
            dirty.insert(currentImagePath);
            emit maskRangeChanged(mm.paramsJson);   // live-update the loupe tint
            emit paramsChanged();                   // re-composite the masked layer
            if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
        }
        return;
    }

    /* The selected mask tool's settings write into the active layer's mask model. Feather/Invert
       change the mask, so they update the live overlay AND re-composite the masked layer. */
    if (source == "maskFeather" || source == "maskInvert") {
        EditLayer *l = activeLayer();
        if (l && selectedMaskIndex >= 0 && selectedMaskIndex < l->masks.size()) {
            if (source == "maskFeather") {
                l->masks[selectedMaskIndex].feather = v.toFloat();
                emit maskFeatherChanged(v.toDouble());      // live-update the overlay ramp
            }
            else {
                l->masks[selectedMaskIndex].inverted = v.toBool();
                emit maskInvertChanged(v.toBool());         // live-flip the overlay
            }
            dirty.insert(currentImagePath);
            emit paramsChanged();                           // re-composite the masked layer
            if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
        }
        return;
    }

    /* The "Demosaic" combo (raw-only, Base) selects the RAW decode ENGINE (Apple Core Image vs the
       in-house Winnow decoder). It is not an EditParams value -- it forces a re-decode -- so handle
       it here (emit to MW) and skip applyKeyToParams / the normal preview render. */
    if (source == "demosaic") {
        emit demosaicEngineChanged(v.toString() == "Apple");   // MW sets decodeRawEngine
        /* The engine drives whether the raw-denoise children (and so the expand arrow)
           exist -- Winnow yes, Apple no. Rebuild the tree to add/remove them, but DEFER
           it: doing it inline would delete this combo's live editor mid-signal.
           G::decodeRawEngine is set synchronously by the MW handler above, so the
           rebuild sees the new engine. */
        QTimer::singleShot(0, this, [this]{ buildTree(); });
        return;
    }

    /* Write the changed adjustment into the CURRENT IMAGE's active-layer params, mark it dirty,
       and drive the live preview. Persistence to the sidecar happens on navigate-away / quit /
       pre-op (always) and, if G::isDevelopDebounceWrite, a short time after edits settle. */
    if (!source.isEmpty()) {
        if (currentImagePath.isEmpty()) return;
        /* This slider modifies the masked pixels (not the mask itself). If a mask overlay
           is shown, hide its red coverage so the user sees the effect. Covers all Basic/
           Color/Effects sliders and any future adjustment key via applyKeyToParams. */
        if (maskOverlayActive()) emit maskTintHideRequested();
        applyKeyToParams(source, v, activeParams());
        dirty.insert(currentImagePath);
        emit paramsChanged();
        if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
    }
}

void DevelopProperties::applyKeyToParams(const QString &key, const QVariant &v, EditParams &p)
{
    /* Slider EditRole carries the real (div-scaled) value, e.g. exposure in EV, or 0..100 for the
       denoise sliders. Map the dock key onto the matching EditParams field. */
    const float f = v.toFloat();
    if      (key == "temp")       p.temp       = f;
    else if (key == "tint")       p.tint       = f;
    else if (key == "exposure")   p.exposure   = f;
    else if (key == "contrast")   p.contrast   = f;
    else if (key == "highlights") p.highlights = f;
    else if (key == "shadows")    p.shadows    = f;
    else if (key == "whites")     p.whites     = f;
    else if (key == "blacks")     p.blacks     = f;
    else if (key == "texture")    p.texture    = f;
    else if (key == "dehaze")     p.dehaze     = f;
    else if (key == "red")        p.red        = f;
    else if (key == "green")      p.green      = f;
    else if (key == "blue")       p.blue       = f;
    else if (key == "hue")        p.hue        = f;
    else if (key == "saturation") p.saturation = f;
    else if (key == "vibrance")   p.vibrance   = f;
    else if (key == "luminance")  p.luminance  = f;
    else if (key == "denoiseLuma")   p.denoiseLuma   = f / 100.0f;   // Base "Denoise raw" (0..100 -> 0..1)
    else if (key == "denoiseChroma") p.denoiseChroma = f / 100.0f;
    else if (key == "localDenoise")       p.localDenoiseLuma   = f / 100.0f;  // "Denoise"
    else if (key == "localDenoiseChroma") p.localDenoiseChroma = f / 100.0f;
    else if (key == "vignetteExposure")   p.vignetteExposure   = f;           // EV
    else if (key == "vignetteFeather")    p.vignetteFeather    = f / 100.0f;
    else if (key == "grainAmount")        p.grainAmount        = f / 100.0f;   // "Grain"
    else if (key == "grainSize")          p.grainSize          = f / 100.0f;
    else if (key == "grainRoughness")     p.grainRoughness     = f / 100.0f;
}

EditParams DevelopProperties::editParams()
{
    /* The renderer shows the ACTIVE layer (no mask/opacity compositing yet). */
    if (currentImagePath.isEmpty()) return EditParams();
    const EditStack s = stackCache.value(currentImagePath);
    if (s.layers.isEmpty()) return EditParams();
    const int idx = (activeLayerIndex >= 0 && activeLayerIndex < s.layers.size()) ? activeLayerIndex : 0;
    return effectiveLayerParams(s.layers[idx]);   // fold previewed-off groups to identity
}

DevelopProperties::StackRenderJob DevelopProperties::stackJob()
{
    /* Capture the WHOLE stack as plain values (GUI thread; also handed to the off-thread full-res
       render). The render shows every enabled layer regardless of which one is active for editing,
       so a saved mask is visible the moment the image loads. */
    StackRenderJob job;
    if (currentImagePath.isEmpty()) return job;
    const EditStack s = stackCache.value(currentImagePath);
    /* Transform Preview: previewed off -> bypass geometry at render (identity), while the stored
       crop/warp stays in the cache so the overlay still draws (see currentGeometry). */
    job.geometry = s.geometry.show ? s.geometry : Geometry();
    job.spots    = s.spots;              // healed before geometry (Base-only too)
    if (s.layers.isEmpty()) return job;
    job.base = effectiveLayerParams(s.layers[0]);   // Base (layer 0), previewed groups folded off
    for (int i = 1; i < s.layers.size(); ++i) {
        const EditLayer &l = s.layers[i];
        if (!l.enabled) continue;
        const EditParams ep = effectiveLayerParams(l);
        if (ep.isIdentity()) continue;          // no (visible) adjustment -> nothing to composite
        StackRenderJob::Layer lj;
        lj.params  = ep;
        lj.combine = l.combine;
        for (const MaskComponent &m : l.masks) {
            if (!m.enabled) continue;
            /* Content-derived AI masks (Select Subject/Sky/Background) carry NO paramsJson -- their
               coverage is the model's map, not stored geometry -- so the empty-params skip must not
               drop them (it still drops a geometry tool that has not been placed yet). */
            const bool contentDerived = (m.tool == int(MaskTool::Subject) ||
                                         m.tool == int(MaskTool::Sky) ||
                                         m.tool == int(MaskTool::Background));
            if (m.paramsJson.isEmpty() && !contentDerived) continue;
            lj.masks.append(m);
        }
        job.layers.append(lj);
    }
    return job;
}

Geometry DevelopProperties::currentGeometry() const
{
    if (currentImagePath.isEmpty()) return Geometry();
    return stackCache.value(currentImagePath).geometry;
}

void DevelopProperties::setCurrentGeometry(const Geometry &g)
{
    if (currentImagePath.isEmpty()) return;
    EditStack &s = stackCache[currentImagePath];
    if (s.layers.isEmpty()) s.layers.append(EditLayer());   // keep the stack shape consistent
    s.geometry = g;
    dirty.insert(currentImagePath);
    if (debounceWriteTimer) debounceWriteTimer->start(kDebounceWriteMs);
}

QStringList DevelopProperties::currentLayerNames() const
{
    QStringList names;
    if (!currentImagePath.isEmpty()) {
        const EditStack s = stackCache.value(currentImagePath);
        for (const EditLayer &l : s.layers) names << l.name;
    }
    if (names.isEmpty()) names << "Base";    // combo always has at least one entry
    return names;
}

EditParams &DevelopProperties::activeParams()
{
    /* Caller guarantees currentImagePath is non-empty. Ensures a layer exists and the index is
       in range, then returns the active layer's params for in-place editing. */
    EditStack &s = stackCache[currentImagePath];
    if (s.layers.isEmpty()) s.layers.append(EditLayer());
    if (activeLayerIndex < 0 || activeLayerIndex >= s.layers.size()) activeLayerIndex = 0;
    return s.layers[activeLayerIndex].params;
}

void DevelopProperties::refreshLayerCombo()
{
    layerList = currentLayerNames();
    if (activeLayerIndex < 0 || activeLayerIndex >= layerList.size()) activeLayerIndex = 0;
    /* The dropdown lives in the LayerHeader widget; push the names + selection (no signal). */
    if (layerHeader) layerHeader->setLayers(layerList, activeLayerIndex);
    updateMaskMenuBtn();        // hide [M] + disable rename/remove on the Base layer
    updateSectionHeaderCaptions();
}

void DevelopProperties::updateMaskMenuBtn()
{
    /* The Base layer (index 0) applies globally: no mask [M], and it cannot be renamed or removed. */
    if (layerHeader) layerHeader->setBaseActive(activeLayerIndex == 0);
}

/* ----------------------------------------------------------------------------------------
   Per-image edit state (load / save / populate)
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::setCurrentImage(const QString &fPath)
{
    if (G::isLogger) G::log("DevelopProperties::setCurrentImage", fPath);
    if (fPath == currentImagePath) return;

    flushImage(currentImagePath);              // persist edits to the image we are leaving
    currentImagePath = fPath;
    if (fPath.isEmpty()) return;

    /* Load the stack on first touch (cheap: no sidecar => fast empty read), then cache it. */
    if (!stackCache.contains(fPath))
        stackCache.insert(fPath, EditStack::fromBase64(Metadata::readDevelopSidecar(fPath)));

    /* Ensure at least one layer so the combo + active editing have something to point at. An
       all-identity stack is still treated as identity (not persisted), so this never creates a
       sidecar for an unedited image. */
    EditStack &s = stackCache[fPath];
    if (s.layers.isEmpty()) s.layers.append(EditLayer());
    s.layers[0].name = "Base";                 // index 0 is always the (un-removable) Base layer
    activeLayerIndex = 0;
    selectedMaskIndex = -1;

    refreshLayerCombo();
    buildTree();                   // Base active: Core rows + Basic/Color/Effects
    if (spotMode) emitSpotPins();  // spot tool armed across a nav -> refresh pins
}

bool DevelopProperties::currentIsIdentity() const
{
    if (currentImagePath.isEmpty()) return true;
    return stackCache.value(currentImagePath).isIdentity();
}

void DevelopProperties::flushImage(const QString &fPath)
{
    if (fPath.isEmpty() || !dirty.contains(fPath)) return;
    dirty.remove(fPath);
    const EditStack &s = stackCache[fPath];
    /* Identity => write "" so writeDevelopSidecar clears the attribute (or skips if no sidecar),
       rather than persisting an empty edit. */
    const QString blob = s.isIdentity() ? QString() : s.toBase64();
    Metadata::writeDevelopSidecar(fPath, blob);   // synchronous; sidecar is a few KB, never on a drag
}

void DevelopProperties::flushAll()
{
    const QList<QString> paths = dirty.values();
    for (const QString &p : paths) flushImage(p);
}

void DevelopProperties::populateSlidersFromStack()
{
    EditParams p;
    const EditStack s = stackCache.value(currentImagePath);
    if (!s.layers.isEmpty()) {
        const int idx = (activeLayerIndex >= 0 && activeLayerIndex < s.layers.size()) ? activeLayerIndex : 0;
        p = s.layers[idx].params;
    }
    isPopulating = true;
    setSliderReal("temp",       p.temp);
    setSliderReal("tint",       p.tint);
    setSliderReal("exposure",   p.exposure);
    setSliderReal("contrast",   p.contrast);
    setSliderReal("highlights", p.highlights);
    setSliderReal("shadows",    p.shadows);
    setSliderReal("whites",     p.whites);
    setSliderReal("blacks",     p.blacks);
    setSliderReal("texture",    p.texture);
    setSliderReal("dehaze",     p.dehaze);
    setSliderReal("red",        p.red);
    setSliderReal("green",      p.green);
    setSliderReal("blue",       p.blue);
    setSliderReal("hue",        p.hue);
    setSliderReal("saturation", p.saturation);
    setSliderReal("vibrance",   p.vibrance);
    setSliderReal("luminance",  p.luminance);
    setSliderReal("denoiseLuma",   p.denoiseLuma   * 100.0);      // Base "Denoise raw" (0..1 -> 0..100)
    setSliderReal("denoiseChroma", p.denoiseChroma * 100.0);
    setSliderReal("localDenoise",       p.localDenoiseLuma   * 100.0);   // "Denoise"
    setSliderReal("localDenoiseChroma", p.localDenoiseChroma * 100.0);
    setSliderReal("vignetteExposure",   p.vignetteExposure);            // "Vignette" (EV)
    setSliderReal("vignetteFeather",    p.vignetteFeather    * 100.0);
    setSliderReal("grainAmount",        p.grainAmount        * 100.0);   // "Grain"
    setSliderReal("grainSize",          p.grainSize          * 100.0);
    setSliderReal("grainRoughness",     p.grainRoughness     * 100.0);
    if (toneSlider)
        toneSlider->setPositions(p.toneShadowCenter, p.toneCrossover, p.toneHighlightCenter);
    isPopulating = false;
    refreshPreviewButtons();        // sync the eye icons to this layer's Preview flags
}

void DevelopProperties::setSliderReal(const QString &key, double real)
{
    const QModelIndex vIdx = sourceIdx.value(key);
    if (!vIdx.isValid()) return;
    if (vIdx.data(UR_Editor).value<void*>() == nullptr) return;   // editor not realized
    /* SliderEditor::setValue takes the raw slider int (real * div; div 0 => 1). */
    const int div = vIdx.data(UR_Div).toInt();
    const int factor = (div == 0) ? 1 : div;
    setItemValue(vIdx, static_cast<int>(qRound(real * factor)));
}

void DevelopProperties::setCheckboxValue(const QString &key, bool on)
{
    const QModelIndex vIdx = sourceIdx.value(key);
    if (!vIdx.isValid()) return;
    if (vIdx.data(UR_Editor).value<void*>() == nullptr) return;
    setItemValue(vIdx, on);
}

QString DevelopProperties::diagnostics()
{
    QString rpt;
    QTextStream rs(&rpt);
    rs << "Develop layers: " << layerList.join(", ") << "\n";
    rs << "Current layer: " << layerName << "\n";
    return rpt;
}

void DevelopProperties::howThisWorks()
{
    if (G::isLogger) G::log("DevelopProperties::howThisWorks");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Develop Module",
                   ":/Docs/develophelp.html",
                   QSize(700, 600), r, window());
}
