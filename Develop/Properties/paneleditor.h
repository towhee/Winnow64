#ifndef PANELEDITOR_H
#define PANELEDITOR_H

#include "PropertyEditor/propertyeditor.h"
#include <QString>
#include <QPersistentModelIndex>
#include <QPointer>

class QVariantAnimation;

/*
    PanelEditor -- a small, reusable PropertyEditor for a handful of slider/checkbox rows
    INSIDE a dock panel (RawPanel, MaskPanel) so they render with the SAME delegate as the
    Develop property tree (identical look + alignment) rather than hand-built QSliders.

    The owner builds rows with addSlider/addCheckbox, seeds them with setSliderReal/
    setCheckboxValue, aligns the value column with setCaptionWidth, and listens to
    settingChanged for edits. Clicking a slider caption focuses its slider + flashes the
    caption, exactly like the main tree. MaskEditor extends this with per-tool rows + the
    Color Range wheel.
*/
class PanelEditor : public PropertyEditor
{
    Q_OBJECT
public:
    explicit PanelEditor(QWidget *parent = nullptr);

    void clearRows();          // drop all rows (call before rebuilding)
    /* min/max are in SLIDER units: with div > 0 the row shows value/div (e.g. min 1,
       max 1000, div 10 = 0.1 .. 100.0), matching DevelopProperties::addSlider. */
    void addSlider(const QString &key, const QString &caption, const QString &tooltip,
                   int min, int max, int div = 0);
    void addCheckbox(const QString &key, const QString &caption, const QString &tooltip);
    void setSliderReal(const QString &key, double real);
    void setCheckboxValue(const QString &key, bool on);
    void setRowEnabled(const QString &key, bool on);   // grey / enable a row
    int  sliderInt(const QString &key) const;          // current int value of a row
    void setCaptionWidth(int w);   // align the value column to the main tree
    void fitHeight();              // size the view to its rows (call after building)

signals:
    /* A slider/checkbox changed: key is its param key, value the new int/bool. */
    void settingChanged(const QString &key, const QVariant &value);

public slots:
    void itemChange(QModelIndex index) override;   // an editor value changed

protected:
    void mousePressEvent(QMouseEvent *event) override;   // caption click -> focus slider
    void flashCaption(const QModelIndex &capIdx);        // brief white caption flash

    ItemInfo i;                                    // addItem scratch
    bool isPopulating = false;                     // suppress itemChange while seeding
    QPersistentModelIndex flashIdx;                // caption currently flashing
    QPointer<QVariantAnimation> flashAnim;
};

#endif // PANELEDITOR_H
