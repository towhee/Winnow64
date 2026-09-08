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
    delete, import), and the zone above it assigns keywords to the SELECTION. The two jobs
    are separated because they have different consequences -- reorganising a vocabulary is
    cheap and reversible, retagging a thousand files is neither -- and because a checkbox
    that does both puts every reorganising gesture one mis-click from a mass edit.

    A DOT marks a node the current image carries. It is the one thing here that depends on
    what is selected, and it is a hint rather than a control: the tags above are what the
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
    /*  Images were dropped on a keyword node: tag them with its path. The view does not
        touch a file -- the dock routes this to MW::applyKeywordsToSelection. */
    void assignToPaths(const QString &keywordPath, const QStringList &imagePaths);
    /*  A node was renamed or re-parented and its path changed. The dock decides what, if
        anything, to do about the images that carry the old path. */
    void pathChanged(const QString &oldPath, const QString &newPath);
    /*  The user asked to put this path on the selection (Enter, or double-click). */
    void assignRequested(const QString &path);
    /*  "Tidy flat keywords...": file the whole legacy flat vocabulary into the tree.
        Emitted rather than done here for the same reason as the rest of this class --
        it rewrites IMAGES, and this view never touches one. MW::tidyFlatKeywords. */
    void tidyRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    /*  Drag and drop, all at the VIEW rather than in the model. Two different things can
        be dropped here -- a keyword node (re-parent) and a set of images (tag them) --
        and only one of them is the model's business, so keeping both here avoids a model
        that knows what an image is. */
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void renameSelected();
    void addChildOfSelected();
    void insertParentAboveSelected();
    void deleteSelected();
    void buildFromCatalog();
    void importLightroom();
    void exportLightroom();
    QModelIndex currentNode() const;
    /*  Names already used beside a node, for InputDlg's doNotUse list. */
    QStringList siblingNames(const QModelIndex &parent, const QModelIndex &except) const;
    void applyFilter(const QModelIndex &parent, const QString &needle);

    KeywordVocab *vocab = nullptr;
    QString filterText;
};

#endif // KEYWORDTREE_H
