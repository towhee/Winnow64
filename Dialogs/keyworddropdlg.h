#ifndef KEYWORDDROPDLG_H
#define KEYWORDDROPDLG_H

#include <QDialog>
#include <QList>
#include <QString>

/*
    ONE MOVE THE DROP IS ABOUT TO MAKE. images is how many of the DROPPED images carry
    this keyword -- not how many exist in the library -- because that is the number this
    operation will actually rewrite.
*/
struct KeywordMove
{
    QString from;           // the checked keyword, as the catalog spells it
    QString to;             // the vocabulary path it becomes
    bool createsNode = false;   // "to" does not exist in the keyword list yet
    int images = 0;
};

/*
    "You dropped these images on a keyword. Here is what filing them will do."

    WHY A DIALOG AND NOT JUST THE DRAG. The drag says where; it cannot say how much. A
    drop onto a branch can rewrite ten thousand sidecars and add three nodes to a
    vocabulary the user has been curating by hand, and neither of those is visible in the
    gesture that asked for it. Every line here is a thing the drop is about to do that the
    drag did not show.

    THE NEW NODES ARE NAMED, AND THEY ARE NOT CREATED YET. Winnow does not change the
    keyword list on its own; a drop onto a branch is the user asking for a child, so the
    child is offered by name and made only if they accept. Cancelling leaves the
    vocabulary exactly as it was, not merely the files.

    THE SCOPE LINE IS THE ONE NOBODY WOULD OTHERWISE SEE. A drop can only rewrite images
    the datamodel has loaded -- MW::applyKeywordToPaths skips a path it cannot find a row
    for -- so in Folders scope the keyword survives in every other folder in the library,
    silently. Catalog scope loads the catalog and does not have that limit. Saying which
    one is running, and what it leaves behind, is the difference between a tidy that
    finished and one the user believes finished.

    NOT UNDOABLE, and it says so. The write goes to the images' own sidecars.
*/
class KeywordDropDlg : public QDialog
{
    Q_OBJECT

public:
    /*  moves is every checked keyword that has something to do. skipped names the rest,
        each already carrying its own reason ("Bunny - already filed there"), listed
        rather than dropped silently: the user checked those rows, and a dialog showing
        fewer moves than they checked would look like it had lost some.
        folderScope drives the scope paragraph; folderName is shown in it. */
    KeywordDropDlg(const QString &targetPath, const QList<KeywordMove> &moves,
                   const QStringList &skipped, bool folderScope,
                   const QString &folderName, QWidget *parent = nullptr);
};

#endif // KEYWORDDROPDLG_H
