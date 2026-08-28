#include "Develop/Properties/maskeditor.h"
#include "Develop/Properties/colorrangewheel.h"
#include "Main/global.h"

#include <QJsonDocument>
#include <QJsonObject>

/* Read a numeric field from a mask tool's paramsJson (def if absent). */
static double pnum(const QJsonObject &o, const QString &key, double def)
{
    return o.contains(key) ? o.value(key).toDouble(def) : def;
}

MaskEditor::MaskEditor(QWidget *parent) : PanelEditor(parent)
{
    if (G::isLogger) G::log("MaskEditor::MaskEditor");
}

/* Every tool gets this row, so the wording has to hold for a gradient, a brush and an AI
   cutout alike -- and it has to say the one thing Feather does not do. */
static const char *kEdgeTip =
    "Grow (+) or shrink (-) this submask's boundary, in pixels of the full-size image. "
    "Feather softens an edge; Edge moves it.";

void MaskEditor::showTool(const MaskComponent &m)
{
    isPopulating = true;
    wheel = nullptr;                 // freed with its row by clearRows() below
    clearRows();

    const QJsonObject o = QJsonDocument::fromJson(m.paramsJson.toUtf8()).object();
    const int feather = int(m.feather + 0.5f);
    const int edge = int(m.edge < 0.0f ? m.edge - 0.5f : m.edge + 0.5f);

    switch (m.tool) {
    case int(MaskTool::Brush):
        addSlider("maskSize", "Size", "Brush diameter (% of the long edge).", 1, 1000, 10);
        addSlider("maskFeather", "Feather", "Soft edge outside the brush size (0 = crisp).", 0, 100);
        addSlider("maskEdge", "Edge", kEdgeTip, -100, 100);
        addSlider("maskFlow", "Flow", "How much each stroke builds up.", 1, 100);
        addCheckbox("maskAutoMask", "Auto mask",
                    "Keep the brush inside the edges under the cursor. Toggle with A.");
        addCheckbox("maskInvert", "Invert", "Invert this mask's contribution.");
        setSliderReal("maskSize", pnum(o, "size", 20));
        setSliderReal("maskFeather", feather);
        setSliderReal("maskEdge", edge);
        setSliderReal("maskFlow", pnum(o, "flow", 100));
        setCheckboxValue("maskAutoMask", o.value("autoMask").toBool(false));
        setCheckboxValue("maskInvert", m.inverted);
        break;

    case int(MaskTool::LuminanceRange):
        addSlider("maskRangeLo", "Range Min", "Lower luminance bound of the band.", 0, 100);
        addSlider("maskRangeHi", "Range Max", "Upper luminance bound of the band.", 0, 100);
        addSlider("maskFeather", "Feather", "Soften the band edges.", 0, 100);
        addSlider("maskEdge", "Edge", kEdgeTip, -100, 100);
        addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.");
        setSliderReal("maskRangeLo", pnum(o, "lo", 0.25) * 100.0);
        setSliderReal("maskRangeHi", pnum(o, "hi", 0.75) * 100.0);
        setSliderReal("maskFeather", feather);
        setSliderReal("maskEdge", edge);
        setCheckboxValue("maskInvert", m.inverted);
        break;

    case int(MaskTool::Depth):
        addSlider("maskRangeLo", "Near", "Near bound of the depth band (0 = nearest).", 0, 100);
        addSlider("maskRangeHi", "Far", "Far bound of the depth band (100 = farthest).", 0, 100);
        addSlider("maskFeather", "Feather", "Soften the depth-band edges.", 0, 100);
        addSlider("maskEdge", "Edge", kEdgeTip, -100, 100);
        addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.");
        setSliderReal("maskRangeLo", pnum(o, "lo", 0.0) * 100.0);
        setSliderReal("maskRangeHi", pnum(o, "hi", 0.5) * 100.0);
        setSliderReal("maskFeather", feather);
        setSliderReal("maskEdge", edge);
        setCheckboxValue("maskInvert", m.inverted);
        break;

    case int(MaskTool::ColorRange):
        addColorRangeWheel(m);
        addSlider("maskHueLo", "Hue Lo", "Widen the selection toward LOWER hues.", 0, 90);
        addSlider("maskHueHi", "Hue Hi", "Widen the selection toward HIGHER hues.", 0, 90);
        addSlider("maskSatLo", "Sat Lo", "Widen toward LOWER saturation.", 0, 100);
        addSlider("maskSatHi", "Sat Hi", "Widen toward HIGHER saturation.", 0, 100);
        addSlider("maskFeather", "Feather", "Soften the selection edge.", 0, 100);
        addSlider("maskEdge", "Edge", kEdgeTip, -100, 100);
        addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.");
        setSliderReal("maskHueLo", pnum(o, "hueLo", 20));
        setSliderReal("maskHueHi", pnum(o, "hueHi", 20));
        setSliderReal("maskSatLo", pnum(o, "satLo", 25));
        setSliderReal("maskSatHi", pnum(o, "satHi", 25));
        setSliderReal("maskFeather", feather);
        setSliderReal("maskEdge", edge);
        setCheckboxValue("maskInvert", m.inverted);
        break;

    case int(MaskTool::Object):
        addSlider("maskSize", "Size", "Perimeter brush diameter (% of the long edge).", 1, 1000, 10);
        addSlider("maskFeather", "Feather", "Soften the refined cutout edge.", 0, 100);
        addSlider("maskEdge", "Edge", kEdgeTip, -100, 100);
        addCheckbox("maskInvert", "Invert", "Invert this mask's contribution.");
        setSliderReal("maskSize", pnum(o, "size", 8));
        setSliderReal("maskFeather", feather);
        setSliderReal("maskEdge", edge);
        setCheckboxValue("maskInvert", m.inverted);
        break;

    default:    // Gradients, Subject, Sky, Background: Feather + Invert
        addSlider("maskFeather", "Feather", "Soften the mask edge.", 0, 100);
        addSlider("maskEdge", "Edge", kEdgeTip, -100, 100);
        addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.");
        setSliderReal("maskFeather", feather);
        setSliderReal("maskEdge", edge);
        setCheckboxValue("maskInvert", m.inverted);
        break;
    }

    isPopulating = false;
    fitHeight();
}

/*
    The MASK-level rows: Edge and Halo, applied to the mask every submask folds into. They
    are NOT part of showTool's per-tool rows because they must survive with no submask
    selected -- that block hides, this one does not.
*/
void MaskEditor::showMaskLevel(float edgeVal, float haloVal, bool haloUseful)
{
    isPopulating = true;
    wheel = nullptr;
    clearRows();
    addSlider("scopeMaskEdge", "Edge",
              "Grow (+) or shrink (-) the WHOLE mask -- every submask folded together -- "
              "in pixels of the full-size image. Adds to each submask's own Edge.",
              -100, 100);
    setSliderReal("scopeMaskEdge",
                  int(edgeVal < 0.0f ? edgeVal - 0.5f : edgeVal + 0.5f));

    /* Says what the symptom looks like, because that is how the user finds this row --
       they see a glow, not a "misregistered alpha". */
    addSlider("scopeHalo", "Halo",
              haloUseful
                  ? "Remove the halo left when the mask edge misses the subject: fades "
                    "the adjustment out across the real edge in the picture. Raise "
                    "until the rim goes."
                  : "Needs an edge in the picture to work from. This mask is all "
                    "gradient, so there is none.",
              0, 100);
    setSliderReal("scopeHalo", int(haloVal + 0.5f));
    setRowEnabled("scopeHalo", haloUseful);
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

void MaskEditor::itemChange(QModelIndex index)
{
    if (isPopulating) return;
    const QString src = index.data(UR_Source).toString();
    /* A Hue/Sat slider drag also moves the wheel band; then report the change out. */
    if (src == "maskHueLo" || src == "maskHueHi" ||
        src == "maskSatLo" || src == "maskSatHi")
        syncWheelFromSliders();
    emit settingChanged(src, index.data(Qt::EditRole));
}
