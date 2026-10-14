#ifndef SUBMASKLIST_H
#define SUBMASKLIST_H

#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

class QLabel;
class QVBoxLayout;
class BarBtn;

/*
    One row of the SubmaskList: everything the list needs to draw a submask without
    reaching into the edit stack. Mirrors ScopeHeader::ScopeRowInfo.
*/
struct SubmaskRowInfo
{
    QString toolName;                 // "Linear Gradient", "Brush", ...
    int     op       = 0;             // MaskOp: 0 Add, 1 Subtract, 2 Intersect
    bool    enabled  = true;          // MaskComponent::enabled (show/hide)
    bool    inverted = false;         // MaskComponent::inverted (row tooltip only)
    bool    pending  = false;         // still being built (not yet committed)
};

/*
    SubmaskList -- the submask rows inside the MaskPanel. It lists the active mask's
    submasks IN FOLD ORDER and is the one place a committed submask can be reached again:

        | v Mask                     [+] [■] [:] |   <- MaskPanel's band: [+] appends (M)
        |     Edge / Halo                         |
        |   > Linear Gradient Submask (+)  [eye][:] |
        |   > Brush Submask (-)            [eye][:] |
        |   v Luminance Range Submask (n)  [eye][:] |   <- selected: editing it

    It has NO header of its own (2026-10-14): the "Submasks" band was folded into the
    Mask band, which carries its [+] and whose [:] carries Show / Hide all submasks. The
    rows' arrows sit where that band's arrow sat, in the Mask band's arrow column.

    The op glyph, after the name, is a BUTTON: clicking it cycles Add -> Subtract -> Intersect, because
    the op used to be settable only by holding a modifier at commit time and was then
    frozen forever. The eye shows/hides that submask's contribution, and the [:] menu
    carries the rest (edit, op, invert, move, duplicate, delete).

    Carries NO model state beyond what setSubmasks() was handed: every control emits an
    index-based signal and DevelopProperties pushes the new list straight back. Like
    ScopeHeader, every emission is DEFERRED a tick -- the handler loops back through
    setSubmasks(), which deletes the very row widget the click is being handled in.
*/
class SubmaskList : public QWidget
{
    Q_OBJECT
public:
    explicit SubmaskList(QWidget *parent = nullptr);

    /* Rebuild the rows. selected == -1 means no submask is open for editing. */
    void setSubmasks(const QVector<SubmaskRowInfo> &rows, int selected);
    int  count() const { return infos.size(); }
    /* Half the height a submask row lacks against a tree row (G::propertyRowHeight): the
       rows are compact, so a caption sits closer to its row's edges than a tree caption
       does. Added above the first row, and by MaskPanel below the last, so the gaps
       around the list read at the tree's divider spacing. */
    int  rowPad() const { return pad; }

    /* True while ANY submask still contributes (MaskComponent::enabled). */
    bool anyEnabled() const;
    /* Show or hide every submask's contribution: the Mask band menu's Show / Hide all
       submasks. Emitted per row as enabledToggled, deferred like every emission here. */
    void setAllEnabled(bool on);

    /* Display name of a MaskOp, shared with the on-canvas op chip's wording. */
    static QString opName(int op);
    /* The one character a row's op chip shows (+ / - / n). Public because SubmaskDialog
       labels its op choices with it: the glyph picked there is the glyph the submask's
       row wears afterwards, so the two cannot drift. */
    static QString opGlyph(int op);

    /* Control-group separator above the rows (G::groupSeparator*), set by MaskPanel
       while the Edge/Halo rows sit above them. Without them the rows follow the Mask
       band directly: no separator. */
    void setTopRule(bool on);

signals:
    void submaskSelected(int index);              // row click: re-open it for editing
    void enabledToggled(int index, bool on);      // row eye (or setAllEnabled, per row)
    void opChanged(int index, int op);            // op glyph / menu
    void invertRequested(int index);
    void deleteRequested(int index);
    void moveRequested(int from, int to);
    void duplicateRequested(int index);

protected:
    void paintEvent(QPaintEvent *) override;      // surface + the top rule
    bool eventFilter(QObject *watched, QEvent *event) override;   // row clicks

private:
    void buildUi();
    void rebuild();
    QWidget *makeRow(int index, const SubmaskRowInfo &r, bool selected);
    void showRowMenu(int index);
    static void setEyeIcon(BarBtn *b, bool shown);      // eye.png / eye_off.png
    /* Fire on the next tick: the handler rebuilds these rows (see the class comment). */
    void emitDeferred(std::function<void()> fn);

    bool         topRule       = false;
    QWidget     *rowsContainer = nullptr;
    QVBoxLayout *rowsLayout    = nullptr;

    QVector<SubmaskRowInfo> infos;
    int  selectedIndex = -1;
    int  pad           = 0;               // rowPad()
};

#endif // SUBMASKLIST_H
