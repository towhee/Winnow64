#ifndef MASKEDITOR_H
#define MASKEDITOR_H

#include "Develop/Properties/paneleditor.h"
#include "Develop/editstack.h"      // MaskComponent
#include <QString>
#include <QPointF>
#include <QVector>

class ColorRangeWheel;

/*
    MaskEditor -- a PanelEditor that renders ONE mask tool's settings inside the MaskPanel
    (above the Scope list). It reuses PanelEditor's tree-styled slider/checkbox rows, so
    the controls look identical to Exposure/Contrast, and adds the per-tool row set plus
    the Color Range hue/sat wheel.

    It carries no model state: DevelopProperties calls showTool() with the tool's current
    MaskComponent and handles the change signals (PanelEditor::settingChanged for the
    sliders/checkboxes, wheelChanged for the wheel). The rows mirror
    DevelopProperties::addToolRow. Color Range hosts a ColorRangeWheel; the wheel and its
    Hue/Sat sliders are kept in step inside this class, and the values reported out.
*/
class MaskEditor : public PanelEditor
{
    Q_OBJECT
public:
    explicit MaskEditor(QWidget *parent = nullptr);

    /* Build + seed the rows for the tool being edited (reads m.tool, m.feather,
       m.inverted, and m.paramsJson for size/flow/range/hue/sat). */
    void showTool(const MaskComponent &m);
    /* Color Range: push the pipette-sampled colours into the wheel (hue 0..360, sat
       0..1). No-op unless a Color Range tool is showing. */
    void setWheelSamples(const QVector<QPointF> &samples);

signals:
    /* Color Range wheel dragged: hue bounds in degrees, sat bounds in 0..100, commit on
       release. */
    void wheelChanged(int hueLo, int hueHi, int satLo, int satHi, bool commit);

public slots:
    void itemChange(QModelIndex index) override;   // add wheel sync, then settingChanged

private:
    void addColorRangeWheel(const MaskComponent &m);   // build + wire the wheel row
    void onWheelBounds(bool commit);               // wheel drag -> sliders + wheelChanged
    void syncWheelFromSliders();                   // Hue/Sat sliders -> wheel band

    ColorRangeWheel *wheel = nullptr;              // Color Range only; null otherwise
};

#endif // MASKEDITOR_H
