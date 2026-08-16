#ifndef SCOPEHEADERLAB_H
#define SCOPEHEADERLAB_H

#include "Develop/Properties/scopeheaderbase.h"

#include <QIcon>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;
class QCheckBox;
class QVBoxLayout;
class BarBtn;

/*
    ScopeHeaderLab (experimental, behind G::useScopeHeaderLab) -- the redesigned Develop
    dock scope control. It replaces the dropdown ScopeHeader with a VERTICAL LIST:

        | Scopes                            [v] |   <- gradient header + panel menu
        | [x] Global                              |   <- checkbox = show/hide (no [v] menu)
        | [x] Scope 1                       [v] |   <- checkbox = show/hide, [v] = actions
        | [ ] Scope 2                       [v] |

    The ACTIVE row carries nested detail widgets (setRowDetail), in slot order: the
    MaskPanel, then the adjustment tree (DevelopProperties). A mask's submasks and the
    Basic/Color/... sections therefore sit under the scope they belong to rather than in
    separate strips below the whole list. They are the caller's widgets: rebuild() leaves
    them parented to rowsContainer while it deletes the rows, then re-inserts them under
    the new active row.

    Still a ScopeHeaderBase, so DevelopProperties binds it exactly like the dropdown
    (bindScopeHeader) and drives it via setScopeRows(); the class name stays
    ScopeHeaderLab so the swap-in path (copy over scopeheader.*) is unchanged.

    Interaction (all rebuild-safe -- emissions that loop back into setScopeRows are
    deferred a tick so a row widget is never deleted inside its own signal handler):
      - Click a row body        -> scopeSelected(name)      (makes it the active scope)
      - Row checkbox            -> scopeEnabledToggled(i,on) (EditScope::enabled)
      - Panel [v]               -> addScopeRequested
      - Row  [v]                -> selects the row, then addMask / reset / remove / rename

    The whole-mask preview eye, collapse arrow and mask-overlay menu rows of the old
    header are gone: show/hide is the per-row checkbox, and the mask-overlay controls
    move to the Mask panel. Those base setters are kept as inert state-holders so the
    interface (and DevelopProperties' existing calls) still compile and behave.
*/
class ScopeHeaderLab : public ScopeHeaderBase
{
    Q_OBJECT
public:
    explicit ScopeHeaderLab(QWidget *parent = nullptr);

    /* Legacy contract: forwards to setScopeRows (all enabled, index 0 = Global). */
    void setScopes(const QStringList &names, int currentIndex) override;
    /* Primary refresh: rebuild the list from names + per-scope enabled + Global flag. */
    void setScopeRows(const QVector<ScopeRowInfo> &rows, int active) override;

    /* Widgets nested UNDER the active scope's row, drawn top to bottom in slot order:
       MaskDetail is the MaskPanel (indented under the scope name); EditsDetail is the
       adjustment tree, which is NOT indented so its containment rail lines up with this
       widget's. Each is owned by the CALLER and survives every rebuild. Pass nullptr to
       detach a slot. */
    enum RowDetailSlot { MaskDetail = 0, EditsDetail = 1, RowDetailSlotCount };
    void setRowDetail(QWidget *detail, int slot, int indent);

    /* Left inset of a nested detail: the row's own 10px margin plus roughly the show/hide
       checkbox, so the detail starts under the scope NAME. */
    static constexpr int kDetailIndent = 22;

    void setPreviewShown(bool shown) override { previewShown = shown; }
    void setGlobalActive(bool isGlobal) override { globalActive = isGlobal; }
    void setMaskOverlayAvailable(bool available) override { maskOverlayAvailable = available; }
    void setMaskOverlayShown(bool shown) override { maskOverlayShown = shown; }
    void setCollapsed(bool collapsed) override;      // hide/show the rows container
    bool isCollapsed() const override { return collapsed; }
    QString currentScopeName() const override;

protected:
    void paintEvent(QPaintEvent *) override;         // gradient behind the header band
    bool eventFilter(QObject *watched, QEvent *event) override;   // row-body clicks

private:
    void rebuild(const QVector<ScopeRowInfo> &rows, int active);
    QWidget *makeRow(int index, const ScopeRowInfo &r, bool active);
    /* The selected scope's block (its row + the details nested under it) in this widget's
       coordinates -- what the containment rail brackets. Empty if there is nothing to
       bracket (list collapsed). */
    QRect activeBlockRect() const;
    void showPanelMenu();                 // panel [v]: Add new scope
    void showRowMenu(int index, const QString &name);   // row [v]: per-scope actions
    void selectRowDeferred(const QString &name);        // emit scopeSelected next tick
    /* The header's collapse arrow: hide/show the scope LIST (not the tree). Collapsing
       falls back to Global, since no scope can be picked while the list is hidden. */
    void toggleListCollapsed();
    void updateListCollapseIcon();

    QWidget     *headerBand    = nullptr;
    BarBtn      *collapseBtn   = nullptr;
    QLabel      *titleLabel    = nullptr;
    BarBtn      *panelMenuBtn  = nullptr;
    QWidget     *rowsContainer = nullptr;
    QVBoxLayout *rowsLayout    = nullptr;
    /* The SELECTED scope's row widget, re-captured on every rebuild: the rail brackets
       it and the details below it, and nothing else. */
    QWidget     *activeRow     = nullptr;
    /* The nested detail widgets (setRowDetail) and the indenting wrappers they sit in.
       The wrappers are ours; the details inside them are not, so neither is ever deleted
       by the row teardown. */
    QWidget     *rowDetail[RowDetailSlotCount]  = {};
    QWidget     *detailWrap[RowDetailSlotCount] = {};

    /* menuIcon removed: the scope menu buttons now load ellipsis_vertical.png through
       BarBtn::setIcon(path, G::iconOpacity) at each call site. */

    QStringList names;                     // current row names (for currentScopeName)
    int  activeIndex  = 0;
    bool previewShown = true;
    bool collapsed    = false;             // base setCollapsed state (inert)
    bool listCollapsed = false;            // header arrow: the scope LIST is hidden
    bool globalActive   = true;
    bool maskOverlayAvailable = false;
    bool maskOverlayShown     = true;
};

#endif // SCOPEHEADERLAB_H
