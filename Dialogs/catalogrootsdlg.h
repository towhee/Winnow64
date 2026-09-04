#ifndef CATALOGROOTSDLG_H
#define CATALOGROOTSDLG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>

#include "Main/catalogscope.h"

/*
    Which folders Winnow indexes in the background, and the button that starts a scan.
    See notes/Documentation.txt "Cataloguing Designated Folders (the Scope Table and Background Scanner)".

    IT IS ONE TABLE, NOT TWO LISTS. Include and exclude are the same kind of statement --
    a folder, and how far down it reaches -- so they belong in one place, read top to
    bottom: a library with one branch carved out is two rows. Two separate lists made the
    reader hold the relationship between them in their head, and made "include subfolders"
    look like a property of the catalog when it is a property of each row.

    FOUR COLUMNS. Include/Exclude, the folder path, whether the row reaches into
    subfolders, and how many images the folder holds ON DISK. Everything a rule says is
    visible without selecting it, which is the whole reason for a table rather than a list
    with the state hidden in buttons.

    THE IMAGES COLUMN COUNTS THE DISK, NOT THE INDEX, and that is what makes it worth a
    column. A count read out of the catalog would agree with the catalog by construction
    and could never say the one thing the user needs to know -- whether a scan is owed.
    Counted this way the includes less the excludes is what the catalog SHOULD hold, the
    status line below shows both numbers, and their difference is the outstanding work.

    WHY THIS IS NOT IN THE CATALOG DOCK ANY MORE. It was, and it took up the lower third
    of a panel whose whole job is asking questions. Editing the scope is CONFIGURATION --
    done once, revisited rarely -- while the panel above it is used constantly, so the
    two do not belong in the same column competing for the same vertical space. Everything
    else configurable in Winnow lives in Preferences, and this now opens from there
    (Preferences > Catalog) as well as from the dock's own Manage button.

    IT IS NOT MODAL. A scan takes minutes to hours, and this window is where its state is
    shown; a modal dialog would either block the app for the duration or have to be closed
    to watch it, and neither is a reasonable way to run a background job. Progress itself
    stays on the status-bar row, which is visible whether this is open or not.

    IT OWNS NOTHING. MW holds the scope table and its QSettings persistence (catalogScope)
    exactly as before; this is only the editor, and it emits what changed. That is
    deliberate: the scope is the ONE piece of catalog state that is user intent rather
    than derived data, so it must not depend on a widget's lifetime.
*/
class CatalogRootsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit CatalogRootsDlg(QWidget *parent = nullptr);

    /* Fill from MW, which owns the table. Not a user edit, so it does not signal back. */
    void setScope(const CatalogScope &scope);
    CatalogScope scope() const;

    /* Reflect whether a scan is running, so the button says Stop and the table cannot be
       edited underneath it. */
    void setScanning(bool scanning);

    /*  WHAT A SCAN WOULD DELETE, shown before it is pressed. MW works it out (it owns
        the catalog query) and hands it here, because forgetting tens of thousands of
        rows is not something a user should discover afterwards from a changed number.
        images == 0 means nothing would be forgotten and the note says nothing. */
    void setPendingForget(int images, int folders);

    /*  The image count for each row, in row order. -2 is "counting" (an ellipsis) and
        -1 is "not counted" (an em dash, for a folder that is not there or whose volume
        is unmounted) -- neither is a zero, because "none" is a different statement.
        MW works these out: the walk is off the GUI thread and it owns the scope. */
    void setRowCounts(const QVector<int> &counts);

    /* How many images and folders are indexed, shown so the user can see whether adding a
       folder actually achieved anything. */
    void setCatalogStatus(const QString &text);

signals:
    void scopeChanged(const CatalogScope &scope);
    void scanRequested();
    void stopScanRequested();

private:
    /* Build one table row. Widgets, not item flags: a combo says what the two states ARE
       without the user having to click to find out. */
    void addRow(const CatalogScopeEntry &e);
    /* Emit scopeChanged and refresh the note, unless we are repopulating. */
    void noteEdit();
    /* Say, in place, when a row cannot do anything -- an exclude that lies outside every
       included tree excludes nothing, and silently accepting it would leave the user
       believing they had carved out a branch that was never going to be scanned. */
    void updateNote();

    QTableWidget *table = nullptr;
    QPushButton *addBtn = nullptr;
    QPushButton *removeBtn = nullptr;
    QPushButton *scanBtn = nullptr;
    QPushButton *closeBtn = nullptr;
    QLabel *noteLabel = nullptr;
    QLabel *statusLabel = nullptr;
    bool scanning = false;
    bool populating = false;
    int pendingForgetImages = 0;
    int pendingForgetFolders = 0;
};

#endif // CATALOGROOTSDLG_H
