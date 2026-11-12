#ifndef CATALOGROOTSDLG_H
#define CATALOGROOTSDLG_H

#include <QDialog>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Main/catalogscope.h"

struct CatalogScanProgress;
class CollapsibleSections;

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
    two do not belong in the same column competing for the same vertical space. It opens
    from the dock's own Manage button and File > Manage Catalog.

    IT IS NOT MODAL. A scan takes minutes to hours, and this window is where its state is
    shown; a modal dialog would either block the app for the duration or have to be closed
    to watch it, and neither is a reasonable way to run a background job. Progress is on
    the status-bar row, visible whether this is open or not, and here too -- a bar, the
    rate and the time left -- because this is where someone comes to ask "how long".

    IT OWNS NOTHING. MW holds the scope table and its QSettings persistence (catalogScope)
    exactly as before; this is only the editor, and it hands back what changed. That is
    deliberate: the scope is the ONE piece of catalog state that is user intent rather
    than derived data, so it must not depend on a widget's lifetime.

    THE TABLE IS A DRAFT. Append, Remove and the row controls change the table and
    nothing else -- no count, no "Remove from Catalog?", no scan. Each of those used to
    fire on every edit, so building a scope of five rows was five recounts and a
    confirmation in the middle, and nothing said the folder just added was not being
    catalogued yet. Now a highlighted line under the table says the edits are pending
    and that Scan applies them; applying (forget what the table disowns, then scan what
    it gained) is ONE act, requested by applyAndScanRequested. Closing the window with
    edits pending applies them the same way, because a window that silently threw an
    edit away would be worse than one that asked for nothing.
*/
class CatalogRootsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit CatalogRootsDlg(QWidget *parent = nullptr);

    /* Fill from MW, which owns the table. Not a user edit, so it does not signal back;
       it is also what the draft is compared against (hasPendingEdits). */
    void setScope(const CatalogScope &scope);
    CatalogScope scope() const;
    /* Does the table differ from what MW last committed? Row order does not count. */
    bool hasPendingEdits() const;

    /* Reflect whether a scan is running, so the button says Stop and the table cannot be
       edited underneath it. what names a scan of only part of the scope ("Photos"). */
    void setScanning(bool scanning, const QString &what = QString());
    /* Show the progress block busy with this text NOW, painted before returning: MW
       calls it in front of synchronous work (the reconcile) the user is waiting on. */
    void showBusy(const QString &text);

    /*  The image count for each row, in row order. -2 is "counting" (an ellipsis) and
        -1 is "not counted" (an em dash, for a folder that is not there or whose volume
        is unmounted) -- neither is a zero, because "none" is a different statement.
        MW works these out: the walk is off the GUI thread and it owns the scope. */
    void setRowCounts(const QVector<int> &counts);

    /* How many images and folders are indexed, shown so the user can see whether adding a
       folder actually achieved anything. */
    void setCatalogStatus(const QString &text);

    /*  Where the running scan is: a bar, and one line with the counts, the rate and the
        time left, or why it is paused. Hidden again by setScanning(false). */
    void setScanProgress(const CatalogScanProgress &p);

signals:
    /* Scan with the table as it stands now committed: the edits are applied first. */
    void applyAndScanRequested(const CatalogScope &scope);
    /* Scan with nothing pending -- the whole scope, as it is. */
    void scanRequested();
    void stopScanRequested();
    /* The status line's "See which" link. */
    void diagnosticsRequested();

protected:
    /* Every way the window closes -- Close, Esc, the title bar -- arrives here, so this
       is where pending edits are applied rather than lost. */
    void done(int r) override;

private:
    /* A collapsible section's group box, styled as ExportDlg's; inner receives its
       body layout. */
    QGroupBox *makeSection(const QString &title, QVBoxLayout *&inner);
    /* One-line summaries shown when a section is closed. */
    QString scopeSummary() const;
    QString infoSummary() const;
    /* Build one table row. Widgets, not item flags: a combo says what the two states ARE
       without the user having to click to find out. */
    void addRow(const CatalogScopeEntry &e);
    /* Refresh the note, the pending line and the counts, unless we are repopulating.
       Emits nothing: the table is a draft until Scan. */
    void noteEdit();
    /* Show or hide the pending line, and mark Scan as the button to press. */
    void updatePending();
    /* The Images column from lastCounts: a row MW has not counted (new or edited since
       the last commit) shows an em dash. */
    void refreshCounts();
    /* One row's identity for comparing drafts and looking up counts. */
    static QString entryKey(const CatalogScopeEntry &e);
    /* Say, in place, when a row cannot do anything -- an exclude that lies outside every
       included tree excludes nothing, and silently accepting it would leave the user
       believing they had carved out a branch that was never going to be scanned. */
    void updateNote();
    /* Size the table to its rows (up to a cap), then the window to its contents. */
    void fitTableHeight();
    /* Resize the window's height to what the layout needs; deferred and coalesced. */
    void fitToContents();
    bool fitPending = false;

    QTableWidget *table = nullptr;
    QPushButton *addBtn = nullptr;
    QPushButton *removeBtn = nullptr;
    QPushButton *scanBtn = nullptr;
    QPushButton *closeBtn = nullptr;
    QPushButton *helpBtn = nullptr;
    CollapsibleSections *sections = nullptr;
    QLabel *noteLabel = nullptr;
    QLabel *pendingLabel = nullptr;    // "Table changed -- click Scan", while edits pend
    QLabel *statusLabel = nullptr;
    QWidget *scanBlock = nullptr;      // scanBar + scanLabel, shown together
    QProgressBar *scanBar = nullptr;
    QLabel *scanLabel = nullptr;
    bool scanning = false;
    bool populating = false;
    bool applying = false;             // done() is applying; a nested close must not
    CatalogScope committed;            // what MW holds; the draft is compared with it
    QHash<QString, int> lastCounts;    // entryKey -> image count, from setRowCounts
    QString scanWhat;                  // the part being scanned, empty for all of it
};

#endif // CATALOGROOTSDLG_H
