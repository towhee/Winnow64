#include "Main/mainwindow.h"
#include "Image/stalethumb.h"
#include "Metadata/ExifTool.h"
#include "Utilities/fileops.h"
#include "Utilities/utilities.h"

/*
    STALE EMBEDDED JPEG THUMBNAILS: NOTICE, ASK, REPAIR.

    Some JPEGs were rotated long ago by tools that turned the pixels (and reset
    Orientation to 1) but left the small EXIF thumbnail, and any maker preview, the old
    way round. Image/stalethumb.h says how one is recognised. Winnow already SHOWS such
    a file correctly -- Thumb::loadThumb draws the icon from the image instead, and a
    stale icon already in ThumbCache is re-made (Reader::readIcon,
    MW::prefetchIconsFromIndex). Other programs that trust the embedded thumbnail still
    show it on its side, and only repairing the file fixes that.

    REPAIR MEANS THE THUMBNAIL, NOT THE ORIENTATION TAG. Orientation = 1 is right for
    the pixels; changing it would turn the real image sideways everywhere. The embedded
    thumbnail and preview are rebuilt from the pixels instead (ExifTool::
    replaceEmbeddedImages), so nothing the user sees in a full-size view changes.

    NEVER WRITTEN FROM THE READ PATH. Detection happens on Reader threads while the user
    scrolls -- in the Library, across files they never selected. So a detection only
    reports (Thumb::staleEmbeddedThumb -> Reader -> MetaRead -> noteStaleEmbeddedThumb),
    reports are collected until the burst settles, and one prompt asks. Every write is
    a click on that prompt:
      - "Permit image file modification" ON: Repair N files.
      - OFF: Repair N files (this time only). That authorises THIS repair of THESE
        files and nothing else. G::modifySourceFiles is never flipped for it: other
        threads read it, and would be free to write originals while it was on.
    G::backupBeforeModifying is honoured either way.

    Each path is reported once a session (staleThumbSeen), so scrolling back over a
    folder does not ask again; "Don't show this message again" is persisted.
*/

namespace {

/*  The image's own pixels, in STORED orientation (no auto-transform) -- the embedded
    images sit under the same Orientation tag as the main image, so they must be stored
    the same way round. setScaledSize lets the JPEG decoder scale while decoding. */
bool storedImageScaled(const QString &fPath, int longSide, QImage &out)
{
    QImageReader r(fPath);
    r.setAutoTransform(false);
    const QSize full = r.size();
    if (!full.isValid()) return false;
    r.setScaledSize(full.scaled(longSide, longSide, Qt::KeepAspectRatio));
    return r.read(&out) && !out.isNull();
}

struct RepairItem {
    QString path;
    QDateTime mtime;        // as it was when the user said yes
};

} // namespace

void MW::noteStaleEmbeddedThumb(QString fPath)
{
    if (G::isLogger) G::log("MW::noteStaleEmbeddedThumb", fPath);
    if (fPath.isEmpty() || staleThumbSeen.contains(fPath)) return;
    staleThumbSeen.insert(fPath);
    if (skipStaleThumbPrompt) return;
    staleThumbPending.insert(fPath);

    /*  Debounced: a folder of them arrives one per Reader decode, and the prompt should
        come once, after the burst, with the whole count. */
    if (!staleThumbTimer) {
        staleThumbTimer = new QTimer(this);
        staleThumbTimer->setSingleShot(true);
        staleThumbTimer->setInterval(1500);
        connect(staleThumbTimer, &QTimer::timeout, this, &MW::showStaleThumbPrompt);
    }
    staleThumbTimer->start();
}

void MW::showStaleThumbPrompt()
{
    if (G::isLogger) G::log("MW::showStaleThumbPrompt");
    if (staleThumbPending.isEmpty()) return;

    /*  Not over a load (a modal loop mid-load is the nested-event-loop trouble this
        codebase keeps paying for), not over another dialog, not during a repair. Try
        again later; the pending set keeps growing meanwhile. */
    if (!G::allMetadataAttempted || G::isModifyingDatamodel || staleThumbAbort
        || QApplication::activeModalWidget()) {
        staleThumbTimer->start();
        return;
    }

    QStringList paths;
    for (const QString &p : std::as_const(staleThumbPending))
        if (QFileInfo::exists(p)) paths << p;
    staleThumbPending.clear();
    if (paths.isEmpty()) return;
    paths.sort(Qt::CaseInsensitive);

    const int n = paths.size();
    const QString images = n == 1 ? "1 image has" : QString::number(n) + " images have";
    const QString files = n == 1 ? "1 file" : QString::number(n) + " files";

    QString info =
        "Each image was rotated by a program that did not update the small preview "
        "stored inside the file. Winnow shows these images correctly, but other programs "
        "that use the embedded thumbnail may show them sideways.\n\n"
        "Repair rebuilds the embedded thumbnail (and the larger embedded preview, if "
        "there is one) from the image itself. The image and its other metadata are not "
        "changed.";
    if (G::backupBeforeModifying)
        info += " A backup of each file is made first.";
    if (!G::modifySourceFiles)
        info += "\n\n\"Permit image file modification\" is off in Preferences, so Winnow "
                "will only repair these files if you ask it to this one time. The "
                "preference stays off.";

    QMessageBox box(this);
    box.setWindowTitle("Embedded Thumbnails");
    box.setIcon(QMessageBox::Warning);
    box.setText(images + " an embedded thumbnail that no longer matches the image.");
    box.setInformativeText(info);
    box.setDetailedText(paths.join("\n"));
    QPushButton *repair = box.addButton(
        G::modifySourceFiles ? "Repair " + files
                             : "Repair " + files + " (this time only)",
        QMessageBox::AcceptRole);
    QPushButton *notNow = box.addButton("Not Now", QMessageBox::RejectRole);
    box.setDefaultButton(notNow);
    QCheckBox *skip = new QCheckBox("Don't show this message again", &box);
    box.setCheckBox(skip);
    box.exec();

    if (skip->isChecked()) {
        skipStaleThumbPrompt = true;
        if (settings) settings->setValue("skipStaleThumbPrompt", true);
    }
    if (box.clickedButton() == repair) repairStaleThumbs(paths);
}

void MW::repairStaleThumbs(const QStringList &paths)
{
/*
    Off the GUI thread: one ExifTool process for the batch, a scaled decode per file.
    Progress in the popup; Esc stops after the current file (MW::keyReleaseEvent).

    The ONLY caller is the prompt's Repair button, which is the authorisation -- either
    the preference is on, or the user chose "this time only" for exactly these paths.
    So this does not consult G::modifySourceFiles, and it writes nothing but `paths`.

    A file whose modification time moved since the user said yes is skipped: whatever
    changed it is not something they agreed to have overwritten.
*/
    if (G::isLogger) G::log("MW::repairStaleThumbs", QString::number(paths.size()));
    if (paths.isEmpty() || staleThumbAbort) return;

    QVector<RepairItem> items;
    for (const QString &p : paths) {
        if (FileOps::refuseVersionKey(p, "MW::repairStaleThumbs")) continue;
        const QFileInfo fi(p);
        if (!fi.exists() || fi.isSymLink()) continue;
        if (!StaleThumb::isChecked(fi.suffix())) continue;
        items.append({p, fi.lastModified()});
    }
    if (items.isEmpty()) return;

    auto abort = std::make_shared<std::atomic<bool>>(false);
    staleThumbAbort = abort;
    const bool backup = G::backupBeforeModifying;
    const int total = items.size();

    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(total);
    G::popup->setProgress(0);
    G::popup->showPopupNow("Repairing embedded thumbnails in " + QString::number(total)
                           + (total == 1 ? " file" : " files")
                           + "<p>Press <font color=\"red\"><b>Esc</b></font> to stop.",
                           0, true, 1.0);

    struct Outcome { QStringList fixed; QStringList failed; };
    auto *watcher = new QFutureWatcher<Outcome>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, total] {
        const Outcome o = watcher->result();
        watcher->deleteLater();
        staleThumbAbort.reset();
        G::popup->setProgressVisible(false);
        G::popup->reset();

        /*  The files changed on disk, so their rows are re-read exactly as a row
            ScrollVerify found out of date is: metadata, icon, and the catalog row. The
            new mtime makes the old ThumbCache entry a miss by itself. */
        if (!o.fixed.isEmpty()) refreshStaleRows(o.fixed);

        QString msg = "Repaired " + QString::number(o.fixed.size()) + " of "
                      + QString::number(total) + (total == 1 ? " file." : " files.");
        if (!o.failed.isEmpty())
            msg += "<p>" + QString::number(o.failed.size())
                   + " could not be repaired; see Help > Logs > View session issues.";
        G::popup->showPopup(msg, 3000);
    });

    watcher->setFuture(QtConcurrent::run([items, abort, backup]() {
        Outcome o;
        QTemporaryDir tmp;
        if (!tmp.isValid()) {
            for (const RepairItem &it : items) o.failed << it.path;
            G::issue("Warning", "Could not create a temporary folder.",
                     "MW::repairStaleThumbs");
            return o;
        }
        const QString thumbPath = tmp.filePath("thumb.jpg");
        const QString previewPath = tmp.filePath("preview.jpg");

        ExifTool et;
        et.setOverWrite(true);
        int done = 0;
        for (const RepairItem &it : items) {
            if (abort->load() || G::stop) break;
            QMetaObject::invokeMethod(G::popup, [d = ++done] { G::popup->setProgress(d); },
                                      Qt::QueuedConnection);

            if (QFileInfo(it.path).lastModified() != it.mtime) {
                G::issue("Warning", "File changed since the repair was confirmed; "
                         "skipped.", "MW::repairStaleThumbs", -1, it.path);
                o.failed << it.path;
                continue;
            }

            /*  160 px is the EXIF/DCF thumbnail convention and keeps IFD1 well inside
                the 64 KB APP1 segment. The preview, where the file has one, is written
                after the image data and has no such limit; 640 px matches the camera
                previews this was found with (Nikon, 570 px). */
            QImage thumb, preview;
            if (!storedImageScaled(it.path, 160, thumb)
                || !storedImageScaled(it.path, 640, preview)
                || !thumb.save(thumbPath, "JPG", 75)
                || !preview.save(previewPath, "JPG", 85)) {
                G::issue("Warning", "Could not decode the image to rebuild its embedded "
                         "thumbnail.", "MW::repairStaleThumbs", -1, it.path);
                o.failed << it.path;
                continue;
            }

            if (backup && !Utilities::backup(it.path, "backup")) {
                G::issue("Warning", "Backup failed; embedded thumbnail not repaired.",
                         "MW::repairStaleThumbs", -1, it.path);
                o.failed << it.path;
                continue;
            }

            QString err;
            if (et.replaceEmbeddedImages(thumbPath, previewPath, it.path, err)) {
                o.fixed << it.path;
            }
            else {
                G::issue("Warning", "ExifTool could not repair the embedded thumbnail: "
                         + err, "MW::repairStaleThumbs", -1, it.path);
                o.failed << it.path;
            }
        }
        et.close();
        return o;
    }));
}
