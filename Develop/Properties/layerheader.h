#ifndef LAYERHEADER_H
#define LAYERHEADER_H

#include <QWidget>
#include <QString>
#include <QStringList>

class QComboBox;
class BarBtn;

/*
    The Develop dock's layer header: a compact gradient band (styled like TransformPanel's header)
    that sits ABOVE the DevelopProperties tree and replaces the old in-tree "Layers" header. It owns
    the layer dropdown and the layer-action buttons; it carries no model state and simply emits a
    signal per user action (DevelopProperties does the work and drives this widget's display back).

    Layout:
        > [ Layer dropdown                       v ] [?] [E]
        |   |                                        |   |
        |   selected layer's items shown            |   whole-layer preview eye
        |   in the tree below                       actions menu (also on right-click):
        collapse arrow (hide/show the                  Rename / Reset / Add / Remove / Add mask
        tree = the layer's items)

    The actions menu is also raised by right-clicking the header. Rename, Remove and Add mask are
    disabled on the Base layer (index 0), which applies globally and cannot be masked, removed or
    renamed.
*/
class LayerHeader : public QWidget
{
    Q_OBJECT
public:
    explicit LayerHeader(QWidget *parent = nullptr);

    /* Refill the dropdown and select currentIndex WITHOUT emitting layerSelected. */
    void setLayers(const QStringList &names, int currentIndex);
    void setPreviewShown(bool shown);           // eye icon (whole-layer preview)
    void setBaseActive(bool isBase);            // Base: disable Rename / Remove / Add mask
    bool isCollapsed() const { return collapsed; }
    QString currentLayerName() const;

signals:
    void layerSelected(const QString &name);    // user picked a different layer in the dropdown
    void renameRequested();                      // menu: rename the selected layer
    void resetLayerRequested();                  // menu: reset the whole layer to identity
    void removeLayerRequested();                 // menu: remove the selected layer
    void addLayerRequested();                    // menu: add a new layer
    void maskMenuRequested();                    // menu: pop the add-mask menu
    void previewToggled(bool shown);             // [E] show/ignore the whole layer
    void collapseToggled(bool collapsed);        // > hide/show the layer's tree items

protected:
    void paintEvent(QPaintEvent *) override;         // the property-header gradient band
    void contextMenuEvent(QContextMenuEvent *) override;   // right-click raises the actions menu

private:
    void updatePreviewIcon();
    void updateCollapseIcon();
    void showActionsMenu(const QPoint &globalPos);   // the [?] / right-click layer-actions menu

    BarBtn    *collapseBtn = nullptr;
    QComboBox *combo       = nullptr;
    BarBtn    *menuBtn     = nullptr;   // [?] pops the layer-actions menu
    BarBtn    *previewBtn  = nullptr;

    bool previewShown = true;
    bool collapsed    = false;
    bool baseActive   = true;           // the selected layer is Base (index 0)
};

#endif // LAYERHEADER_H
