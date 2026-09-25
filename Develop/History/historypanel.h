#ifndef HISTORYPANEL_H
#define HISTORYPANEL_H

#include <QList>
#include <QString>
#include <QWidget>

class QContextMenuEvent;
class QLabel;
class QListWidget;
class QSettings;
class QSplitter;
class QHBoxLayout;
class BarBtn;
class HistoryView;
class PresetsView;

/*
    PanelSectionHeader -- one gradient band naming a section of a panel, with a leading
    arrow that folds the section's body away and a trailing strip of BarBtns.

        | > History                              [:] |
        | > Presets                          [+] [:] |

    The band is THE SAME BAND as the Develop editor's Basic / Color / Effects headers: the
    gradient is the a -> b ramp off G::backgroundShade that PropertyDelegate::paint uses
    for a header row, and the caption carries G::header2Color at the app's string font
    size. A section header painting its own band is the idiom every Develop panel already
    follows (RawPanel, MaskPanel, SubMaskList, ScopeHeader) -- Utilities/gradientheader.h
    is the moc-free version for a band that is nothing but a title, which this is not.

    The header NEVER hides: collapsing a section hides the body below it, so the two bands
    always sit in the panel and the user can always get the section back.
*/
class PanelSectionHeader : public QWidget
{
    Q_OBJECT
public:
    explicit PanelSectionHeader(const QString &title, QWidget *parent = nullptr);

    /* Append a button to the trailing strip, in the order every band in the app uses:
       the actions first, the [:] menu last. */
    void addButton(BarBtn *btn);

    /* Silent: state only, no toggled() -- that signal is a USER click. */
    void setExpanded(bool expanded);
    bool isExpanded() const { return expanded; }

signals:
    void toggled(bool expanded);        // the arrow (or the caption) was clicked

protected:
    void paintEvent(QPaintEvent *) override;                 // the gradient band
    bool eventFilter(QObject *obj, QEvent *event) override;  // caption click -> toggle

private:
    void updateArrowIcon();

    BarBtn      *arrowBtn = nullptr;
    QLabel      *label    = nullptr;
    QHBoxLayout *hb       = nullptr;
    bool         expanded = true;
};

/*
    HistoryPanel -- the body of the History dock: the develop HISTORY list and the develop
    PRESETS list, each under its own collapsible section header.

        History                        X    <- the dock's own DockTitleBar
        > History                     [:]   <- this widget
        >   (HistoryView)
        ================================== <- splitter handle: drag to resize
        > Presets                 [+] [:]
        >   (PresetsView)

    The two lists were separate docks until they were folded in here: three tabs for one
    tool was one tab too many, and History and Presets are always read together. Each list
    is still a dumb view over a model DevelopProperties owns, so this widget CREATES the
    two views and hands them out (historyView() / presetsView()) for MW to bind -- it
    knows nothing about develop state itself.

    The two sections sit in a vertical QSplitter, so the user sizes them against each
    other; the split persists under Develop/SectionSplit. A folded section's pane is
    capped at its band, so the open one takes the dock.

    Expand state persists per section under Develop/SectionExpanded/<name>, the same key
    shape the Develop tree's Basic / Color / Detail / Effects sections use. This widget is
    the single source of truth for it; presetsDockVisibleAction (the "P" View-menu item)
    mirrors the Presets section rather than owning it.
*/
class HistoryPanel : public QWidget
{
    Q_OBJECT
public:
    explicit HistoryPanel(QWidget *parent = nullptr, QSettings *setting = nullptr);

    HistoryView *historyView() const { return histView; }
    PresetsView *presetsView() const { return presView; }

    bool historyExpanded() const;
    bool presetsExpanded() const;
    /* SOLO MODE (Lightroom's): at most one section open, so opening one folds the other.
       Persisted, and offered in the panel's context menu. */
    bool soloMode() const { return solo; }
    void setSoloMode(bool on);
    /* Open the section (and make sure it is on screen). The H and P keys land here. */
    void expandHistory();
    void expandPresets();

protected:
    /* Expand all / Collapse all / Solo mode, from anywhere in the panel. The bands and the
       History list do not consume the right-click, so it arrives here; the Presets list
       keeps its own preset menu. */
    void contextMenuEvent(QContextMenuEvent *event) override;

signals:
    void historyExpandedChanged(bool expanded);
    void presetsExpandedChanged(bool expanded);
    /* The Presets band's [+]: make a preset from the current image (MW::developSavePreset). */
    void newPresetRequested();

private:
    /* The two ways a section changes. Each persists, emits its own signal, and -- in solo
       mode -- folds the OTHER section (emitting for it too). They never call each other,
       so there is no recursion to guard. */
    void setHistorySection(bool expanded);
    void setPresetsSection(bool expanded);
    void setSectionExpanded(PanelSectionHeader *header, QWidget *body,
                            const QString &key, bool expanded);
    /* Cap a folded section's pane at its band and re-apply the remembered split when
       both are open. Run after every expand / collapse. */
    void applySplit();
    void showHistoryMenu();
    static void showHistoryHelp();
    static void showPresetsHelp();

    QSettings          *setting     = nullptr;
    bool                solo        = false;   // at most one section open
    PanelSectionHeader *histHeader  = nullptr;
    PanelSectionHeader *presHeader  = nullptr;
    HistoryView        *histView    = nullptr;
    PresetsView        *presView    = nullptr;
    QSplitter          *splitter    = nullptr;
    QWidget            *histPane    = nullptr;   // histHeader + histView
    QWidget            *presPane    = nullptr;   // presHeader + presView
    QList<int>          splitSizes  = {3, 2};    // expanded pane heights (proportions)
};

#endif // HISTORYPANEL_H
