#ifndef CATALOGROOTSDLG_H
#define CATALOGROOTSDLG_H

#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>

/*
    Which folders Winnow indexes in the background, and the button that starts a scan.
    See notes/Documentation.txt "Cataloguing Designated Roots (the Background Scanner)".

    WHY THIS IS NOT IN THE CATALOG DOCK ANY MORE. It was, and it took up the lower third
    of a panel whose whole job is asking questions. Editing the root list is CONFIGURATION
    -- done once, revisited rarely -- while the panel above it is used constantly, so the
    two do not belong in the same column competing for the same vertical space. Everything
    else configurable in Winnow lives in Preferences, and this now opens from there
    (Preferences > Catalog) as well as from the dock's own Manage button.

    IT IS NOT MODAL. A scan takes minutes to hours, and this window is where its state is
    shown; a modal dialog would either block the app for the duration or have to be closed
    to watch it, and neither is a reasonable way to run a background job. Progress itself
    stays on the status-bar row, which is visible whether this is open or not.

    IT OWNS NOTHING. MW holds the root list and its QSettings persistence (catalogRoots,
    catalogRootsRecurse) exactly as before; this is only the editor, and it emits what
    changed. That is deliberate: the root list is the ONE piece of catalog state that is
    user intent rather than derived data, so it must not depend on a widget's lifetime.
*/
class CatalogRootsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit CatalogRootsDlg(QWidget *parent = nullptr);

    /* Fill from MW, which owns the list. Not a user edit, so it does not signal back. */
    void setRoots(const QStringList &roots, bool recurse);
    QStringList roots() const;
    bool rootsRecurse() const;

    /* Reflect whether a scan is running, so the button says Stop and the list cannot be
       edited underneath it. */
    void setScanning(bool scanning);

    /* How many images and folders are indexed, shown so the user can see whether adding a
       folder actually achieved anything. */
    void setCatalogStatus(const QString &text);

signals:
    void rootsChanged(const QStringList &roots, bool recurse);
    void scanRequested();
    void stopScanRequested();

private:
    QListWidget *rootList = nullptr;
    QCheckBox *recurseBox = nullptr;
    QPushButton *addRootBtn = nullptr;
    QPushButton *removeRootBtn = nullptr;
    QPushButton *scanBtn = nullptr;
    QPushButton *closeBtn = nullptr;
    QLabel *statusLabel = nullptr;
    bool scanning = false;
};

#endif // CATALOGROOTSDLG_H
