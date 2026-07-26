#ifndef LAYERHEADERBASE_H
#define LAYERHEADERBASE_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>

/*
    Abstract seam for the Develop dock's layer header. It lets a shipping
    implementation (LayerHeader) and an experimental one (LayerHeaderLab) be
    swapped at construction time behind G::useLayerHeaderLab, without either
    concrete type leaking past DevelopProperties::bindLayerHeader().

    DevelopProperties talks to the header through this interface ONLY: it connects
    the signals below and drives the pure-virtual setters. Both concrete classes
    therefore present an identical contract, so the lab can be reshaped freely and,
    once it proves out, its file contents replace layerheader.* and the flag/base
    can be retired.

    Signals are declared here (not in the concrete classes) so a single
    connect(&LayerHeaderBase::layerSelected, ...) binds whichever class was built.
*/
class LayerHeaderBase : public QWidget
{
    Q_OBJECT
public:
    explicit LayerHeaderBase(QWidget *parent = nullptr) : QWidget(parent) {}

    /* One layer's display state for the list panel (LayersPanel). The dropdown header
       ignores enabled/isBase and uses only the name (via the default setLayerRows). */
    struct LayerRowInfo {
        QString name;
        bool    enabled = true;    // EditLayer::enabled -> the row's show/hide checkbox
        bool    isBase  = false;   // index 0: no checkbox, applies globally
    };

    /* Refill the dropdown and select currentIndex WITHOUT emitting layerSelected. */
    virtual void setLayers(const QStringList &names, int currentIndex) = 0;

    /* Richer refresh used by the list panel: names + per-layer enabled + which is Base.
       Default forwards to setLayers so the dropdown header needs no change; the list
       panel overrides it to build the per-row checkboxes. DevelopProperties always calls
       THIS (the dropdown just drops the extra state). */
    virtual void setLayerRows(const QVector<LayerRowInfo> &rows, int active) {
        QStringList names;
        names.reserve(rows.size());
        for (const LayerRowInfo &r : rows) names << r.name;
        setLayers(names, active);
    }
    virtual void setPreviewShown(bool shown) = 0;           // eye icon (layer preview)
    virtual void setBaseActive(bool isBase) = 0;            // Base: omit per-layer group
    virtual void setMaskOverlayAvailable(bool available) = 0;
    virtual void setMaskOverlayShown(bool shown) = 0;
    virtual void setMaskBreakdownShown(bool shown) = 0;     // Result <-> Breakdown state
    virtual void setCollapsed(bool collapsed) = 0;          // programmatic, no signal
    virtual bool isCollapsed() const = 0;
    virtual QString currentLayerName() const = 0;

signals:
    void layerSelected(const QString &name);    // user picked a different layer
    void renameRequested();                      // menu: rename the selected layer
    void resetLayerRequested();                  // menu: reset the layer to identity
    void removeLayerRequested();                 // menu: remove the selected layer
    void addLayerRequested();                    // menu: add a new layer
    void addMaskRequested();                     // menu: add a mask tool to this layer
    void maskOverlayToggled();                   // menu: show/hide the mask overlay
    void maskBreakdownToggled();                 // menu: Result view <-> Breakdown view
    void previewToggled(bool shown);             // [E] show/ignore the whole layer
    void collapseToggled(bool collapsed);        // > hide/show the layer's tree items
    /* List panel only: a layer row's show/hide checkbox was toggled (index into the
       layer stack, 0 = Base which has no checkbox). */
    void layerEnabledToggled(int index, bool on);
};

#endif // LAYERHEADERBASE_H
