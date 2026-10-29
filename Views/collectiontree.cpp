#include "Views/collectiontree.h"
#include "File/hoverdelegate.h"
#include "Main/global.h"
#include "Utilities/queryexpr.h"
#include "Utilities/fileops.h"
#ifdef Q_OS_MAC
#include "Utilities/mac.h"
#endif

#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyleFactory>

#include <functional>

namespace {

/*  The node being dragged inside the tree. Its own format, so no other widget in Winnow
    (FSTree, Bookmarks, the Finder) recognises it as something to act on. */
const char *kNodeMime = "application/x-winnow-collection-node";

/*  Collections are violet, so they read as neither a folder on disk (blue on macOS,
    yellow on Windows) nor the Library row's teal. Queries are amber, and draw their own
    glyph (a magnifier over a list), so a query never reads as a collection -- one holds
    images, the other describes them. */
const QColor kCollectionColor(160, 130, 210);
const QColor kQueryColor(222, 150, 70);
/*  The target collection's glyph: a saturated magenta, kin to the violet but plainly
    not it, so the target is found at a glance (a lighter violet was too subtle). */
const QColor kTargetCollectionColor(240, 80, 200);

QIcon tinted(const QString &resource, const QColor &c)
{
    QPixmap pm(resource);
    if (pm.isNull()) return QIcon();
    QPixmap out(pm.size());
    out.setDevicePixelRatio(pm.devicePixelRatio());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawPixmap(0, 0, pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), c);
    p.end();
    return QIcon(out);
}

/*  See the class comment: a thumbnail drop is ALWAYS a copy, on every handler, or the
    drag source deletes the files. */
void acceptWithoutMoving(QDropEvent *event)
{
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

/*  The images in a URL drag: its sidecars (G::includeSidecars) are not, and the drop
    skips them (MW::addPathsToCollection), so the drag cursor's count badge must too. */
void showImageCountBadge(const QMimeData *m)
{
#ifdef Q_OS_MAC
    const QStringList &sidecar = FileOps::sidecarSuffixes();
    int images = 0;
    for (const QUrl &u : m->urls())
        if (u.isLocalFile()
            && !sidecar.contains(QFileInfo(u.toLocalFile()).suffix(), Qt::CaseInsensitive))
            ++images;
    Mac::setDragBadgeCount(images);
#else
    Q_UNUSED(m)                     // Windows drags carry no count badge
#endif
}

}  // namespace

CollectionTree::CollectionTree(CollectionStore::Kind kind, const QString &countMetric,
                               int countMargin, QWidget *parent)
    : QTreeWidget(parent), nodeKind(kind), countMetric(countMetric),
      countMargin(countMargin)
{
    if (G::isLogger) G::log("CollectionTree::CollectionTree");
#ifdef Q_OS_MAC
    Mac::installDragBadgeHook();    // once per process; see showImageCountBadge
#endif
    setObjectName(kind == CollectionStore::Kind::Collection ? "collectionTree"
                                                            : "queryTree");
    nodeIcon = kind == CollectionStore::Kind::Query
        ? tinted(":/images/icon16/queries_white.png", kQueryColor)
        : tinted(":/images/icon16/collections_white.png", kCollectionColor);
    if (kind == CollectionStore::Kind::Collection)
        targetIcon = tinted(":/images/icon16/collections_white.png",
                            kTargetCollectionColor);

    setColumnCount(2);
    setHeaderHidden(true);
    setRootIsDecorated(true);
    setUniformRowHeights(true);
    setIndentation(16);                             // FSTree's
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    /*  NO SELECTION BY THE MOUSE, as in LibTree: what is selected is what is OPEN, and
        MW pushes that back (syncFromFilter) from the Filters panel, which holds the
        one collection filter. */
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);     // Rename / F2 call editItem
    /*  Drops are handled here in full (dragEnter/Move/drop); the base class's own
        drag-and-drop machinery is not used, so internal moves cannot go through
        QTreeWidget's item shuffling behind the store's back. */
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDragEnabled(false);
    setDropIndicatorShown(false);
    // DefaultContextMenu: Winnow's docks have used ActionsContextMenu, which bypasses it
    setContextMenuPolicy(Qt::DefaultContextMenu);

#ifdef Q_OS_WIN
    // As FSTree: Fusion keeps the column separator the Windows style drops.
    if (QStyle *fusion = QStyleFactory::create("Fusion")) {
        fusion->setParent(this);
        setStyle(fusion);
    }
#endif

    delegate = new HoverDelegate(this);
    setItemDelegate(delegate);
    setMouseTracking(true);
    connect(delegate, &HoverDelegate::hoverChanged, viewport(),
            QOverload<>::of(&QWidget::update));

    connect(this, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *item) {
        if (reloading) return;
        expanded.insert(idOf(item));
        emit expansionChanged();
    });
    connect(this, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *item) {
        if (reloading) return;
        expanded.remove(idOf(item));
        emit expansionChanged();
    });
    /*  A RENAME is the only edit there is. An empty or refused name puts the old one
        back rather than leaving the row saying something the store does not. */
    connect(this, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int col) {
        if (reloading || col != 0) return;
        const qint64 id = idOf(item);
        const QString want = item->text(0).trimmed();
        if (want.isEmpty() || !CollectionStore::instance().rename(id, want)) {
            reloading = true;
            item->setText(0, CollectionStore::instance().node(id).name);
            reloading = false;
        }
    });

    CollectionStore &store = CollectionStore::instance();
    connect(&store, &CollectionStore::nodesChanged, this,
            [this](CollectionStore::Kind k) { if (k == nodeKind) reload(); });
    connect(&store, &CollectionStore::membersChanged, this, &CollectionTree::refreshCounts);

    updateStyle();
}

QString CollectionTree::kindWord(bool plural) const
{
    if (nodeKind == CollectionStore::Kind::Query) return plural ? tr("queries") : tr("query");
    return plural ? tr("collections") : tr("collection");
}

qint64 CollectionTree::idOf(const QTreeWidgetItem *item)
{
    return item ? item->data(0, IdRole).toLongLong() : 0;
}

QTreeWidgetItem *CollectionTree::itemFor(qint64 id) const
{
    if (id <= 0) return nullptr;
    QTreeWidgetItemIterator it(const_cast<CollectionTree *>(this));
    while (*it) {
        if (idOf(*it) == id) return *it;
        ++it;
    }
    return nullptr;
}

QString CollectionTree::nameOf(qint64 id) const
{
    const QTreeWidgetItem *item = itemFor(id);
    return item ? item->text(0) : QString();
}

QString CollectionTree::pathOf(qint64 id) const
{
    QStringList names;
    for (const QTreeWidgetItem *i = itemFor(id); i; i = i->parent()) names.prepend(i->text(0));
    return names.join(" / ");
}

void CollectionTree::setTargetId(qint64 id)
{
    if (nodeKind != CollectionStore::Kind::Collection) return;
    target = id;
    reloading = true;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        const bool isTarget = target && idOf(*it) == target;
        QFont f = (*it)->font(0);
        f.setBold(isTarget);
        (*it)->setFont(0, f);
        (*it)->setIcon(0, isTarget ? targetIcon : nodeIcon);
        (*it)->setToolTip(0, isTarget
            ? tr("The target collection: Edit > Add to Target Collection (%1) adds the "
                 "selected images to it, or takes them out if they are all in it already.")
                  .arg(QKeySequence("B").toString(QKeySequence::NativeText))
            : QString());
        ++it;
    }
    reloading = false;
}

void CollectionTree::reload()
{
    if (G::isLogger) G::log("CollectionTree::reload");
    const int scroll = verticalScrollBar() ? verticalScrollBar()->value() : 0;
    reloading = true;
    clear();
    CollectionStore &store = CollectionStore::instance();
    const QVector<CollectionStore::Node> nodes = store.nodes(nodeKind);
    const QHash<qint64, int> counts = store.memberCounts(nodeKind);
    QHash<qint64, QTreeWidgetItem *> byId;
    // parents come first (CollectionStore::nodes), so every parent already has its item
    for (const CollectionStore::Node &n : nodes) {
        QTreeWidgetItem *parent = byId.value(n.parent, nullptr);
        QTreeWidgetItem *item = parent ? new QTreeWidgetItem(parent)
                                       : new QTreeWidgetItem(this);
        item->setData(0, IdRole, n.id);
        item->setText(0, n.name);
        item->setIcon(0, nodeIcon);
        if (nodeKind == CollectionStore::Kind::Collection)
            item->setText(1, QString::number(counts.value(n.id)));
        else if (pushedCounts.contains(n.id))
            item->setText(1, QString::number(pushedCounts.value(n.id)));
        if (nodeKind == CollectionStore::Kind::Query) {
            const QString text = Query::Expr::fromJsonText(n.definition).toText();
            item->setToolTip(0, text.isEmpty() ? tr("An empty query: matches everything.")
                                               : text);
        }
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        byId.insert(n.id, item);
    }
    for (auto it = byId.constBegin(); it != byId.constEnd(); ++it)
        if (expanded.contains(it.key())) it.value()->setExpanded(true);
    reloading = false;

    // a deleted node is no longer checked
    QVector<qint64> keep;
    for (qint64 id : checkedSet) if (byId.contains(id)) keep << id;
    syncFromFilter(keep);
    // ... nor the target (MW hears so, to forget it)
    if (target && !byId.contains(target)) {
        setTargetId(0);
        emit targetChanged(0);
    }
    else setTargetId(target);
    resizeColumns();
    if (verticalScrollBar()) verticalScrollBar()->setValue(scroll);
}

void CollectionTree::setCounts(const QHash<qint64, int> &counts)
{
    pushedCounts = counts;
    reloading = true;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        const qint64 id = idOf(*it);
        (*it)->setText(1, counts.contains(id) ? QString::number(counts.value(id))
                                              : QString());
        styleItem(*it);
        ++it;
    }
    reloading = false;
    resizeColumns();
}

void CollectionTree::refreshCounts()
{
    if (nodeKind != CollectionStore::Kind::Collection) return;
    const QHash<qint64, int> counts =
        CollectionStore::instance().memberCounts(nodeKind);
    reloading = true;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        (*it)->setText(1, QString::number(counts.value(idOf(*it))));
        ++it;
    }
    reloading = false;
    resizeColumns();
}

void CollectionTree::syncFromFilter(const QVector<qint64> &ids)
{
/*
    MW's push of the Collections filter. Emits nothing. A checked collection inside a
    collapsed branch has its ancestors expanded: a filter the user cannot see being
    applied is indistinguishable from a panel that did nothing.
*/
    checkedSet = ids;
    const QSet<qint64> set(ids.begin(), ids.end());
    reloading = true;
    clearSelection();
    QTreeWidgetItemIterator it(this);
    while (*it) {
        QTreeWidgetItem *item = *it;
        if (set.contains(idOf(item))) {
            item->setSelected(true);
            for (QTreeWidgetItem *up = item->parent(); up; up = up->parent())
                if (!up->isExpanded()) {
                    up->setExpanded(true);
                    expanded.insert(idOf(up));
                }
        }
        styleItem(item);
        ++it;
    }
    reloading = false;
}

QList<qint64> CollectionTree::expandedIds() const
{
    QList<qint64> out(expanded.begin(), expanded.end());
    std::sort(out.begin(), out.end());
    return out;
}

void CollectionTree::setExpandedIds(const QList<qint64> &ids)
{
    expanded = QSet<qint64>(ids.begin(), ids.end());
    reloading = true;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if (expanded.contains(idOf(*it))) (*it)->setExpanded(true);
        ++it;
    }
    reloading = false;
}

void CollectionTree::createNode(qint64 parent)
{
    if (nodeKind == CollectionStore::Kind::Query) {
        emit newQueryRequested(parent);
        return;
    }
/*
    A new node gets a name that is already unique among its siblings, and goes straight
    into edit so the user types over it -- Lightroom asks in a dialog first, but a name
    typed in place is one step, and Esc keeps the default.
*/
    const QString base = nodeKind == CollectionStore::Kind::Query ? tr("New Query")
                                                                   : tr("New Collection");
    QSet<QString> taken;
    QTreeWidgetItem *p = itemFor(parent);
    const int n = p ? p->childCount() : topLevelItemCount();
    for (int i = 0; i < n; ++i)
        taken.insert((p ? p->child(i) : topLevelItem(i))->text(0));
    QString name = base;
    for (int i = 2; taken.contains(name); ++i) name = base + " " + QString::number(i);

    /*  create() emits nodesChanged, which rebuilds this tree synchronously: every item
        pointer taken above is gone, so both rows are looked up again by id. */
    const qint64 id = CollectionStore::instance().create(nodeKind, parent, name);
    if (id == 0) return;
    if (QTreeWidgetItem *pp = itemFor(parent)) {
        expanded.insert(parent);
        pp->setExpanded(true);
    }
    if (QTreeWidgetItem *item = itemFor(id)) {
        scrollToItem(item);
        setCurrentItem(item);
        editItem(item, 0);
    }
}

QList<QTreeWidgetItem *> CollectionTree::visibleItems() const
{
    QList<QTreeWidgetItem *> out;
    QTreeWidgetItemIterator it(const_cast<CollectionTree *>(this));
    while (*it) {
        bool shown = true;
        for (QTreeWidgetItem *up = (*it)->parent(); up; up = up->parent())
            if (!up->isExpanded()) { shown = false; break; }
        if (shown) out << *it;
        ++it;
    }
    return out;
}

void CollectionTree::requestFor(QTreeWidgetItem *item, Qt::KeyboardModifiers mods)
{
/*
    Turn a click into the set it asks for, and ASK. Cmd arrives as ControlModifier on
    macOS and Ctrl is ControlModifier on Windows, so one test serves both.
*/
    if (!item) return;
    const qint64 id = idOf(item);
    const bool alt = mods & Qt::AltModifier;
    const bool cmd = mods & Qt::ControlModifier;
    const bool shift = mods & Qt::ShiftModifier;
    QVector<qint64> ids = checkedSet;

    if (shift && rangeAnchorId > 0) {
        const QList<QTreeWidgetItem *> vis = visibleItems();
        int a = -1, b = -1;
        for (int i = 0; i < vis.size(); ++i) {
            if (idOf(vis.at(i)) == rangeAnchorId) a = i;
            if (vis.at(i) == item) b = i;
        }
        if (a >= 0 && b >= 0) {
            ids.clear();
            for (int i = qMin(a, b); i <= qMax(a, b); ++i) ids << idOf(vis.at(i));
            emit filterRequested(ids, alt);
            return;
        }
    }

    if (cmd) {
        if (ids.contains(id)) ids.removeAll(id);
        else ids << id;
    }
    else ids = {id};
    rangeAnchorId = id;
    emit filterRequested(ids, alt);
}

void CollectionTree::mousePressEvent(QMouseEvent *event)
{
    QTreeWidgetItem *item = itemAt(event->position().toPoint());
    pressItem = nullptr;
    pressIsClick = false;
    /*  The expand arrow sits LEFT of the item's rect and belongs to the base class, as
        does the right button (the context menu). */
    if (!item || event->button() != Qt::LeftButton
        || event->position().x() < visualItemRect(item).left()) {
        QTreeWidget::mousePressEvent(event);
        return;
    }
    setCurrentItem(item);
    pressItem = item;
    pressPos = event->position().toPoint();
    pressMods = event->modifiers();
    pressIsClick = true;
    event->accept();
}

void CollectionTree::mouseMoveEvent(QMouseEvent *event)
{
    const QModelIndex idx = indexAt(event->position().toPoint());
    delegate->setHoveredIndex(idx.isValid() ? idx.siblingAtColumn(0) : QModelIndex());

    if (pressIsClick && pressItem && (event->buttons() & Qt::LeftButton)
        && (event->position().toPoint() - pressPos).manhattanLength()
               >= QApplication::startDragDistance()) {
        pressIsClick = false;
        QTreeWidgetItem *item = pressItem;
        pressItem = nullptr;
        startNodeDrag(item);
        return;
    }
    QTreeWidget::mouseMoveEvent(event);
}

void CollectionTree::mouseReleaseEvent(QMouseEvent *event)
{
    if (pressIsClick && pressItem && event->button() == Qt::LeftButton) {
        QTreeWidgetItem *item = pressItem;
        pressItem = nullptr;
        pressIsClick = false;
        requestFor(item, pressMods);
        event->accept();
        return;
    }
    pressItem = nullptr;
    pressIsClick = false;
    QTreeWidget::mouseReleaseEvent(event);
}

void CollectionTree::mouseDoubleClickEvent(QMouseEvent *event)
{
    /*  The first click already filtered. The second EDITS a query -- the quick way to the
        builder, as a double-click opens a Lightroom smart collection -- and otherwise
        opens or closes the branch. */
    QTreeWidgetItem *item = itemAt(event->position().toPoint());
    if (item && nodeKind == CollectionStore::Kind::Query) emit editQueryRequested(idOf(item));
    else if (item && item->childCount()) item->setExpanded(!item->isExpanded());
    pressItem = nullptr;
    pressIsClick = false;
    event->accept();
}

void CollectionTree::leaveEvent(QEvent *event)
{
    delegate->setHoveredIndex(QModelIndex());
    QTreeWidget::leaveEvent(event);
}

void CollectionTree::keyPressEvent(QKeyEvent *event)
{
    if (state() == QAbstractItemView::EditingState) {
        QTreeWidget::keyPressEvent(event);
        return;
    }
    QTreeWidgetItem *item = currentItem();
    // Return or Space filters by the current row, as a click would; F2 renames it
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        requestFor(item, event->modifiers());
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F2 && item) {
        editItem(item, 0);
        event->accept();
        return;
    }
    QTreeWidget::keyPressEvent(event);
}

void CollectionTree::deleteNode(qint64 id)
{
    QTreeWidgetItem *item = itemFor(id);
    if (!item) return;
    std::function<int(QTreeWidgetItem *)> countUnder = [&](QTreeWidgetItem *i) {
        int n = i->childCount();
        for (int c = 0; c < i->childCount(); ++c) n += countUnder(i->child(c));
        return n;
    };
    const int inside = countUnder(item);
    /*  A collection is the user's own work and nothing can rebuild it, so deleting one
        asks. The images themselves are not touched, and the question says so. */
    QString text = tr("Delete the %1 \"%2\"?").arg(kindWord(), item->text(0));
    if (inside)
        text += "\n\n" + tr("The %n %1 inside it will be deleted too.", "", inside)
                             .arg(kindWord(inside != 1));
    if (nodeKind == CollectionStore::Kind::Collection)
        text += "\n\n" + tr("No images are deleted: only the %1 is.").arg(kindWord());
    if (QMessageBox::question(this, tr("Delete"), text,
                              QMessageBox::Yes | QMessageBox::Cancel,
                              QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    // a checked node's filter goes with it (Filters::setSetNodes)
    CollectionStore::instance().remove(id);
}

void CollectionTree::contextMenuEvent(QContextMenuEvent *event)
{
/*
    THE DISCOVERABLE PATH to everything the tree does by gesture: New, Rename, Delete,
    and adding or removing the grid's selection without a drag. A blank-space click
    offers New alone.
*/
    QTreeWidgetItem *item = itemAt(event->pos());
    const qint64 id = idOf(item);
    const QString name = item ? item->text(0) : QString();
    const bool isQuery = nodeKind == CollectionStore::Kind::Query;

    QMenu menu(this);
    menu.setToolTipsVisible(true);

    QAction *newTop = menu.addAction(isQuery ? tr("New Query...") : tr("New Collection"));
    connect(newTop, &QAction::triggered, this, [this] { createNode(0); });
    if (item) {
        QAction *newIn = menu.addAction(tr("New %1 Inside \"%2\"")
                                        .arg(isQuery ? tr("Query") : tr("Collection"), name));
        connect(newIn, &QAction::triggered, this, [this, id] { createNode(id); });
    }

    if (item && acceptsImages()) {
        menu.addSeparator();
        QAction *add = menu.addAction(tr("Add Selected Images to \"%1\"").arg(name));
        add->setToolTip(tr("Or drag thumbnails onto the collection."));
        connect(add, &QAction::triggered, this, [this, id] { emit addSelectionRequested(id); });

        /*  REMOVE ONLY WHILE FILTERING BY IT: then the grid is showing this collection's
            images, and "the selected images" plainly means some of them. Otherwise the
            selection is images from anywhere, and taking them out of a collection the
            user is not looking at is a guess. Disabled with the reason. */
        QAction *remove = menu.addAction(tr("Remove Selected Images from \"%1\"").arg(name));
        const bool showing = checkedSet.contains(id);
        remove->setEnabled(showing);
        remove->setToolTip(showing
            ? tr("The images stay in the Library; only the collection changes.")
            : tr("Click this collection first, then select the images to remove."));
        connect(remove, &QAction::triggered, this,
                [this, id] { emit removeSelectionRequested(id); });

        /*  TARGET COLLECTION: checked on the current target, where it clears it. */
        QAction *tgt = menu.addAction(tr("Set as Target Collection"));
        tgt->setCheckable(true);
        tgt->setChecked(id == target);
        tgt->setToolTip(tr("Edit > Add to Target Collection (%1) then adds the selected "
                           "images here, or takes them out if they are all here already.")
                            .arg(QKeySequence("B").toString(QKeySequence::NativeText)));
        connect(tgt, &QAction::triggered, this, [this, id](bool on) {
            setTargetId(on ? id : 0);
            emit targetChanged(target);
        });
    }

    if (item) {
        menu.addSeparator();
        if (isQuery) {
            QAction *editQ = menu.addAction(tr("Edit Query..."));
            editQ->setToolTip(tr("Open it in the Query Builder. Or double-click it."));
            connect(editQ, &QAction::triggered, this, [this, id] {
                emit editQueryRequested(id);
            });
        }
        QAction *dup = menu.addAction(tr("Duplicate"));
        dup->setToolTip(isQuery ? tr("A copy of the query, to change without losing this one.")
                                : tr("A copy holding the same images."));
        connect(dup, &QAction::triggered, this, [id] {
            CollectionStore::instance().duplicate(id);
        });
        QAction *rename = menu.addAction(tr("Rename"));
        rename->setShortcut(QKeySequence(Qt::Key_F2));
        connect(rename, &QAction::triggered, this, [this, id] {
            if (QTreeWidgetItem *i = itemFor(id)) editItem(i, 0);
        });
        QAction *del = menu.addAction(tr("Delete %1...").arg(isQuery ? tr("Query")
                                                                     : tr("Collection")));
        connect(del, &QAction::triggered, this, [this, id] { deleteNode(id); });
        if (item->parent()) {
            QAction *top = menu.addAction(tr("Move to Top Level"));
            top->setToolTip(tr("Or drag it onto the blank space below the list."));
            connect(top, &QAction::triggered, this,
                    [id] { CollectionStore::instance().reparent(id, 0); });
        }
    }

    /*  LAST, after a separator: Cancel clears the collection filter -- the whole Library,
        with every other filter as it is. Disabled, with the reason, when no collection
        is filtering. */
    menu.addSeparator();
    QAction *cancel = menu.addAction(tr("Cancel"));
    cancel->setEnabled(!checkedSet.isEmpty());
    cancel->setToolTip(checkedSet.isEmpty()
        ? tr("No %1 is filtering: the whole Library is showing.").arg(kindWord())
        : tr("Stop filtering by %1 and show the whole Library, keeping the other "
             "filters.").arg(kindWord(checkedSet.size() > 1)));
    connect(cancel, &QAction::triggered, this, [this] { emit cancelRequested(); });
    menu.exec(event->globalPos());
}

void CollectionTree::startNodeDrag(QTreeWidgetItem *item)
{
    const qint64 id = idOf(item);
    if (id <= 0) return;
    auto *mime = new QMimeData;
    mime->setData(kNodeMime, QByteArray::number(id));
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    const QRect r = visualItemRect(item);
    QPixmap pm(r.size() * devicePixelRatioF());
    pm.setDevicePixelRatio(devicePixelRatioF());
    pm.fill(Qt::transparent);
    viewport()->render(&pm, QPoint(), QRegion(r));
    drag->setPixmap(pm);
    drag->setHotSpot(QPoint(16, r.height() / 2));
    drag->exec(Qt::MoveAction);
    showDropSpot(DropSpot());           // a drag that ended elsewhere leaves no marker
}

CollectionTree::DropSpot CollectionTree::nodeDropSpot(qint64 id, const QPoint &pos) const
{
/*
    THREE BANDS PER ROW. The top and bottom quarters place the node BEFORE or AFTER the
    row, as its sibling; the middle puts it INTO the row, last among its children. After
    an open branch is its FIRST child, which is where the line below that row visibly
    sits. Below the rows is last at the top level.
*/
    DropSpot spot;
    const CollectionStore::Node n = CollectionStore::instance().node(id);
    if (id <= 0 || n.id == 0 || n.kind != nodeKind) return spot;

    // the sibling after item, skipping the node being dragged; 0 = item is last
    auto nextAfter = [this, id](QTreeWidgetItem *item) -> qint64 {
        QTreeWidgetItem *p = item->parent();
        const int count = p ? p->childCount() : topLevelItemCount();
        for (int i = (p ? p->indexOfChild(item) : indexOfTopLevelItem(item)) + 1;
             i < count; ++i) {
            const qint64 sib = idOf(p ? p->child(i) : topLevelItem(i));
            if (sib != id) return sib;
        }
        return 0;
    };
    const int right = viewport()->width() - 1;

    QTreeWidgetItem *target = itemAt(pos);
    if (!target) {
        spot.parent = 0;
        spot.before = 0;
        const QList<QTreeWidgetItem *> vis = visibleItems();
        if (!vis.isEmpty()) {
            const int y = visualItemRect(vis.last()).bottom() + 1;
            spot.line = QLine(visualItemRect(topLevelItem(0)).left(), y, right, y);
        }
    }
    else {
        const QRect r = visualItemRect(target);
        const int band = qMax(1, r.height() / 4);
        if (pos.y() < r.top() + band) {
            spot.parent = idOf(target->parent());
            spot.before = idOf(target);
            spot.line = QLine(r.left(), r.top(), right, r.top());
        }
        else if (pos.y() > r.bottom() - band) {
            const int y = r.bottom() + 1;
            if (target->isExpanded() && target->childCount()) {
                spot.parent = idOf(target);
                const qint64 first = idOf(target->child(0));
                spot.before = first != id ? first : nextAfter(target->child(0));
                spot.line = QLine(r.left() + indentation(), y, right, y);
            }
            else {
                spot.parent = idOf(target->parent());
                spot.before = nextAfter(target);
                spot.line = QLine(r.left(), y, right, y);
            }
        }
        else {
            spot.parent = idOf(target);
            spot.before = 0;
            spot.into = target;
        }
    }

    // not into itself or its own subtree, and not where it already is
    if (spot.before == id || spot.parent == id) return spot;
    if (CollectionStore::instance().wouldCycle(id, spot.parent)) return spot;
    if (QTreeWidgetItem *self = itemFor(id))
        if (spot.parent == n.parent && spot.before == nextAfter(self)) return spot;
    spot.valid = true;
    return spot;
}

void CollectionTree::showDropSpot(const DropSpot &spot)
{
    delegate->setHoveredIndex(spot.valid && spot.into ? indexFromItem(spot.into, 0)
                                                      : QModelIndex());
    const QLine line = spot.valid ? spot.line : QLine();
    if (line != dropLine) {
        dropLine = line;
        viewport()->update();
    }
}

void CollectionTree::paintEvent(QPaintEvent *event)
{
    QTreeWidget::paintEvent(event);
    if (dropLine.isNull()) return;
    // the insertion marker of a node drag: a line with a ring at its start
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(palette().color(QPalette::Highlight), 2));
    p.setBrush(Qt::NoBrush);
    p.drawLine(dropLine.p1() + QPoint(4, 0), dropLine.p2());
    p.drawEllipse(QPointF(dropLine.p1()) + QPointF(1.5, 0), 3, 3);
}

void CollectionTree::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *m = event->mimeData();
    if (m->hasFormat(kNodeMime) && event->source() == this) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    else if (acceptsImages() && m->hasUrls() && isEnabled()) {
        showImageCountBadge(m);
        acceptWithoutMoving(event);
    }
    else event->ignore();
}

void CollectionTree::dragMoveEvent(QDragMoveEvent *event)
{
/*
    REFUSED WHILE THE DRAG IS STILL MOVING, so the "no" cursor says so before the drop:
    a collection onto itself, into its own subtree or where it already is; thumbnails
    onto the blank space, which is not a collection. A line marks where a collection
    would be inserted, the hover highlight the row it would go into; both are cleared
    on every refusal (nodeDropSpot).
*/
    QTreeWidgetItem *target = itemAt(event->position().toPoint());
    const QMimeData *m = event->mimeData();
    if (m->hasFormat(kNodeMime) && event->source() == this) {
        const DropSpot spot = nodeDropSpot(m->data(kNodeMime).toLongLong(),
                                           event->position().toPoint());
        showDropSpot(spot);
        if (!spot.valid) {
            event->ignore();
            return;
        }
        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }
    if (acceptsImages() && m->hasUrls()) showImageCountBadge(m);
    if (acceptsImages() && m->hasUrls() && target) {
        delegate->setHoveredIndex(indexFromItem(target, 0));
        acceptWithoutMoving(event);
        return;
    }
    delegate->setHoveredIndex(QModelIndex());
    event->ignore();
}

void CollectionTree::dragLeaveEvent(QDragLeaveEvent *event)
{
    showDropSpot(DropSpot());
    QTreeWidget::dragLeaveEvent(event);
}

void CollectionTree::dropEvent(QDropEvent *event)
{
    QTreeWidgetItem *target = itemAt(event->position().toPoint());
    showDropSpot(DropSpot());
    const QMimeData *m = event->mimeData();

    if (m->hasFormat(kNodeMime) && event->source() == this) {
        const qint64 id = m->data(kNodeMime).toLongLong();
        const DropSpot spot = nodeDropSpot(id, event->position().toPoint());
        if (!spot.valid) { event->ignore(); return; }
        event->setDropAction(Qt::MoveAction);
        event->accept();
        if (spot.parent > 0) expanded.insert(spot.parent);  // show where it went
        CollectionStore::instance().move(id, spot.parent, spot.before);
        return;
    }

    if (acceptsImages() && m->hasUrls() && target) {
        // the action first, so no early return can leave the source believing it moved
        acceptWithoutMoving(event);
        QStringList paths;
        for (const QUrl &u : m->urls())
            if (u.isLocalFile()) paths << u.toLocalFile();
        if (!paths.isEmpty()) emit addPathsRequested(idOf(target), paths);
        return;
    }
    event->ignore();
}

void CollectionTree::resizeColumns()
{
    QFont f = font();
    f.setPointSize(G::strFontSize.toInt());
    int countWidth = QFontMetrics(f).boundingRect(countMetric).width();
    const QFontMetrics wfm = fontMetrics();
    int widest = 0;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        widest = qMax(widest, wfm.horizontalAdvance((*it)->text(1)));
        ++it;
    }
    countWidth = qMax(countWidth, widest + 8);
    const int avail = viewport() ? viewport()->width() : width();
    setColumnWidth(1, countWidth);
    setColumnWidth(0, qMax(0, avail - countWidth - countMargin));
}

void CollectionTree::resizeEvent(QResizeEvent *event)
{
    QTreeWidget::resizeEvent(event);
    resizeColumns();
}

void CollectionTree::styleItem(QTreeWidgetItem *item)
{
    // an empty collection is dimmed: filtering by it shows nothing; the count says why
    const bool empty = nodeKind == CollectionStore::Kind::Collection
                       && item->text(1) == "0" && item->childCount() == 0;
    if (empty) item->setForeground(1, QBrush(G::disabledColor));
    else item->setData(1, Qt::ForegroundRole, QVariant());
}

void CollectionTree::updateStyle()
{
    QTreeWidgetItemIterator it(this);
    while (*it) { styleItem(*it); ++it; }
}
