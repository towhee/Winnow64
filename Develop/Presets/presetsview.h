#ifndef PRESETSVIEW_H
#define PRESETSVIEW_H

#include <QListWidget>
#include <QStyledItemDelegate>
#include <QString>

class QTimer;
class DevelopPresets;

/*
    PresetDelegate -- paints one preset row: the name left, elided if the dock is narrow.
    Two row states are painted: the HOVERED row gets a light fill because hovering is
    live (the loupe is showing that preset applied), and the empty-list PLACEHOLDER row
    is drawn in the disabled colour and never highlights.
*/
class PresetDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit PresetDelegate(QObject *parent = nullptr);

    /* Roles carried by each QListWidgetItem (DisplayRole holds the text). */
    enum Roles {
        NameRole = Qt::UserRole + 1     // QString: the preset name; empty = placeholder
    };

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
    PresetsView -- the Presets dock's list of user develop presets (Lightroom's Presets
    panel). Deliberately the same interaction as the History dock next to it: hovering a
    preset previews it on the loupe, clicking it applies it.

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

signals:
    void presetHovered(const QString &name);
    void hoverEnded();
    void presetChosen(const QString &name);
    void newPresetRequested();
    void updateRequested(const QString &name);
    void renameRequested(const QString &from, const QString &to);
    void deleteRequested(const QString &name);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    static constexpr int kHoverDelayMs = 120;

    QString nameForRow(int row) const;      // empty when the row is the placeholder
    void setHover(int row);                 // ends the preview off a preset row
    void endHoverPreview();                 // emits hoverEnded() if a preview is live
    void showMenu(const QPoint &pos);

    DevelopPresets *presets = nullptr;
    PresetDelegate *delegate = nullptr;
    QTimer *hoverTimer = nullptr;
    int hoverRow = -1;              // row the cursor is over (-1 = none)
    bool hoverEmitted = false;      // a preview is live, so a leave must restore
};

#endif // PRESETSVIEW_H
