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

    The dropdown lists ONLY the image's layers (the active one carries a checkmark):

        Base
        Layer 1
      v Layer 2                (active layer)
        More Layers

    Picking a layer emits layerSelected. The layer actions live on a separate menu button
    (layerMenuBtn, an ellipsis) that pops up a QMenu (like EmbelProperties::effectContextMenu):

        Add new layer
        ---                    (per-layer group, hidden when Base is active)
        Add mask to Layer 2
        Show mask overlay     (checkable; only while a mask tool is being edited)
        Reset Layer 2
        Remove Layer 2
        Rename Layer 2

    Each menu row emits its signal. The per-layer group (Add mask / Show mask overlay / Reset /
    Remove / Rename) is omitted for the Base layer (index 0), which applies globally.
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
    /* Mask overlay tint state, pushed by DevelopProperties so the menu row shows the
       right check state and only appears while a mask tool is being edited. */
    void setMaskOverlayAvailable(bool available);
    void setMaskOverlayShown(bool shown);
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
    void addMaskRequested();                     // menu: add a mask tool to this layer
    void maskOverlayToggled();                   // menu: show/hide the mask overlay
    void previewToggled(bool shown);             // [E] show/ignore the whole layer
    void collapseToggled(bool collapsed);        // > hide/show the layer's tree items

protected:
    void paintEvent(QPaintEvent *) override;         // the property-header gradient band
    /* Clicking the "Layer" label toggles collapse just like the arrow (filtered here
       because QLabel has no clicked signal). */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updatePreviewIcon();
    void updateCollapseIcon();
    void toggleCollapsed();             // arrow click or "Layer" label click
    void showLayerMenu();               // layerMenuBtn: pop up the layer-actions menu

    BarBtn    *collapseBtn  = nullptr;
    QLabel    *layerLabel   = nullptr;
    QComboBox *combo        = nullptr;
    BarBtn    *layerMenuBtn = nullptr;
    BarBtn    *previewBtn   = nullptr;

    QIcon checkIcon;                    // marks the active layer in the dropdown
    QIcon blankIcon;                    // same-size spacer so other layers stay aligned

    int  layerCount   = 0;              // number of layer rows at the top of the combo
    int  activeIndex  = 0;              // the selected layer's row (the box reverts here)
    bool previewShown = true;
    bool collapsed    = false;
    bool baseActive   = true;           // the selected layer is Base (index 0)
    bool maskOverlayAvailable = false;  // a mask tool is being edited -> menu row applies
    bool maskOverlayShown     = true;   // the red coverage tint is currently visible
};

#endif // LAYERHEADER_H
