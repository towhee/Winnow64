#ifndef HISTORYVIEW_H
#define HISTORYVIEW_H

#include <QListWidget>
#include <QStyledItemDelegate>
#include <QString>

class QTimer;
class DevelopHistory;

/*
    HistoryDelegate -- paints one history row as two columns:

        Global: Exposure                 +0.35
        Subject Mask: Hue                  -18

    The action reads left (elided if the dock is narrow), the value right-aligned in the
    header colour. Three row states are painted: the CURRENT position (the state the image
    is actually in) gets the selection fill, rows AFTER the current position are dimmed
    (they only exist between a revert and the next edit, which discards them), and the
    HOVERED row gets a light fill because hovering previews that state in the loupe.
*/
class HistoryDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit HistoryDelegate(QObject *parent = nullptr);

    /* Roles carried by each QListWidgetItem (DisplayRole holds the action text). */
    enum Roles {
        ValueRole   = Qt::UserRole + 1,   // QString: right-hand value, may be empty
        StateRole   = Qt::UserRole + 2,   // int RowState below
        EntryRole   = Qt::UserRole + 3    // int: DevelopHistory index (chronological)
    };
    enum RowState { Past = 0, Current = 1, Future = 2 };

    void setHoveredRow(int row);

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    int hoveredRow = -1;
};

/*
    HistoryView -- the History dock's list of develop actions (Lightroom's History panel).

    Rows are NEWEST FIRST, so the baseline ("Original" / "Saved settings") sits at the
    bottom. The list is a dumb view over DevelopHistory: it rebuilds whole on any change
    (the lists are short and this is what every sibling Develop panel does) and reports
    three things back to DevelopProperties:

      entryHovered(i)  hover settled on a row  -> preview that state in the loupe
      hoverEnded()     cursor left the list    -> restore the real state
      entryChosen(i)   a row was clicked       -> revert the image to that state

    Hover is DEBOUNCED (kHoverDelayMs) so sweeping the cursor down the list does not queue
    a render per row, and the click is emitted a tick late because applying an entry
    rebuilds the property tree from inside the row's own event handler.

    A plain QListWidget is used rather than the hand-built row widgets of the sibling
    panels: this list scrolls, hovers and selects, all of which come free here, and
    WidgetCSS::listWidget() already themes QListWidget app-wide.
*/
class HistoryView : public QListWidget
{
    Q_OBJECT
public:
    explicit HistoryView(QWidget *parent = nullptr);

    /* Bind the model once (owned by DevelopProperties); refreshes on its changed(). */
    void bindHistory(DevelopHistory *h);
    /* Show this image's history (empty path = nothing to show). */
    void setImage(const QString &path);
    void refresh();

signals:
    void entryHovered(int index);   // chronological index into DevelopHistory
    void hoverEnded();
    void entryChosen(int index);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    static constexpr int kHoverDelayMs = 120;

    int entryForRow(int row) const;     // -1 when the row is invalid
    void setHover(int row);

    DevelopHistory *history = nullptr;
    HistoryDelegate *delegate = nullptr;
    QTimer *hoverTimer = nullptr;
    QString imagePath;
    int hoverRow = -1;              // row the cursor is over (-1 = none)
    bool hoverEmitted = false;      // a preview is live, so a leave must restore
};

#endif // HISTORYVIEW_H
