#include "Main/mainwindow.h"

#include <QMessageBox>

/*
    DRAG TO GEOTAG -- the Map module's way to give images a location: drag thumbnails
    onto the map and drop them where they were taken (MapView::geotagRequested).

    WHERE THE LOCATION GOES. The library and the sidecar, NEVER the image file:
      o the SIDECAR gets exif:GPSLatitude / exif:GPSLongitude, Lightroom's form
        (Metadata::writeGpsToSidecar), and Metadata::parseSidecar reads it back over the
        file's own GPS on every later load;
      o the DATAMODEL row gets G::GPSCoordColumn, in the string GPS::decode builds
        (Geo::formatCoord), so the location reads exactly like a camera's -- HasGPS,
        the map's pins and the Places membership follow from it;
      o the CATALOG gets the whole row (updateCatalogForRow), so a Library search sees
        it without the folder being reopened.
    The camera's own GPS block in the file is never touched, whatever "Permit image
    file modification" says: a location dropped on a map is a correction, and the
    original stays recoverable by deleting the two sidecar properties.

    A FILE, NOT A ROW. A version (virtual copy) is its file, and so is each half of a
    combined raw+jpg pair's file: every row of each file named gets the location, and
    each sidecar is written once.

    A drop that would REPLACE locations asks first -- a drag that lands a few pixels off
    should not silently move a camera's coordinates.
*/

void MW::geotagRows(const QList<int> &dmRows, double lat, double lon)
{
    if (G::isLogger) G::log("MW::geotagRows", QString::number(dmRows.size()));
    if (!dm || !metadata || dmRows.isEmpty()) return;
    if (G::isLoadRunning || G::isModifyingDatamodel) {
        if (G::popup)
            G::popup->showPopup(tr("Still loading. Drop the images again when it has "
                                   "finished."), 2500);
        return;
    }

    // the files: each row's source file, and the other half of a raw+jpg pair
    QStringList files;
    QSet<QString> seen;
    auto addFile = [&](int row) {
        if (row < 0 || row >= dm->rowCount()) return;
        const QString src =
            dm->index(row, G::PathColumn).data(G::SourcePathRole).toString();
        if (src.isEmpty() || seen.contains(src)) return;
        seen.insert(src);
        files << src;
    };
    for (int r : dmRows) {
        addFile(r);
        if (combineRawJpg) addFile(dm->dupOtherRow(r));
    }
    if (files.isEmpty()) return;

    // every row of those files (versions included), and how many already have a place
    const int n = dm->rowCount();
    QList<int> rows;
    int located = 0;
    for (int r = 0; r < n; ++r) {
        const QString file =
            dm->index(r, G::PathColumn).data(G::SourcePathRole).toString();
        if (!seen.contains(file)) continue;
        rows << r;
        double la, lo;
        if (Geo::parseCoord(dm->index(r, G::GPSCoordColumn).data().toString(), la, lo))
            ++located;
    }

    const QString where = Geo::formatCoord(lat, lon);
    if (located > 0) {
        QMessageBox box(QMessageBox::Question, tr("Set Location"),
                        tr("%1 of the %2 images already have a location.\n\n"
                           "Replace it with %3?\n\nThe image files are not changed: the "
                           "new location is written to the sidecars and the library.")
                            .arg(located).arg(rows.size()).arg(where),
                        QMessageBox::Cancel, this);
        QPushButton *replace = box.addButton(tr("Replace"), QMessageBox::AcceptRole);
        box.setDefaultButton(replace);
        box.exec();
        if (box.clickedButton() != replace) return;
    }

    // the sidecars first: a row is changed only when its file's sidecar took the write
    QSet<QString> written;
    QStringList failed;
    for (const QString &f : std::as_const(files)) {
        if (metadata->writeGpsToSidecar(f, lat, lon)) written.insert(f);
        else failed << QFileInfo(f).fileName();
    }

    const QString src = "MW::geotagRows";
    int changed = 0;
    for (int r : std::as_const(rows)) {
        const QString f = dm->index(r, G::PathColumn).data(G::SourcePathRole).toString();
        if (!written.contains(f)) continue;
        emit setValDm(r, G::GPSCoordColumn, where, dm->instance, src, Qt::EditRole);
        updateCatalogForRow(r);
        ++changed;
    }

    /*  What reads the column: the GPS category's counts, a filter or sort on it, the
        Places membership (which refilters itself if it changed), and the map's pins --
        setValDm only notifies the rows on screen, so the map is told directly. */
    if (changed) {
        if (buildFilters && filters && !filters->buildingFilters) buildFilters->recount();
        if (editNeedsRefilter(G::GPSCoordColumn) || editNeedsRefilter(G::HasGPSColumn))
            filterChange(src);
        refreshPlaceMembership();
        if (mapView && mapView->isVisible()) mapView->activate();
        /*  The Metadata panel reads the model, but only when the selection changes --
            and a drop does not change it. The current image is usually one dropped. */
        if (G::useInfoView && infoView && dm->currentSfRow >= 0)
            infoView->updateInfo(dm->currentSfRow);
    }

    if (!failed.isEmpty()) {
        QMessageBox::warning(this, tr("Set Location"),
            tr("The location could not be written to the sidecar of %1 image(s), so "
               "they were left unchanged:\n\n%2")
                .arg(failed.size()).arg(failed.mid(0, 12).join("\n")
                                        + (failed.size() > 12 ? "\n..." : "")));
    }
    else if (G::popup) {
        G::popup->showPopup(changed == 1
            ? tr("Location set: %1").arg(where)
            : tr("Location set for %1 images: %2").arg(changed).arg(where), 2500);
    }
}
