#ifndef SCOPEHEADER_H
#define SCOPEHEADER_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;
class QMenu;
class QVBoxLayout;
class QComboBox;
class BarBtn;

/*
    ScopeHeader -- the Develop dock's scope control: ONE gradient-headed line naming the
    scope everything below it edits.

        | Edits  [Global v] [+] [eye] [:] |   <- the whole scope control
        |   MaskPanel                     |   <- detail, slot MaskDetail
        |   Basic / Color / ...           |   <- detail, slot EditsDetail (the tree)

    The combo carries every scope on the image (index 0 is always Global); [+] adds a
    mask; the trailing [eye] [:] pair -- eye then menu, the order every band and row in
    this dock uses -- acts on whichever scope the combo has selected. There is NO scope
    list and NO containment rail: the bar and the editor below it are adjacent and alone,
    so there is nothing for a rail to bracket and no list of candidates to pick the
    editor's owner out of.

    The detail widgets (setRowDetail), drawn top to bottom in slot order: the MaskPanel,
    then the adjustment tree (DevelopProperties). They belong to the CALLER -- this widget
    only positions them -- and they are NOT indented, so the tree's left edge (and any
    rule it paints) coincides with this widget's. There is only ever ONE editor:
    DevelopProperties::buildTree repopulates it on every scope switch.

    Interaction (emissions that loop back into setScopeRows are DEFERRED a tick, so a
    widget is never deleted inside its own signal handler):
      - Combo pick   -> scopeSelected(name)          (makes it the active scope)
      - [+]          -> addScopeRequested            (new mask)
      - [eye]        -> scopeEnabledToggled(i, on)   (EditScope::enabled)
      - [:]          -> the active scope's actions, plus the panel-wide "Reset all edits"
                        and "Edits help" (this bar's menu is the only menu in the panel)

    The whole-mask preview eye, the collapse arrow and the mask-overlay menu rows of the
    original dropdown header are gone: show/hide is the bar's eye, and the mask-overlay
    controls live in the Mask panel. Their setters are kept as inert state-holders so
    DevelopProperties' existing calls still compile and behave.
*/
class ScopeHeader : public QWidget
{
    Q_OBJECT
public:
    explicit ScopeHeader(QWidget *parent = nullptr);

    /* One scope's display state. isGlobal marks index 0, which applies to the whole
       image and so cannot be renamed or deleted. */
    struct ScopeRowInfo {
        QString name;
        bool    enabled  = true;    // EditScope::enabled -> the bar's eye
        bool    isGlobal = false;   // index 0: applies globally
    };

    /* Refill the combo and select currentIndex WITHOUT emitting scopeSelected. */
    void setScopes(const QStringList &names, int currentIndex);
    /* Primary refresh: names + per-scope enabled + which is Global. */
    void setScopeRows(const QVector<ScopeRowInfo> &rows, int active);

    /* The editor widgets, drawn top to bottom in slot order: MaskDetail is the MaskPanel,
       EditsDetail the adjustment tree. Each is owned by the CALLER and survives every
       refresh. Pass nullptr to detach a slot. */
    enum RowDetailSlot { MaskDetail = 0, EditsDetail = 1, RowDetailSlotCount };
    void setRowDetail(QWidget *detail, int slot);

    /* Inert state-holders: this header has no preview eye, no per-scope group and no
       mask-overlay menu (see the class comment), but DevelopProperties still pushes the
       state and reads it back. */
    void setPreviewShown(bool shown) { previewShown = shown; }
    void setGlobalActive(bool isGlobal) { globalActive = isGlobal; }
    void setMaskOverlayAvailable(bool available) { maskOverlayAvailable = available; }
    void setMaskOverlayShown(bool shown) { maskOverlayShown = shown; }
    void setCollapsed(bool collapsed);      // state only -- the bar never hides
    bool isCollapsed() const { return collapsed; }
    QString currentScopeName() const;

signals:
    void scopeSelected(const QString &name);    // user picked a different scope
    void renameRequested();                      // menu: rename the selected scope
    void resetScopeRequested();                  // menu: reset the scope to identity
    void removeScopeRequested();                 // menu: remove the selected scope
    void addScopeRequested();                    // [+] / menu: add a new scope
    void resetAllEditsRequested();               // menu: wipe every edit on this image
    void addMaskRequested();                     // menu: add a mask tool to this scope
    void maskOverlayToggled();                   // show/hide the mask overlay
    void previewToggled(bool shown);             // show/ignore the whole scope
    void collapseToggled(bool collapsed);        // hide/show the scope's tree items
    /* A scope's show/hide eye was toggled (index into the scope stack, 0 = Global). */
    void scopeEnabledToggled(int index, bool on);
    /* Menu: "Edits help" -- the owner opens Docs/developeditshelp.html (this header knows
       nothing about Docs or HtmlWindow). */
    void helpRequested();

protected:
    void paintEvent(QPaintEvent *) override;         // gradient behind the scope bar

private:
    void buildScopeBar(QVBoxLayout *outer);   // the one-line scope selector
    void updateScopeBar();                    // refill the combo + eye (emits nothing)
    void showScopeMenu();                     // the bar's [:]
    void addHelpAction(QMenu *menu);          // trailing "Edits help"
    void toggleActiveEnabled();               // the bar's eye: flip EditScope::enabled
    static void setEyeIcon(BarBtn *b, bool shown);      // eye.png / eye_off.png
    void selectScopeDeferred(const QString &name);      // emit scopeSelected next tick

    /* The whole scope control on one line -- "Edits [Global v] [+] [eye] [:]". It stands
       in for a panel header band, so paintEvent draws the property-header gradient
       behind it. */
    QWidget     *scopeBar        = nullptr;
    QComboBox   *scopeCombo      = nullptr;
    BarBtn      *barAddBtn       = nullptr;   // [+] new mask
    BarBtn      *barEyeBtn       = nullptr;
    BarBtn      *barMenuBtn      = nullptr;

    /* Host for the detail widgets. Translucent because under the app stylesheet a plain
       QWidget fills its background opaquely, painting over the dock background. */
    QWidget     *rowsContainer = nullptr;
    QVBoxLayout *rowsLayout    = nullptr;
    /* The detail widgets (setRowDetail) and the wrappers they sit in. The wrappers are
       ours; the details inside them are not, so neither is deleted here. */
    QWidget     *rowDetail[RowDetailSlotCount]  = {};
    QWidget     *detailWrap[RowDetailSlotCount] = {};

    QStringList names;                     // current scope names (for currentScopeName)
    /* Per-scope enabled state from the last setScopeRows, so the eye can report and flip
       the ACTIVE scope without asking the owner. */
    QVector<bool> enabledStates;
    int  activeIndex  = 0;
    bool previewShown = true;
    bool collapsed    = false;             // setCollapsed state (inert)
    bool globalActive = true;
    bool maskOverlayAvailable = false;
    bool maskOverlayShown     = true;
};

#endif // SCOPEHEADER_H
