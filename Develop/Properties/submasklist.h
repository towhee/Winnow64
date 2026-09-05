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
    SubmaskList -- the collapsible "Submasks" section inside the MaskPanel. It lists the
    active mask's submasks IN FOLD ORDER and is the one place a committed submask can be
    reached again:

        | v Submasks                          [+] |   <- [+] appends (same as M)
        |   [x] (+) Linear Gradient           [:] |
        |   [x] (-) Brush                     [:] |
        |   [x] (n) Luminance Range      *    [:] |   <- selected: editing it
        |   [ ] (+) Sky            (off)      [:] |

    The op glyph is a BUTTON: clicking it cycles Add -> Subtract -> Intersect, because
    the op used to be settable only by holding a modifier at commit time and was then
    frozen forever. The eye shows/hides that submask's contribution, and the [:] menu
    carries the rest (edit, op, invert, move, duplicate, delete). The band repeats the
    same trailing pair -- eye (all submasks) then menu -- with the menu last, matching the
    scope rows and the section headers.

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

    /* Collapsed hides the rows AND (via collapsedChanged, in MaskPanel) the selected
       submask's settings below them: the section is one thing to the user. The list
       starts collapsed; MaskPanel::beginPending re-opens it when a submask is added. */
    bool isCollapsed() const { return collapsed; }
    void setCollapsed(bool collapse);

    /* Display name of a MaskOp, shared with the panel's commit button wording. */
    static QString opName(int op);

signals:
    void addRequested();                          // [+] / "Add submask"
    void submaskSelected(int index);              // row click: re-open it for editing
    void enabledToggled(int index, bool on);      // row eye (or the band eye, per row)
    void opChanged(int index, int op);            // op glyph / menu
    void invertRequested(int index);
    void deleteRequested(int index);
    void moveRequested(int from, int to);
    void duplicateRequested(int index);
    void collapsedChanged(bool collapsed);        // hide/show the settings below too
    void helpRequested();                         // menu: "Submasks help"

protected:
    void paintEvent(QPaintEvent *) override;      // gradient behind the header band
    bool eventFilter(QObject *watched, QEvent *event) override;   // header + row clicks

private:
    void buildUi();
    void rebuild();
    QWidget *makeRow(int index, const SubmaskRowInfo &r, bool selected);
    void showRowMenu(int index);
    void showListMenu();                  // band [:]: add / show all / hide all
    void toggleAllEnabled();              // band eye: hide everything, or bring it back
    void updateBandEyeIcon();             // band eye follows "any submask still shown"
    static void setEyeIcon(BarBtn *b, bool shown);      // eye.png / eye_off.png
    void toggleCollapsed();
    void updateCollapseIcon();
    /* Fire on the next tick: the handler rebuilds these rows (see the class comment). */
    void emitDeferred(std::function<void()> fn);
    static QString opGlyph(int op);

    QWidget     *headerBand    = nullptr;
    BarBtn      *collapseBtn   = nullptr;
    QLabel      *titleLabel    = nullptr;
    BarBtn      *addBtn        = nullptr;
    BarBtn      *eyeBtn        = nullptr;   // band eye: show/hide every submask
    BarBtn      *menuBtn       = nullptr;   // band [:]: list actions
    QWidget     *rowsContainer = nullptr;
    QVBoxLayout *rowsLayout    = nullptr;

    QVector<SubmaskRowInfo> infos;
    int  selectedIndex = -1;
    /* Collapsed by default: a mask usually has one or two submasks, so the list is
       noise until the user goes looking for it. beginPending() re-opens the section
       whenever a submask is actually being built. */
    bool collapsed     = true;
};

#endif // SUBMASKLIST_H
