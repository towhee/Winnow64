#ifndef LAYERHEADERLAB_H
#define LAYERHEADERLAB_H

#include "Develop/Properties/layerheaderbase.h"

#include <QIcon>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;
class QCheckBox;
class QVBoxLayout;
class BarBtn;

/*
    LayersPanel (experimental, behind G::useLayerHeaderLab) -- the redesigned Develop
    dock layer control. It replaces the dropdown LayerHeader with a VERTICAL LIST:

        | Layers                            [v] |   <- gradient header + panel menu
        | [x] Base                              |   <- checkbox = show/hide (no [v] menu)
        | [x] Layer 1                       [v] |   <- checkbox = show/hide, [v] = actions
        | [ ] Layer 2                       [v] |

    Still a LayerHeaderBase, so DevelopProperties binds it exactly like the dropdown
    (bindLayerHeader) and drives it via setLayerRows(); the class name stays
    LayerHeaderLab so the swap-in path (copy over layerheader.*) is unchanged.

    Interaction (all rebuild-safe -- emissions that loop back into setLayerRows are
    deferred a tick so a row widget is never deleted inside its own signal handler):
      - Click a row body        -> layerSelected(name)      (makes it the active layer)
      - Row checkbox            -> layerEnabledToggled(i,on) (EditLayer::enabled)
      - Panel [v]               -> addLayerRequested
      - Row  [v]                -> selects the row, then addMask / reset / remove / rename

    The whole-layer preview eye, collapse arrow and mask-overlay menu rows of the old
    header are gone: show/hide is the per-row checkbox, and the mask-overlay controls
    move to the Mask panel. Those base setters are kept as inert state-holders so the
    interface (and DevelopProperties' existing calls) still compile and behave.
*/
class LayerHeaderLab : public LayerHeaderBase
{
    Q_OBJECT
public:
    explicit LayerHeaderLab(QWidget *parent = nullptr);

    /* Legacy contract: forwards to setLayerRows (all enabled, index 0 = Base). */
    void setLayers(const QStringList &names, int currentIndex) override;
    /* Primary refresh: rebuild the list from names + per-layer enabled + Base flag. */
    void setLayerRows(const QVector<LayerRowInfo> &rows, int active) override;

    void setPreviewShown(bool shown) override { previewShown = shown; }
    void setBaseActive(bool isBase) override { baseActive = isBase; }
    void setMaskOverlayAvailable(bool available) override { maskOverlayAvailable = available; }
    void setMaskOverlayShown(bool shown) override { maskOverlayShown = shown; }
    void setMaskBreakdownShown(bool shown) override { maskBreakdownShown = shown; }
    void setCollapsed(bool collapsed) override;      // hide/show the rows container
    bool isCollapsed() const override { return collapsed; }
    QString currentLayerName() const override;

protected:
    void paintEvent(QPaintEvent *) override;         // gradient behind the header band
    bool eventFilter(QObject *watched, QEvent *event) override;   // row-body clicks

private:
    void rebuild(const QVector<LayerRowInfo> &rows, int active);
    QWidget *makeRow(int index, const LayerRowInfo &r, bool active);
    void showPanelMenu();                 // panel [v]: Add new layer
    void showRowMenu(int index, const QString &name);   // row [v]: per-layer actions
    void selectRowDeferred(const QString &name);        // emit layerSelected next tick
    /* The header's collapse arrow: hide/show the layer LIST (not the tree). Collapsing
       falls back to Base, since no layer can be picked while the list is hidden. */
    void toggleListCollapsed();
    void updateListCollapseIcon();

    QWidget     *headerBand    = nullptr;
    BarBtn      *collapseBtn   = nullptr;
    QLabel      *titleLabel    = nullptr;
    BarBtn      *panelMenuBtn  = nullptr;
    QWidget     *rowsContainer = nullptr;
    QVBoxLayout *rowsLayout    = nullptr;

    QIcon menuIcon;                        // contextMenu.png (light-gray chevron)

    QStringList names;                     // current row names (for currentLayerName)
    int  activeIndex  = 0;
    bool previewShown = true;
    bool collapsed    = false;             // base setCollapsed state (inert)
    bool listCollapsed = false;            // header arrow: the layer LIST is hidden
    bool baseActive   = true;
    bool maskOverlayAvailable = false;
    bool maskOverlayShown     = true;
    bool maskBreakdownShown   = true;
};

#endif // LAYERHEADERLAB_H
