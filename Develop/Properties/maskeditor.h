#ifndef MASKEDITOR_H
#define MASKEDITOR_H

#include "PropertyEditor/propertyeditor.h"
#include "Develop/editstack.h"      // MaskComponent
#include <QString>
#include <QPointF>
#include <QVector>
#include <QPersistentModelIndex>
#include <QPointer>

class ColorRangeWheel;
class QVariantAnimation;

/*
    MaskEditor -- a small PropertyEditor that renders ONE mask tool's settings inside the
    MaskPanel (above the Layers list). It reuses the same model + PropertyDelegate as the
    Develop property tree, so the sliders/checkboxes look identical to Exposure/Contrast,
    but it lives in the panel rather than in the main tree below the Layers list.

    It carries no model state: DevelopProperties calls showTool() with the tool's current
    MaskComponent and handles the change signals (updating the component + re-rendering).
    Every mask tool routes through here; the rows mirror DevelopProperties::addToolRow.
    Color Range additionally hosts a ColorRangeWheel; the wheel and its Hue/Sat sliders
    are kept in step inside this class, and the values are reported out for persistence.
*/
class MaskEditor : public PropertyEditor
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
    /* Match the main tree's caption/value split so the value column lines up. */
    void setCaptionWidth(int w);

signals:
    /* A slider/checkbox changed: key is the mask param ("maskFeather", "maskSize",
       "maskRangeLo", "maskHueLo", ...), value the new int/bool. */
    void settingChanged(const QString &key, const QVariant &value);
    /* Color Range wheel dragged: hue bounds in degrees, sat bounds in 0..100, commit on
       release. */
    void wheelChanged(int hueLo, int hueHi, int satLo, int satHi, bool commit);

public slots:
    void itemChange(QModelIndex index) override;   // an editor value changed

protected:
    /* Click a slider row's caption -> focus that slider (arrow keys nudge it) and flash
       the caption, exactly like the main Develop tree. */
    void mousePressEvent(QMouseEvent *event) override;

private:
    void flashCaption(const QModelIndex &capIdx);  // brief white caption flash
    /* Local copies of the generic tree-row builders (same as DevelopProperties'). */
    void addSlider(const QString &key, const QString &caption, const QString &tooltip,
                   int min, int max);
    void addCheckbox(const QString &key, const QString &caption, const QString &tooltip);
    void setSliderReal(const QString &key, double real);
    void setCheckboxValue(const QString &key, bool on);
    int  sliderInt(const QString &key) const;      // current int value of a slider row
    void fitHeight();                              // size the view to its rows

    void addColorRangeWheel(const MaskComponent &m);   // build + wire the wheel row
    void onWheelBounds(bool commit);               // wheel drag -> sliders + wheelChanged
    void syncSlidersFromWheel();                   // wheel getters -> Hue/Sat sliders
    void syncWheelFromSliders();                   // Hue/Sat sliders -> wheel band

    ColorRangeWheel *wheel = nullptr;              // Color Range only; null otherwise
    ItemInfo i;                                    // addItem scratch (own copy)
    bool isPopulating = false;                     // suppress itemChange while seeding
    QPersistentModelIndex flashIdx;                // caption currently flashing
    QPointer<QVariantAnimation> flashAnim;
};

#endif // MASKEDITOR_H
