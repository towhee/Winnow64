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

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

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
    setItemDelegate(new AppliedDotDelegate(this));

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

void KeywordTree::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(kVocabNodeMime) || event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        event->ignore();
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

    if (event->mimeData()->hasFormat(kVocabNodeMime)) {
        const QByteArray data = event->mimeData()->data(kVocabNodeMime);
        for (const QByteArray &raw : data.split('\n')) {
            const QModelIndex src = vocab->indexForPath(QString::fromUtf8(raw));
            if (!src.isValid()) continue;
            /*  Onto itself, onto its own parent (a move to where it already is) or onto
                one of its own descendants: all no-ops or cycles. */
            if (src == target || src.parent() == target) { event->ignore(); return; }
            if (vocab->wouldCycle(src, target)) { event->ignore(); return; }
        }
        event->acceptProposedAction();
        return;
    }

    /*  Images can only land ON a keyword, never between two of them: there is no
        meaning to "tag these with the gap above Heron". */
    if (event->mimeData()->hasUrls() && target.isValid()) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void KeywordTree::dropEvent(QDropEvent *event)
{
    const QModelIndex target = indexAt(event->position().toPoint());

    if (event->mimeData()->hasFormat(kVocabNodeMime)) {
        const QByteArray data = event->mimeData()->data(kVocabNodeMime);
        for (const QByteArray &raw : data.split('\n')) {
            const QModelIndex src = vocab->indexForPath(QString::fromUtf8(raw));
            if (!src.isValid()) continue;
            if (!vocab->reparent(src, target)) {
                /*  reparent refuses a name already taken under the new parent. Merging
                    the two is a real operation and a bigger one than a drag -- it has to
                    decide what happens to both nodes' images -- so it is named rather
                    than attempted. */
                QMessageBox::warning(this, "Move keyword",
                    QString("Could not move \"%1\" there.\n\n"
                            "A keyword of that name is already inside the destination.")
                        .arg(keywordLeafOf(QString::fromUtf8(raw))));
            }
        }
        if (target.isValid()) setExpanded(target, true);
        event->acceptProposedAction();
        return;
    }

    if (event->mimeData()->hasUrls() && target.isValid()) {
        QStringList images;
        for (const QUrl &u : event->mimeData()->urls())
            if (u.isLocalFile()) images << u.toLocalFile();
        const QString path = target.data(KeywordVocab::PathRole).toString();
        if (!images.isEmpty() && !path.isEmpty()) emit assignToPaths(path, images);
        event->acceptProposedAction();
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
