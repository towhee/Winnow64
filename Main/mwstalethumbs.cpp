#include "Main/mainwindow.h"
#include "Image/stalethumb.h"
#include "Cache/thumbcache.h"
#include "Cache/catalog.h"
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

/*  "Reason:" then its files, indented, one group per reason. */
QString groupedByReason(const QList<QPair<QString, QString>> &pathReason)
{
    QMap<QString, QStringList> groups;
    for (const auto &pr : pathReason) groups[pr.second] << pr.first;
    QStringList out;
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        out << it.key() + ":";
        for (const QString &f : it.value()) out << "    " + f;
        out << "";
    }
    return out.join("\n").trimmed();
}

/*  THE ONE LOOK for this feature's dialogs (the prompt and the result). A QDialog, not
    a QMessageBox: on macOS a message box shows no title and cannot be resized, and a
    list of a hundred paths needs both. The caller adds buttons to `buttons` and any
    extra widgets above them through `extras`. */
struct StaleThumbDialog {
    QDialog dlg;
    QVBoxLayout *extras = nullptr;
    QDialogButtonBox *buttons = nullptr;

    StaleThumbDialog(QWidget *parent, QStyle::StandardPixmap icon,
                     const QString &headline, const QString &body,
                     const QString &listText)
        : dlg(parent)
    {
        dlg.setWindowTitle("Embedded Thumbnails");
        dlg.setSizeGripEnabled(true);
        auto *layout = new QVBoxLayout(&dlg);

        auto *top = new QHBoxLayout;
        auto *iconLabel = new QLabel(&dlg);
        iconLabel->setPixmap(dlg.style()->standardIcon(icon).pixmap(48, 48));
        iconLabel->setAlignment(Qt::AlignTop);
        top->addWidget(iconLabel);
        auto *textCol = new QVBoxLayout;
        auto *head = new QLabel("<b>" + headline.toHtmlEscaped() + "</b>", &dlg);
        head->setWordWrap(true);
        textCol->addWidget(head);
        if (!body.isEmpty()) {
            auto *bodyLabel = new QLabel(body, &dlg);
            bodyLabel->setWordWrap(true);
            textCol->addSpacing(6);
            textCol->addWidget(bodyLabel);
        }
        top->addLayout(textCol, 1);
        layout->addLayout(top);

        if (!listText.isEmpty()) {
            auto *list = new QPlainTextEdit(listText, &dlg);
            list->setReadOnly(true);
            list->setLineWrapMode(QPlainTextEdit::NoWrap);
            layout->addSpacing(6);
            layout->addWidget(list, 1);
        }
        extras = new QVBoxLayout;
        layout->addLayout(extras);
        buttons = new QDialogButtonBox(&dlg);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        layout->addWidget(buttons);
        dlg.resize(680, listText.isEmpty() ? 220 : 480);
    }
};

const QString kLockedReason =
#if defined(Q_OS_MAC)
    "Locked or read-only (unlock it in Finder > Get Info, then try again)";
#elif defined(Q_OS_WIN)
    "Read-only (clear Read-only in the file's Properties, then try again)";
#else
    "Read-only (give yourself write permission, e.g. chmod u+w, then try again)";
#endif

} // namespace

void MW::noteStaleEmbeddedThumb(QString fPath)
{
    if (G::isLogger) G::log("MW::noteStaleEmbeddedThumb", fPath);
    if (fPath.isEmpty() || staleThumbSeen.contains(fPath)) return;
    staleThumbSeen.insert(fPath);
    /*  Remembered across sessions: from now on this file's icon is a cache hit and
        nothing will look at its embedded thumb again (recallStaleThumbs). Written in
        a batch when the debounce fires (flushStaleThumbMarks), not here: this is the
        GUI thread, and ThumbCache's mutex is held by its writer for whole batch
        commits. */
    staleThumbToMark << fPath;
    if (!skipStaleThumbPrompt) staleThumbPending.insert(fPath);
    scheduleStaleThumbPrompt();
}

void MW::flushStaleThumbMarks()
{
    if (staleThumbToMark.isEmpty()) return;
    const QStringList paths = staleThumbToMark;
    staleThumbToMark.clear();
    (void)QtConcurrent::run([paths] { ThumbCache::instance().markStale(paths); });
}

void MW::scheduleStaleThumbPrompt()
{
    /*  Debounced: a folder of them arrives one per Reader decode, and the prompt should
        come once, after the burst, with the whole count. Also the retry when the prompt
        has to wait for a load, a dialog or a running repair. */
    if (!staleThumbTimer) {
        staleThumbTimer = new QTimer(this);
        staleThumbTimer->setSingleShot(true);
        staleThumbTimer->setInterval(1500);
        connect(staleThumbTimer, &QTimer::timeout, this, &MW::showStaleThumbPrompt);
    }
    staleThumbTimer->start();
}

void MW::noteStaleEmbeddedThumbs(const QStringList &paths)
{
/*
    A catalog scan's findings: every JPEG it indexed (added through Manage Catalog, or
    changed since) is checked by CatalogScanner::parseInto. Same path as a decode-time
    detection -- remembered, debounced, one prompt.
*/
    for (const QString &p : paths) noteStaleEmbeddedThumb(p);
}

void MW::recallStaleThumbs()
{
/*
    ONCE A SESSION, after the first load: offer again what an earlier session found and
    the user did not repair. Needed because detection only happens when an embedded
    thumb is decoded, and after the first time a stale file's icon comes from
    ThumbCache -- a "Not Now" would otherwise be forgotten at quit. ThumbCache::
    staleFiles stats every remembered file (off the GUI thread: a path may be on a slow
    or sleeping volume) and returns only those unchanged since they were found.
*/
    if (staleThumbsRecalled || skipStaleThumbPrompt) return;
    staleThumbsRecalled = true;
    auto *watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        const QStringList paths = watcher->result();
        watcher->deleteLater();
        bool any = false;
        for (const QString &p : paths) {
            if (staleThumbSeen.contains(p)) continue;
            staleThumbSeen.insert(p);
            staleThumbPending.insert(p);
            any = true;
        }
        if (any) scheduleStaleThumbPrompt();
    });
    /*  WRITABLE ONLY. A locked or read-only stale file stays remembered but is not
        offered at every start -- it would bring up an OK-only prompt each session for a
        file nothing can repair. Unlocking it (chflags / Get Info) does not move its
        mtime, so its row survives and it is offered on the first start after. It is
        still listed, under the locked heading, whenever it is freshly detected. */
    watcher->setFuture(QtConcurrent::run([] {
        QStringList out;
        for (const QString &p : ThumbCache::instance().staleFiles())
            if (QFileInfo(p).isWritable()) out << p;
        return out;
    }));
}

void MW::showStaleThumbPrompt()
{
    if (G::isLogger) G::log("MW::showStaleThumbPrompt");
    flushStaleThumbMarks();
    if (staleThumbPending.isEmpty()) return;

    /*  Not over a load (a modal loop mid-load is the nested-event-loop trouble this
        codebase keeps paying for), not over another dialog, not during a repair. Try
        again later; the pending set keeps growing meanwhile. */
    if (!G::allMetadataAttempted || G::isModifyingDatamodel || staleThumbAbort
        || QApplication::activeModalWidget()) {
        scheduleStaleThumbPrompt();
        return;
    }

    QStringList paths;
    for (const QString &p : std::as_const(staleThumbPending))
        if (QFileInfo::exists(p)) paths << p;
    staleThumbPending.clear();
    if (paths.isEmpty()) return;
    paths.sort(Qt::CaseInsensitive);

    /*  LOCKED FILES ARE SAID UP FRONT. A locked (macOS uchg) or read-only file cannot
        be rewritten -- ExifTool writes a copy and then fails to rename it over the
        original -- and the user's library holds 429 locked JPEGs. They are listed with
        the reason and left out of the Repair count, rather than discovered as failures
        afterwards. QFileInfo::isWritable is access(W_OK), which sees uchg on macOS and
        the read-only attribute on Windows. */
    QStringList writable, locked;
    for (const QString &p : std::as_const(paths))
        (QFileInfo(p).isWritable() ? writable : locked) << p;

    const int n = paths.size();
    const int w = writable.size();
    const QString images = n == 1 ? "1 image has" : QString::number(n) + " images have";
    const QString files = w == 1 ? "1 file" : QString::number(w) + " files";

    QString info =
        "Each image was rotated by a program that did not update the small preview "
        "stored inside the file. Winnow shows these images correctly, but other programs "
        "that use the embedded thumbnail may show them sideways.\n\n"
        "Repair rebuilds the embedded thumbnail (and the larger embedded preview, if "
        "there is one) from the image itself. The image and its other metadata are not "
        "changed.";
    if (G::backupBeforeModifying)
        info += " A backup of each file is made first.";
    if (!locked.isEmpty())
        info += "\n\n" + QString::number(locked.size())
                + (locked.size() == 1 ? " file is" : " files are")
                + " locked or read-only and cannot be repaired until unlocked.";
    if (!G::modifySourceFiles && w)
        info += "\n\n\"Permit image file modification\" is off in Preferences, so Winnow "
                "will only repair these files if you ask it to this one time. The "
                "preference stays off.";

    QString listText;
    if (!writable.isEmpty())
        listText = (locked.isEmpty() ? QString() : QString("Can be repaired:\n    "))
                   + writable.join(locked.isEmpty() ? "\n" : "\n    ");
    if (!locked.isEmpty()) {
        if (!listText.isEmpty()) listText += "\n\n";
        listText += kLockedReason + ":\n    " + locked.join("\n    ");
    }

    StaleThumbDialog r(this, QStyle::SP_MessageBoxWarning,
                   images + " an embedded thumbnail that no longer matches the image.",
                   info, listText);
    auto *skip = new QCheckBox("Don't show this message again", &r.dlg);
    r.extras->addWidget(skip);
    QPushButton *notNow = r.buttons->addButton(w ? "Not Now" : "OK",
                                               QDialogButtonBox::RejectRole);
    if (w) r.buttons->addButton(G::modifySourceFiles
                                    ? "Repair " + files
                                    : "Repair " + files + " (this time only)",
                                QDialogButtonBox::AcceptRole);
    /*  CHECK WHOLE CATALOG. What is listed here was found as files were displayed or
        added to the catalog; files catalogued before this check existed are found
        only by asking. Closes this prompt; the check's own prompt lists everything it
        finds, these included. */
    bool checkCatalog = false;
    if (Catalog::instance().isAvailable()) {
        QPushButton *check = r.buttons->addButton("Check Whole Catalog",
                                                  QDialogButtonBox::ActionRole);
        check->setToolTip("Look for stale embedded thumbnails in every catalogued "
                          "JPEG.\nReads file headers only; Esc stops it.");
        connect(check, &QPushButton::clicked, &r.dlg, [&] {
            checkCatalog = true;
            r.dlg.reject();
        });
    }
    notNow->setDefault(true);
    const bool doRepair = r.dlg.exec() == QDialog::Accepted && w;

    if (skip->isChecked()) {
        skipStaleThumbPrompt = true;
        if (settings) settings->setValue("skipStaleThumbPrompt", true);
    }
    if (doRepair) repairStaleThumbs(writable);
    else if (checkCatalog) checkCatalogStaleThumbs();
}

void MW::checkCatalogStaleThumbs()
{
/*
    EVERY CATALOGUED JPEG, headers only (StaleThumb::fileIsStale: two short seeks per
    file; 32,000 files measured at 13.5 s). Run from the prompt's Check Whole Catalog
    button, never on its own: the check of NEW files happens as a scan indexes them
    (CatalogScanner::parseInto), and the files catalogued before that existed are found
    only when asked. Off the GUI thread with popup progress; Esc stops it.

    WHAT IT FINDS IS REMEMBERED (ThumbCache::markStale) and shown in one prompt -- ALL of
    it, including files already reported this session, because the user asked for the
    whole picture. A file it cannot open (an unplugged drive) is simply not stale.
*/
    if (G::isLogger) G::log("MW::checkCatalogStaleThumbs");
    if (staleThumbAbort) return;                // a repair or check is running

    auto abort = std::make_shared<std::atomic<bool>>(false);
    staleThumbAbort = abort;

    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(1);
    G::popup->setProgress(0);
    G::popup->showPopupNow("Checking catalogued JPEGs for stale embedded thumbnails"
                           "<p>Press <font color=\"red\"><b>Esc</b></font> to stop.",
                           0, true, 1.0);

    struct Result { QStringList stale; int total = 0; int checked = 0; };
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        const Result res = watcher->result();
        watcher->deleteLater();
        staleThumbAbort.reset();
        G::popup->setProgressVisible(false);
        G::popup->reset();

        const bool stopped = res.checked < res.total;
        const QString counted = QString::number(res.checked) + " of "
                                + QString::number(res.total) + " catalogued JPEGs";
        if (res.stale.isEmpty()) {
            StaleThumbDialog r(this, QStyle::SP_MessageBoxInformation,
                               stopped ? "Stopped. No stale embedded thumbnails were "
                                         "found in the files checked."
                                       : "No stale embedded thumbnails were found.",
                               "Checked " + counted + ".", QString());
            r.buttons->addButton(QDialogButtonBox::Ok)->setDefault(true);
            r.dlg.exec();
            return;
        }
        for (const QString &p : res.stale) {          // marked by the worker
            staleThumbSeen.insert(p);
            staleThumbPending.insert(p);
        }
        if (stopped)
            G::popup->showPopup("Stopped after " + counted + ".", 2000);
        showStaleThumbPrompt();
    });

    watcher->setFuture(QtConcurrent::run([abort]() {
        Result res;
        const QStringList paths = Catalog::instance().liveJpegPaths();
        res.total = paths.size();
        QMetaObject::invokeMethod(G::popup, [n = res.total] {
            G::popup->setProgressMax(qMax(1, n));
        }, Qt::QueuedConnection);
        for (const QString &p : paths) {
            if (abort->load()) break;
            if (StaleThumb::fileIsStale(p)) res.stale << p;
            if (++res.checked % 250 == 0)
                QMetaObject::invokeMethod(G::popup, [d = res.checked] {
                    G::popup->setProgress(d);
                }, Qt::QueuedConnection);
        }
        ThumbCache::instance().markStale(res.stale);   // here: one transaction, off GUI
        return res;
    }));
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

    struct Outcome {
        QStringList fixed;
        QList<QPair<QString, QString>> failed;      // path, reason the user can act on
        QStringList notReached;
    };
    auto *watcher = new QFutureWatcher<Outcome>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, total] {
        const Outcome o = watcher->result();
        watcher->deleteLater();
        staleThumbAbort.reset();
        G::popup->setProgressVisible(false);
        G::popup->reset();

        /*  The files changed on disk, so their rows are re-read exactly as a row
            ScrollVerify found out of date is: metadata, icon, and the catalog row. The
            new mtime makes the old ThumbCache entry a miss by itself. AFTER any result
            dialog, never under it: refreshStaleRows starts a re-read, and a modal loop
            running over a load is what showStaleThumbPrompt is careful to avoid. */
        const QStringList fixed = o.fixed;
        auto reread = [this, fixed] { if (!fixed.isEmpty()) refreshStaleRows(fixed); };

        const QString summary = "Repaired " + QString::number(o.fixed.size()) + " of "
                                + QString::number(total)
                                + (total == 1 ? " file." : " files.");
        if (o.failed.isEmpty() && o.notReached.isEmpty()) {
            G::popup->showPopup(summary, 2000);
            reread();
            return;
        }
        /*  ANYTHING LEFT UNDONE IS A DIALOG, NOT A POPUP: which files, and why, in
            words the user can act on -- a popup is gone before it is read, and "see
            the issue log" made them go and find out. */
        QList<QPair<QString, QString>> rows = o.failed;
        for (const QString &p : o.notReached) rows.append({p, "Not attempted (stopped)"});
        QString body;
        if (!o.failed.isEmpty())
            body += QString::number(o.failed.size())
                    + (o.failed.size() == 1 ? " file was" : " files were")
                    + " not repaired.";
        if (!o.notReached.isEmpty())
            body += (body.isEmpty() ? "" : " ") + QString("Stopped before ")
                    + QString::number(o.notReached.size())
                    + (o.notReached.size() == 1 ? " file." : " files.");
        body += " Nothing was changed in these files.";
        {
            StaleThumbDialog r(this, QStyle::SP_MessageBoxWarning, summary, body,
                               groupedByReason(rows));
            r.buttons->addButton(QDialogButtonBox::Ok)->setDefault(true);
            r.dlg.exec();
        }
        reread();
    });

    watcher->setFuture(QtConcurrent::run([items, abort, backup]() {
        Outcome o;
        QTemporaryDir tmp;
        if (!tmp.isValid()) {
            for (const RepairItem &it : items)
                o.failed.append({it.path, "Could not create a temporary folder"});
            G::issue("Warning", "Could not create a temporary folder.",
                     "MW::repairStaleThumbs");
            return o;
        }
        const QString thumbPath = tmp.filePath("thumb.jpg");
        const QString previewPath = tmp.filePath("preview.jpg");

        ExifTool et;
        et.setOverWrite(true);
        int done = 0;
        /*  ONLY Esc or quit stops it (the shared abort flag). It used to test G::stop
            as well, which is the LOAD-cancel flag: the background catalog scan and
            every folder change set it, and the first repair ended silently after 8 of
            110 files. */
        for (const RepairItem &it : items) {
            if (abort->load()) {
                for (qsizetype i = done; i < items.size(); ++i)
                    o.notReached << items.at(i).path;
                break;
            }
            QMetaObject::invokeMethod(G::popup,
                                      [d = ++done] { G::popup->setProgress(d); },
                                      Qt::QueuedConnection);

            const QFileInfo fi(it.path);
            if (fi.lastModified() != it.mtime) {
                G::issue("Warning", "File changed since the repair was confirmed; "
                         "skipped.", "MW::repairStaleThumbs", -1, it.path);
                o.failed.append({it.path, "Changed by something else after Repair was "
                                          "clicked, so it was left alone"});
                continue;
            }
            // locked (macOS uchg) or read-only: ExifTool would fail at its rename
            if (!fi.isWritable()) {
                G::issue("Warning", "Locked or read-only; not repaired.",
                         "MW::repairStaleThumbs", -1, it.path);
                o.failed.append({it.path, kLockedReason});
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
                o.failed.append({it.path, "The image could not be decoded"});
                continue;
            }

            if (backup && !Utilities::backup(it.path, "backup")) {
                G::issue("Warning", "Backup failed; embedded thumbnail not repaired.",
                         "MW::repairStaleThumbs", -1, it.path);
                o.failed.append({it.path, "The backup copy could not be made, so the "
                                          "file was left alone"});
                continue;
            }

            QString err;
            if (et.replaceEmbeddedImages(thumbPath, previewPath, it.path, err)) {
                o.fixed << it.path;
            }
            else {
                G::issue("Warning", "ExifTool could not repair the embedded thumbnail: "
                         + err, "MW::repairStaleThumbs", -1, it.path);
                o.failed.append({it.path, "ExifTool: "
                                          + (err.isEmpty() ? QString("unknown error")
                                                           : err)});
            }
        }
        et.close();
        ThumbCache::instance().clearStale(o.fixed);    // here: one transaction, off GUI
        return o;
    }));
}
