#include "Develop/Properties/maskeditor.h"
#include "Develop/Properties/colorrangewheel.h"
#include "PropertyEditor/propertywidgets.h"     // SliderEditor
#include "Main/global.h"

#include <QScrollBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QVariantAnimation>

/* Read a numeric field from a mask tool's paramsJson (0 if absent). */
static double pnum(const QJsonObject &o, const QString &key, double def)
{
    return o.contains(key) ? o.value(key).toDouble(def) : def;
}

MaskEditor::MaskEditor(QWidget *parent) : PropertyEditor(parent)
{
    if (G::isLogger) G::log("MaskEditor::MaskEditor");

    /* Same chrome as the Develop tree so the rows render identically. */
    setIndentation(10);
    setAlternatingRowColors(false);
    setMouseTracking(false);
    setHeaderHidden(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setAutoFillBackground(false);

    stringToFitCaptions = "=captions column=";
    stringToFitValues   = "======values column======";
    resizeColumns();
}

void MaskEditor::setCaptionWidth(int w)
{
    if (w > 0) setColumnWidth(CapColumn, w);
}

/* ---- generic tree-row builders (mirror DevelopProperties, at root level) ---- */

void MaskEditor::addSlider(const QString &key, const QString &caption,
                           const QString &tooltip, int min, int max)
{
    clearItemInfo(i);
    i.name = key;
    i.parIdx = QModelIndex();
    i.parentName = "";
    i.captionText = caption;
    i.tooltip = tooltip;
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = key;
    i.defaultValue = 0;
    i.path = "";
    i.value = i.defaultValue;
    i.delegateType = DT_Slider;
    i.type = "int";                 // div 0 = integer slider (0..100)
    i.min = min;
    i.max = max;
    i.div = 0;
    i.logScale = false;
    i.step = 1;
    i.color = G::darkgray;
    i.color1 = G::lightgray;
    i.fixedWidth = 50;
    addItem(i);
}

void MaskEditor::addCheckbox(const QString &key, const QString &caption, const QString &tooltip)
{
    clearItemInfo(i);
    i.name = key;
    i.parIdx = QModelIndex();
    i.parentName = "";
    i.captionText = caption;
    i.tooltip = tooltip;
    i.isIndent = true;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.key = key;
    i.defaultValue = false;
    i.path = "";
    i.value = i.defaultValue;
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
}

void MaskEditor::setSliderReal(const QString &key, double real)
{
    const QModelIndex vIdx = sourceIdx.value(key);
    if (!vIdx.isValid()) return;
    if (vIdx.data(UR_Editor).value<void*>() == nullptr) return;   // editor not realized
    const int div = vIdx.data(UR_Div).toInt();
    const int factor = (div == 0) ? 1 : div;
    setItemValue(vIdx, static_cast<int>(qRound(real * factor)));
}

void MaskEditor::setCheckboxValue(const QString &key, bool on)
{
    const QModelIndex vIdx = sourceIdx.value(key);
    if (!vIdx.isValid()) return;
    if (vIdx.data(UR_Editor).value<void*>() == nullptr) return;
    setItemValue(vIdx, on);
}

int MaskEditor::sliderInt(const QString &key) const
{
    const QModelIndex vIdx = sourceIdx.value(key);
    return vIdx.isValid() ? vIdx.data(Qt::EditRole).toInt() : 0;
}

/* ---- build a tool's settings ---- */

void MaskEditor::showTool(const MaskComponent &m)
{
    isPopulating = true;
    wheel = nullptr;                 // freed with its row by removeRows/close below

    if (model->rowCount() > 0) {
        close(QModelIndex());
        model->removeRows(0, model->rowCount());
    }
    sourceIdx.clear();

    const QJsonObject o = QJsonDocument::fromJson(m.paramsJson.toUtf8()).object();
    const int feather = int(m.feather + 0.5f);

    switch (m.tool) {
    case int(MaskTool::Brush):
        addSlider("maskSize", "Size", "Brush diameter (% of the long edge).", 1, 100);
        addSlider("maskFeather", "Feather", "Soft edge outside the brush size (0 = crisp).", 0, 100);
        addSlider("maskFlow", "Flow", "How much each stroke builds up.", 1, 100);
        addCheckbox("maskAutoMask", "Auto mask",
                    "Limit the brush to the edge under the stroke start. Toggle with A.");
        addCheckbox("maskAutoMaskAi", "AI edge (SAM)",
                    "On = SAM object under the stroke start; off = similar-luminance band.");
        addCheckbox("maskInvert", "Invert", "Invert this mask's contribution.");
        setSliderReal("maskSize", pnum(o, "size", 20));
        setSliderReal("maskFeather", feather);
        setSliderReal("maskFlow", pnum(o, "flow", 100));
        setCheckboxValue("maskAutoMask", o.value("autoMask").toBool(false));
        setCheckboxValue("maskAutoMaskAi", o.value("autoMaskMode").toString("lum") == "ai");
        setCheckboxValue("maskInvert", m.inverted);
        break;

    case int(MaskTool::LuminanceRange):
        addSlider("maskRangeLo", "Range Min", "Lower luminance bound of the band.", 0, 100);
        addSlider("maskRangeHi", "Range Max", "Upper luminance bound of the band.", 0, 100);
        addSlider("maskFeather", "Feather", "Soften the band edges.", 0, 100);
        addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.");
        setSliderReal("maskRangeLo", pnum(o, "lo", 0.25) * 100.0);
        setSliderReal("maskRangeHi", pnum(o, "hi", 0.75) * 100.0);
        setSliderReal("maskFeather", feather);
        setCheckboxValue("maskInvert", m.inverted);
        break;

    case int(MaskTool::Depth):
        addSlider("maskRangeLo", "Near", "Near bound of the depth band (0 = nearest).", 0, 100);
        addSlider("maskRangeHi", "Far", "Far bound of the depth band (100 = farthest).", 0, 100);
        addSlider("maskFeather", "Feather", "Soften the depth-band edges.", 0, 100);
        addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.");
        setSliderReal("maskRangeLo", pnum(o, "lo", 0.0) * 100.0);
        setSliderReal("maskRangeHi", pnum(o, "hi", 0.5) * 100.0);
        setSliderReal("maskFeather", feather);
        setCheckboxValue("maskInvert", m.inverted);
        break;

    case int(MaskTool::ColorRange):
        addColorRangeWheel(m);
        addSlider("maskHueLo", "Hue Lo", "Widen the selection toward LOWER hues.", 0, 90);
        addSlider("maskHueHi", "Hue Hi", "Widen the selection toward HIGHER hues.", 0, 90);
        addSlider("maskSatLo", "Sat Lo", "Widen toward LOWER saturation.", 0, 100);
        addSlider("maskSatHi", "Sat Hi", "Widen toward HIGHER saturation.", 0, 100);
        addSlider("maskFeather", "Feather", "Soften the selection edge.", 0, 100);
        addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.");
        setSliderReal("maskHueLo", pnum(o, "hueLo", 20));
        setSliderReal("maskHueHi", pnum(o, "hueHi", 20));
        setSliderReal("maskSatLo", pnum(o, "satLo", 25));
        setSliderReal("maskSatHi", pnum(o, "satHi", 25));
        setSliderReal("maskFeather", feather);
        setCheckboxValue("maskInvert", m.inverted);
        break;

    case int(MaskTool::Object):
        addSlider("maskSize", "Size", "Perimeter brush diameter (% of the long edge).", 1, 100);
        addSlider("maskFeather", "Feather", "Soften the refined cutout edge.", 0, 100);
        addCheckbox("maskInvert", "Invert", "Invert this mask's contribution.");
        setSliderReal("maskSize", pnum(o, "size", 8));
        setSliderReal("maskFeather", feather);
        setCheckboxValue("maskInvert", m.inverted);
        break;

    default:    // Gradients, Subject, Sky, Background: Feather + Invert
        addSlider("maskFeather", "Feather", "Soften the mask edge.", 0, 100);
        addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.");
        setSliderReal("maskFeather", feather);
        setCheckboxValue("maskInvert", m.inverted);
        break;
    }

    isPopulating = false;
    fitHeight();
}

void MaskEditor::addColorRangeWheel(const MaskComponent &m)
{
    /* A spanned, tall row hosting the hue/sat wheel (like addToolRow's ColorRange). */
    clearItemInfo(i);
    i.name = "maskColorWheel";
    i.parIdx = QModelIndex();
    i.parentName = "";
    i.captionText = "";
    i.isIndent = true;
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);
    const QModelIndex wheelIdx = capIdx;
    model->setData(wheelIdx, 200, UR_ExtraRowHeight);
    setFirstColumnSpanned(wheelIdx.row(), QModelIndex(), true);

    wheel = new ColorRangeWheel;
    const QJsonObject o = QJsonDocument::fromJson(m.paramsJson.toUtf8()).object();
    wheel->setBounds(pnum(o, "hueLo", 20), pnum(o, "hueHi", 20),
                     pnum(o, "satLo", 25) / 100.0, pnum(o, "satHi", 25) / 100.0);
    connect(wheel, &ColorRangeWheel::boundsChanged,   this, [this]{ onWheelBounds(false); });
    connect(wheel, &ColorRangeWheel::boundsCommitted, this, [this]{ onWheelBounds(true); });
    setIndexWidget(wheelIdx, wheel);
}

void MaskEditor::setWheelSamples(const QVector<QPointF> &samples)
{
    if (wheel) wheel->setSamples(samples);
}

void MaskEditor::onWheelBounds(bool commit)
{
    if (!wheel) return;
    const int hueLo = int(wheel->hueLo() + 0.5f);
    const int hueHi = int(wheel->hueHi() + 0.5f);
    const int satLo = int(wheel->satLo() * 100.0f + 0.5f);
    const int satHi = int(wheel->satHi() * 100.0f + 0.5f);
    /* Keep the Hue/Sat sliders in step without re-entering itemChange. */
    const bool wasPop = isPopulating;
    isPopulating = true;
    setSliderReal("maskHueLo", hueLo);
    setSliderReal("maskHueHi", hueHi);
    setSliderReal("maskSatLo", satLo);
    setSliderReal("maskSatHi", satHi);
    isPopulating = wasPop;
    emit wheelChanged(hueLo, hueHi, satLo, satHi, commit);
}

void MaskEditor::syncWheelFromSliders()
{
    if (!wheel) return;
    wheel->setBounds(sliderInt("maskHueLo"), sliderInt("maskHueHi"),
                     sliderInt("maskSatLo") / 100.0f, sliderInt("maskSatHi") / 100.0f);
}

void MaskEditor::fitHeight()
{
    int h = 6;
    for (int r = 0; r < model->rowCount(); ++r)
        h += sizeHintForRow(r);
    setFixedHeight(h);
}

void MaskEditor::itemChange(QModelIndex index)
{
    if (isPopulating) return;
    const QString src = index.data(UR_Source).toString();
    const QVariant v  = index.data(Qt::EditRole);
    /* A Hue/Sat slider drag also moves the wheel band. */
    if (src == "maskHueLo" || src == "maskHueHi" ||
        src == "maskSatLo" || src == "maskSatHi")
        syncWheelFromSliders();
    emit settingChanged(src, v);
}

void MaskEditor::mousePressEvent(QMouseEvent *event)
{
    /* Click a slider row's caption -> focus its slider (arrow keys nudge) + flash the
       caption, like DevelopProperties::mousePressEvent (the base does not select on
       click). */
    if (event->button() == Qt::LeftButton) {
        const QModelIndex idx = indexAt(event->pos());
        if (idx.isValid() && idx.column() == CapColumn) {
            const QModelIndex valIdx = model->index(idx.row(), ValColumn, idx.parent());
            if (valIdx.data(UR_DelegateType).toInt() == DT_Slider) {
                if (auto *se = static_cast<SliderEditor*>(
                        valIdx.data(UR_Editor).value<void*>()))
                    se->focusSlider();
                flashCaption(idx);
                return;
            }
        }
    }
    PropertyEditor::mousePressEvent(event);
}

void MaskEditor::flashCaption(const QModelIndex &capIdx)
{
    if (!capIdx.isValid()) return;
    if (flashAnim) { flashAnim->stop(); flashAnim->deleteLater(); }
    if (flashIdx.isValid() && flashIdx != capIdx)
        model->setData(flashIdx, QVariant(), UR_FlashLevel);
    flashIdx = capIdx;
    auto *anim = new QVariantAnimation(this);
    flashAnim = anim;
    anim->setDuration(450);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        if (flashIdx.isValid()) model->setData(flashIdx, val.toReal(), UR_FlashLevel);
    });
    connect(anim, &QVariantAnimation::finished, this, [this, anim]{
        if (flashIdx.isValid()) model->setData(flashIdx, QVariant(), UR_FlashLevel);
        flashIdx = QModelIndex();
        anim->deleteLater();
    });
    anim->start();
}
