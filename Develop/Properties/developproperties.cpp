#include "Develop/Properties/developproperties.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include "Develop/Scopes/toneregionslider.h"

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
    addCoreHeader();
    /* The Layers combo now lists the CURRENT IMAGE's layers (per-image EditStack), not app-global
       QSettings presets. Seed one name so the combo is valid before any image is selected. */
    layerList = QStringList() << "Base";
    layerName = "Base";
    addLayersHeader();
    addLayerItems();

    collapseAll();
    expand(model->index(_layers, 0));
    expand(model->index(_basic, 0));
    updateHiddenRows(QModelIndex());
    setMouseTracking(true);

    /* Optional debounce write: flush the in-memory stack to the sidecar a short time after edits
       settle, so a crash loses less. Gated by G::isDevelopDebounceWrite; navigate-away / quit /
       pre-op flushes always run regardless. */
    debounceWriteTimer = new QTimer(this);
    debounceWriteTimer->setSingleShot(true);
    connect(debounceWriteTimer, &QTimer::timeout, this, [this]{ flushImage(currentImagePath); });

    /* Programmatic row selection reveals that mask tool's settings (clicks go via mousePressEvent,
       since the base PropertyEditor does not select on click). */
    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &, const QModelIndex &){ onMaskSelectionChanged(); });
}

void DevelopProperties::initialize()
{
    if (G::isLogger) G::log("DevelopProperties::initialize");

    setSolo(false);
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

void DevelopProperties::addCoreHeader()
{
    if (G::isLogger) G::log("DevelopProperties::addCoreHeader");

    i.name = "CoreHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.okToCollapseRoot = true;
    i.isDecoration = true;
    i.captionText = "Core";
    i.tooltip = "Applies to all layers.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;
    addItem(i);

    QModelIndex parIdx = capIdx;

    i.name = "demosaic";
    i.parentName = "CoreHeader";
    i.captionText = "Demosaic";
    i.tooltip = "Select demosaic engine.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = "Apple";
    i.key = "demosaic";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    i.dropList << "Apple"
               << "Winnow";
    addItem(i);

    addCheckbox("denoise", "Denoise", "Apply raw noise reduction.", parIdx, "CoreHeader", false);

}

void DevelopProperties::addLayersHeader()
{
    if (G::isLogger) G::log("DevelopProperties::addLayersHeader");

    /* Layers header (root row 0) with delete (-) and new (+) buttons, the same idiom
       as the Embellish Templates header. */
    clearItemInfo(i);
    i.name = "LayersHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.okToCollapseRoot = false;
    i.isDecoration = false;
    i.captionText = "Layers";
    i.tooltip = "A layer holds one set of develop adjustments.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.delegateType = DT_BarBtns;

    BarBtn *layerDeleteBtn = new BarBtn();
    layerDeleteBtn->setIcon(":/images/icon16/delete.png", G::iconOpacity);
    layerDeleteBtn->setToolTip("Delete the selected layer");
    connect(layerDeleteBtn, &BarBtn::clicked, this, &DevelopProperties::deleteLayer);
    btns.append(layerDeleteBtn);

    BarBtn *layerNewBtn = new BarBtn();
    layerNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    layerNewBtn->setToolTip("Create a new layer");
    connect(layerNewBtn, &BarBtn::clicked, this, &DevelopProperties::newLayer);
    btns.append(layerNewBtn);

    addItem(i);
    layersIdx = model->index(_layers, 1, root);
    QModelIndex parIdx = capIdx;

    /* Select layer combo. */
    clearItemInfo(i);
    i.name = "layerList";
    i.parIdx = parIdx;
    i.parentName = "LayersHeader";
    i.captionText = "Select layer";
    i.tooltip = "The layer whose adjustments are shown below (this image's layers).";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = currentLayerNames().value(activeLayerIndex);
    i.key = "layerList";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.color = "#1b8a83";
    i.dropList = currentLayerNames();

    /* [M] mask-menu button trailing the Select layer combo. The ComboBoxEditor drains the global
       `btns` vector (like BarBtnEditor), so queuing it here places it right after the combo. The
       menu content is built on click (showMaskMenu) so it can offer Subtract only once a tool
       exists. */
    maskMenu = new QMenu(this);
    maskMenuBtn = new BarBtn();
    maskMenuBtn->setText("M");
    maskMenuBtn->setToolTip("Add a mask tool (gradient, brush, range, AI select) to this layer");
    connect(maskMenuBtn, &BarBtn::clicked, this, &DevelopProperties::showMaskMenu);
    btns.append(maskMenuBtn);

    layerListEditor = static_cast<ComboBoxEditor*>(addItem(i));

    updateMaskMenuBtn();        // Base layer carries no mask, so [M] starts hidden

    QModelIndex idx = sourceIdx["layerList"];
    model->setData(idx, currentLayerNames().value(activeLayerIndex));
    propertyDelegate->setEditorData(layerListEditor, idx);
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

void DevelopProperties::addLayerItems()
{
    if (G::isLogger) G::log("DevelopProperties::addLayerItems");
    addBasic();
    addColor();
    addEffects();
}

void DevelopProperties::newLayer()
{
    if (G::isLogger) G::log("DevelopProperties::newLayer");
    if (currentImagePath.isEmpty()) return;

    EditStack &s = stackCache[currentImagePath];
    if (s.layers.isEmpty()) s.layers.append(EditLayer());     // ensure a base layer exists
    s.layers[0].name = "Base";                                // index 0 is always Base
    EditLayer l;
    /* Extra layers are numbered "Layer 1", "Layer 2", ... -- the first one above Base is "Layer 1"
       (Base is index 0, so the new layer's index equals the pre-append size). */
    l.name = uniqueLayerName("Layer " + QString::number(s.layers.size()));
    s.layers.append(l);
    activeLayerIndex = s.layers.size() - 1;                   // edit the new layer
    dirty.insert(currentImagePath);

    /* New layer is identity, so the rendered result is unchanged; just refresh the combo and
       show the (zeroed) sliders. The stack only persists once a layer actually changes a pixel. */
    selectedMaskIndex = -1;
    refreshLayerCombo();
    populateSlidersFromStack();
    rebuildMaskTools();
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
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
    populateSlidersFromStack();
    rebuildMaskTools();
    emit paramsChanged();                                    // removing a layer changes the result
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

/* ----------------------------------------------------------------------------------------
   Mask (one mask per non-Base layer, built from an ordered list of Add/Subtract tools)

   Tree shape, all children of the Layers header:

     Select layer   [Layer 2  v] [M]     <- combo (row 0) with the [M] mask-menu button trailing it
     Add Linear Gradient          [-]    <- one row per tool (caption = op + tool; [-] removes it)
     Subtract Brush               [-]    <- selected tool is expanded; its settings are ITS children
        Feather  [slider]
        Invert   [checkbox]
        Done     [checkbox]              <- ends the build (collapses the tool)

   Tool rows are header-style (single-line full-width caption + expand arrow) but flagged
   UR_LeafSingleLine so they paint in the leaf colour, not category teal. The [M] menu
   (showMaskMenu) appends a tool (newMask); Subtract is offered only once a tool exists ("the first
   must be added"). Only the SELECTED tool carries its settings children (no duplicate tool-name
   row); clicking a tool (mousePressEvent) toggles which one is expanded. Spatial editing (drag/
   paint/AI-select on the image) is deferred to the canvas; rendering does not yet composite the
   mask.
   ---------------------------------------------------------------------------------------- */

QString DevelopProperties::maskToolName(int tool)
{
    switch (static_cast<MaskTool>(tool)) {
    case MaskTool::LinearGradient:  return "Linear Gradient";
    case MaskTool::RadialGradient:  return "Radial Gradient";
    case MaskTool::Brush:           return "Brush";
    case MaskTool::ColorRange:      return "Color Range";
    case MaskTool::LuminanceRange:  return "Luminance Range";
    case MaskTool::Subject:         return "Select Subject";
    case MaskTool::Sky:             return "Select Sky";
    case MaskTool::Background:      return "Select Background";
    case MaskTool::Depth:           return "Depth Range";
    }
    return "Mask";
}

int DevelopProperties::maskToolFromName(const QString &name)
{
    for (int t = 0; t <= int(MaskTool::Depth); ++t)
        if (maskToolName(t) == name) return t;
    return int(MaskTool::LinearGradient);
}

QString DevelopProperties::opName(int op)
{
    return (op == int(MaskOp::Subtract)) ? "Subtract" : "Add";
}

EditLayer *DevelopProperties::activeLayer()
{
    if (currentImagePath.isEmpty()) return nullptr;
    EditStack &s = stackCache[currentImagePath];
    if (activeLayerIndex < 0 || activeLayerIndex >= s.layers.size()) return nullptr;
    return &s.layers[activeLayerIndex];
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
    for (int t = 0; t <= int(MaskTool::Depth); ++t) {
        QAction *a = maskMenu->addAction("Add " + maskToolName(t));
        connect(a, &QAction::triggered, this, &DevelopProperties::newMask);
        if (t == int(MaskTool::Brush) || t == int(MaskTool::LuminanceRange))
            maskMenu->addSeparator();           // group geometric / range / AI tools
    }
    if (!layer->masks.isEmpty()) {
        maskMenu->addSeparator();
        for (int t = 0; t <= int(MaskTool::Depth); ++t) {
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
       tool name (handles multi-word names like "Select Subject"). */
    const QString txt = a->text();
    MaskComponent m;
    m.op   = txt.startsWith("Subtract") ? int(MaskOp::Subtract) : int(MaskOp::Add);
    m.tool = maskToolFromName(txt.section(' ', 1));
    m.paramsJson = defaultMaskParams(m.tool);
    layer->masks.append(m);
    selectedMaskIndex = layer->masks.size() - 1;            // start editing the new tool
    dirty.insert(currentImagePath);

    rebuildMaskTools();
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

    rebuildMaskTools();
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
    i.tooltip = "Mask tool. Click to show/hide its settings; [-] to remove.";
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
    setFirstColumnSpanned(toolIdx.row(), parIdx, true);

    /* The selected tool reveals its settings as its OWN children (no duplicate tool-name row).
       Clicking the tool caption again collapses it (see mousePressEvent), so no separate Done row is
       needed. Gradients: Feather + Invert. Brush: Size, Feather, Flow, Auto mask, Invert. */
    if (selected) {
        if (m.tool == int(MaskTool::Brush)) {
            addSlider("maskSize", "Size", "Brush diameter (% of the long edge).",
                      toolIdx, "", 1, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFeather", "Feather", "Soften the brush edge.",
                      toolIdx, "", 0, 100, 0, G::darkgray, G::lightgray);
            addSlider("maskFlow", "Flow", "How much each stroke builds up.",
                      toolIdx, "", 1, 100, 0, G::darkgray, G::lightgray);
            addCheckbox("maskAutoMask", "Auto mask", "Limit the brush to similar areas (coming soon).",
                        toolIdx, "", false);
            addCheckbox("maskInvert", "Invert", "Invert this mask's contribution.", toolIdx, "", false);
            setSliderReal("maskSize", brushNum(m.paramsJson, "size", 20));
            setSliderReal("maskFeather", m.feather);
            setSliderReal("maskFlow", brushNum(m.paramsJson, "flow", 50));
            setCheckboxValue("maskAutoMask", brushBool(m.paramsJson, "autoMask", false));
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

void DevelopProperties::rebuildMaskTools()
{
    if (G::isLogger) G::log("DevelopProperties::rebuildMaskTools");
    QModelIndex lh = findCaptionIndex("LayersHeader");
    if (!lh.isValid()) return;

    isRebuildingMasks = true;
    isPopulating = true;

    /* Clear everything under the Layers header EXCEPT row 0 (the "Select layer" combo). */
    if (model->rowCount(lh) > 1)
        model->removeRows(1, model->rowCount(lh) - 1, lh);

    EditLayer *layer = activeLayer();
    if (layer && activeLayerIndex > 0) {                    // Base layer carries no mask
        const int n = layer->masks.size();
        if (selectedMaskIndex >= n) selectedMaskIndex = n - 1;
        for (int m = 0; m < n; ++m)
            addToolRow(lh, m, layer->masks[m], m == selectedMaskIndex);
        expand(lh);
    }
    else {
        selectedMaskIndex = -1;
    }

    isPopulating = false;
    isRebuildingMasks = false;

    updateMaskEdit();       // begin/end the ImageView overlay for the (now) active mask tool
}

void DevelopProperties::setSelectedMask(int index)
{
    selectedMaskIndex = index;
    rebuildMaskTools();
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
        o["flow"] = 50;
        o["autoMask"] = false;
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
    if (m.tool != int(MaskTool::Brush)) return;
    const int s = qBound(1, int(size + 0.5), 100);
    m.paramsJson = brushWith(m.paramsJson, "size", s);
    dirty.insert(currentImagePath);
    isPopulating = true;                  // sync the Size slider without re-entering itemChange
    setSliderReal("maskSize", s);
    isPopulating = false;
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::emitBrushSettings(const MaskComponent &m)
{
    emit maskBrushSettingsChanged(brushNum(m.paramsJson, "size", 20),
                                  m.feather,
                                  brushNum(m.paramsJson, "flow", 50),
                                  brushBool(m.paramsJson, "autoMask", false));
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
            m.tool == int(MaskTool::Brush)) {
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
            const QVariant v = idx.siblingAtColumn(0).data(UR_MaskIndex);
            if (v.isValid()) {
                const int m = v.toInt();
                /* The [-] remove glyph is delegate-drawn at the row's right edge (must mirror the
                   geometry in PropertyDelegate::paint for UR_DeleteBtn). A click there removes the
                   tool; a click anywhere else on the row toggles its settings. */
                const QRect rowRect = visualRect(idx);
                const int sz = 16;
                const QRect delRect(rowRect.right() - sz - 4,
                                    rowRect.top() + (rowRect.height() - sz)/2, sz, sz);
                if (delRect.contains(event->pos())) {
                    deleteMask(m);
                    return;
                }
                setSelectedMask(m == selectedMaskIndex ? -1 : m);   // click open tool to collapse
                return;
            }
        }
    }
    PropertyEditor::mousePressEvent(event);
}

/* ----------------------------------------------------------------------------------------
   Item builders
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::addHeader(const QString &name, const QString &caption, const QString &tooltip)
{
    clearItemInfo(i);
    i.name = name;
    i.parentName = "???";
    i.isHeader = true;
    i.decorateGradient = true;
    i.isDecoration = true;
    i.captionText = caption;
    i.tooltip = tooltip;
    i.hasValue = false;
    i.captionIsEditable = false;
    i.path = "Develop/" + name;
    i.delegateType = DT_None;
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
    addHeader("BasicHeader", "Basic", "Core tone, white balance and presence adjustments.");
    QModelIndex parIdx = capIdx;

    /* Lightroom-like ranges. Most adjustments are integer sliders -100..100 (div 0).
       Exposure is a 2-decimal EV slider (-5.00..5.00, div 100). All default to 0
       (identity), matching EditParams. */
    addSlider("temp",       "Temp",       "White balance temperature.",          parIdx, "BasicHeader", -100, 100, 0,   G::lightblue, G::lightyellow);
    addSlider("tint",       "Tint",       "White balance tint (green/magenta).", parIdx, "BasicHeader", -100, 100, 0,   G::lightgreen, G::lightmagenta);
    addSlider("exposure",   "Exposure",   "Overall exposure in stops (EV).",     parIdx, "BasicHeader", -500, 500, 100, G::darkgray, G::lightgray);
    addSlider("contrast",   "Contrast",   "Global contrast.",                    parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("highlights", "Highlights", "Recover or lift the highlights.",     parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("shadows",    "Shadows",    "Recover or deepen the shadows.",      parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("whites",     "Whites",     "Set the white point.",                parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("blacks",     "Blacks",     "Set the black point.",                parIdx, "BasicHeader", -100, 100, 0,   G::darkgray, G::lightgray);
    addSlider("texture",    "Texture",    "Enhance or smooth fine detail.",      parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    addSlider("dehaze",     "Dehaze",     "Remove or add atmospheric haze.",     parIdx, "BasicHeader", -100, 100, 0,   G::darkyellow, G::lightyellow);
    addCheckbox("denoise", "Denoise", "Apply local luminace noise reduction.", parIdx, "BasicHeader", false);
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
    addHeader("ColorHeader", "Color", "RGB and HSL adjustments.");
    QModelIndex parIdx = capIdx;

    /* All integer sliders -100..100 (div 0), default 0 (identity), matching EditParams.
       RGB = per-channel gain; HSL = global hue rotation / saturation / luminance. */
    addSlider("red",        "Red",        "Per-channel red gain.",                 parIdx, "ColorHeader", -100, 100, 0, G::darkred,   G::lightred);
    addSlider("green",      "Green",      "Per-channel green gain.",               parIdx, "ColorHeader", -100, 100, 0, G::darkgreen, G::lightgreen);
    addSlider("blue",       "Blue",       "Per-channel blue gain.",                parIdx, "ColorHeader", -100, 100, 0, G::darkblue,  G::lightblue);
    addSlider("hue",        "Hue",        "Rotate all hues.",                      parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addSlider("saturation", "Saturation", "Global saturation (grey at -100).",     parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
    addSlider("luminance",  "Luminance",  "Global luminance (brightness).",        parIdx, "ColorHeader", -100, 100, 0, G::darkgray,  G::lightgray);
}

/* ----------------------------------------------------------------------------------------
   Effects
   ---------------------------------------------------------------------------------------- */

void DevelopProperties::addEffects()
{
    if (G::isLogger) G::log("DevelopProperties::addEffects");
    addHeader("EffectsHeader", "Effects", "Creative effects (to be added).");
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

    if (source == "layerList") {
        /* Switch the active layer of the CURRENT IMAGE: repopulate the sliders from that layer
           and re-render (the renderer shows the active layer). No tree rebuild => no flicker. */
        if (currentImagePath.isEmpty()) return;
        const int idx2 = currentLayerNames().indexOf(v.toString());
        if (idx2 >= 0 && idx2 != activeLayerIndex) {
            activeLayerIndex = idx2;
            selectedMaskIndex = -1;
            populateSlidersFromStack();
            rebuildMaskTools();
            updateMaskMenuBtn();        // hide [M] on the Base layer
            emit paramsChanged();
        }
        return;
    }

    /* Brush current settings (size/feather/flow/autoMask) set what the NEXT stroke uses; existing
       strokes keep their own snapshot, so these do NOT re-composite -- they just refresh the cursor
       and brush state in ImageView. (Invert still flips the whole mask -- handled below.) */
    if (source == "maskSize" || source == "maskFlow" || source == "maskAutoMask" ||
        (source == "maskFeather" && activeMaskTool() == int(MaskTool::Brush))) {
        EditLayer *l = activeLayer();
        if (l && selectedMaskIndex >= 0 && selectedMaskIndex < l->masks.size()) {
            MaskComponent &mm = l->masks[selectedMaskIndex];
            if      (source == "maskFeather")  mm.feather = v.toFloat();
            else if (source == "maskSize")     mm.paramsJson = brushWith(mm.paramsJson, "size", v.toInt());
            else if (source == "maskFlow")     mm.paramsJson = brushWith(mm.paramsJson, "flow", v.toInt());
            else if (source == "maskAutoMask") mm.paramsJson = brushWith(mm.paramsJson, "autoMask", v.toBool());
            dirty.insert(currentImagePath);
            emitBrushSettings(mm);
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

    /* Write the changed adjustment into the CURRENT IMAGE's active-layer params, mark it dirty,
       and drive the live preview. Persistence to the sidecar happens on navigate-away / quit /
       pre-op (always) and, if G::isDevelopDebounceWrite, a short time after edits settle. */
    if (!source.isEmpty()) {
        if (currentImagePath.isEmpty()) return;
        applyKeyToParams(source, v, activeParams());
        dirty.insert(currentImagePath);
        emit paramsChanged();
        if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
    }
}

void DevelopProperties::applyKeyToParams(const QString &key, const QVariant &v, EditParams &p)
{
    /* Slider EditRole carries the real (div-scaled) value, e.g. exposure in EV; the denoise
       checkbox carries a bool. Map the dock key onto the matching EditParams field. */
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
    else if (key == "luminance")  p.luminance  = f;
    else if (key == "denoise")  { const bool on = v.toBool();
                                  p.denoiseLuma = on ? 1.0f : 0.0f;
                                  p.denoiseChroma = on ? 1.0f : 0.0f; }
}

EditParams DevelopProperties::editParams()
{
    /* The renderer shows the ACTIVE layer (no mask/opacity compositing yet). */
    if (currentImagePath.isEmpty()) return EditParams();
    const EditStack s = stackCache.value(currentImagePath);
    if (s.layers.isEmpty()) return EditParams();
    const int idx = (activeLayerIndex >= 0 && activeLayerIndex < s.layers.size()) ? activeLayerIndex : 0;
    return s.layers[idx].params;
}

DevelopProperties::StackRenderJob DevelopProperties::stackJob()
{
    /* Capture the WHOLE stack as plain values (GUI thread; also handed to the off-thread full-res
       render). The render shows every enabled layer regardless of which one is active for editing,
       so a saved mask is visible the moment the image loads. */
    StackRenderJob job;
    if (currentImagePath.isEmpty()) return job;
    const EditStack s = stackCache.value(currentImagePath);
    if (s.layers.isEmpty()) return job;
    job.base = s.layers[0].params;              // Base (layer 0), applied globally
    for (int i = 1; i < s.layers.size(); ++i) {
        const EditLayer &l = s.layers[i];
        if (!l.enabled) continue;
        if (l.params.isIdentity()) continue;    // no adjustment -> nothing to composite (skip)
        StackRenderJob::Layer lj;
        lj.params  = l.params;
        lj.combine = l.combine;
        for (const MaskComponent &m : l.masks)
            if (m.enabled && !m.paramsJson.isEmpty()) lj.masks.append(m);
        job.layers.append(lj);
    }
    return job;
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
    if (!layerListEditor) return;
    /* Guard so the combo's programmatic setValue does not re-enter itemChange. */
    isPopulating = true;
    layerListEditor->refresh(layerList);
    layerListEditor->setValue(layerList.value(activeLayerIndex));
    isPopulating = false;
    updateMaskMenuBtn();        // hide [M] on the Base layer
}

void DevelopProperties::updateMaskMenuBtn()
{
    /* The Base layer (index 0) carries no mask, so its [M] add-mask button is hidden. */
    if (maskMenuBtn) maskMenuBtn->setVisible(activeLayerIndex > 0);
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
    populateSlidersFromStack();
    rebuildMaskTools();
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
    setSliderReal("luminance",  p.luminance);
    setCheckboxValue("denoise", p.denoiseLuma > 0.0f);
    if (toneSlider)
        toneSlider->setPositions(p.toneShadowCenter, p.toneCrossover, p.toneHighlightCenter);
    isPopulating = false;
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
