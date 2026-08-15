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
        | |  v Submasks                      [+]  |   <- SubmaskList: the mask's contents
        | |    [x] (+) Linear Gradient       [:]  |
        | |    [x] (-) Brush            *    [:]  |   <- selected -> settings below
        | |    Feather   -----o-----              |   <- embedded MaskEditor
        | |    Invert    [ ]                      |
        | |    [][][][][][]   [Gray]              |   <- overlay colour swatches
        | |    [    Add and Commit    ]           |   <- pending; existing reads "Done"

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
    /* The mask's submasks. DevelopProperties pushes rows in and binds its signals. */
    SubmaskList *list() const { return submaskList; }
    /* THE overlay-colour palette. Static because the Develop action-row tint button's
       context menu offers the same colours: one list, so the two pickers cannot drift. */
    static const QVector<QColor> &overlayColours();
    /* Display names for overlayColours(), same order (a menu needs words, chips do
       not). Kept beside the palette so adding a colour cannot leave a menu unlabelled. */
    static const QStringList &overlayColourNames();
    /* The overlay colour / grayscale flag was changed somewhere ELSE (the action-row
       tint button's context menu); repaint the chips so this panel agrees. */
    void syncOverlayControls();

signals:
    /* The commit button was clicked. No op is carried: the op is resolved from the LIVE
       modifier state at the instant of the commit (DevelopProperties::
       maskOpFromModifiers), the same source that drives this button's label, so the two
       can never disagree. On an existing submask this means "Done" -- deselect. */
    void committed();
    void cancelled();                      // [x] / Cancel (discard the pending submask)
    void overlayColourChanged();           // a swatch was clicked (G:: already updated)
    /* The grayscale toggle was flipped (G::maskOverlayGrayscale already updated): the
       image under the overlay is shown desaturated so the veil's colour stands out. */
    void overlayGrayscaleChanged();

private:
    void buildUi();
    void refreshSwatches();                // re-draw the selected border after a pick
    void refreshGrayBtn();                 // reflect G::maskOverlayGrayscale on the chip
    void refreshCommitBtn();               // label + cancel visibility for the state
    void syncAttrVisible();                // attrShown AND the list is not collapsed

    SubmaskList *submaskList = nullptr;    // the mask's contents, above the settings
    QLabel      *scopeLabel  = nullptr;    // "changes apply to ..." above the settings
    MaskEditor  *maskEditor  = nullptr;    // tree-rendered settings of the selected one
    QWidget     *attrWrap    = nullptr;    // maskEditor + commit row (hidden when none)
    QPushButton *commitBtn   = nullptr;    // Add / Subtract / Intersect "and Commit"
    BarBtn      *cancelBtn   = nullptr;    // discard a pending submask (hidden if not)
    QWidget     *swatchRow   = nullptr;    // overlay-colour picker
    QPushButton *grayBtn     = nullptr;    // desaturate the image under the overlay
    QVector<QPushButton*> swatches;
    QVector<QColor>       swatchColors;
    int          pendingOp  = 0;           // MaskOp the button/label currently shows
    bool         firstMask  = true;        // only Add is possible on an empty mask
    bool         editingExisting = false;  // re-opened submask: "Done", no cancel
    bool         attrShown  = false;       // a submask is selected (collapse aside)
};

#endif // MASKPANEL_H
