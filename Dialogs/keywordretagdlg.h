#ifndef KEYWORDRETAGDLG_H
#define KEYWORDRETAGDLG_H

#include <QDialog>
#include <QString>

/*
    "You renamed a keyword. Do you want the photographs changed too?"

    TWO SCOPES AND A DEFER, which is why this is a dialog and not a QMessageBox. Renaming
    or re-parenting a vocabulary node changes a NAME; the images that carry the old path
    still carry it, in their own files, and bringing them into line means rewriting a
    sidecar each. That is not a decision to make silently in either direction:

      o Doing it automatically -- what Lightroom does -- turns one keystroke into
        thousands of file writes with no warning.
      o Never doing it leaves the vocabulary and the library permanently disagreeing.

    So the user is told HOW MANY images each choice would touch, before anything is
    written. The counts come from Catalog::imagesUnderKeyword and include the whole
    SUBTREE, because a rename high in the tree changes every path beneath it.

    "NOT NOW" NEEDS NO BOOKKEEPING. Declining leaves those images carrying a path with no
    vocabulary node, which the dock shows as unfiled -- visible in the panel the user is
    already looking at, rather than recorded in a pending-work table they cannot see. A
    resumable deferred retag was considered and deliberately not built; see _ToDo.txt.
*/
class KeywordRetagDlg : public QDialog
{
    Q_OBJECT

public:
    enum Choice { NotNow, ThisFolder, Everywhere };

    /*  oldPath/newPath name the change; folderCount and catalogCount are what each scope
        would touch. A scope whose count is zero is offered but disabled with the count
        showing, rather than hidden: "0 images here" is an answer, and a button that
        appears and disappears teaches nothing. */
    KeywordRetagDlg(const QString &oldPath, const QString &newPath,
                    int folderCount, int catalogCount, QWidget *parent = nullptr);

    Choice choice() const { return result_; }

private:
    Choice result_ = NotNow;
};

#endif // KEYWORDRETAGDLG_H
