#include "Views/keywordtree.h"

#include "Datamodel/keywordvocab.h"
#include "Main/global.h"
#include "Metadata/keywordpaths.h"
#include "Utilities/inputdlg.h"
#include "Utilities/popup.h"

#include <QAction>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>

#include <functional>
#include <QPainter>
#include <QStyledItemDelegate>

/*
    See keywordtree.h for what this is and what it deliberately does not do.
*/

namespace {

/*  Draws the "on this image" dot before the name. A delegate rather than a decoration
    role because the dot is not an icon: it is a small mark in the text colour that has to
    sit tight against the label at any row height, and an icon would be padded and would
    fight the branch indicator for space. */
class AppliedDotDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    /*  THE ROW A DRAG IS CURRENTLY OVER, painted in the same grey the Folders and
        Bookmarks trees use for the same thing (File/hoverdelegate.cpp -- backgroundShade
        + 20). Qt's own drop indicator is a one-pixel rule between rows, which answers the
        wrong question: dropping BETWEEN two keywords has no meaning, and what the user
        needs to know is which keyword they are about to file into. Winnow already
        answered that with a filled row everywhere else a drop lands on something, so this
        does not invent a second visual language for it.

        HERE RATHER THAN IN File/hoverdelegate.h, because this tree also draws the applied
        dot and a view has one delegate. Sharing the colour is what matters; sharing the
        class would cost the dot. */
    void setDropRow(const QModelIndex &idx)
    {
        const QModelIndex row = idx.isValid()
            ? idx.sibling(idx.row(), KeywordVocab::NameColumn) : QModelIndex();
        dropRow = row;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        if (dropRow.isValid()
            && index.sibling(index.row(), KeywordVocab::NameColumn) == dropRow) {
            const int b = G::backgroundShade + 20;
            painter->fillRect(opt.rect, QColor(b, b, b));
        }

        const bool applied = index.data(KeywordVocab::AppliedRole).toBool();
        if (applied && index.column() == KeywordVocab::NameColumn) {
            const int d = 5;
            const int x = opt.rect.left() + 1;
            const int y = opt.rect.center().y() - d / 2;
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(Qt::NoPen);
            painter->setBrush(opt.palette.color(QPalette::Text));
            painter->drawEllipse(QRect(x, y, d, d));
            painter->restore();
            opt.rect.setLeft(x + d + 4);
        }
        QStyledItemDelegate::paint(painter, opt, index);
    }

private:
    QModelIndex dropRow;
};

}   // namespace

KeywordTree::KeywordTree(KeywordVocab *vocab, QWidget *parent)
    : QTreeView(parent), vocab(vocab)
{
    setModel(vocab);
    setUniformRowHeights(true);
    /*  A double-click ASSIGNS the keyword rather than expanding the branch -- expanding
        is what the disclosure arrow is for, and the fast gesture should do the thing the
        panel is for. */
    setExpandsOnDoubleClick(false);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    dotDelegate = new AppliedDotDelegate(this);
    setItemDelegate(dotDelegate);

    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(KeywordVocab::NameColumn, QHeaderView::Stretch);
    header()->setSectionResizeMode(KeywordVocab::CountColumn,
                                   QHeaderView::ResizeToContents);
    header()->setVisible(false);

    /*  Qt::ActionsContextMenu would make contextMenuEvent NEVER fire -- a trap this
        codebase has been caught by before -- so the policy is left at the default and
        the override below does the work. */
    setContextMenuPolicy(Qt::DefaultContextMenu);

    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);

    connect(this, &QTreeView::doubleClicked, this, [this](const QModelIndex &idx) {
        const QString path = idx.data(KeywordVocab::PathRole).toString();
        if (!path.isEmpty()) emit assignRequested(path);
    });

    connect(vocab, &KeywordVocab::pathChanged, this, &KeywordTree::pathChanged);

    /*  THE TREE MUST NOT COLLAPSE EVERY TIME THE VOCABULARY CHANGES. See
        rememberOpenBranches. Connected to the model's own reset signals rather than
        called from each mutator, for the same reason MW::refreshFilterVocabMarking is:
        rename, re-parent, merge, insert, delete and reload all reset, three of them are
        reached from the view without telling anyone, and a seventh route would forget. */
    connect(vocab, &QAbstractItemModel::modelAboutToBeReset,
            this, &KeywordTree::rememberOpenBranches);
    connect(vocab, &QAbstractItemModel::modelReset,
            this, &KeywordTree::restoreOpenBranches);
}

void KeywordTree::rememberOpenBranches()
{
/*
    WHICH BRANCHES WERE OPEN, AND WHERE THE USER WAS, BEFORE A MODEL RESET.

    Every vocabulary mutation resets the model -- rename, re-parent, merge, insertChild,
    remove, reload -- and QTreeView collapses the entire tree on a reset. That is not a
    detail on this panel: filing keywords means dragging photographs into a deep tree, and
    MW::applyKeywordMoves inserts a node per created target while
    KeywordVocab::removeUnusedRoots removes one per emptied root, so a single drop could
    collapse the tree several times over. The same principle is already written down at
    MW::ensureKeywordVocabLoaded, which declines to reload a populated vocabulary
    precisely so that being raised as a tab does not collapse the branch someone is
    reading.

    KEYED ON THE FOLDED PATH, not on a QModelIndex or a row -- an index does not survive
    the reset, which is the whole problem, and a row number means nothing once the tree
    re-sorts. It is the same identity, and the same technique, that
    Filters::updateKeywordItems uses to carry check and expansion state across its own
    rebuild.

    A PATH THAT NO LONGER EXISTS SIMPLY DOES NOT MATCH. A branch that was renamed, merged
    away or deleted is gone, and nothing should try to reopen it; a rename reappears under
    its new path collapsed, which is honest -- it is a different branch now.
*/
    openFold.clear();
    currentFold.clear();

    if (!vocab) return;

    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
        for (int i = 0; i < vocab->rowCount(parent); ++i) {
            const QModelIndex idx = vocab->index(i, 0, parent);
            if (!isExpanded(idx)) continue;     // a collapsed branch hides collapsed ones
            const QString p = idx.data(KeywordVocab::PathRole).toString();
            if (!p.isEmpty()) openFold.insert(keywordFold(p));
            walk(idx);
        }
    };
    walk(QModelIndex());

    const QModelIndex cur = currentIndex();
    if (cur.isValid())
        currentFold = keywordFold(cur.data(KeywordVocab::PathRole).toString());
}

void KeywordTree::restoreOpenBranches()
{
/*
    Put them back. Parent before child, which falls out of the walk order: expanding a
    node whose parent is still collapsed is legal but the view has no rows to give it.

    THE WALK STOPS AT A CLOSED BRANCH, so the cost is the OPEN part of the tree rather
    than all of it -- a few dozen rows on a vocabulary of a few thousand.
*/
    if (!vocab || openFold.isEmpty()) {
        if (!currentFold.isEmpty()) restoreCurrent();
        return;
    }

    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
        for (int i = 0; i < vocab->rowCount(parent); ++i) {
            const QModelIndex idx = vocab->index(i, 0, parent);
            const QString p = idx.data(KeywordVocab::PathRole).toString();
            if (p.isEmpty() || !openFold.contains(keywordFold(p))) continue;
            setExpanded(idx, true);
            walk(idx);
        }
    };
    walk(QModelIndex());

    restoreCurrent();
}

void KeywordTree::restoreCurrent()
{
    if (currentFold.isEmpty() || !vocab) return;
    const QModelIndex idx = vocab->indexForPath(currentFold);
    if (idx.isValid()) {
        setCurrentIndex(idx);
        scrollTo(idx, QAbstractItemView::EnsureVisible);
    }
}

QStringList KeywordTree::siblingNames(const QModelIndex &parent,
                                      const QModelIndex &except) const
{
/*
    The names already in use beside a node, handed to InputDlg as its doNotUse list so the
    dialog refuses a duplicate WHILE THE USER IS TYPING rather than after they commit. The
    model refuses it too -- the unique index on pathfold would refuse it in the end
    regardless -- but a dialog that will not accept a bad name is a better answer than a
    warning explaining why the good one was thrown away.
*/
    QStringList out;
    const int rows = model()->rowCount(parent);
    for (int r = 0; r < rows; ++r) {
        const QModelIndex sib = model()->index(r, KeywordVocab::NameColumn, parent);
        if (sib == except) continue;
        out << sib.data(Qt::DisplayRole).toString();
    }
    return out;
}

QModelIndex KeywordTree::currentNode() const
{
    const QModelIndex idx = currentIndex();
    if (!idx.isValid()) return QModelIndex();
    return idx.sibling(idx.row(), KeywordVocab::NameColumn);
}

void KeywordTree::setFilterText(const QString &text)
{
    filterText = text.trimmed();
    applyFilter(QModelIndex(), keywordFold(filterText));
}

void KeywordTree::applyFilter(const QModelIndex &parent, const QString &needle)
{
/*
    Hide what does not match, keeping the ANCESTORS of a match visible.

    A flat "hide non-matching rows" would hide the branches a match lives in, leaving
    leaves floating with no context -- and in a vocabulary where the same leaf appears in
    several places, the context is the only thing that tells them apart. So a row is shown
    when it matches OR when anything beneath it does, which is what makes the filter
    usable as a way of FINDING a keyword rather than just listing hits.
*/
    const int rows = model()->rowCount(parent);
    for (int r = 0; r < rows; ++r) {
        const QModelIndex idx = model()->index(r, KeywordVocab::NameColumn, parent);
        applyFilter(idx, needle);

        if (needle.isEmpty()) {
            setRowHidden(r, parent, false);
            continue;
        }

        bool match = keywordFold(idx.data(Qt::DisplayRole).toString()).contains(needle);
        if (!match)
            for (const QString &s : idx.data(KeywordVocab::SynonymsRole).toStringList())
                if (keywordFold(s).contains(needle)) { match = true; break; }

        bool childShown = false;
        for (int c = 0; c < model()->rowCount(idx); ++c)
            if (!isRowHidden(c, idx)) { childShown = true; break; }

        setRowHidden(r, parent, !match && !childShown);
        /*  Expanded so the match is actually on screen. Collapsing again when the filter
            clears is not attempted: the user's own expansion state is more valuable than
            tidiness, and a filter that silently collapsed branches would lose it. */
        if (childShown) setExpanded(idx, true);
    }
}

void KeywordTree::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex idx = currentNode();
    const bool haveNode = idx.isValid();
    const QString path = idx.data(KeywordVocab::PathRole).toString();

    QMenu menu(this);

    QAction *assign = menu.addAction(haveNode
        ? QString("Add \"%1\" to selected images").arg(idx.data().toString())
        : QString("Add to selected images"));
    assign->setEnabled(haveNode);
    connect(assign, &QAction::triggered, this, [this, path]{
        if (!path.isEmpty()) emit assignRequested(path);
    });

    menu.addSeparator();

    QAction *rename = menu.addAction("Rename...");
    rename->setEnabled(haveNode);
    connect(rename, &QAction::triggered, this, &KeywordTree::renameSelected);

    QAction *addChild = menu.addAction(haveNode ? "New keyword inside..."
                                                : "New keyword...");
    connect(addChild, &QAction::triggered, this, &KeywordTree::addChildOfSelected);

    QAction *insertParent = menu.addAction("Insert parent above...");
    insertParent->setEnabled(haveNode);
    connect(insertParent, &QAction::triggered, this,
            &KeywordTree::insertParentAboveSelected);

    QAction *del = menu.addAction("Delete keyword");
    del->setEnabled(haveNode);
    connect(del, &QAction::triggered, this, &KeywordTree::deleteSelected);

    menu.addSeparator();

    QAction *tidy = menu.addAction("Tidy flat keywords...");
    tidy->setToolTip("File keywords that sit outside the tree into their branches, "
                     "in the keyword list and in the images.");
    connect(tidy, &QAction::triggered, this, &KeywordTree::tidyRequested);

    QAction *build = menu.addAction("Build vocabulary from catalog");
    build->setToolTip("Add a keyword for everything the catalog has indexed. "
                      "Never removes anything.");
    connect(build, &QAction::triggered, this, &KeywordTree::buildFromCatalog);

    menu.addSeparator();

    QAction *import = menu.addAction("Import Lightroom keywords...");
    import->setToolTip("Merge a Lightroom keyword export into your list. "
                       "Never removes anything.");
    connect(import, &QAction::triggered, this, &KeywordTree::importLightroom);

    QAction *exportAct = menu.addAction("Export keywords...");
    connect(exportAct, &QAction::triggered, this, &KeywordTree::exportLightroom);

    menu.exec(event->globalPos());
    event->accept();
}

void KeywordTree::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_F2:
        renameSelected();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        const QString path = currentNode().data(KeywordVocab::PathRole).toString();
        if (!path.isEmpty()) { emit assignRequested(path); return; }
        break;
    }
    default: break;
    }
    QTreeView::keyPressEvent(event);
}

void KeywordTree::renameSelected()
{
    const QModelIndex idx = currentNode();
    if (!idx.isValid()) return;

    const QString was = idx.data().toString();
    QString name = was;
    QStringList taken = siblingNames(idx.parent(), idx);

    InputDlg dlg("Rename keyword", "New name for \"" + was + "\":", name, taken, this);
    if (dlg.exec() != QDialog::Accepted) return;
    name = name.trimmed();
    if (name.isEmpty() || name == was) return;

    if (!vocab->rename(idx, name)) {
        QMessageBox::warning(this, "Rename keyword",
            "Could not rename to \"" + name + "\".\n\n"
            "A keyword cannot contain \"|\" -- that is the path separator.");
    }
}

void KeywordTree::addChildOfSelected()
{
    const QModelIndex idx = currentNode();
    const QString under = idx.isValid() ? idx.data().toString() : QString();

    QString name;
    const QString prompt = under.isEmpty()
        ? QString("New keyword:")
        : QString("New keyword inside \"%1\":").arg(under);
    QStringList taken = siblingNames(idx, QModelIndex());

    InputDlg dlg("New keyword", prompt, name, taken, this);
    if (dlg.exec() != QDialog::Accepted) return;
    name = name.trimmed();
    if (name.isEmpty()) return;

    const QModelIndex created = vocab->insertChild(idx, name);
    if (!created.isValid()) {
        QMessageBox::warning(this, "New keyword",
            "Could not add \"" + name + "\".\n\n"
            "A keyword cannot contain \"|\", and one of that name already exists here.");
        return;
    }
    if (idx.isValid()) setExpanded(idx, true);
    setCurrentIndex(created);
    scrollTo(created);
}

void KeywordTree::insertParentAboveSelected()
{
/*
    "These places should have been under a Location branch all along." Creates a node
    beside the selected one and moves the selected one inside it -- which changes the
    PATH of the selected node and everything beneath it, so it raises the same retag
    question a rename does, through the same signal.
*/
    const QModelIndex idx = currentNode();
    if (!idx.isValid()) return;

    QString name;
    QStringList taken = siblingNames(idx.parent(), QModelIndex());

    InputDlg dlg("Insert parent",
                 "New parent above \"" + idx.data().toString() + "\":",
                 name, taken, this);
    if (dlg.exec() != QDialog::Accepted) return;
    name = name.trimmed();
    if (name.isEmpty()) return;

    const QModelIndex created = vocab->insertParentAbove(idx, name);
    if (!created.isValid()) {
        QMessageBox::warning(this, "Insert parent",
            "Could not insert \"" + name + "\" above that keyword.");
        return;
    }
    expandRecursively(created);
    setCurrentIndex(created);
}

void KeywordTree::deleteSelected()
{
    const QModelIndex idx = currentNode();
    if (!idx.isValid()) return;

    const QString name = idx.data().toString();
    const int children = model()->rowCount(idx);
    const int count = idx.data(KeywordVocab::CountRole).toInt();

    QString msg = "Delete the keyword \"" + name + "\"";
    if (children > 0) msg += " and everything inside it";
    msg += "?";
    /*  NAMING WHAT IS NOT AFFECTED, because it is the thing a user would reasonably
        fear. Deleting a vocabulary node removes a NAME; the keyword stays in every file
        that carries it, and those images simply show it as unfiled. */
    msg += "\n\nThis removes it from your keyword list only. ";
    msg += count > 0
        ? QString("The %1 image%2 already tagged with it keep the keyword in their own "
                  "files; it will show as unfiled.").arg(count).arg(count == 1 ? "" : "s")
        : QString("No images are tagged with it.");

    if (QMessageBox::question(this, "Delete keyword", msg,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes) return;

    vocab->remove(idx);
}

void KeywordTree::buildFromCatalog()
{
    const int added = vocab->buildFromCatalog();
    if (G::popup) {
        G::popup->showPopup(added > 0
            ? QString("Added %1 keyword%2 from the catalog.")
                  .arg(added).arg(added == 1 ? "" : "s")
            : QString("Your keyword list already has everything the catalog holds."),
            3000);
    }
}

/*  Shared with Views/keywordtags.cpp, which accepts a keyword dragged down from here. */
extern const char *kVocabNodeMime;

void KeywordTree::startDrag(Qt::DropActions)
{
/*
    Dragging a keyword carries its PATH, not its row or its id. A row is meaningless once
    the tree re-sorts, and an id would make the tag zone -- which has no business knowing
    the vocabulary's primary keys -- look one up. The path is the identity everywhere else
    in the keyword code, so it is the identity here too.
*/
    QStringList paths;
    for (const QModelIndex &idx : selectionModel()->selectedRows(
             KeywordVocab::NameColumn)) {
        const QString p = idx.data(KeywordVocab::PathRole).toString();
        if (!p.isEmpty()) paths << p;
    }
    if (paths.isEmpty()) return;

    QMimeData *mime = new QMimeData;
    mime->setData(kVocabNodeMime, paths.join('\n').toUtf8());
    /*  Plain text too, so the path can be dropped into any text field -- a search box, a
        note -- which costs nothing and is occasionally exactly what someone wants. */
    mime->setText(paths.join('\n'));

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
}

/*  The delegate, typed. It is created here and never replaced, so the cast is a fact
    rather than a hope; the member is held as the base type because the delegate class is
    private to this file. */
static AppliedDotDelegate *dropDelegateOf(QStyledItemDelegate *d)
{
    return static_cast<AppliedDotDelegate *>(d);
}

void KeywordTree::setDropRow(const QModelIndex &idx)
{
    if (dotDelegate == nullptr) return;
    dropDelegateOf(dotDelegate)->setDropRow(idx);
    viewport()->update();
}

void KeywordTree::dragLeaveEvent(QDragLeaveEvent *event)
{
    setDropRow(QModelIndex());
    QTreeView::dragLeaveEvent(event);
}

/*
    TAGGING IS NOT A FILE MOVE, AND SAYING SO IS NOT OPTIONAL.

    IconView::startDrag runs a no-modifier thumbnail drag with MoveAction as its default,
    and deletes the dragged files when the drop comes back as a MOVE -- on macOS, into the
    Trash. A target that calls acceptProposedAction() on that drag has therefore agreed to
    the originals being deleted. This one did, and six photographs went before the pattern
    was seen; cancelling the confirmation made no difference, because the delete happens in
    the drag SOURCE after exec() returns.

    THE ANSWER IS Qt::CopyAction, NOT Qt::IgnoreAction. Ignore does prevent the delete, but
    it also tells Qt the target refuses -- dropEvent is never delivered and the drag does
    nothing at all, which is what the first attempt at this fix shipped. Copy says the
    truthful thing: the drop is accepted, and nothing moved. The Keywords dock is not a
    destination for files; dropping images on a keyword TAGS them and there is nothing for
    the source to clean up.

    ON EVERY ONE OF THE THREE HANDLERS, because the action is re-proposed as the pointer
    moves and what exec() returns is whatever the last handler said. The drop sets it
    BEFORE emitting assignToPaths, so an early return, or a user who cancels the
    confirmation, still cannot leave the source believing a move happened.
*/
static void acceptWithoutMoving(QDropEvent *event)
{
    event->setDropAction(Qt::CopyAction);
    event->accept();
}

void KeywordTree::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) acceptWithoutMoving(event);
    else if (event->mimeData()->hasFormat(kVocabNodeMime)) event->acceptProposedAction();
    else event->ignore();
}

void KeywordTree::dragMoveEvent(QDragMoveEvent *event)
{
/*
    REJECTING WHILE THE DRAG IS STILL MOVING, not at the drop. A drop that silently does
    nothing reads as a broken widget, and the case users actually hit -- dragging a parent
    onto its own child -- is exactly the one that has to be refused. ignore() here is what
    puts the "no" cursor under the pointer.
*/
    const QModelIndex target = indexAt(event->position().toPoint());

    /*  THE HIGHLIGHT IS CLEARED ON EVERY REFUSAL, not only set on every acceptance. A
        grey row left behind under a "no" cursor says the drop will land there, which is
        the one thing it will not do. */
    if (event->mimeData()->hasFormat(kVocabNodeMime)) {
        const QByteArray data = event->mimeData()->data(kVocabNodeMime);
        for (const QByteArray &raw : data.split('\n')) {
            const QModelIndex src = vocab->indexForPath(QString::fromUtf8(raw));
            if (!src.isValid()) continue;
            /*  Onto itself, onto its own parent (a move to where it already is) or onto
                one of its own descendants: all no-ops or cycles. */
            if (src == target || src.parent() == target) {
                setDropRow(QModelIndex());
                event->ignore();
                return;
            }
            if (vocab->wouldCycle(src, target)) {
                setDropRow(QModelIndex());
                event->ignore();
                return;
            }
        }
        setDropRow(target);
        event->acceptProposedAction();
        return;
    }

    /*  Images can only land ON a keyword, never between two of them: there is no
        meaning to "tag these with the gap above Heron". */
    if (event->mimeData()->hasUrls() && target.isValid()) {
        setDropRow(target);
        acceptWithoutMoving(event);
        return;
    }
    setDropRow(QModelIndex());
    event->ignore();
}

void KeywordTree::dropEvent(QDropEvent *event)
{
    const QModelIndex target = indexAt(event->position().toPoint());
    setDropRow(QModelIndex());

    if (event->mimeData()->hasFormat(kVocabNodeMime)) {
        /*  THE TARGET IS REMEMBERED AS A PATH, NOT AS AN INDEX, and that is the whole
            reason this is not one line.

            The tree allows ExtendedSelection, so a drag can carry several keywords, and
            every pass of the loop below calls reparent or reparentMerging -- both of
            which wrap their work in beginResetModel/endResetModel, and both of which call
            reload() when a write fails. A reset invalidates every QModelIndex taken
            before it; reload() goes further and clearTree()s, DELETING every node. The
            target index captured above therefore held a dangling internalPointer from the
            SECOND keyword onward, and nodeOf() dereferenced it -- as did the setExpanded
            at the end of this block.

            src was never exposed to this because it is re-derived from its path each
            pass. The target works the same way now, which is also why it re-checks
            validity: the node it names can legitimately have been merged away by an
            earlier pass of this very loop, and stopping is the honest answer.

            AN INVALID TARGET IS THE ROOT, not a missing one -- dropping on blank space
            below the tree is how a node gets back to the top level -- so that case is
            carried as a flag rather than as an empty path. */
        const bool toRoot = !target.isValid();
        const QString targetPath = target.data(KeywordVocab::PathRole).toString();

        const QByteArray data = event->mimeData()->data(kVocabNodeMime);
        for (const QByteArray &raw : data.split('\n')) {
            const QModelIndex src = vocab->indexForPath(QString::fromUtf8(raw));
            if (!src.isValid()) continue;

            /*  RE-RESOLVED EVERY PASS, because the previous one reset the model. See the
                note above the loop. */
            const QModelIndex tgt = toRoot ? QModelIndex()
                                           : vocab->indexForPath(targetPath);
            if (!toRoot && !tgt.isValid()) break;       // merged away by an earlier pass

            /*  A NAME ALREADY TAKEN IN THE DESTINATION IS A MERGE, NOT A DEAD END. This
                used to warn and stop, on the reasoning that merging is a bigger operation
                than a drag -- which is true, and is why it is ASKED rather than done. It
                is also the operation a vocabulary grown from two sources most needs: the
                same place filed twice, "Canada|BC|..." beside
                "Location|Canada|BC|...", is exactly what a drag onto Location is trying
                to say. */
            if (vocab->wouldMerge(src, tgt)) {
                const QString from = src.data(KeywordVocab::PathRole).toString();
                const QString into = tgt.data(KeywordVocab::PathRole).toString();
                const QString answer = QString(
                    "\"%1\" already exists inside \"%2\".\n\n"
                    "Merge the two? Everything under \"%1\" moves across, keeping its "
                    "own branches, and \"%1\" itself is removed from the keyword "
                    "list.\n\nYour photographs are not changed yet -- you will be asked "
                    "about those next.").arg(from, into);
                if (QMessageBox::question(this, "Merge keywords", answer,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No) != QMessageBox::Yes) continue;

                if (vocab->reparentMerging(src, tgt).isEmpty()) {
                    QMessageBox::warning(this, "Merge keywords",
                        QString("Could not merge \"%1\".").arg(from));
                }
            }
            else if (!vocab->reparent(src, tgt)) {
                QMessageBox::warning(this, "Move keyword",
                    QString("Could not move \"%1\" there.")
                        .arg(keywordLeafOf(QString::fromUtf8(raw))));
            }
        }
        /*  Re-resolved for the same reason: the index this function opened with has been
            through a model reset per dragged keyword by now. */
        if (!toRoot) {
            const QModelIndex tgt = vocab->indexForPath(targetPath);
            if (tgt.isValid()) setExpanded(tgt, true);
        }
        event->acceptProposedAction();
        return;
    }

    if (event->mimeData()->hasUrls() && target.isValid()) {
        QStringList images;
        for (const QUrl &u : event->mimeData()->urls())
            if (u.isLocalFile()) images << u.toLocalFile();
        const QString path = target.data(KeywordVocab::PathRole).toString();
        /*  IGNORE-BUT-ACCEPTED BEFORE the tagging is even asked for, so that an early
            return, an exception, or a user who cancels the confirmation still cannot
            leave the source believing the files were moved. */
        acceptWithoutMoving(event);
        if (!images.isEmpty() && !path.isEmpty()) emit assignToPaths(path, images);
        return;
    }
    event->ignore();
}

void KeywordTree::importLightroom()
{
/*
    MERGE, AND SAY SO BEFORE AND AFTER. An import that might replace someone's curated
    vocabulary is a frightening thing to click, so the dialog title says "Merge" and the
    result says what was added -- including "nothing new", which is the ordinary outcome
    of a second import and should not look like a failure.
*/
    const QString path = QFileDialog::getOpenFileName(
        this, "Merge Lightroom keywords", QString(),
        "Lightroom keyword export (*.csv *.txt);;All files (*)");
    if (path.isEmpty()) return;

    QStringList skipped;
    const int added = vocab->importLightroom(path, &skipped);

    QString msg;
    if (added > 0)
        msg = QString("Added %1 keyword%2.").arg(added).arg(added == 1 ? "" : "s");
    else msg = "Nothing new -- your keyword list already has everything in that file.";
    /*  A partial import is REPORTED rather than rounded up to success. The usual cause is
        a name that collides with a sibling under a different spelling, and the user can
        only act on it if they are told. */
    if (!skipped.isEmpty()) {
        msg += QString("\n\n%1 could not be added:\n    %2")
                   .arg(skipped.size())
                   .arg(skipped.mid(0, 8).join("\n    "));
        if (skipped.size() > 8) msg += "\n    ...";
    }
    QMessageBox::information(this, "Merge Lightroom keywords", msg);
}

void KeywordTree::exportLightroom()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Export keywords", "Keywords.csv",
        "Lightroom keyword export (*.csv);;All files (*)");
    if (path.isEmpty()) return;
    if (!path.contains('.')) path += ".csv";

    if (!vocab->exportLightroom(path)) {
        QMessageBox::warning(this, "Export keywords",
                             "Could not write \"" + path + "\".");
    }
    else if (G::popup) {
        G::popup->showPopup("Keywords exported.", 2500);
    }
}
