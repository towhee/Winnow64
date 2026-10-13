#ifndef PRESETSVIEW_H
#define PRESETSVIEW_H

#include <QListWidget>
#include <QStyledItemDelegate>
#include <QString>

class QTimer;
class DevelopPresets;

/*
    PresetDelegate -- paints the three kinds of row: a category HEADER (disclosure arrow,
    bold name, preset count, a band fill), a PRESET (name indented under its header,
    elided if the dock is narrow) and the empty-list PLACEHOLDER (disabled colour, never
    highlights). The HOVERED preset gets a light fill because hovering is live (the loupe
    is showing that preset applied); a header that is the target of a drag is outlined.
*/
class PresetDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit PresetDelegate(QObject *parent = nullptr);

    /* Roles carried by each QListWidgetItem (DisplayRole holds the text). */
    enum Roles {
        NameRole = Qt::UserRole + 1,    // QString: preset name (Preset rows only)
        KindRole,                       // int: RowKind
        CategoryRole,                   // QString: the category the row belongs to
        CollapsedRole,                  // bool: header is folded (Header rows only)
        CountRole                       // int: presets in the category (Header rows)
    };
    enum RowKind { Header, Preset, Placeholder };

    void setHoveredRow(int row);
    void setDropHeaderRow(int row);     // header outlined as a drop target, -1 = none

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    int hoveredRow = -1;
    int dropHeaderRow = -1;
};

/*
    PresetsView -- the Presets dock's list of user develop presets (Lightroom's Presets
    panel). Deliberately the same interaction as the History dock next to it: hovering a
    preset previews it on the loupe, clicking it applies it.

    Presets are grouped under header CATEGORIES (DevelopPresets::layout); "Winnow" is
    permanent. Clicking a header folds it. DRAG reorders: a preset row can be dropped
    between presets of any category or onto a header (top of that category), and a
    header can be dropped between categories. The drag is done by hand rather than with
    QDrag / InternalMove because the rows are not independent -- a header carries its
    presets with it, and every drop must land in a category -- so the view computes a
    (category, index) target itself, paints a drop line, and hands the move to the
    store. Category management (new / rename / delete / move / fold) is pure
    organisation with no develop state involved, so the view performs it on the store
    directly; the preset actions below still go to DevelopProperties.

    The list is a dumb view over DevelopPresets: it rebuilds whole on the store's
    changed() and reports back to DevelopProperties:

      presetHovered(name)  hover settled on a row  -> preview that preset in the loupe
      hoverEnded()         hover left a preset row -> restore the real state
      presetChosen(name)   a row was clicked       -> apply the preset to the image

    plus the management requests raised from the context menu (the view owns the rename /
    delete prompts, so the caller only ever receives a decided action).

    Hover is DEBOUNCED (kHoverDelayMs) so sweeping the cursor down the list does not queue
    a render per row, and the click is emitted a tick late because applying a preset
    rebuilds the Develop property tree from inside the row's own event handler.
*/
class PresetsView : public QListWidget
{
    Q_OBJECT
public:
    explicit PresetsView(QWidget *parent = nullptr);

    /* Bind the store once (owned by DevelopProperties); refreshes on its changed(). */
    void bindPresets(DevelopPresets *p);
    void refresh();

    /* The Presets section band's [:] menu: the SAME menu the list's right-click raises,
       acting on the selected preset (only "New Preset..." when nothing is selected), plus
       a trailing help item. One set of preset actions, not two that can drift. */
    void showSectionMenu(const QPoint &globalPos);

signals:
    void presetHovered(const QString &name);
    void hoverEnded();
    void presetChosen(const QString &name);
    /* category = where the new preset should go by default; empty = no preference
       (a header's menu passes its own category). */
    void newPresetRequested(const QString &category);
    void updateRequested(const QString &name);
    void renameRequested(const QString &from, const QString &to);
    void deleteRequested(const QString &name);
    void helpRequested();           // the section menu's trailing help item

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;   // + the drop line during a drag

private:
    static constexpr int kHoverDelayMs = 120;

    /* Where a drag would land. For a preset drag: (category, index) within it. For a
       header drag: catIndex = the gap among the categories. headerRow >= 0 when the
       target is "into this header" (outlined) rather than a line at lineY. */
    struct DropTarget {
        bool    valid = false;
        QString category;
        int     index = 0;
        int     catIndex = 0;
        int     headerRow = -1;
        int     lineY = 0;
    };

    int     kindForRow(int row) const;      // -1 when there is no such row
    QString nameForRow(int row) const;      // empty unless the row is a preset
    QString categoryForRow(int row) const;
    int     lastVisibleRowOf(const QString &category) const;
    int     positionInCategory(int row) const;  // preset rows above it in its category
    void    toggleCollapsed(int headerRow);
    DropTarget dropTargetAt(const QPoint &pos) const;
    void    performDrop(const DropTarget &t);
    void    endDrag();
    void setHover(int row);                 // ends the preview off a preset row
    void endHoverPreview();                 // emits hoverEnded() if a preview is live
    void showMenu(const QPoint &pos);
    /* The one menu both entry points build, for the row it acts on (-1 = none);
       includeHelp appends the trailing help item (the section band only). */
    void runMenu(const QPoint &globalPos, int row, bool includeHelp);
    QString askCategoryName(const QString &title, const QString &current);

    DevelopPresets *presets = nullptr;
    PresetDelegate *delegate = nullptr;
    QTimer *hoverTimer = nullptr;
    int hoverRow = -1;              // row the cursor is over (-1 = none)
    bool hoverEmitted = false;      // a preview is live, so a leave must restore

    int pressRow = -1;              // row under a left press (drag source candidate)
    QPoint pressPos;
    bool dragging = false;          // past the drag threshold
    DropTarget drop;                // current target while dragging
};

#endif // PRESETSVIEW_H
