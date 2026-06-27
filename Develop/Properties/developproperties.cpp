#include "Develop/Properties/developproperties.h"
#include "Main/mainwindow.h"
#include "Main/global.h"

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
    /* The Layers combo now lists the CURRENT IMAGE's layers (per-image EditStack), not app-global
       QSettings presets. Seed one name so the combo is valid before any image is selected. */
    layerList = QStringList() << "Layer 1";
    layerName = "Layer 1";
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
        layerName = "Layer 1";
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
    layersIdx = model->index(_layers, 0, root);
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
    layerListEditor = static_cast<ComboBoxEditor*>(addItem(i));

    QModelIndex idx = sourceIdx["layerList"];
    model->setData(idx, currentLayerNames().value(activeLayerIndex));
    propertyDelegate->setEditorData(layerListEditor, idx);
}

void DevelopProperties::addLayerItems()
{
    if (G::isLogger) G::log("DevelopProperties::addLayerItems");
    addBasic();
    addEffects();
}

void DevelopProperties::newLayer()
{
    if (G::isLogger) G::log("DevelopProperties::newLayer");
    if (currentImagePath.isEmpty()) return;

    EditStack &s = stackCache[currentImagePath];
    if (s.layers.isEmpty()) s.layers.append(EditLayer());     // ensure a base layer exists
    EditLayer l;
    l.name = uniqueLayerName("Layer " + QString::number(s.layers.size() + 1));
    s.layers.append(l);
    activeLayerIndex = s.layers.size() - 1;                   // edit the new layer
    dirty.insert(currentImagePath);

    /* New layer is identity, so the rendered result is unchanged; just refresh the combo and
       show the (zeroed) sliders. The stack only persists once a layer actually changes a pixel. */
    refreshLayerCombo();
    populateSlidersFromStack();
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
}

void DevelopProperties::deleteLayer()
{
    if (G::isLogger) G::log("DevelopProperties::deleteLayer");
    if (currentImagePath.isEmpty()) return;

    EditStack &s = stackCache[currentImagePath];
    if (s.layers.size() < 2) {
        emit centralMsg("At least one develop layer is required.");
        return;
    }
    if (activeLayerIndex < 0 || activeLayerIndex >= s.layers.size()) activeLayerIndex = 0;
    s.layers.removeAt(activeLayerIndex);
    if (activeLayerIndex >= s.layers.size()) activeLayerIndex = s.layers.size() - 1;
    dirty.insert(currentImagePath);

    refreshLayerCombo();
    populateSlidersFromStack();
    emit paramsChanged();                                    // removing a layer changes the result
    if (G::isDevelopDebounceWrite) debounceWriteTimer->start(kDebounceWriteMs);
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

    addCheckbox("denoise", "Denoise", "Apply noise reduction.", parIdx, "BasicHeader", false);

    QString silver = "#C0C0C0";
    QString lightgray = "#999999";
    QString darkgray = "#222222";

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
    addSlider("texture",    "Texture",    "Enhance or smooth fine detail.",      parIdx, "BasicHeader", -100, 100, 0,   G::darkblue, G::lightblue);
    addSlider("dehaze",     "Dehaze",     "Remove or add atmospheric haze.",     parIdx, "BasicHeader", -100, 100, 0,   G::darkblue, G::lightblue);
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
            populateSlidersFromStack();
            emit paramsChanged();
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

QStringList DevelopProperties::currentLayerNames() const
{
    QStringList names;
    if (!currentImagePath.isEmpty()) {
        const EditStack s = stackCache.value(currentImagePath);
        for (const EditLayer &l : s.layers) names << l.name;
    }
    if (names.isEmpty()) names << "Layer 1";    // combo always has at least one entry
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
    activeLayerIndex = 0;

    refreshLayerCombo();
    populateSlidersFromStack();
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
    setCheckboxValue("denoise", p.denoiseLuma > 0.0f);
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
