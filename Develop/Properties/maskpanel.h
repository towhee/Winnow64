#ifndef MASKPANEL_H
#define MASKPANEL_H

#include <QColor>
#include <QVector>
#include <QWidget>
#include <QString>
#include <QStringList>

#include "Develop/Properties/submasklist.h"

class QLabel;
class QPushButton;
class BarBtn;
class MaskEditor;

/*
    MaskPanel (behind G::useScopeHeaderLab) -- the active mask's editor, NESTED under its
    own row in the Edits (scope) list. It is up whenever a mask is the active scope, not
    only while a submask is being built:

        | [x] Mask 2                          [:] |   <- the scope row (ScopeHeaderLab)
        | |  v Mask                          [:]  |   <- MASK level: the folded result
        | |    Edge      -----o-----              |
        | |    Halo      -----o-----              |
        | |  v Submasks                      [+]  |   <- SubmaskList: the mask's contents
        | |    [x] (+) Linear Gradient       [:]  |
        | |    [x] (-) Brush            *    [:]  |   <- selected -> settings below
        | |    Feather   -----o-----              |   <- embedded MaskEditor (submask)
        | |    Invert    [ ]                      |
        | |    Edge      -----o-----              |
        | |    [    Add and Commit    ]           |   <- pending; existing reads "Done"

    The overlay's APPEARANCE (colour, grayscale background) is NOT here: it describes the
    veil rather than any submask, and is edited from the Develop action row's tint button
    (right-click). Only the palette still lives with this class -- overlayColours().

    Terms: the MASK is what the scope applies; each SUBMASK is a building block folded
    into it, in list order. The submask's SETTINGS render in the embedded MaskEditor so
    they look identical to the property tree's other rows.

    Two states, because a submask is only fragile while it is NEW:
      - PENDING (just picked): it exists in the model but is discarded on Cancel/Esc or
        on leaving the image. ONE commit button, whose label names the op the overlay is
        previewing AND the act -- no modifier "Add and Commit", Opt "Subtract and
        Commit", Shift+Opt "Intersect and Commit" (a bare "Update" read as "refresh"
        once the render went live). MW arbitrates the modifiers
        (developShortcutIntercept) and calls DevelopProperties::setPendingMaskOp, which
        relabels the button. Return commits too.
      - EXISTING (re-opened from the list): its edits are already real, so the button is
        just "Done" (deselect) and there is nothing to cancel. Modifiers are inert -- the
        op is changed on the submask's own row.

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
    /* Relabel the commit button as the previewed op changes (MaskOp Add/Subtract/
       Intersect). Ignored while first, where only Add is possible, and while editing an
       existing submask, whose op is set on its row. */
    void setPendingOp(int op);
    /* Which of the two states above the panel is in: an existing submask cannot be
       cancelled, and its button reads "Done". */
    void setEditingExisting(bool existing);
    /* Show or hide the settings + commit block. Hidden when no submask is selected, so
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
    /* The commit button was clicked. No op is carried: the op is resolved from the LIVE
       modifier state at the instant of the commit (DevelopProperties::
       maskOpFromModifiers), the same source that drives this button's label, so the two
       can never disagree. On an existing submask this means "Done" -- deselect. */
    void committed();
    void cancelled();                      // [x] / Cancel (discard the pending submask)
    /* Mask band [:]: put the folded mask back to Edge 0 / Halo 0. The panel holds no
       model state, so DevelopProperties does the zeroing and pushes the rows back. */
    void resetMaskLevelRequested();
    /* Mask band [:] last item, as on every band in the dock: the mask help page (the
       same one the Submasks band opens -- Edge and Halo are documented there). */
    void helpRequested();

protected:
    void paintEvent(QPaintEvent *) override;      // gradient behind the "Mask" band
    bool eventFilter(QObject *watched, QEvent *event) override;   // band click=collapse

private:
    void buildUi();
    void buildMaskLevel(QVBoxLayout *outer);   // the "Mask" band + its Edge/Halo rows
    void refreshCommitBtn();               // label + cancel visibility for the state
    void syncAttrVisible();                // attrShown AND the list is not collapsed
    void syncLevelVisible();               // levelShown, and the band's own collapse
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
    QWidget     *attrWrap    = nullptr;    // maskEditor + commit row (hidden when none)
    QPushButton *commitBtn   = nullptr;    // Add / Subtract / Intersect "and Commit"
    BarBtn      *cancelBtn   = nullptr;    // discard a pending submask (hidden if not)
    int          pendingOp  = 0;           // MaskOp the button/label currently shows
    bool         firstMask  = true;        // only Add is possible on an empty mask
    bool         editingExisting = false;  // re-opened submask: "Done", no cancel
    bool         attrShown  = false;       // a submask is selected (collapse aside)
    bool         levelShown = false;       // the mask has at least one submask
    /* Open by default, unlike the Submasks list: these two sliders ARE the section, so a
       collapsed band would hide the whole point of adding it. */
    bool         levelCollapsed = false;
};

#endif // MASKPANEL_H
