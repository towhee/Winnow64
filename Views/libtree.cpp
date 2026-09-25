#include "Views/libtree.h"
#include "File/hoverdelegate.h"
#include "Main/global.h"
#include "Utilities/foldertree.h"

#include <QContextMenuEvent>
#include <QDir>
#include <QFileIconProvider>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyleFactory>

namespace {
/*  THE LIBRARY ROW IS TEAL, the colour the retired Catalog row above the Folders tree
    was, so it reads as "not a folder on disk" against the blue folders macOS draws and
    the yellow ones Windows draws. Its folders beneath are ordinary folder icons: they
    ARE folders on disk, which is the whole point of showing them this way. */
const QColor kLibraryColor(78, 176, 166);
}  // namespace

LibTree::LibTree(const QString &countMetric, int countMargin, QWidget *parent)
    : QTreeWidget(parent), countMetric(countMetric), countMargin(countMargin)
{
    if (G::isLogger) G::log("LibTree::LibTree");
    setObjectName("libTree");

    libraryIcon = tintedIcon(":/images/icon16/foldertree_white.png", kLibraryColor);
    catalogIcon = tintedIcon(":/images/icon16/foldertree_white.png",
                             kLibraryColor.lighter(115));
    folderIcon = QFileIconProvider().icon(QFileIconProvider::Folder);
    excludedColor = QColor(0xd0, 0x60, 0x60);       // Filters::itemIsExcludedColor

    setColumnCount(2);
    setHeaderHidden(true);
    setRootIsDecorated(true);
    setUniformRowHeights(true);
    setIndentation(16);                             // FSTree's
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    /*  NO SELECTION BY THE MOUSE. What is selected is what the Filters panel includes,
        and MW pushes that back (syncFromFilters); letting a click select as well would
        show the click before the filter had been applied -- and leave it showing if the
        filter was refused. Programmatic selection still works. */
    setSelectionMode(QAbstractItemView::NoSelection);
    setAcceptDrops(false);
    setDragEnabled(false);
    /*  DefaultContextMenu, stated because Winnow's docks have used ActionsContextMenu,
        under which contextMenuEvent is never called. */
    setContextMenuPolicy(Qt::DefaultContextMenu);

#ifdef Q_OS_WIN
    // As FSTree: Fusion drops the native column separator the Windows style draws.
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
        const QString p = item->data(0, PathRole).toString();
        if (!p.isEmpty()) expanded.insert(p);
    });
    connect(this, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *item) {
        expanded.remove(item->data(0, PathRole).toString());
    });

    updateStyle();
    setSources({}, -1);
}

QIcon LibTree::tintedIcon(const QString &resource, const QColor &c)
{
    QPixmap pm(resource);
    if (pm.isNull()) return QIcon();
    QPixmap tinted(pm.size());
    tinted.setDevicePixelRatio(pm.devicePixelRatio());
    tinted.fill(Qt::transparent);
    QPainter p(&tinted);
    p.drawPixmap(0, 0, pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(tinted.rect(), c);
    p.end();
    return QIcon(tinted);
}

void LibTree::setSources(const QVector<LibrarySource> &sources, qint64 totalCount)
{
/*
    Rebuild the tree. It is a few thousand rows at most -- the Library's FOLDERS, not its
    images -- so rebuilding is cheap and cannot leave a half-patched tree behind; what
    the user set (expansion, and the scroll position) is carried across by path.
*/
    if (G::isLogger) G::log("LibTree::setSources");
    const int scroll = verticalScrollBar() ? verticalScrollBar()->value() : 0;
    const QSet<QString> keepExpanded = expanded;

    clear();
    libraryItem = new QTreeWidgetItem(this);
    libraryItem->setText(0, tr("Library"));
    /*  NO THOUSANDS SEPARATOR, as in the Folders tree: the count column is narrow, and a
        six-figure library would elide to "52,4...". -1 is "the index is not open", which
        is not the same fact as an empty library, so say nothing rather than zero. */
    libraryItem->setText(1, totalCount < 0 ? QString() : QString::number(totalCount));
    libraryItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    libraryItem->setIcon(0, libraryIcon);
    libraryItem->setToolTip(0, tr(
        "Every image in the Library, across all of its folders.\n\n"
        "Click a folder below to show only it and the folders in it.\n"
        "Alt/Opt+click: this folder only.  Ctrl/Cmd+click: add or remove a folder.\n"
        "Shift+click: a range of folders."));

    const bool showCatalogs = sources.size() > 1;
    for (const LibrarySource &src : sources) {
        QTreeWidgetItem *top = libraryItem;
        const QMap<QString, int> totals = FolderTree::expandCounts(src.folderCounts);
        if (showCatalogs) {
            int n = 0;
            for (int c : src.folderCounts) n += c;
            top = new QTreeWidgetItem(libraryItem);
            top->setText(0, src.name);
            top->setText(1, QString::number(n));
            top->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            top->setIcon(0, catalogIcon);
            top->setData(0, AnchorsRole, src.anchors);
            top->setToolTip(0, tr("The folders catalogued in %1.").arg(src.name));
        }

        QHash<QString, QTreeWidgetItem *> byPath;
        const QVector<FolderTree::Node> nodes =
            FolderTree::hierarchy(src.folderCounts.keys(), src.anchors);
        for (const FolderTree::Node &n : nodes) {
            QTreeWidgetItem *parent = n.parent.isEmpty() ? top : byPath.value(n.parent, top);
            QTreeWidgetItem *item = new QTreeWidgetItem(parent);
            item->setText(0, n.label);
            item->setText(1, QString::number(totals.value(n.path)));
            item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            item->setIcon(0, folderIcon);
            item->setData(0, PathRole, n.path);
            bool offline = false;
            for (const QString &a : src.offlineAnchors)
                if (FolderTree::isAtOrUnder(n.path, a)) { offline = true; break; }
            item->setData(0, OfflineRole, offline);
            item->setToolTip(0, offline
                ? tr("%1\n\nNot available: its volume is not mounted.")
                      .arg(QDir::toNativeSeparators(n.path))
                : QDir::toNativeSeparators(n.path));
            byPath.insert(n.path, item);
            if (keepExpanded.contains(n.path)) item->setExpanded(true);
        }
        if (top != libraryItem) top->setExpanded(true);
    }
    libraryItem->setExpanded(true);
    expanded = keepExpanded;

    syncFromFilters(includes, excludes);
    resizeColumns();
    if (verticalScrollBar()) verticalScrollBar()->setValue(scroll);
}

void LibTree::syncFromFilters(const QStringList &inc, const QStringList &exc)
{
/*
    MW's push of the one folder filter. Nothing here emits, so this can be called from
    inside the Filters change that caused it without looping back.

    AN INCLUDED FOLDER IS MADE VISIBLE -- its ancestors are expanded -- because a folder
    checked in the Filters panel that is selected here inside a collapsed branch is a
    filter the user cannot see being applied.
*/
    includes = inc;
    excludes = exc;
    const QSet<QString> incSet(inc.begin(), inc.end());

    clearSelection();
    QTreeWidgetItemIterator it(this);
    while (*it) {
        QTreeWidgetItem *item = *it;
        const QString p = item->data(0, PathRole).toString();
        if (!p.isEmpty() && incSet.contains(p)) {
            item->setSelected(true);
            for (QTreeWidgetItem *up = item->parent(); up; up = up->parent())
                if (!up->isExpanded()) up->setExpanded(true);
        }
        styleItem(item);
        ++it;
    }
    // no folder filter: the whole Library is what is being looked at
    if (libraryItem && inc.isEmpty() && exc.isEmpty()) libraryItem->setSelected(true);
}

void LibTree::styleItem(QTreeWidgetItem *item)
{
/*
    Excluded is a strikeout in the Filters exclude colour -- the same marks the Filters
    panel draws, so the two views of one filter look like one filter. Offline is the
    disabled colour and no strikeout: it is a fact about the disk, not a choice.
*/
    const QString p = item->data(0, PathRole).toString();
    const bool excluded = !p.isEmpty() && excludes.contains(p);
    const bool offline = item->data(0, OfflineRole).toBool();
    QFont f = font();
    f.setStrikeOut(excluded);
    item->setFont(0, f);
    if (excluded)     item->setForeground(0, QBrush(excludedColor));
    else if (offline) item->setForeground(0, QBrush(G::disabledColor));
    else              item->setData(0, Qt::ForegroundRole, QVariant());
}

QStringList LibTree::expandedPaths() const
{
    QStringList out(expanded.begin(), expanded.end());
    out.sort();
    return out;
}

void LibTree::setExpandedPaths(const QStringList &paths)
{
    expanded = QSet<QString>(paths.begin(), paths.end());
    QTreeWidgetItemIterator it(this);
    while (*it) {
        const QString p = (*it)->data(0, PathRole).toString();
        if (!p.isEmpty() && expanded.contains(p)) (*it)->setExpanded(true);
        ++it;
    }
}

QList<QTreeWidgetItem *> LibTree::visibleItems() const
{
    QList<QTreeWidgetItem *> out;
    QTreeWidgetItemIterator it(const_cast<LibTree *>(this));
    while (*it) {
        bool shown = true;
        for (QTreeWidgetItem *up = (*it)->parent(); up; up = up->parent())
            if (!up->isExpanded()) { shown = false; break; }
        if (shown) out << *it;
        ++it;
    }
    return out;
}

void LibTree::requestFor(QTreeWidgetItem *item, Qt::KeyboardModifiers mods)
{
/*
    Turn a click into the folder filter it asks for, and ASK -- MW applies it to the
    Filters panel and pushes the result back. Cmd arrives as ControlModifier on macOS and
    Ctrl is ControlModifier on Windows, so one test serves both.
*/
    if (!item) return;
    if (item == libraryItem) {
        rangeAnchorPath.clear();
        emit folderFilterRequested({}, {});
        return;
    }

    const QString path = item->data(0, PathRole).toString();
    const QStringList anchors = item->data(0, AnchorsRole).toStringList();
    const QStringList targets = anchors.isEmpty() ? QStringList{path} : anchors;
    QStringList inc = includes;
    QStringList exc = excludes;

    const bool alt = mods & Qt::AltModifier;
    const bool cmd = mods & Qt::ControlModifier;
    const bool shift = mods & Qt::ShiftModifier;

    if (shift && !rangeAnchorPath.isEmpty() && !path.isEmpty()) {
        const QList<QTreeWidgetItem *> vis = visibleItems();
        int a = -1, b = -1;
        for (int i = 0; i < vis.size(); ++i) {
            const QString p = vis.at(i)->data(0, PathRole).toString();
            if (p == rangeAnchorPath) a = i;
            if (vis.at(i) == item) b = i;
        }
        if (a >= 0 && b >= 0) {
            inc.clear();
            exc.clear();
            for (int i = qMin(a, b); i <= qMax(a, b); ++i) {
                const QString p = vis.at(i)->data(0, PathRole).toString();
                if (!p.isEmpty()) inc << p;
            }
            emit folderFilterRequested(inc, exc);
            return;
        }
    }

    if (cmd) {
        bool allIn = true;
        for (const QString &t : targets) if (!inc.contains(t)) { allIn = false; break; }
        for (const QString &t : targets) {
            if (allIn) inc.removeAll(t);
            else {
                if (!inc.contains(t)) inc << t;
                exc.removeAll(t);
            }
        }
    }
    else {
        inc = targets;
        exc.clear();
        /*  THIS FOLDER ONLY is not a new kind of filter: it is the folder with each
            folder directly inside it excluded, which is what the Filters panel can
            already say -- so it shows there truthfully rather than as a mode it cannot
            display. */
        if (alt) {
            for (int i = 0; i < item->childCount(); ++i) {
                const QString c = item->child(i)->data(0, PathRole).toString();
                if (!c.isEmpty()) exc << c;
            }
        }
    }
    if (!path.isEmpty()) rangeAnchorPath = path;
    emit folderFilterRequested(inc, exc);
}

void LibTree::mousePressEvent(QMouseEvent *event)
{
    QTreeWidgetItem *item = itemAt(event->position().toPoint());
    /*  The expand arrow sits LEFT of the item's rect (the indentation), and belongs to
        the base class: it expands, it does not filter. So does the right button, which
        is the context menu's. */
    if (!item || event->button() != Qt::LeftButton
        || event->position().x() < visualItemRect(item).left()) {
        QTreeWidget::mousePressEvent(event);
        return;
    }
    setCurrentItem(item);
    requestFor(item, event->modifiers());
    swallowRelease = true;
    event->accept();
}

void LibTree::mouseReleaseEvent(QMouseEvent *event)
{
    if (swallowRelease) {
        swallowRelease = false;
        event->accept();
        return;
    }
    QTreeWidget::mouseReleaseEvent(event);
}

void LibTree::mouseDoubleClickEvent(QMouseEvent *event)
{
    // the press already filtered; a double click also opens or closes the folder
    QTreeWidgetItem *item = itemAt(event->position().toPoint());
    if (item && item->childCount() && item != libraryItem)
        item->setExpanded(!item->isExpanded());
    swallowRelease = true;
    event->accept();
}

void LibTree::mouseMoveEvent(QMouseEvent *event)
{
    const QModelIndex idx = indexAt(event->position().toPoint());
    delegate->setHoveredIndex(idx.isValid() ? idx.siblingAtColumn(0) : QModelIndex());
    QTreeWidget::mouseMoveEvent(event);
}

void LibTree::leaveEvent(QEvent *event)
{
    delegate->setHoveredIndex(QModelIndex());
    QTreeWidget::leaveEvent(event);
}

void LibTree::keyPressEvent(QKeyEvent *event)
{
    // arrows move the current row; Return or Space applies it, as a click would
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        requestFor(currentItem(), event->modifiers());
        event->accept();
        return;
    }
    QTreeWidget::keyPressEvent(event);
}

void LibTree::contextMenuEvent(QContextMenuEvent *event)
{
/*
    READ-ONLY on purpose: where a folder is and which folders are catalogued. A folder
    whose volume is not mounted cannot be revealed, and says why in the menu rather than
    in a popup after the fact.
*/
    QTreeWidgetItem *item = itemAt(event->pos());
    const QString path = item ? item->data(0, PathRole).toString() : QString();
    const bool offline = item && item->data(0, OfflineRole).toBool();

    QMenu menu(this);
    menu.setToolTipsVisible(true);
    if (!path.isEmpty()) {
        QAction *reveal = menu.addAction(tr("Reveal in Folders"));
        QAction *show = menu.addAction(
#ifdef Q_OS_MAC
            tr("Show in Finder")
#else
            tr("Show in Explorer")
#endif
        );
        for (QAction *a : {reveal, show}) {
            a->setEnabled(!offline);
            if (offline) a->setToolTip(tr("Its volume is not mounted."));
        }
        connect(reveal, &QAction::triggered, this,
                [this, path] { emit revealInFoldersRequested(path); });
        connect(show, &QAction::triggered, this,
                [this, path] { emit showInFileManagerRequested(path); });
        menu.addSeparator();
    }
    QAction *manage = menu.addAction(tr("Manage Catalog..."));
    manage->setToolTip(tr("Choose which folders are catalogued, and scan them."));
    connect(manage, &QAction::triggered, this, [this] { emit manageCatalogRequested(); });
    menu.exec(event->globalPos());
}

void LibTree::resizeColumns()
{
/*
    The count column is FSTree's width, or the widest count in it when that is wider:
    the whole Library's total is an order of magnitude bigger than a folder's, and an
    elided total is worse than a column a few pixels wider.
*/
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

void LibTree::resizeEvent(QResizeEvent *event)
{
    QTreeWidget::resizeEvent(event);
    resizeColumns();
}

void LibTree::updateStyle()
{
    offlineColor = G::disabledColor;
    QTreeWidgetItemIterator it(this);
    while (*it) { styleItem(*it); ++it; }
}
