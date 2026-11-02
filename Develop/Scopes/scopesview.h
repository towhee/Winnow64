#ifndef SCOPESVIEW_H
#define SCOPESVIEW_H

#include <QWidget>
#include "Develop/Scopes/scopedata.h"
#include "Develop/Scopes/histogramview.h"
#include "Develop/Scopes/vectorscopeview.h"
#include "Develop/Scopes/toneregionslider.h"

class QVBoxLayout;
class QHBoxLayout;
class QMenu;
class BarBtn;

/*
    The Develop dock's scopes panel: a fixed-height strip placed ABOVE the property
    tree (see MW::createDevelopDock) holding the histogram on the left and the
    vectorscope on the right. The left/right split is the QHBoxLayout stretch ratio
    (histogram wider, since its x-axis is the full tonal range).

    setScopeLayout() switches which scopes the strip shows: both side-by-side, the
    histogram alone or the vectorscope alone, the single scope expanding to fill the
    strip. The layout is picked from the right-click menu of EITHER scope (each scope
    builds its own items, this widget relays the part-built menu out as menuRequested and
    MW::showDevelopScopesMenu appends the shared layout section and shows it) or from the
    Develop menu's Scopes submenu; MW persists the choice.

    setData() fans one ScopeData out to both child scopes; clear() blanks them when no
    image is shown. MW owns this widget and feeds it from updateDevelopScopes().

    The strip carries a "Histogram" header band, styled like the Raw panel's: a collapse
    arrow, the caption and a trailing menu button. Clicking the band (or the arrow)
    collapses / expands the scopes and emits expandToggled, which MW turns into the
    persisted "Develop/scopesVisible". The menu button opens the same scopes-layout menu
    a right-click does (menuRequested with an empty menu). The header stays on screen
    while collapsed, so it is the way back to the scopes.
*/
class ScopesView : public QWidget
{
    Q_OBJECT
public:
    /* Strip contents. The values are persisted in QSettings, so do not renumber. */
    enum ScopeLayout {
        Both = 0,               // histogram (+ tone slider) left, vectorscope right
        HistogramOnly = 1,      // histogram (+ tone slider) fills the strip
        VectorscopeOnly = 2     // vectorscope fills the strip
    };

    explicit ScopesView(QWidget *parent = nullptr);
    void setScopeLayout(ScopeLayout layout);
    ScopeLayout scopeLayout() const { return curLayout; }
    void setData(const ScopeData &d);
    void clear();
    void setMarker(int r, int g, int b);   // loupe cursor readout on both scopes
    void clearMarker();
    void setVectorscopeZoom(double z);     // apply a persisted/initial zoom
    void setVectorscopeSkinLine(bool on);  // apply a persisted/initial skin-line state
    /* Show / hide the scopes below the header band (programmatic: no expandToggled). */
    void setExpanded(bool expanded);
    bool isExpanded() const { return expanded; }
    ToneRegionSlider *toneRegionSlider() const { return tone; }

signals:
    void vectorscopeZoomChanged(double z);     // user picked a zoom from the vectorscope menu
    void vectorscopeSkinLineChanged(bool on);  // user toggled the skin line
    void expandToggled(bool expanded);         // USER clicked the header band / arrow
    /* Right-click anywhere in the strip. `menu` carries whatever items the scope under
       the cursor added (the vectorscope's zoom / skin line; none from the histogram yet);
       MW appends the shared scopes-layout section and execs it. The menu lives on the
       emitting scope's stack, so the receiver must handle it synchronously. */
    void menuRequested(QMenu *menu, QPoint globalPos);

protected:
    /* Consume double-clicks landing on the strip's margins so they do not bubble to
       the dock (which would un/redock); the child scopes consume their own. */
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    /* Right-clicks that no scope claimed (the tone slider, the strip's margins): an
       empty menu, so only the shared scopes-layout section shows. */
    void contextMenuEvent(QContextMenuEvent *event) override;
    /* Header gradient, content background and the separator rule (G::panelBorderHeight
       in G::panelSeparatorColor) across the bottom edge, like every other Develop panel. */
    void paintEvent(QPaintEvent *event) override;
    /* Clicking the "Histogram" header band toggles collapse like the arrow. */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void toggleExpanded();              // header band / arrow click
    void updateCollapseIcon();
    void updateHeight();

    QWidget *headerBand = nullptr;
    BarBtn *collapseBtn = nullptr;
    BarBtn *menuBtn = nullptr;
    QWidget *body = nullptr;            // the scopes (collapse hides)
    bool expanded = true;
    HistogramView *histogram;
    VectorscopeView *vectorscope;
    ToneRegionSlider *tone;
    QVBoxLayout *leftCol = nullptr;     // histogram + tone slider column
    QHBoxLayout *rowLay = nullptr;      // leftCol | vectorscope
    ScopeLayout curLayout = Both;
};

#endif // SCOPESVIEW_H
