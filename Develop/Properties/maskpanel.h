#ifndef MASKPANEL_H
#define MASKPANEL_H

#include <QWidget>
#include <QString>

class QLabel;
class QPushButton;
class BarBtn;
class MaskEditor;

/*
    MaskPanel (experimental, behind G::useScopeHeaderLab) -- the transient mask build-up
    strip. It appears ABOVE the Scopes list only while a mask tool is being defined and
    closes on commit or cancel/Esc.

        | Mask: Linear Gradient             [x] |   <- header + cancel
        | [Add]   [Subtract]   [Intersect]      |   (2nd+ tool: blue, fold into the mask)
        | [Done]                                |   (first tool: red, already the mask)

    The tool's SETTINGS (feather, brush size/flow, etc.) are NOT in this panel -- they
    render in the property tree below (addToolRow), so they look identical to the tree's
    other sliders/checkboxes. This panel only names the tool and commits it: the first
    tool on an empty mask is committed immediately (edited red, [Done]); each later tool
    is a blue preview folded in via [Add]/[Subtract]/[Intersect]. DevelopProperties owns
    the mask model and drives the panel.
*/
class MaskPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MaskPanel(QWidget *parent = nullptr);

    /* Show the panel for a tool. first == true: the mask was empty, so this tool is the
       mask (red) and the panel shows [Done]; else it is a preview (blue) with the three
       combine buttons. */
    void showForTool(const QString &toolName, bool first);
    /* The embedded tree that renders the tool's settings (Size/Feather/etc.) -- populated
       and wired by DevelopProperties so it edits the mask model. */
    MaskEditor *editor() const { return maskEditor; }

signals:
    void committed(int op);                // MaskOp Add/Subtract/Intersect (2nd+ tool)
    void finished();                       // [Done] on the first tool (keep it)
    void cancelled();                      // [x] / Cancel (discard the preview)

protected:
    void paintEvent(QPaintEvent *) override;   // gradient behind the header band

private:
    void buildUi();

    QLabel      *titleLabel = nullptr;
    QWidget     *headerBand = nullptr;
    BarBtn      *cancelBtn  = nullptr;
    MaskEditor  *maskEditor = nullptr;     // tree-rendered settings, above the buttons
    QWidget     *combineRow = nullptr;     // [Add][Subtract][Intersect] (2nd+ tool)
    QPushButton *doneBtn    = nullptr;     // [Done] (first tool)
};

#endif // MASKPANEL_H
