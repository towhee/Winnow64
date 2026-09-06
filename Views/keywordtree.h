#ifndef KEYWORDTREE_H
#define KEYWORDTREE_H

#include <QTreeView>
#include <QWidget>

class KeywordVocab;
class QLineEdit;

/*
    THE VOCABULARY TREE -- the top zone of the Keywords dock.

    IT HAS NO CHECKBOXES AND IT NEVER TAGS A PHOTOGRAPH. That is the whole shape of the
    panel: this tree curates the keyword LIST (rename, re-parent, insert a parent, add,
    delete, import), and the zone below it assigns keywords to the SELECTION. The two jobs
    are separated because they have different consequences -- reorganising a vocabulary is
    cheap and reversible, retagging a thousand files is neither -- and because a checkbox
    that does both puts every reorganising gesture one mis-click from a mass edit.

    A DOT marks a node the current image carries. It is the one thing here that depends on
    what is selected, and it is a hint rather than a control: the chips below are what the
    user acts on.

    RENAME AND RE-PARENT ASK BEFORE TOUCHING FILES. The model changes the vocabulary
    immediately and emits pathChanged; the dock turns that into a confirmation naming how
    many images would be rewritten, in this folder and everywhere. Declining leaves the
    vocabulary renamed and the files alone, which is visible rather than hidden: those
    images' keywords no longer match a node, so they show as unfiled.
*/
class KeywordTree : public QTreeView
{
    Q_OBJECT

public:
    explicit KeywordTree(KeywordVocab *vocab, QWidget *parent = nullptr);

    /*  The filter box above the tree. Typing hides everything that does not match, by
        LEAF or by synonym, keeping ancestors of a match visible so the branch can be
        read. */
    void setFilterText(const QString &text);

signals:
    /*  A node was renamed or re-parented and its path changed. The dock decides what, if
        anything, to do about the images that carry the old path. */
    void pathChanged(const QString &oldPath, const QString &newPath);
    /*  The user asked to put this path on the selection (Enter, or double-click). */
    void assignRequested(const QString &path);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void renameSelected();
    void addChildOfSelected();
    void insertParentAboveSelected();
    void deleteSelected();
    void buildFromCatalog();
    QModelIndex currentNode() const;
    /*  Names already used beside a node, for InputDlg's doNotUse list. */
    QStringList siblingNames(const QModelIndex &parent, const QModelIndex &except) const;
    void applyFilter(const QModelIndex &parent, const QString &needle);

    KeywordVocab *vocab = nullptr;
    QString filterText;
};

#endif // KEYWORDTREE_H
