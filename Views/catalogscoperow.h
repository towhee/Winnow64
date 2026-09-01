#ifndef CATALOGSCOPEROW_H
#define CATALOGSCOPEROW_H

#include <QToolButton>
#include <QString>

/*
    THE CATALOG AS A SCOPE, SHOWN WHERE SCOPE IS CHOSEN.

    A full-width checkable row that sits ABOVE the Folders tree and above the
    Bookmarks list. Selecting it means "look at the whole catalog" the same way
    clicking a folder means "look at that folder" -- which is the point: scope
    is chosen in one place, and filtering then means the same thing in both.
    Before this the catalog was reachable only from inside the Find dock, so a
    user had to know it existed to find it.

    IT IS NOT A ROW OF EITHER TREE, deliberately. The Folders tree is a
    QTreeView over a QFileSystemModel, which is filesystem-backed and cannot
    carry a synthetic row without overriding its node structure; Bookmarks is a
    QTreeWidget where a real item would be draggable, deletable and
    indistinguishable from a bookmark. A widget above each tree reads as the
    first row, belongs to neither model, and interferes with neither.

    TWO INSTANCES, ONE FACT. MW owns one per dock and both mirror G::scope;
    they do not talk to each other. Clicking either one asks MW to change scope,
    and MW pushes the result back to both, so a scope change from any of the
    three entry points (either row, or the Find dock's buttons) leaves all of
    them agreeing.
*/
class CatalogScopeRow : public QToolButton
{
    Q_OBJECT

public:
    explicit CatalogScopeRow(QWidget *parent = nullptr);

    /*  The catalogued image count shown beside the name. Passing -1 means "not
        known yet" and shows the bare name -- a count of 0 and an unopened index
        are different facts, and the panel says so rather than implying the
        library is empty. */
    void setImageCount(qint64 count);

    // Re-apply the palette-derived stylesheet; MW::setBackgroundShade calls it.
    void updateStyle();

private:
    qint64 imageCount = -1;
    void refreshText();
};

#endif // CATALOGSCOPEROW_H
