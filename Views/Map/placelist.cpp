#include "Views/Map/placelist.h"

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMenu>
#include "Main/global.h"

PlaceList::PlaceList(QWidget *parent) : QTreeWidget(parent)
{
    setObjectName("placeList");
    setColumnCount(2);
    setHeaderLabels({tr("Place"), tr("Images")});
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    headerItem()->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    setRootIsDecorated(false);
    setUniformRowHeights(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSortingEnabled(false);
    setToolTip(tr("Click a place to show only the images taken inside it.\n"
                  "Cmd+click to add or remove a place, Shift+click for a range.\n"
                  "Click empty space to stop filtering by place.\n"
                  "Right-click to rename, reshape or delete."));

    connect(this, &QTreeWidget::itemSelectionChanged, this, [this] {
        QVector<qint64> ids;
        for (qint64 id : selectedIds()) ids << id;
        emit filterRequested(ids);
    });
    connect(this, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *item, int) { emit zoomRequested(idOf(item)); });
}

qint64 PlaceList::idOf(const QTreeWidgetItem *item)
{
    return item ? item->data(0, Qt::UserRole).toLongLong() : 0;
}

QTreeWidgetItem *PlaceList::itemFor(qint64 id) const
{
    for (int i = 0; i < topLevelItemCount(); ++i)
        if (idOf(topLevelItem(i)) == id) return topLevelItem(i);
    return nullptr;
}

void PlaceList::reload(const QList<Row> &rows)
{
/*
    Rebuild from the store, keeping the selection and the counts by id. Signals are
    blocked: a rebuild is not a click, and the Filters category already says what is
    checked.
*/
    const QList<qint64> keep = selectedIds();
    QHash<qint64, QVariant> counts;
    for (int i = 0; i < topLevelItemCount(); ++i)
        counts.insert(idOf(topLevelItem(i)), topLevelItem(i)->data(1, Qt::DisplayRole));
    QSignalBlocker block(this);
    clear();
    for (const Row &r : rows) {
        auto *item = new QTreeWidgetItem(this);
        item->setText(0, r.name);
        item->setData(0, Qt::UserRole, r.id);
        item->setData(1, Qt::DisplayRole, counts.value(r.id, 0));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setToolTip(0, tr("Click to show only the images taken in \"%1\".\n"
                               "Cmd+click to add it to the places showing.\n"
                               "Double-click to zoom the map to it.").arg(r.name));
        if (keep.contains(r.id)) item->setSelected(true);
    }
}

void PlaceList::setCounts(const QHash<qint64, int> &counts)
{
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = topLevelItem(i);
        item->setData(1, Qt::DisplayRole, counts.value(idOf(item), 0));
    }
}

void PlaceList::syncFromFilter(const QList<qint64> &ids)
{
    QSignalBlocker block(this);
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = topLevelItem(i);
        const bool want = ids.contains(idOf(item));
        if (item->isSelected() != want) item->setSelected(want);
    }
}

QList<qint64> PlaceList::selectedIds() const
{
    QList<qint64> ids;
    for (int i = 0; i < topLevelItemCount(); ++i)
        if (topLevelItem(i)->isSelected()) ids << idOf(topLevelItem(i));
    return ids;
}

void PlaceList::contextMenuEvent(QContextMenuEvent *e)
{
    QTreeWidgetItem *item = itemAt(viewport()->mapFrom(this, e->pos()));
    QMenu menu(this);
    QAction *add = menu.addAction(tr("New Place..."));
    connect(add, &QAction::triggered, this, &PlaceList::newRequested);
    if (item) {
        const qint64 id = idOf(item);
        const QString name = item->text(0);
        menu.addSeparator();
        connect(menu.addAction(tr("Zoom Map to \"%1\"").arg(name)), &QAction::triggered,
                this, [this, id] { emit zoomRequested(id); });
        connect(menu.addAction(tr("Rename...")), &QAction::triggered,
                this, [this, id] { emit renameRequested(id); });
        connect(menu.addAction(tr("Edit Shape...")), &QAction::triggered,
                this, [this, id] { emit editRequested(id); });
        menu.addSeparator();
        connect(menu.addAction(tr("Delete \"%1\"...").arg(name)), &QAction::triggered,
                this, [this, id] { emit deleteRequested(id); });
    }
    menu.exec(e->globalPos());
}
