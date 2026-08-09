#ifndef MASKPANEL_H
#define MASKPANEL_H

#include <QColor>
#include <QVector>
#include <QWidget>
#include <QString>

class QLabel;
class QPushButton;
class BarBtn;
class MaskEditor;

/*
    MaskPanel (experimental, behind G::useScopeHeaderLab) -- the transient submask
    build-up strip. It appears ABOVE the Scopes list only while a submask is being
    defined and closes on commit or cancel/Esc.

        | Mask: Linear Gradient             [x] |   <- header + cancel
        | <submask settings>                    |   (embedded MaskEditor)
        | [][][][][][]                          |   <- overlay colour swatches
        | [        Update        ]              |   <- label follows the held modifier

    Terms: the MASK is what the scope applies; each SUBMASK is a building block folded
    into it. The submask's SETTINGS (feather, brush size/flow, etc.) render in the
    embedded MaskEditor so they look identical to the property tree's other rows.

    ONE commit button, whose label tracks the op the overlay is previewing: no modifier
    "Update" (Add), Opt "Subtract", Shift+Opt "Intersect". MW arbitrates the modifiers
    (developShortcutIntercept) and calls DevelopProperties::setPendingMaskOp, which
    relabels the button. Return commits too. DevelopProperties owns the mask model and
    drives the panel.
*/
class MaskPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MaskPanel(QWidget *parent = nullptr);

    /* Show the panel for a submask. first == true: the mask was empty, so there is
       nothing to combine with -- the op modifiers are inert and the hint is hidden. */
    void showForTool(const QString &toolName, bool first);
    /* Relabel the commit button as the previewed op changes (MaskOp Add/Subtract/
       Intersect). Ignored while first, where only Add is possible. */
    void setPendingOp(int op);
    /* The embedded tree that renders the submask's settings (Size/Feather/etc.) --
       populated and wired by DevelopProperties so it edits the mask model. */
    MaskEditor *editor() const { return maskEditor; }

signals:
    /* [Update] was clicked. No op is carried: the op is resolved from the LIVE modifier
       state at the instant of the commit (DevelopProperties::maskOpFromModifiers), the
       same source that drives this button's label, so the two can never disagree. */
    void committed();
    void cancelled();                      // [x] / Cancel (discard the submask)
    void overlayColourChanged();           // a swatch was clicked (G:: already updated)

protected:
    void paintEvent(QPaintEvent *) override;   // gradient behind the header band

private:
    void buildUi();
    void refreshSwatches();                // re-draw the selected border after a pick

    QLabel      *titleLabel = nullptr;
    QWidget     *headerBand = nullptr;
    BarBtn      *cancelBtn  = nullptr;
    MaskEditor  *maskEditor = nullptr;     // tree-rendered settings, above the buttons
    QPushButton *commitBtn  = nullptr;     // Update / Subtract / Intersect
    QWidget     *swatchRow  = nullptr;     // overlay-colour picker
    QVector<QPushButton*> swatches;
    QVector<QColor>       swatchColors;
    int          pendingOp  = 0;           // MaskOp the button/label currently shows
    bool         firstMask  = true;        // only Add is possible on an empty mask
};

#endif // MASKPANEL_H
