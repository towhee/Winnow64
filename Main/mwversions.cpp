#include "Main/mainwindow.h"
#include "Utilities/versionkey.h"
#include "Utilities/fileops.h"
#include "Metadata/versions.h"
#include "Cache/devpreviewcache.h"

/*
    VERSIONS (VIRTUAL COPIES) IN THE GRID.

    A version is its own row, keyed path + "/#v" + id (Utilities/versionkey.h), grouped
    right after its master and collapsed behind it by default. Its values come from the
    master's metadata with its own name, rating, label, pick and develop state laid over
    it (DataModel::fillVersionRow); the versions themselves live in the master's sidecar
    (Metadata/versions.h). See "Versions (Virtual Copies)" in notes/Documentation.txt.

    WHEN ROWS ARE ADDED. Only through applyModelChange, and never while a load is still
    reading metadata: MetaRead and ImageCache address the model by row number, and an
    insert mid-load would move rows under reads in flight. So the whole set is reconciled
    once when the load completes (metadataComplete), and again -- coalesced -- whenever a
    master's sidecar is re-read after that (DataModel::versionsChanged).
*/

void MW::scheduleVersionReconcile()
{
    if (versionReconcilePending) return;
    versionReconcilePending = true;
    QTimer::singleShot(0, this, [this] {
        versionReconcilePending = false;
        reconcileVersionRows("DataModel::versionsChanged");
    });
}

void MW::reconcileVersionRows(const QString &src, bool atLoadEnd)
{
    if (G::isLogger) G::log("MW::reconcileVersionRows", src);
    /*  NOT UNTIL THE LOAD HAS FINISHED -- G::isLoadRunning. This once tested
        G::isModifyingDatamodel alone, which a replacing load's stop() clears at once, so
        a master read mid-fill ran applyModelChange mid-fill: new instance, readers
        restarted while loadingModel still refused their icons, and those thumbnails never
        painted. MW::metadataComplete clears isLoadRunning and then calls this itself,
        with atLoadEnd because an Add load's isModifyingDatamodel is still set there. */
    if (!dm || G::stop || dm->loadingModel || G::isLoadRunning) return;
    if (!atLoadEnd && G::isModifyingDatamodel) return;

    dm->sf->setShowAllVersions(showAllVersionsAction && showAllVersionsAction->isChecked());

    QStringList add, remove;
    dm->versionRowChanges(add, remove);
    if (!add.isEmpty() || !remove.isEmpty()) {
        /*  If the current image is a version that is going, land on its master rather
            than wherever the proxy's current index falls. */
        if (remove.contains(dm->currentKey))
            dm->currentKey = VersionKey::sourceOf(dm->currentKey);
        applyModelChange(add, remove, "versions: " + src);
    }

    /*  Refill the version rows that stayed: a master re-read (a rating written by
        another application, a version renamed in another window) changes what they
        show. Rows applyModelChange just inserted were filled there. */
    for (const QString &k : dm->versionRowKeys())
        if (!add.contains(k)) dm->fillVersionRow(dm->rowFromKey(k));
}

void MW::setVersionsExpanded(const QString &masterKey, bool expanded)
{
    if (G::isLogger) G::log("MW::setVersionsExpanded", masterKey);
    if (!dm || masterKey.isEmpty()) return;
    if (dm->sf->versionsExpanded(masterKey) == expanded
        && !dm->sf->showAllVersions()) return;

    /*  Collapsing hides the group's versions; if the current image is one of them, move
        to the master FIRST, so the selection lands on the image the user was looking
        at rather than on whatever row the proxy leaves current. */
    if (!expanded && VersionKey::sourceOf(dm->currentKey) == masterKey
        && VersionKey::isVersion(dm->currentKey)) {
        const QModelIndex m = dm->proxyIndexFromKey(masterKey);
        if (m.isValid()) sel->select(m, Qt::NoModifier, "MW::setVersionsExpanded");
    }
    dm->sf->setVersionsExpanded(masterKey, expanded);
    filterChange("MW::setVersionsExpanded");
}

void MW::toggleVersionsExpanded(const QString &key)
{
    const QString master = VersionKey::sourceOf(key);
    if (!dm) return;
    /*  With Show All Versions on every group is open; a click on one badge turns the
        global switch off and leaves only that group open, which is what the click
        visibly asks for. */
    if (dm->sf->showAllVersions()) {
        dm->sf->clearVersionsExpanded();
        dm->sf->setShowAllVersions(false);
        if (showAllVersionsAction) showAllVersionsAction->setChecked(false);
        if (isSettings) settings->setValue("showAllVersions", false);
        dm->sf->setVersionsExpanded(master, true);
        filterChange("MW::toggleVersionsExpanded");
        return;
    }
    setVersionsExpanded(master, !dm->sf->versionsExpanded(master));
}

void MW::setShowAllVersions(bool show)
{
    if (G::isLogger) G::log("MW::setShowAllVersions", show ? "on" : "off");
    if (isSettings) settings->setValue("showAllVersions", show);
    if (showAllVersionsAction && showAllVersionsAction->isChecked() != show)
        showAllVersionsAction->setChecked(show);
    if (!dm) return;
    /*  Turning it off hides every version whose own group is closed. If the current
        image is one, move to its master first (as setVersionsExpanded does). */
    if (!show && VersionKey::isVersion(dm->currentKey)) {
        const QString master = VersionKey::sourceOf(dm->currentKey);
        dm->sf->setShowAllVersions(false);
        const bool staysVisible = dm->sf->versionsExpanded(master);
        dm->sf->setShowAllVersions(true);       // still shown until filterChange
        if (!staysVisible) {
            const QModelIndex m = dm->proxyIndexFromKey(master);
            if (m.isValid()) sel->select(m, Qt::NoModifier, "MW::setShowAllVersions");
        }
    }
    dm->sf->setShowAllVersions(show);
    filterChange("MW::setShowAllVersions");
}

/* ---------------------------------------------------------------------------------
   Version operations (Develop > Versions, thumbnail context menu)

   Every one of them writes the MASTER's sidecar and never the image file. The flush
   comes first: a debounced Develop edit landing after the sidecar was rewritten would
   put a stale recipe back.
   --------------------------------------------------------------------------------- */

namespace {

// the row's rating / label / pick as a version record holds them
void versionValuesFromRow(DataModel *dm, int dmRow, ImageVersion &v)
{
    v.rating = dm->index(dmRow, G::RatingColumn).data().toString().toInt();
    v.label = dm->index(dmRow, G::LabelColumn).data().toString();
    const QString pick = dm->index(dmRow, G::PickColumn).data().toString();
    v.pick = (pick == "Unpicked") ? QString() : pick;      // "" is unpicked
}

QImage jpgImage(const QByteArray &jpg)
{
    QImage im;
    if (!jpg.isEmpty()) im.loadFromData(jpg, "JPG");
    return im;
}

} // namespace

void MW::rereadVersionMasters(const QStringList &masters, const QString &src)
{
/*
    Bring the grid in line with masters' rewritten sidecars: re-read each master
    (applyModelChange, which captures its version list again), then reconcile at once
    rather than on the coalesced signal, so the caller can select a new row straight
    after.
*/
    if (masters.isEmpty()) return;
    applyModelChange(masters, QStringList(), src);
    reconcileVersionRows(src);
    // and the index, which serves these rows without reading the sidecar (schema 17)
    for (const QString &m : masters) updateCatalogForRow(dm->rowFromKey(m));
}

void MW::syncVersionsMenu()
{
    const QString key = dm ? dm->currentKey : QString();
    const bool haveImage = !key.isEmpty() && dm->rowFromKey(key) >= 0
        && !dm->index(dm->rowFromKey(key), G::VideoColumn).data().toBool();
    const bool isVersion = VersionKey::isVersion(key);
    newVersionAction->setEnabled(haveImage);
    renameVersionAction->setEnabled(haveImage && isVersion);
    deleteVersionAction->setEnabled(haveImage && isVersion);
    setVersionAsMasterAction->setEnabled(haveImage && isVersion);
}

void MW::newVersion()
{
/*
    A new version of every selected image (Lightroom's Create Virtual Copy), starting
    from that image's current recipe, preview, rating, label and pick -- a copy, so
    nothing on screen changes until it is edited. The new row is shown and, for the
    current image, selected, which is where the user goes next: to edit it.
*/
    if (G::isLogger) G::log("MW::newVersion");
    if (!dm) return;
    /*  The version row is inserted through applyModelChange, which a load in progress
        would have moving rows under it (see reconcileVersionRows). */
    if (G::isModifyingDatamodel || G::isLoadRunning) {
        G::popup->showPopup("Please wait until the images have finished loading.", 2000);
        return;
    }

    QStringList keys;
    for (const QModelIndex &idx : dm->selectionModel->selectedRows())
        keys << idx.data(G::KeyRole).toString();
    if (keys.isEmpty() && !dm->currentKey.isEmpty()) keys << dm->currentKey;

    FileOps::flushPendingEdits();

    QStringList masters;
    QString selectKey;
    for (const QString &key : std::as_const(keys)) {
        const int dmRow = dm->rowFromKey(key);
        if (dmRow < 0 || dm->index(dmRow, G::VideoColumn).data().toBool()) continue;
        const QString master = VersionKey::sourceOf(key);

        ImageVersion v;
        v.develop = Metadata::readDevelopSidecar(key);
        const QByteArray thumb = Metadata::readDevThumb(key);    // key-checked
        if (!v.develop.isEmpty() && !thumb.isEmpty()) {
            v.preview = thumb.toBase64();
            v.previewKey = Metadata::devPreviewKey(v.develop, key);
        }
        versionValuesFromRow(dm, dmRow, v);

        int id = 0;
        if (!Versions::update(master, [&](VersionSet &set) { id = set.add(v).id; })
            || id <= 0)
            continue;                               // refused; the reason is issued
        const QString newKey = VersionKey::make(master, id);

        /* The loupe preview too, so the new version opens looking like its origin
           instead of decoding. Same recipe, same hash. */
        if (!v.develop.isEmpty()) {
            const QByteArray hash = Metadata::devPreviewKey(v.develop, key).toLatin1();
            const QByteArray jpg = DevPreviewCache::instance().get(key, hash);
            if (!jpg.isEmpty()) DevPreviewCache::instance().put(newKey, hash, jpg);
        }

        if (!masters.contains(master)) masters << master;
        dm->sf->setVersionsExpanded(master, true);
        if (key == dm->currentKey || selectKey.isEmpty()) selectKey = newKey;
    }
    if (masters.isEmpty()) {
        G::popup->showPopup("No version was created.", 1500);
        return;
    }

    /*  The expand state was set above, so the re-filter inside applyModelChange shows
        the new rows. Select the new version AFTER it: filterChange recovers the
        previous selection as it finishes, which would land back on the master. */
    rereadVersionMasters(masters, "MW::newVersion");
    QTimer::singleShot(0, this, [this, selectKey] {
        const QModelIndex idx = dm->proxyIndexFromKey(selectKey);
        if (idx.isValid()) sel->select(idx, Qt::NoModifier, "MW::newVersion");
    });
}

void MW::renameVersion()
{
    if (G::isLogger) G::log("MW::renameVersion");
    const QString key = dm ? dm->currentKey : QString();
    if (!VersionKey::isVersion(key)) return;
    const QString master = VersionKey::sourceOf(key);
    const int id = VersionKey::idOf(key);
    const int dmRow = dm->rowFromKey(key);
    const QString old = dm->index(dmRow, G::PathColumn).data(G::VersionNameRole).toString();

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename Version"),
        tr("Name for this version of %1:").arg(QFileInfo(master).fileName()),
        QLineEdit::Normal, old.isEmpty() ? QString("v%1").arg(id) : old, &ok).trimmed();
    if (!ok || name == old) return;

    /* "v<id>" is what an unnamed version shows, so typing it back means "no name". */
    const QString stored = (name == QString("v%1").arg(id)) ? QString() : name;
    bool found = false;
    Versions::update(master, [&](VersionSet &set) {
        if (ImageVersion *v = set.find(id)) { v->name = stored; found = true; }
    });
    if (!found) return;
    rereadVersionMasters({master}, "MW::renameVersion");
    QTimer::singleShot(0, this, [this, key] {
        const QModelIndex idx = dm->proxyIndexFromKey(key);
        if (idx.isValid()) sel->select(idx, Qt::NoModifier, "MW::renameVersion");
    });
}

void MW::deleteVersion()
{
    if (dm && VersionKey::isVersion(dm->currentKey)) deleteVersionRecords({dm->currentKey});
}

void MW::deleteVersionRecords(const QStringList &keys, bool confirm)
{
/*
    Removes each version's RECORD from its master's sidecar, with its loupe preview and
    History. The image files, the masters and the other versions are untouched -- which
    the confirmation says, because "delete" next to a photograph otherwise reads as the
    trash. Reached from Develop > Versions > Delete Version (the current image) and from
    Delete with versions selected (MW::deleteSelectedFiles, which has already asked).
*/
    if (G::isLogger) G::log("MW::deleteVersionRecords", keys.join(", "));
    if (!dm) return;
    QStringList versions;
    for (const QString &k : keys) if (VersionKey::isVersion(k)) versions << k;
    versions.removeDuplicates();
    if (versions.isEmpty()) return;

    if (confirm) {
        QString what;
        if (versions.size() == 1) {
            const QString k = versions.first();
            QString name = dm->index(dm->rowFromKey(k), G::PathColumn)
                               .data(G::VersionNameRole).toString();
            if (name.isEmpty()) name = QString("v%1").arg(VersionKey::idOf(k));
            what = tr("Delete version \"%1\" of %2?")
                       .arg(name, QFileInfo(VersionKey::sourceOf(k)).fileName());
        }
        else {
            what = tr("Delete %1 versions?").arg(versions.size());
        }
        QMessageBox box(this);
        box.setWindowTitle(tr("Delete Version"));
        box.setIcon(QMessageBox::Question);
        box.setText(what);
        box.setInformativeText(tr("Only the versions' develop settings are removed. The "
                                  "image files and their other versions are not affected."));
        QPushButton *del = box.addButton(versions.size() == 1 ? tr("Delete Version")
                                                              : tr("Delete Versions"),
                                         QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel);
        box.exec();
        resetFocus();
        if (box.clickedButton() != del) return;
    }

    FileOps::flushPendingEdits();
    // land on the master first, as a collapse does
    if (versions.contains(dm->currentKey)) {
        const QModelIndex m = dm->proxyIndexFromKey(VersionKey::sourceOf(dm->currentKey));
        if (m.isValid()) sel->select(m, Qt::NoModifier, "MW::deleteVersionRecords");
    }

    // one sidecar write per master, however many of its versions go
    QHash<QString, QList<int>> byMaster;
    for (const QString &k : std::as_const(versions))
        byMaster[VersionKey::sourceOf(k)] << VersionKey::idOf(k);
    QStringList masters;
    for (auto it = byMaster.cbegin(); it != byMaster.cend(); ++it) {
        const QList<int> ids = it.value();
        int removed = 0;
        if (!Versions::update(it.key(), [&](VersionSet &set) {
                for (int id : ids) if (set.remove(id)) ++removed;
            }) || !removed)
            continue;
        masters << it.key();
        for (int id : ids) {
            const QString k = VersionKey::make(it.key(), id);
            if (developProperties) developProperties->forgetImage(k);
            DevPreviewCache::instance().removeEntry(k);
            if (icd) icd->remove(k);
        }
    }
    rereadVersionMasters(masters, "MW::deleteVersionRecords");
}

void MW::setVersionAsMaster()
{
/*
    Swap the RECIPES (and their previews) of the current version and its master, so the
    edit the user settled on becomes the one every other application sees in
    winnow:Develop, and the old master's edit lives on as this version. Name, rating,
    label and pick stay with their rows. The selection moves to the master, with the
    edit it now holds.
*/
    if (G::isLogger) G::log("MW::setVersionAsMaster");
    const QString key = dm ? dm->currentKey : QString();
    if (!VersionKey::isVersion(key)) return;
    const QString master = VersionKey::sourceOf(key);

    FileOps::flushPendingEdits();

    const QString masterRecipe = Metadata::readDevelopSidecar(master);
    const QByteArray masterThumb = Metadata::readDevThumb(master);
    const QString versionRecipe = Metadata::readDevelopSidecar(key);
    const QByteArray versionThumb = Metadata::readDevThumb(key);
    if (masterRecipe == versionRecipe) {
        G::popup->showPopup("This version already has the master's settings.", 1500);
        return;
    }

    // loupe previews, swapped with their recipes (each keeps its own hash)
    DevPreviewCache &dpc = DevPreviewCache::instance();
    const QByteArray mHash = Metadata::devPreviewKey(masterRecipe, master).toLatin1();
    const QByteArray vHash = Metadata::devPreviewKey(versionRecipe, key).toLatin1();
    const QByteArray mLoupe = masterRecipe.isEmpty() ? QByteArray() : dpc.get(master, mHash);
    const QByteArray vLoupe = versionRecipe.isEmpty() ? QByteArray() : dpc.get(key, vHash);

    Metadata::writeDevelopSidecar(master, versionRecipe,
                                  QString::fromLatin1(versionThumb.toBase64()));
    Metadata::writeDevelopSidecar(key, masterRecipe,
                                  QString::fromLatin1(masterThumb.toBase64()));

    dpc.removeEntry(master);
    dpc.removeEntry(key);
    if (!vLoupe.isEmpty()) dpc.put(master, vHash, vLoupe);
    if (!mLoupe.isEmpty()) dpc.put(key, mHash, mLoupe);

    if (developProperties) {
        developProperties->forgetImage(master);
        developProperties->forgetImage(key);
    }
    // badge, devPreview key, icon and cached full-size image, for both rows
    devPreviewUpdated(master, jpgImage(versionThumb));
    devPreviewUpdated(key, jpgImage(masterThumb));

    /*  THE SELECTION FOLLOWS THE EDIT. The recipe the user was looking at now lives in
        the master's row; staying on the version row would silently put them on the
        OLD master's settings, so the next adjustment edits the wrong one. Selecting
        the master also reloads the Develop panel from the swapped recipe. */
    if (dm->currentKey == key) {
        const QModelIndex m = dm->proxyIndexFromKey(master);
        if (m.isValid()) sel->select(m, Qt::NoModifier, "MW::setVersionAsMaster");
    }
}

void MW::writeVersionValues(const QStringList &versionKeys)
{
/*
    A version row's rating, label or pick changed in the model (DataModel::
    noteVersionValueWrite catches every writer). Persist the row's current values to the
    version's record in the master's sidecar -- never to the image file, whatever
    "Permit image file modification" says: they are Winnow's, like the recipe.
*/
    if (G::isLogger) G::log("MW::writeVersionValues", versionKeys.join(", "));
    if (!dm) return;
    for (const QString &key : versionKeys) {
        const int dmRow = dm->rowFromKey(key);
        if (dmRow < 0) continue;                    // gone since the edit
        ImageVersion now;
        versionValuesFromRow(dm, dmRow, now);
        const QString master = VersionKey::sourceOf(key);
        const int id = VersionKey::idOf(key);
        bool found = false;
        Versions::update(master, [&](VersionSet &set) {
            if (ImageVersion *v = set.find(id)) {
                v->rating = now.rating;
                v->label = now.label;
                v->pick = now.pick;
                found = true;
            }
        });
        if (!found) {
            G::issue("Warning", "Could not save this version's rating, label or pick.",
                     "MW::writeVersionValues", dmRow, key);
            continue;
        }
        dm->noteVersionValues(key, now.rating, now.label, now.pick);
        updateCatalogForRow(dm->rowFromKey(master));    // the index holds them too
    }
}
