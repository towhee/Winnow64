#ifndef SUBMASKDIALOG_H
#define SUBMASKDIALOG_H

#include <QDialog>

class QListWidget;
class QRadioButton;
class QPushButton;
class QLabel;

/*
    SubmaskDialog: the one place a new submask is described before it exists -- WHAT it
    does to the mask, and WHAT KIND of thing it is.

        +---------------------------------------------------+
        |  Add submask                                      |
        |                                                   |
        |  What it does to the mask                         |
        |    (o) +  Add          joins the masked area      |
        |    ( ) -  Subtract     cuts out of it             |
        |    ( ) n  Intersect    keeps only the overlap     |
        |                                                   |
        |  Submask type                                     |
        |    Drawn                                          |
        |      Linear Gradient Mask                         |
        |      Radial Gradient Mask                         |
        |      Brush Mask                                   |
        |    Content                                        |
        |      Color Range Mask                    ...      |
        |                                                   |
        |                        [ Cancel ] [ Add submask ] |
        +---------------------------------------------------+

    It replaces a bare pop-up menu of tool names. That menu could only ask half the
    question: the op came from a modifier held at some later instant, which is
    undiscoverable (nothing on screen says Opt subtracts until you are already painting)
    and easy to lose (release the key at the wrong moment and the subtract you drew
    became an add). Asking both halves up front makes the op a written choice with its
    own words, and the modifiers stay as the shortcut for people who know them -- holding
    one still overrides the dialog's op while shaping (see DevelopProperties::latchMaskOp).

    The op is pinned to Add on an EMPTY mask: there is nothing to subtract from or
    intersect with, so the other two are disabled and the reason is on screen rather than
    left to be inferred from a control that does nothing.

    Owns no model state -- it reports a choice and DevelopProperties::beginMaskTool acts
    on it. Tool names come from DevelopProperties::maskToolName and op names from
    SubmaskList::opName, so this dialog cannot drift from the list or the canvas chip.
*/
class SubmaskDialog : public QDialog
{
    Q_OBJECT
public:
    /* Pop the dialog and return true if the user chose. op/tool are written only then;
       op comes back Add whenever firstMask is true. */
    static bool choose(QWidget *parent, bool firstMask, int &op, int &tool);

    explicit SubmaskDialog(bool firstMask, QWidget *parent = nullptr);

    int chosenOp() const;
    int chosenTool() const;                 // -1 until a type row is selected

private:
    void buildUi();
    void addToolGroup(const QString &heading, int firstTool, int lastTool);
    void syncOkEnabled();

    bool          firstMask = true;         // empty mask: only Add is possible
    QRadioButton *addBtn       = nullptr;
    QRadioButton *subtractBtn  = nullptr;
    QRadioButton *intersectBtn = nullptr;
    QLabel       *opNote       = nullptr;   // why the other two are off (first submask)
    QListWidget  *toolList     = nullptr;
    QPushButton  *okBtn        = nullptr;
};

#endif // SUBMASKDIALOG_H
