#ifndef SCOPEHEADERLAB_H
#define SCOPEHEADERLAB_H

#include "Develop/Properties/scopeheaderbase.h"

#include <QIcon>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;
class QVBoxLayout;
class QComboBox;
class BarBtn;

/*
    ScopeHeaderLab (experimental, behind G::useScopeHeaderLab) -- the redesigned Develop
    dock scope control. It replaces the dropdown ScopeHeader with a VERTICAL LIST:

        | Edits                      [eye] [v] |   <- gradient header + panel menu
        |   Global                   [eye] [v] |   <- eye = show/hide, [v] = actions
        |   Scope 1                  [eye] [v] |
        |   Scope 2                  [eye] [v] |

    Every band and every row carries the SAME trailing pair, eye then menu: the eye
    shows/hides that row's changes, the vertical ellipsis opens its actions, and the menu
    is always the last thing on the line.

    The detail widgets (setRowDetail), in slot order: the MaskPanel, then the adjustment
    tree (DevelopProperties). They are the caller's widgets: rebuild() leaves them
    parented to rowsContainer while it deletes the rows, then re-inserts them.

    WHERE they are re-inserted -- and whether the scope list exists at all -- is what
    G::developEditsLayout settles:

      Flat                     Nested                   Minimal
      | Edits      [eye][v] |  | Edits      [eye][v] |  | Scope [Global v][+][eye][v] |
      |   Global   [eye][v] |  |   Global   [eye][v] |  |   MaskPanel                 |
      |   Mask 1   [eye][v] |  |   Mask 1   [eye][v] |  |   Basic / Color / ...       |
      |   Mask 2   [eye][v] |  |     MaskPanel       |
      | Mask 1     [eye][v] |  |     Basic / Color   |
      |   MaskPanel         |  |   Mask 2   [eye][v] |
      |   Basic / Color     |

    FLAT keeps the scope list whole and the sliders at a fixed height whatever is
    selected; the editor region below carries its own band naming the active scope
    (editorBand), since it no longer touches the row it belongs to. NESTED puts the
    editor directly under its row, so it moves with the selection. MINIMAL drops the list
    for a one-line scope bar -- a dropdown carrying every scope, plus [+] new mask, the
    active scope's [eye] and its [:] -- with the editor straight below and NO containment
    rail: with one bar and one editor adjacent and alone there is nothing left to
    bracket. Every layout has only ONE editor: DevelopProperties::buildTree repopulates
    it on every switch.

    Still a ScopeHeaderBase, so DevelopProperties binds it exactly like the dropdown
    (bindScopeHeader) and drives it via setScopeRows(); the class name stays
    ScopeHeaderLab so the swap-in path (copy over scopeheader.*) is unchanged.

    Interaction (all rebuild-safe -- emissions that loop back into setScopeRows are
    deferred a tick so a row widget is never deleted inside its own signal handler):
      - Click a row body        -> scopeSelected(name)      (makes it the active scope)
      - Row eye                 -> scopeEnabledToggled(i,on) (EditScope::enabled)
      - Band eye                -> scopeEnabledToggled(active,on): the ACTIVE scope,
                                   so the band mirrors the selected row's eye
      - Panel [v]               -> addScopeRequested
      - Row  [v]                -> selects the row, then addMask / reset / remove / rename
                                   (Global's menu carries only what applies to it)

    The whole-mask preview eye, collapse arrow and mask-overlay menu rows of the old
    header are gone: show/hide is the per-row eye, and the mask-overlay controls
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

    /* The editor widgets, drawn top to bottom in slot order: MaskDetail is the MaskPanel,
       EditsDetail the adjustment tree (NOT indented, so its containment rail lines up
       with this widget's). Each is owned by the CALLER and survives every rebuild. Pass
       nullptr to detach a slot.
       The indent is the NESTED-layout indent (the detail starts under the scope NAME);
       the flat and minimal layouts drop it to 0, since there is no row above to line up
       with. It is remembered per slot so a live layout flip can re-apply the right one. */
    enum RowDetailSlot { MaskDetail = 0, EditsDetail = 1, RowDetailSlotCount };
    void setRowDetail(QWidget *detail, int slot, int indent);

    /* Left inset of a nested detail: the same inset the row captions carry, so a detail
       starts under the scope NAME. */
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
    /* Flat layout: the rail comes in two pieces, since the selected row and its editor
       are no longer adjacent -- the row's own band, and the editor region below the
       list. Both at the same x, so they read as one interrupted line. */
    QRect activeRowRect() const;
    QRect editorBlockRect() const;
    void applyDetailIndent(int slot);     // wrapper margin for the current layout
    void updateEditorBand();              // caption + visibility of the editor band
    void buildScopeBar(QVBoxLayout *outer);   // minimal layout: one-line scope selector
    void updateScopeBar();                // refill the combo + sync its eye (no signal)
    void applyLayoutMode();               // show the bar or the list, per the layout
    void showPanelMenu();                 // panel [v]: New mask / Reset all edits
    /* The actions that belong to the PANEL rather than to one scope -- Reset all edits
       and the Edits layout picker. Appended to the row menu as well in the minimal
       layout, where the bar's [:] is the only menu there is. */
    void addPanelActions(QMenu *menu);
    void addHelpAction(QMenu *menu);      // trailing "Edits help" on every menu here
    /* Band eye: toggle the ACTIVE scope's enabled flag (the selected row's eye). */
    void toggleActiveEnabled();
    void updateBandEyeIcon();             // sync the band eye to the active row's state
    static void setEyeIcon(BarBtn *b, bool shown);      // eye.png / eye_off.png
    /* Row [v]: per-scope actions. Global gets the shorter menu (no mask, rename or
       delete), so isGlobal picks which set is built. */
    void showRowMenu(int index, const QString &name, bool isGlobal);
    void selectRowDeferred(const QString &name);        // emit scopeSelected next tick
    /* The header's collapse arrow: hide/show the scope LIST (not the tree). Collapsing
       falls back to Global, since no scope can be picked while the list is hidden. */
    void toggleListCollapsed();
    void updateListCollapseIcon();

    QWidget     *headerBand    = nullptr;
    BarBtn      *collapseBtn   = nullptr;
    QLabel      *titleLabel    = nullptr;
    BarBtn      *bandEyeBtn    = nullptr;   // show/hide the ACTIVE scope's changes
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
    int          detailIndent[RowDetailSlotCount] = {};   // nested-layout indent
    /* Flat layout only: the band heading the editor region ("Editing: Mask 1"), so the
       sliders below it still name the scope they belong to. Hidden when nested, where
       adjacency says it instead. Carries the same trailing [eye][:] pair as every other
       band and row here, acting on the ACTIVE scope. */
    QWidget     *editorBand      = nullptr;
    QLabel      *editorBandLabel = nullptr;
    BarBtn      *editorEyeBtn    = nullptr;
    BarBtn      *editorMenuBtn   = nullptr;

    /* MINIMAL layout only: the whole scope control on one line, replacing the header band
       AND the rows -- "Scope: [Global v] [+] [eye] [:]". The combo carries every scope,
       and the trailing pair acts on whichever it has selected. Built once, hidden in the
       other layouts. */
    QWidget     *scopeBar        = nullptr;
    QComboBox   *scopeCombo      = nullptr;
    BarBtn      *barAddBtn       = nullptr;   // [+] new mask (buried in [:] elsewhere)
    BarBtn      *barEyeBtn       = nullptr;
    BarBtn      *barMenuBtn      = nullptr;

    /* menuIcon removed: the scope menu buttons now load ellipsis_vertical.png through
       BarBtn::setIcon(path, G::iconOpacity) at each call site. */

    QStringList names;                     // current row names (for currentScopeName)
    /* Per-row enabled state from the last setScopeRows, so the band eye can report and
       flip the ACTIVE row without asking the owner. */
    QVector<bool> enabledStates;
    int  activeIndex  = 0;
    bool previewShown = true;
    bool collapsed    = false;             // base setCollapsed state (inert)
    bool listCollapsed = false;            // header arrow: the scope LIST is hidden
    bool globalActive   = true;
    bool maskOverlayAvailable = false;
    bool maskOverlayShown     = true;
};

#endif // SCOPEHEADERLAB_H
