#ifndef REPLACEPANEL_H
#define REPLACEPANEL_H

#include <QWidget>

class QLabel;
class QPushButton;
class BarBtn;
class GradientHeader;

/*
    The Develop dock's replace-tool panel: the controls for the tool chosen from the
    action-row Replace menu (MW::showDevelopReplaceMenu). Sibling of TransformPanel -- a
    strip below the scopes and above the property tree (see MW::createDevelopDock), shown
    while the tool is armed and hidden when it is turned off. Its [X] turns the tool
    off, so it can always be closed. While it is up the rest of the Develop dock is
    greyed (MW::setDevelopPanelEnabled).

    The menu CHOOSES the tool; this panel shows what that tool offers. It reads as three
    panels, one class -- setMode swaps the title and the body (panelTitle):

        Spot / Heal / Clone                     Fill (Fill) / Inpaint (Object)
        |------------------------------------|  |------------------------------------|
        | Replace                   [?][E][X]|  | Fill                      [?][E][X]|
        |------------------------------------|  |------------------------------------|
        | Spot                               |  | Paint over something thin or ...   |
        | Click a blemish: it is healed ...  |  | Make as many strokes as you need.  |
        | 3 spots on this image              |  | Hold Opt/Alt ... to erase ...      |
        | [Copy all spots] [Paste all spots] |  | Return or Enter  execute: replace..|
        | 3 spots copied. Paste adds ...     |  | Esc              cancel: clear ... |
        |------------------------------------|  |------------------------------------|

    The Spot-only block (count, Copy, Paste once something is copied) shows for the Spot
    tool alone.

    Copy / Paste carry the Spot-tool repairs (kind "spot") from one image to every
    selected image -- sensor dust sits in the same place on every frame. The panel holds
    no spot state: DevelopProperties owns the copied spots and the image's repairs and
    pushes the counts in through setSpotCounts.
*/
class ReplacePanel : public QWidget
{
    Q_OBJECT
public:
    explicit ReplacePanel(QWidget *parent = nullptr);

    /* The tool's name and its one-line instruction (FillSpotGeom::Kind ids). Shared with
       the Replace menu so the menu tooltips and the panel never disagree. */
    static QString modeName(int mode);
    static QString modeHint(int mode);
    /* The panel's header: "Replace" (Spot, Heal, Clone), "Fill" (Fill),
       "Inpaint" (Object). */
    static QString panelTitle(int mode);
    /* Fill and Object: the strokes accumulate until Return/Enter (or Esc) -- said in the
       panel, and appended to the Replace menu tooltip. */
    static QString paintAreaKeysHint();

    void setMode(int mode);                // the tool chosen in the Replace menu
    void setPreviewShown(bool shown);      // sync the eye (no signal emitted)
    /* onImage = Spot-tool repairs on the current image, copied = spots held for Paste. */
    void setSpotCounts(int onImage, int copied);

signals:
    void tipsRequested();                  // [?]
    void previewToggled(bool shown);       // eye: show (true) or hide (false) the repairs
    void closeRequested();                 // [X]: turn the spot tool off
    void copySpotsRequested();             // Copy all spots
    void pasteSpotsRequested();            // Paste all spots

protected:
    /* Panel background + separator rule across the bottom edge, so stacked Develop
       panels read as distinct blocks (same as TransformPanel). */
    void paintEvent(QPaintEvent *event) override;

private:
    void buildUi();
    void updatePreviewButton();
    void updateSpotTools();

    GradientHeader *header   = nullptr;
    BarBtn      *tipBtn      = nullptr;
    BarBtn      *previewBtn  = nullptr;
    BarBtn      *closeBtn    = nullptr;
    QLabel      *modeLbl     = nullptr;    // tool name
    QLabel      *hintLbl     = nullptr;    // how to use it
    QWidget     *paintKeys   = nullptr;    // Fill / Object: strokes, Return/Enter, Esc
    QWidget     *spotTools   = nullptr;    // Spot tool only: copy / paste block
    QLabel      *countLbl    = nullptr;    // "3 spots on this image" or why Copy is off
    QPushButton *copyBtn     = nullptr;
    QPushButton *pasteBtn    = nullptr;
    QLabel      *copiedLbl   = nullptr;    // what Paste will add

    int  currentMode  = 0;
    bool previewShown = true;
    int  spotsOnImage = 0;
    int  spotsCopied  = 0;
};

#endif // REPLACEPANEL_H
