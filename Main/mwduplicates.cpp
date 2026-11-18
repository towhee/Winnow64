#include "Main/mainwindow.h"
#include "Cache/duplicates.h"
#include "Datamodel/duplicateok.h"
#include "Datamodel/rowselection.h"
#include "Views/iconview.h"
#include "Views/iconviewdelegate.h"
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
      o WHICH ones are twins: the current image's group has its badges inverted
        (updateDuplicateTwins), the badge tooltip lists the twins' paths, and Show
        Duplicates of This Image (showDuplicateTwins) filters to them.
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
    updateDuplicateTwins();
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
    Duplicate Okay and Show Duplicates of This Image. Enabled when a selected image is in
    a duplicates group; Okay is checked when every such image is okayed. The Edit menu's
    are greyed with the reason otherwise -- the tooltip and the disabled-shortcut
    feedback both read it; the thumbnail context menu's copies are simply not there.
*/
    if (!duplicateOkAction || !showDuplicateTwinsAction || !dm) return;
    auto apply = [this](bool enabled, bool checked, const QString &why) {
        const QString okTip =
            tr("Okay the selected duplicates: the duplicates check ignores them and "
               "their number badge turns purple.\nChoose it again to undo.");
        const QString showTip =
            tr("Show only the selected duplicates and their twins (Filters > "
               "Duplicates of).\nUncheck that filter to show all images again.");
        for (QAction *a : {duplicateOkAction, showDuplicateTwinsAction}) {
            a->setProperty("disabledReason", enabled ? QString() : why);
            a->setToolTip(enabled ? (a == duplicateOkAction ? okTip : showTip)
                                  : tr("Unavailable: %1.").arg(why));
            a->setEnabled(enabled);
        }
        duplicateOkAction->setChecked(checked);
        duplicateOkContextAction->setChecked(checked);
        duplicateOkContextAction->setVisible(enabled);
        showDuplicateTwinsContextAction->setVisible(enabled);
        /* The shortcut column, which a context copy has no shortcut of its own to show. */
        auto mirror = [](QAction *copy, const QAction *a) {
            const QString keys = a->shortcut().toString(QKeySequence::NativeText);
            copy->setText(keys.isEmpty() ? a->text() : a->text() + "\t" + keys);
            copy->setToolTip(a->toolTip());
        };
        mirror(duplicateOkContextAction, duplicateOkAction);
        mirror(showDuplicateTwinsContextAction, showDuplicateTwinsAction);
    };

    if (Duplicates::instance().status().isEmpty()) {
        apply(false, false, tr("there are no duplicates in the Library (File > Manage "
                               "Catalog > Duplicates finds them)"));
        return;
    }
    const QModelIndexList rows = RowSelection::Rows(dm->selectionModel);
    if (rows.size() > kMaxOkStateRows) {
        apply(true, false, QString());
        return;
    }
    int inGroup = 0, okayed = 0;
    for (const QModelIndex &idx : rows) {
        const int st = idx.sibling(idx.row(), G::DuplicateColumn).data(Qt::EditRole).toInt();
        if (st == DuplicateStatus::None) continue;
        ++inGroup;
        if (DuplicateOk::IsOk(idx.data(G::KeyRole).toString())) ++okayed;
    }
    if (inGroup == 0) {
        apply(false, false, tr("no selected image is a Library duplicate (red or purple "
                               "number badge)"));
        return;
    }
    apply(true, okayed == inGroup, QString());
}

void MW::updateDuplicateTwins()
{
/*
    The current image's duplicates group into the thumbnail delegates, which invert the
    badges of every image in it (IconViewDelegate::setDuplicateTwinGroup). A view only
    repaints the old and new current cells by itself, so a CHANGE of group repaints the
    views; moving between images in no group costs one hash lookup.
*/
    int group = 0;
    if (dm && dm->sf && dm->selectionModel && !Duplicates::instance().status().isEmpty()) {
        const QModelIndex cur = dm->selectionModel->currentIndex();
        if (cur.isValid()) {
            const QString key = cur.sibling(cur.row(), 0).data(G::KeyRole).toString();
            group = Duplicates::instance().infoOf(key).group;
        }
    }
    if (group == duplicateTwinGroup) return;
    duplicateTwinGroup = group;
    for (IconView *v : {thumbView, gridView}) {
        if (!v || !v->iconViewDelegate) continue;
        v->iconViewDelegate->setDuplicateTwinGroup(group);
        v->viewport()->update();
    }
}

void MW::showDuplicateTwins()
{
/*
    Show only the selected duplicates and their twins: the Filters "Duplicates of" item,
    in ONE filterChange that replaces every other filter, as a map pin click does -- it
    means "show me these". Held as keys (DataModel::setDuplicateTwinKeys), so a sort or
    an insert cannot move it onto other images.

    A twin that is not loaded -- in another folder, with a folder open -- cannot be
    shown; the popup says how many and where to find them (the badge tooltip lists them).

    NOT WHILE A LOAD IS RUNNING: the load's filter build resets the checks when it lands
    (MW::applyMapPinFilter).
*/
    if (G::isLogger) G::log("MW::showDuplicateTwins");
    if (!dm || !filters) return;
    if (G::isLoadRunning || G::isModifyingDatamodel || filters->buildingFilters) {
        if (G::popup)
            G::popup->showPopup(tr("Still loading. Try again when it has finished."), 2500);
        return;
    }
    const Duplicates &d = Duplicates::instance();
    QSet<QString> keys;
    QString firstName;
    int chosen = 0;
    for (const QModelIndex &idx : RowSelection::Rows(dm->selectionModel)) {
        const QString key = idx.data(G::KeyRole).toString();
        if (d.infoOf(key).group <= 0 || keys.contains(key)) continue;
        if (chosen++ == 0) firstName = QFileInfo(key).fileName();
        keys.insert(key);
        for (const QString &twin : d.twinsOf(key)) keys.insert(twin);
    }
    if (keys.isEmpty()) {
        if (G::popup)
            G::popup->showPopup(tr("None of the selected images is a Library duplicate."),
                                2000);
        updateDuplicateOkAction();
        return;
    }
    int loaded = 0;
    for (const QString &k : std::as_const(keys))
        if (dm->rowFromKey(k) >= 0) ++loaded;

    if (!G::allMetadataAttempted) loadEntireMetadataCache("FilterChange");
    uncheckAllFilters();
    filters->searchString = "";
    dm->searchStringChange("");
    dm->setDuplicateTwinKeys(keys);
    const QString label = chosen == 1 ? tr("%1 and its twins").arg(firstName)
                                      : tr("%n selected duplicates and their twins", "",
                                           chosen);
    filters->setDuplicateTwinsItem(loaded, label);
    filterChange("MW::showDuplicateTwins");

    const int elsewhere = int(keys.size()) - loaded;
    if (elsewhere > 0 && G::popup)
        G::popup->showPopup(tr("%n twin(s) are not in what is loaded. Hover a red badge "
                               "to see where they are, or browse the Library to show "
                               "them.", "", elsewhere), 4000);
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
