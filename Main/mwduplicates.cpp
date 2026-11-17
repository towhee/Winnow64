#include "Main/mainwindow.h"
#include "Cache/duplicates.h"
#include "Datamodel/duplicateok.h"
#include "Datamodel/rowselection.h"
#include "Dialogs/catalogrootsdlg.h"
#include "Main/catalogenumerate.h"

#include <QStandardPaths>

/*
    LIBRARY DUPLICATES, MainWindow's side (Cache/duplicates.h is the engine).

      o The Manage Catalog Duplicates button checks the whole Library
        (startLibraryDuplicatesCheck); the status bar's "Duplicates" row and the dialog
        show the progress, and the dialog's Catalog Information says what was found.
      o New and changed images are checked as they are catalogued -- the engine hears it
        from the catalog itself; nothing here asks.
      o The answer reaches the grid through DataModel::setDuplicateStatus: the red or
        purple number badge (IconViewDelegate), the Duplicate column, and the red ones in
        Filters > Duplicates found (G::CompareColumn).
      o Duplicate Okay (toggleDuplicateOk) is the user's "these are meant to be here".
*/

namespace {
/*  The selection is walked to set the action's state; past this many rows it is not --
    the action stays enabled and decides when it is triggered. */
constexpr int kMaxOkStateRows = 5000;
}

void MW::createDuplicates()
{
    if (G::isLogger) G::log("MW::createDuplicates");
    /* Test runs never touch the user's okayed duplicates, as with picks. */
    if (!QStandardPaths::isTestModeEnabled()) DuplicateOk::Open();

    Duplicates &d = Duplicates::instance();
    connect(&d, &Duplicates::statusChanged, this, &MW::refreshDuplicateStatus);
    connect(&d, &Duplicates::progress, this, [this](const DuplicateProgress &p) {
        if (progress) {
            const QLocale loc;
            QString tip = QString("Checking for duplicates: %1 of %2 candidates")
                              .arg(loc.toString(p.done), loc.toString(p.total));
            tip += " · " + catalogFormatEta(p.etaSecs) + (p.etaSecs < 0 ? "" : " left");
            if (p.paused) tip = "Checking for duplicates: paused while a folder loads\n" + tip;
            if (p.full) tip += "\nStop it with the Duplicates button in File > Manage Catalog.";
            progress->setRowToolTip(progressDuplicatesRow, tip);
            if (p.total > 0)
                progress->updateProgress(progressDuplicatesRow, p.done, p.total,
                                         G::progressDuplicatesColor);
        }
        if (catalogRootsDlg) catalogRootsDlg->setDuplicatesProgress(p);
    });
    connect(&d, &Duplicates::finished, this, [this](bool full, bool cancelled) {
        if (progress) {
            progress->clearProgress(progressDuplicatesRow);
            progress->setRowToolTip(progressDuplicatesRow, QString());
        }
        if (!full) return;
        if (catalogRootsDlg) {
            catalogRootsDlg->setDuplicatesChecking(false);
            catalogRootsDlg->setCatalogStatus(catalogStatusText());
        }
        if (cancelled && G::popup)
            G::popup->showPopup(tr("Duplicates check stopped. What it checked is kept; "
                                   "press Duplicates again to finish."), 3000);
    });
    d.start();
}

void MW::startLibraryDuplicatesCheck()
{
    if (G::isLogger) G::log("MW::startLibraryDuplicatesCheck");
    Duplicates &d = Duplicates::instance();
    if (d.isCheckingAll()) return;
    d.checkAll();
    /* Shown at once: finding the candidates is one query, but on a big Library it is a
       noticeable one, and the row says the press was heard. */
    if (progress) {
        progress->setRowToolTip(progressDuplicatesRow, tr("Finding duplicate candidates..."));
        progress->showRow(progressDuplicatesRow, true);
    }
    if (catalogRootsDlg) {
        catalogRootsDlg->setDuplicatesChecking(true);
        catalogRootsDlg->setCatalogStatus(catalogStatusText());
    }
}

void MW::refreshDuplicateStatus()
{
/*
    The engine's answer into the model, then everything that shows it. The column and
    the badge read the side table at paint time, so a repaint is enough for them; the
    "Duplicates found" category counts G::CompareColumn, so it is rebuilt -- and the
    proxy refiltered only when that category is actually filtering.
*/
    if (G::isLogger) G::log("MW::refreshDuplicateStatus");
    if (!dm) return;
    QHash<QString, int> byPath;
    const QHash<QString, DuplicateInfo> &status = Duplicates::instance().status();
    byPath.reserve(status.size());
    for (auto it = status.cbegin(); it != status.cend(); ++it)
        byPath.insert(it.key(), it->status);
    const bool changed = dm->setDuplicateStatus(byPath);

    if (catalogRootsDlg && catalogRootsDlg->isVisible())
        catalogRootsDlg->setCatalogStatus(catalogStatusText());
    updateDuplicateOkAction();
    if (!changed) return;

    if (thumbView) thumbView->viewport()->update();
    if (gridView) gridView->viewport()->update();
    if (tableView) tableView->viewport()->update();

    if (G::isLoadRunning || G::isModifyingDatamodel || !buildFilters || !filters) return;
    /*  runSync: a filterChange may follow, and an async rebuild racing it is a
        use-after-free (BuildFilters::updateCategory). */
    buildFilters->updateCategory(BuildFilters::CompareEdit, BuildFilters::NoAfterAction,
                                 /*runSync*/ true);
    if (filters->isAnyCatItemChecked(filters->compare))
        filterChange("MW::refreshDuplicateStatus");
}

void MW::updateDuplicateOkAction()
{
/*
    Enabled when a selected image is in a duplicates group; checked when every such image
    is okayed. Greyed with the reason otherwise -- the tooltip and the disabled-shortcut
    feedback both read it.
*/
    if (!duplicateOkAction || !dm) return;
    auto gate = [this](bool enabled, const QString &reason) {
        duplicateOkAction->setProperty("disabledReason", reason);
        duplicateOkAction->setEnabled(enabled);
    };
    const QString enabledTip =
        tr("Okay the selected duplicates: the duplicates check ignores them and their "
           "number badge turns purple.\nChoose it again to undo.");

    if (Duplicates::instance().status().isEmpty()) {
        const QString why = tr("there are no duplicates in the Library (File > Manage "
                               "Catalog > Duplicates finds them)");
        duplicateOkAction->setChecked(false);
        duplicateOkAction->setToolTip(tr("Unavailable: %1.").arg(why));
        gate(false, why);
        return;
    }
    const QModelIndexList rows = RowSelection::Rows(dm->selectionModel);
    if (rows.size() > kMaxOkStateRows) {
        duplicateOkAction->setChecked(false);
        duplicateOkAction->setToolTip(enabledTip);
        gate(true, QString());
        return;
    }
    int inGroup = 0, okayed = 0;
    for (const QModelIndex &idx : rows) {
        const int st = idx.sibling(idx.row(), G::DuplicateColumn).data(Qt::EditRole).toInt();
        if (st == DuplicateStatus::None) continue;
        ++inGroup;
        if (DuplicateOk::IsOk(idx.data(G::KeyRole).toString())) ++okayed;
    }
    duplicateOkAction->setChecked(inGroup > 0 && okayed == inGroup);
    if (inGroup == 0) {
        const QString why = tr("no selected image is a Library duplicate (red or purple "
                               "number badge)");
        duplicateOkAction->setToolTip(tr("Unavailable: %1.").arg(why));
        gate(false, why);
        return;
    }
    duplicateOkAction->setToolTip(enabledTip);
    gate(true, QString());
}

void MW::toggleDuplicateOk()
{
/*
    The selected images that are in a duplicates group are okayed -- or, when every one
    of them is okayed already, un-okayed, so the action twice is undone. An okayed image
    is left out of the check (Cache/duplicateclassify.h), so okaying either of a pair
    clears both.
*/
    if (G::isLogger) G::log("MW::toggleDuplicateOk");
    if (!dm) return;
    QStringList paths;
    for (const QModelIndex &idx : RowSelection::Rows(dm->selectionModel)) {
        const int st = idx.sibling(idx.row(), G::DuplicateColumn).data(Qt::EditRole).toInt();
        if (st != DuplicateStatus::None) paths << idx.data(G::KeyRole).toString();
    }
    if (paths.isEmpty()) {
        if (G::popup)
            G::popup->showPopup(tr("None of the selected images is a Library duplicate."),
                                2000);
        updateDuplicateOkAction();
        return;
    }
    if (!DuplicateOk::IsOpen()) {
        if (G::popup)
            G::popup->showPopup(tr("Okayed duplicates cannot be saved: the user data "
                                   "file is unavailable."), 3000);
        updateDuplicateOkAction();
        return;
    }
    bool allOk = true;
    for (const QString &p : std::as_const(paths))
        if (!DuplicateOk::IsOk(p)) { allOk = false; break; }
    for (const QString &p : std::as_const(paths)) DuplicateOk::Set(p, !allOk);
    Duplicates::instance().requestRebuild();
    updateDuplicateOkAction();
    if (G::popup)
        G::popup->showPopup(allOk ? tr("%n duplicate(s) no longer okayed.", "", paths.size())
                                  : tr("%n duplicate(s) okayed.", "", paths.size()),
                            1500);
}
