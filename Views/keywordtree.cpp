#include "Views/keywordtree.h"

#include "Datamodel/keywordvocab.h"
#include "Main/global.h"
#include "Metadata/keywordpaths.h"
#include "Utilities/inputdlg.h"
#include "Utilities/popup.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
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
