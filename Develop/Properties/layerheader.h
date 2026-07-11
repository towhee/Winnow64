#ifndef LAYERHEADER_H
#define LAYERHEADER_H

#include <QWidget>
#include <QIcon>
#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;
class BarBtn;

/*
    The Develop dock's layer header: a compact gradient band (styled like TransformPanel's header)
    that sits ABOVE the DevelopProperties tree and replaces the old in-tree "Layers" header. It owns
    the layer dropdown and the whole-layer preview eye; it carries no model state and simply emits a
    signal per user action (DevelopProperties does the work and drives this widget's display back).

    Layout:
        > Layer [ Layer dropdown                     v ]              [E]
        |       |                                                      |
        |       selected layer's items shown in the tree below        whole-layer preview eye
        collapse arrow (hide/show the tree = the layer's items)

    The dropdown lists the image's layers (the active one carries a checkmark) and, below a
    separator, the layer actions:

        Base
        Layer 1
      v Layer 2                (active layer)
        More Layers
        ---
        Add new layer
        ---                    (per-layer group, hidden when Base is active)
        Add mask to Layer 2
        Reset Layer 2
        Remove Layer 2
        Rename Layer 2

    Picking a layer emits layerSelected; picking an action emits its signal and the box reverts to
    the active layer (an action row is never left showing). The per-layer group (Reset / Remove /
    Rename) is omitted for the Base layer (index 0), which applies globally.
*/
class LayerHeader : public QWidget
{
    Q_OBJECT
public:
    explicit LayerHeader(QWidget *parent = nullptr);

    /* Refill the dropdown and select currentIndex WITHOUT emitting layerSelected. */
    void setLayers(const QStringList &names, int currentIndex);
    void setPreviewShown(bool shown);           // eye icon (whole-layer preview)
    void setBaseActive(bool isBase);            // Base: omit the per-layer action group
    bool isCollapsed() const { return collapsed; }
    /* Programmatic collapse (Expand all / Collapse all / Solo) -- updates the arrow
       WITHOUT emitting collapseToggled; the caller drives the tree itself. */
    void setCollapsed(bool collapsed);
    QString currentLayerName() const;

signals:
    void layerSelected(const QString &name);    // user picked a different layer in the dropdown
    void renameRequested();                      // menu: rename the selected layer
    void resetLayerRequested();                  // menu: reset the whole layer to identity
    void removeLayerRequested();                 // menu: remove the selected layer
    void addLayerRequested();                    // menu: add a new layer
    void addMaskRequested();                     // menu: add a mask tool to the selected layer
    void previewToggled(bool shown);             // [E] show/ignore the whole layer
    void collapseToggled(bool collapsed);        // > hide/show the layer's tree items

protected:
    void paintEvent(QPaintEvent *) override;         // the property-header gradient band

private:
    void updatePreviewIcon();
    void updateCollapseIcon();

    /* Non-negative item data is a layer index; these negative codes tag the action rows. */
    enum ComboAction { ActAddLayer = -1, ActAddMask = -2, ActReset = -3, ActRemove = -4, ActRename = -5 };

    BarBtn    *collapseBtn = nullptr;
    QLabel    *layerLabel  = nullptr;
    QComboBox *combo       = nullptr;
    BarBtn    *previewBtn  = nullptr;

    QIcon checkIcon;                    // marks the active layer in the dropdown
    QIcon blankIcon;                    // same-size spacer so other layers stay aligned

    int  layerCount   = 0;              // number of layer rows at the top of the combo
    int  activeIndex  = 0;              // the selected layer's row (the box reverts here)
    bool previewShown = true;
    bool collapsed    = false;
    bool baseActive   = true;           // the selected layer is Base (index 0)
};

#endif // LAYERHEADER_H
