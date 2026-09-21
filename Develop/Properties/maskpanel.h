#ifndef MASKPANEL_H
#define MASKPANEL_H

#include <QColor>
#include <QVector>
#include <QWidget>
#include <QString>
#include <QStringList>

#include "Develop/Properties/submasklist.h"

class QLabel;
class BarBtn;
class MaskEditor;

/*
    MaskPanel -- the active mask's editor, directly below the Edits scope bar. It is up
    whenever a mask is the active scope, not only while a submask is being built:

        | Edits  [Mask 2 v] [+] [eye] [:]         |   <- the scope bar (ScopeHeader)
        | |  v Mask                          [:]  |   <- MASK level: the folded result
        | |    Edge      -----o-----              |
        | |    Halo      -----o-----              |
        | |  v Submasks                      [+]  |   <- SubmaskList: the mask's contents
        | |    [x] (+) Linear Gradient       [:]  |
        | |    [x] (-) Brush            *    [:]  |   <- selected -> settings below
        | |    Feather   -----o-----              |   <- embedded MaskEditor (submask)
        | |    Invert    [ ]                      |
        | |    Edge      -----o-----              |

    The overlay's APPEARANCE (colour, grayscale background) is NOT here: it describes the
    veil rather than any submask, and is edited from the Develop action row's tint button
    (right-click). Only the palette still lives with this class -- overlayColours().

    Terms: the MASK is what the scope applies; each SUBMASK is a building block folded
    into it, in list order. The submask's SETTINGS render in the embedded MaskEditor so
    they look identical to the property tree's other rows.

    There is no commit button, because there was never anything buffered for one to
    flush: every edit -- shape drag, feather, edge, invert, brush settings -- is written
    into the submask and rendered as it is made. The only thing a commit decides is the
    combine op, and that stays changeable on the submask's row afterwards, so it is not
    a decision worth interrupting the user for. Two states remain:
      - PENDING (just picked, still settling): it exists in the model but is discarded by
        Esc or by deleting its row, and it folds itself into the mask a couple of seconds
        after the last edit (DevelopProperties::armMaskAutoCommit). Until then MW
        arbitrates the modifiers (developShortcutIntercept) and calls
        DevelopProperties::setPendingMaskOp, so the veil, the render and the on-canvas op
        chip preview what the submask will do. Return lands it early.
      - EXISTING (settled, or re-opened from the list): its edits are already real, Esc
        and Return just close it, and modifiers are inert -- the op is changed on the
        submask's own row.

    DevelopProperties owns the mask model and drives the panel.
*/
class MaskPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MaskPanel(QWidget *parent = nullptr);

    /* Open the panel on a NEWLY picked submask. first == true: the mask was empty, so
       there is nothing to combine with and the op modifiers are inert. */
    void beginPending(bool first);
    /* Track the op the veil is previewing (MaskOp Add/Subtract/Intersect). Ignored while
       first, where only Add is possible, and while editing a submask that has already
       landed, whose op is set on its row. */
    void setPendingOp(int op);
    /* Which of the two states above the panel is in: a submask that has already landed
       takes no op from the modifiers. */
    void setEditingExisting(bool existing);
    /* Show or hide the settings block. Hidden when no submask is selected, so
       the panel shows just the list (and the mask's overlay controls). Also hidden while
       the Submasks section is COLLAPSED: the settings belong to a submask in that list,
       so leaving them on screen made a collapsed section look half-open. */
    void showAttributes(bool show);
    /* The header row above the settings, naming WHAT a change to them will affect --
       "Attribute adjustment applies to 7 strokes" / "to next stroke" / "to last stroke" /
       "to this Linear Gradient submask". A brush's attributes can mean any of three
       things depending on whether a stroke is being added and whether Shift is held, and
       nothing on screen used to say which. Empty text hides the row. */
    void setAttributeScope(const QString &text);
    /* The embedded tree that renders the submask's settings (Size/Feather/etc.) --
       populated and wired by DevelopProperties so it edits the mask model. */
    MaskEditor *editor() const { return maskEditor; }
    /* The MASK-level settings (Edge, Halo), rendered under the "Mask" band ABOVE the
       Submasks row -- they act on the whole mask, not on anything in the list. A second
       editor rather than rows inside `editor()`, because that one is rebuilt per selected
       submask and hides when none is. */
    MaskEditor *levelEditor() const { return maskLevelEditor; }
    /* Show the mask-level block (its header row and, unless collapsed, its sliders).
       Hidden when the mask has no submasks: there is no mask to grow or shrink, so the
       controls would be no-ops with nothing to explain them. */
    void showMaskLevel(bool show);
    /* The mask's submasks. DevelopProperties pushes rows in and binds its signals. */
    SubmaskList *list() const { return submaskList; }
    /* THE overlay-colour palette. Static because the Develop action-row tint button's
       context menu offers the same colours: one list, so the two pickers cannot drift. */
    static const QVector<QColor> &overlayColours();
    /* Display names for overlayColours(), same order (a menu needs words, chips do
       not). Kept beside the palette so adding a colour cannot leave a menu unlabelled. */
    static const QStringList &overlayColourNames();

signals:
    /* Mask band [:]: put the folded mask back to Edge 0 / Halo 0. The panel holds no
       model state, so DevelopProperties does the zeroing and pushes the rows back. */
    void resetMaskLevelRequested();
    /* Mask band [:] last item, as on every band in the dock: the mask help page (the
       same one the Submasks band opens -- Edge and Halo are documented there). */
    void helpRequested();
    /* The cursor entered or left the panel, CHILDREN INCLUDED. DevelopProperties reads
       it as "the mask has the user's attention" and brings the coverage veil up (see
       DevelopProperties::maskVeilEngaged). */
    void hoverChanged(bool hovered);

protected:
    void paintEvent(QPaintEvent *) override;      // gradient behind the "Mask" band
    bool eventFilter(QObject *watched, QEvent *event) override;   // band click=collapse
    void hideEvent(QHideEvent *e) override;                       // a Leave that never comes

private:
    /* Recompute hovered from the global cursor position and emit on a change. Driven
       from the application-wide Enter/Leave filter rather than this widget's own
       enterEvent/leaveEvent: moving onto a CHILD sends the panel a Leave, and
       SubmaskList rebuilds its rows wholesale, so there is no stable set of children to
       watch. Two comparisons on each enter/leave anywhere in the app is cheaper than
       keeping that bookkeeping correct. */
    void syncHovered();
    bool hovered = false;

    void buildUi();
    void buildMaskLevel(QVBoxLayout *outer);   // the "Mask" band + its Edge/Halo rows
    void syncAttrVisible();                // attrShown, list not collapsed, mask not folded
    void syncLevelVisible();               // levelShown, and the band's own collapse
    bool maskFolded() const;               // the "Mask" band is on screen and closed
    void toggleLevelCollapsed();
    void updateLevelCollapseIcon();
    void showLevelMenu();                  // Mask band [:]: reset, help

    MaskEditor  *maskLevelEditor = nullptr;// mask-level settings (Edge/Halo), above list
    QWidget     *levelWrap   = nullptr;    // band + sliders (hidden when no submasks)
    QWidget     *levelBand   = nullptr;    // "Mask" header row (gradient, click=collapse)
    BarBtn      *levelCollapseBtn = nullptr;
    QLabel      *levelTitle  = nullptr;
    BarBtn      *levelMenuBtn = nullptr;   // band [:]: reset / help
    QWidget     *levelBody   = nullptr;    // maskLevelEditor's wrapper (collapses)
    SubmaskList *submaskList = nullptr;    // the mask's contents, above the settings
    QLabel      *scopeLabel  = nullptr;    // "changes apply to ..." above the settings
    MaskEditor  *maskEditor  = nullptr;    // tree-rendered settings of the selected one
    QWidget     *attrWrap    = nullptr;    // the maskEditor's wrapper (hidden when none)
    int          pendingOp  = 0;           // MaskOp the veil is currently previewing
    bool         firstMask  = true;        // only Add is possible on an empty mask
    bool         editingExisting = false;  // the open submask has already landed
    bool         attrShown  = false;       // a submask is selected (collapse aside)
    bool         levelShown = false;       // the mask has at least one submask
    /* Open by default, unlike the Submasks list: these two sliders ARE the section, so a
       collapsed band would hide the whole point of adding it. */
    bool         levelCollapsed = false;
};

#endif // MASKPANEL_H
