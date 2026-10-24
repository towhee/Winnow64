#include "Develop/Properties/maskeditor.h"
#include "Develop/Properties/colorrangewheel.h"
#include "Develop/beammask.h"
#include "Main/global.h"
#include "Utilities/segmenter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>

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

void MaskEditor::showTool(const MaskComponent &m, bool firstSubmask, int effectiveOp)
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

    case int(MaskTool::RefineBrush): {
        /* Re-mattes the mask ABOVE it inside the painted region (Develop/maskrefine.h).
           No Edge / Invert rows: it is a step on the folded mask, not a coverage. */
        QStringList modes{"Add"};
        if (!firstSubmask) modes << "Remove";
        modes << "Fix Edge";
        QString tip = "Add: paint over what the mask missed.\n"
                      "Remove: paint over what it wrongly took (or hold Opt).\n"
                      "Fix Edge: paint along a poor edge to snap it to the subject.";
        if (firstSubmask)
            tip += "\n\nRemove is not offered: this is the first submask, so there is "
                   "no mask above it to remove from.";
        addCombo("maskRefineMode", "Mode", tip, modes);
        addSlider("maskSize", "Size", "Brush diameter (% of the long edge).", 1, 1000, 10);
        addSlider("maskFeather", "Feather",
                  "Soft edge outside the brush size: fades the refinement in.", 0, 100);
        addSlider("maskFlow", "Flow", "How much each stroke builds up.", 1, 100);
        addCheckbox("maskAutoMask", "Auto mask",
                    "Keep the brush inside the edges under the cursor. Toggle with A.");
        const int op = effectiveOp >= 0 ? effectiveOp : m.op;
        const QString mode = o.value("mode").toString() == "edge"   ? "Fix Edge"
                           : (op != int(MaskOp::Add) && !firstSubmask) ? "Remove"
                                                                       : "Add";
        setComboValue("maskRefineMode", mode);
        setSliderReal("maskSize", pnum(o, "size", 20));
        setSliderReal("maskFeather", feather);
        setSliderReal("maskFlow", pnum(o, "flow", 100));
        setCheckboxValue("maskAutoMask", o.value("autoMask").toBool(false));
        break;
    }

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

    case int(MaskTool::Segmenter): {
        /* Detail is a property of the TOOL, not of this submask: it picks which level of
           the object hierarchy the canvas offers (whole objects, their parts, finer
           parts), so it is a preference -- changing it re-renders nothing, and the
           selection already made stays as it is. */
        QStringList levels;
        for (int l = 0; l < Segmenter::kLevels; ++l) levels << Segmenter::levelName(l);
        addCombo("maskSegDetail", "Detail",
                 "What a click picks: whole OBJECTS, their PARTS, or finer DETAILS.\n"
                 "[ and ] step through them while the pointer is over the photo.",
                 levels);
        addSlider("maskFeather", "Feather", "Soften the selected objects' edges.", 0, 100);
        addSlider("maskEdge", "Edge", kEdgeTip, -100, 100);
        addCheckbox("maskInvert", "Invert", "Everything except the selected objects.");
        const int level = std::clamp(G::settings ? G::settings->value(
                              "Develop/segmenterDetail", 0).toInt() : 0,
                              0, Segmenter::kLevels - 1);
        setComboValue("maskSegDetail", Segmenter::levelName(level));
        setSliderReal("maskFeather", feather);
        setSliderReal("maskEdge", edge);
        setCheckboxValue("maskInvert", m.inverted);
        break;
    }

    case int(MaskTool::LinearGradient):
        /* Rotation: a precise alternative to the end handles, which sit close together
           (and so are hard to turn) at a low Feather. Tenths of a degree; the value is set
           by the canvas (ImageView::maskAngleReported), which knows the frame's aspect. */
        addSlider("maskFeather", "Feather", "Soften the mask edge.", 0, 100);
        addSlider("maskAngle", "Rotation",
                  "Turn the gradient about its centre (degrees, clockwise).",
                  -1800, 1800, 10);
        addSlider("maskEdge", "Edge", kEdgeTip, -100, 100);
        addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.");
        setSliderReal("maskFeather", feather);
        setSliderReal("maskAngle", 0.0);
        setSliderReal("maskEdge", edge);
        setCheckboxValue("maskInvert", m.inverted);
        break;

    case int(MaskTool::Beam): {
        /* Width is also the canvas's side handles; Taper and Rotation are panel-first
           (the end handles turn the beam, but a precise angle wants the slider). Width
           is the FULL width in % of the frame's short side; params store the half. */
        addSlider("maskFeather", "Feather", "Soften both edges of the beam.", 0, 100);
        addSlider("maskBeamWidth", "Width",
                  "Width of the beam at its centre (% of the photo's short side).",
                  1, 2000, 10);
        addSlider("maskBeamTaper", "Taper",
                  "Narrow the beam toward one end (-) or the other (+). At 100 the edges "
                  "meet at that end's handle.", -100, 100);
        addSlider("maskAngle", "Rotation",
                  "Turn the beam about its centre (degrees, clockwise).", -1800, 1800, 10);
        addSlider("maskEdge", "Edge", kEdgeTip, -100, 100);
        addCheckbox("maskInvert", "Invert", "Invert this tool's contribution.");
        setSliderReal("maskFeather", feather);
        setSliderReal("maskBeamWidth",
                      200.0 * o.value("width").toDouble(BeamMask::kDefaultWidth));
        setSliderReal("maskBeamTaper", o.value("taper").toDouble(0.0));
        setSliderReal("maskAngle", 0.0);
        setSliderReal("maskEdge", edge);
        setCheckboxValue("maskInvert", m.inverted);
        break;
    }

    default:    // Radial Gradient, Subject, Sky, Background: Feather + Invert
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
    The MASK-level rows: Feather, Edge, Halo and Opacity, applied to the mask every submask
    folds into. They
    are NOT part of showTool's per-tool rows because they must survive with no submask
    selected -- that block hides, this one does not.
*/
void MaskEditor::showMaskLevel(float featherVal, float edgeVal, float haloVal,
                               bool haloUseful, float opacityVal)
{
    isPopulating = true;
    wheel = nullptr;
    clearRows();
    /* Above Edge, in the order the submask rows use. Softens the folded mask after Edge
       and Halo (EditScope::maskFeather), so each submask's own Feather still composes. */
    addSlider("scopeMaskFeather", "Feather",
              "Soften the whole mask's edge (0 = as the submasks make it).",
              0, 100);
    setSliderReal("scopeMaskFeather", int(featherVal + 0.5f));
    addSlider("scopeMaskEdge", "Edge",
              "Grow (+) or shrink (-) the whole mask.",
              -100, 100);
    setSliderReal("scopeMaskEdge",
                  int(edgeVal < 0.0f ? edgeVal - 0.5f : edgeVal + 0.5f));

    /* Says what the symptom looks like, because that is how the user finds this row --
       they see a glow, not a "misregistered alpha". */
    addSlider("scopeHalo", "Halo",
              haloUseful
                  ? "Remove a light or dark rim where the mask misses the subject's "
                    "edge. Raise until the rim goes."
                  : "Needs an edge in the picture to work from. This mask is all "
                    "gradient, so there is none.",
              0, 100);
    setSliderReal("scopeHalo", int(haloVal + 0.5f));
    setRowEnabled("scopeHalo", haloUseful);

    /* The strength of the whole mask: the adjustment lands at this fraction of its
       coverage everywhere (EditScope::opacity). Percent here, 0..1 stored. */
    addSlider("scopeOpacity", "Opacity",
              "How strongly this mask's adjustments apply, everywhere it reaches.",
              0, 100);
    setSliderReal("scopeOpacity", int(opacityVal * 100.0f + 0.5f));
    isPopulating = false;
    /* No slack under the last row: the rule below it (SubmaskList::setTopRule) is spaced like
       the tree's group dividers, and the default 6px would push it further away. */
    fitHeight(0);
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
