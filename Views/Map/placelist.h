#ifndef PLACELIST_H
#define PLACELIST_H

#include <QHash>
#include <QTreeWidget>

/*
    THE PLACES PANEL'S LIST: one row per place (Main/mwplaces.cpp) -- its name, and how
    many of the loaded images were taken inside it.

    A SECOND VIEW OF THE FILTERS "Places" CATEGORY, as the Collections panel is of
    Collections. The SELECTION is the filter: a click asks for that place alone, Cmd+click
    adds or removes one, Shift+click takes a range, and a click on empty space asks for
    none (filterRequested). MW applies it and pushes the category's checks back
    (syncFromFilter), with this list's signals blocked, so a check made in the Filters
    panel shows here too.

    The context menu is contextMenuEvent, NOT Qt::ActionsContextMenu: with that policy
    the event is never delivered.
*/
class PlaceList : public QTreeWidget
{
    Q_OBJECT
public:
    struct Row { qint64 id; QString name; };

    explicit PlaceList(QWidget *parent = nullptr);
    void reload(const QList<Row> &rows);
    void setCounts(const QHash<qint64, int> &counts);
    void syncFromFilter(const QList<qint64> &ids);
    QList<qint64> selectedIds() const;
    bool isEmpty() const { return topLevelItemCount() == 0; }

signals:
    void filterRequested(const QVector<qint64> &ids);
    void renameRequested(qint64 id);
    void editRequested(qint64 id);
    void zoomRequested(qint64 id);
    void deleteRequested(qint64 id);
    void newRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *e) override;

private:
    static qint64 idOf(const QTreeWidgetItem *item);
    QTreeWidgetItem *itemFor(qint64 id) const;
};

#endif // PLACELIST_H
