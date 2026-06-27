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
    readLayerList();
    addLayersHeader();
    addLayerItems();

    collapseAll();
    expand(model->index(_layers, 0));
    expand(model->index(_basic, 0));
    updateHiddenRows(QModelIndex());
    setMouseTracking(true);
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
    QString unique = name;
    int n = 1;
    while (layerList.contains(unique)) {
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
    i.tooltip = "The layer whose adjustments are shown below.";
    i.isIndent = false;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = layerName;
    i.key = "layerList";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.color = "#1b8a83";
    i.dropList = layerList;
    layerListEditor = static_cast<ComboBoxEditor*>(addItem(i));

    QModelIndex idx = sourceIdx["layerList"];
    model->setData(idx, layerName);
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
    QString name = uniqueLayerName("Layer " + QString::number(layerList.size() + 1));
    layerList << name;
    setting->setValue("Develop/Layers/" + name + "/created", true);
    setCurrentLayer(name);

    /* Rebuild the combo's drop list and the Basic / Effects sections. */
    layerListEditor->refresh(layerList);
    layerListEditor->setValue(layerName);

    model->removeRows(1, model->rowCount(QModelIndex()) - 1, QModelIndex());
    addLayerItems();
    collapseAll();
    expand(model->index(_layers, 0));
    expand(model->index(_basic, 0));
    updateHiddenRows(QModelIndex());
}

void DevelopProperties::deleteLayer()
{
    if (G::isLogger) G::log("DevelopProperties::deleteLayer");
    if (layerList.size() < 2) {
        emit centralMsg("At least one develop layer is required.");
        return;
    }
    setting->remove("Develop/Layers/" + layerName);
    layerList.removeAll(layerName);
    setCurrentLayer(layerList.first());

    layerListEditor->refresh(layerList);
    layerListEditor->setValue(layerName);

    model->removeRows(1, model->rowCount(QModelIndex()) - 1, QModelIndex());
    addLayerItems();
    collapseAll();
    expand(model->index(_layers, 0));
    expand(model->index(_basic, 0));
    updateHiddenRows(QModelIndex());
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
    i.path = layerRootPath() + key;
    i.value = setting->contains(i.path) ? setting->value(i.path) : i.defaultValue;
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
    i.path = layerRootPath() + key;
    i.value = setting->contains(i.path) ? setting->value(i.path) : i.defaultValue;
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
    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();

    if (source == "layerList") {
        setCurrentLayer(v.toString());
        model->removeRows(1, model->rowCount(QModelIndex()) - 1, QModelIndex());
        addLayerItems();
        collapseAll();
        expand(model->index(_layers, 0));
        expand(model->index(_basic, 0));
        updateHiddenRows(QModelIndex());
        return;
    }

    /* Persist the changed adjustment under the current layer. */
    if (!source.isEmpty()) {
        setting->setValue(layerRootPath() + source, v);
        emit paramsChanged();
    }
}

EditParams DevelopProperties::editParams()
{
    EditParams p;
    p.temp       = static_cast<float>(layerValue("temp"));
    p.tint       = static_cast<float>(layerValue("tint"));
    p.exposure   = static_cast<float>(layerValue("exposure"));
    p.contrast   = static_cast<float>(layerValue("contrast"));
    p.highlights = static_cast<float>(layerValue("highlights"));
    p.shadows    = static_cast<float>(layerValue("shadows"));
    p.whites     = static_cast<float>(layerValue("whites"));
    p.blacks     = static_cast<float>(layerValue("blacks"));
    p.texture    = static_cast<float>(layerValue("texture"));
    p.dehaze     = static_cast<float>(layerValue("dehaze"));
    if (setting->value(layerRootPath() + "denoise").toBool()) {
        p.denoiseLuma = 1.0f;
        p.denoiseChroma = 1.0f;
    }
    return p;
}

QString DevelopProperties::diagnostics()
{
    QString rpt;
    QTextStream rs(&rpt);
    rs << "Develop layers: " << layerList.join(", ") << "\n";
    rs << "Current layer: " << layerName << "\n";
    return rpt;
}
