#include "File/bookmarks.h"
#include "Utilities/fileops.h"
#include "Utilities/foldertree.h"
#include <QSet>
#include "Main/global.h"
#include "Utilities/htmlwindow.h"
#include <QStyleFactory>

/*
A QStringList of paths to bookmarked folders is displayed as top level items
in a QWidgetTree in column 0.  Column 1 holds a count of the readable image files
in the folder.

When the user mouse clicks on one of the folders the itemPressed signal is sent
to the MW slot bookmarkClicked, which reads the path from PathRole (pathOf).

Bookmarks follow the source (G::scope). In Folders a click loads the folder; in the
Library it filters the Library to the folder, and the counts are the Library's. See
setLibraryMode.

Bookmarks can be added via the context menu in FSTree or the folders can be
dragged from the FSTree to Bookmarks.  Folders or files can also be dragged
from the windows explorer or mac finder to be added as a bookmark.

As an abbreviation in the program UI bookmarks are called favs.
*/

BookMarks::BookMarks(QWidget *parent, DataModel *dm,
                     Metadata *metadata,
                     bool showImageCount,
                     bool &combineRawJpg)
                   : QTreeWidget(parent),
                     combineRawJpg(combineRawJpg),
                     delegate(new HoverDelegate(this))
{
    if (G::isLogger) G::log("BookMarks::BookMarks");
    this->dm = dm;
    this->metadata = metadata;
    this->showImageCount = showImageCount;
    viewport()->setObjectName("bookmarksViewPort");
    setObjectName("bookmarks");

    fileFilters = new QStringList;
    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir = new QDir();
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    setItemDelegate(delegate);
    // setItemDelegate(new BookDelegate(this));

#ifdef Q_OS_WIN
    // The native Windows widget style draws a 1px vertical separator at the
    // column boundary in tree bodies; the macOS style does not. Fusion matches
    // the Mac behaviour (no separator). The app-wide stylesheet still applies on
    // top, so the rest of the appearance is unchanged.
    if (QStyle *fusion = QStyleFactory::create("Fusion")) {
        fusion->setParent(this);
        setStyle(fusion);
    }
#endif

    setAcceptDrops(true);
	setDragEnabled(false);
	setDragDropMode(QAbstractItemView::DropOnly);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setRootIsDecorated(false);
    setColumnCount(2);
    setHeaderHidden(true);
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);

    setMouseTracking(true);

    rapidClick.start();

    // Repaint when hover changes: Lambda function to call update
    connect(delegate, &HoverDelegate::hoverChanged, this->viewport(), [this]() {
        this->viewport()->update();});
}

void BookMarks::reloadBookmarks()
{
    if (G::isLogger) G::log("BookMarks::reloadBookmarks");
	clear();
    QSetIterator<QString> it(bookmarkPaths);
	while (it.hasNext()) {
		QString itemPath = it.next();
        addBookmark(itemPath);
    }
    updateCount();
}

void BookMarks::addBookmark(QString itemPath)
{
    if (G::isLogger) G::log("BookMarks::addBookmark", itemPath);
    QTreeWidgetItem *item = new QTreeWidgetItem(this);
    item->setText(0, QFileInfo(itemPath).fileName());
    item->setIcon(0, QIcon(":/images/bookmarks.png"));
    item->setData(0, PathRole, itemPath);
    insertTopLevelItem(0, item);
    item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    styleItem(item);
}

QString BookMarks::pathOf(const QTreeWidgetItem *item)
{
    return item ? item->data(0, PathRole).toString() : QString();
}

void BookMarks::styleItem(QTreeWidgetItem *item)
{
/*
    Dimmed, with the reason in the tooltip, when the bookmark cannot do what a click on it
    does in the current source: the folder is gone (or its volume is not mounted), or --
    in the Library -- the Library holds nothing there. Such a Library bookmark still
    works: the click opens the folder in Folders instead.
*/
    const QString path = pathOf(item);
    QString tip = QDir::toNativeSeparators(path);
    bool dim = false;
    if (!QFileInfo::exists(path)) {
        dim = true;
        tip += tr("\n\nNot available: the folder does not exist or its volume is not "
                  "mounted.");
    }
    else if (libraryMode && libraryKnown && libraryFolders(path).isEmpty()) {
        dim = true;
        tip += tr("\n\nNot in the Library. Click to open it in Folders.");
    }
    item->setToolTip(0, tip);
    item->setToolTip(1, tip);
    if (dim) item->setForeground(0, QBrush(QColor(G::disabledColor)));
    else     item->setData(0, Qt::ForegroundRole, QVariant());
}

void BookMarks::setLibraryMode(bool on)
{
    if (G::isLogger) G::log("BookMarks::setLibraryMode", on ? "Library" : "Folders");
    if (on == libraryMode) return;
    libraryMode = on;
    updateCount();
    // the highlight belonged to the other source; each source re-asserts its own
    selectionModel()->clear();
}

void BookMarks::setLibraryFolders(const QStringList &anchors,
                                  const QMap<QString, int> &perFolder)
{
    if (G::isLogger) G::log("BookMarks::setLibraryFolders");
    libraryAnchors = anchors;
    libraryTotals = FolderTree::expandCounts(perFolder);
    libraryKnown = true;
    if (libraryMode) updateCount();
}

QStringList BookMarks::libraryFolders(const QString &path) const
{
/*
    Folders the Filters Folders category (and LibTree) can hold: the tree starts at the
    Library's include folders, so a bookmark ABOVE them asks for those beneath it -- the
    same thing LibTree's catalog row asks for -- rather than for a folder the filter has
    no row for.
*/
    if (path.isEmpty()) return {};
    for (const QString &a : libraryAnchors)
        if (FolderTree::isAtOrUnder(path, a))
            return libraryTotals.value(path) > 0 ? QStringList{path} : QStringList();
    QStringList under;
    for (const QString &a : libraryAnchors)
        if (FolderTree::isAtOrUnder(a, path) && libraryTotals.value(a) > 0) under << a;
    return under;
}

void BookMarks::syncFromLibraryFilter(const QStringList &includes,
                                      const QStringList &excludes)
{
    if (!libraryMode) return;
    const QSet<QString> inc(includes.begin(), includes.end());
    if (excludes.isEmpty() && !inc.isEmpty()) {
        QTreeWidgetItemIterator it(this);
        while (*it) {
            const QStringList f = libraryFolders(pathOf(*it));
            if (!f.isEmpty() && QSet<QString>(f.begin(), f.end()) == inc) {
                if (currentItem() != *it) setCurrentItem(*it);
                return;
            }
            ++it;
        }
    }
    selectionModel()->clear();
}

void BookMarks::saveBookmarks(QSettings *setting)
{
    if (G::isLogger) G::log("BookMarks::saveBookmarks");
    /* save bookmarks */
    int idx = 0;
    setting->beginGroup("Bookmarks");
    setting->remove("");
    QSetIterator<QString> pathsIter(bookmarkPaths);
    while (pathsIter.hasNext()) {
        setting->setValue("path" + QString::number(++idx), pathsIter.next());
    }
    setting->endGroup();
}

void BookMarks::updateBookmarks()
{
/*
    Update the image count for each folder bookmarked;
*/
    if (G::isLogger)
        G::log("BookMarks::update");
    updateCount();
}

int BookMarks::diskCount(const QString &path)
{
/*
    The readable images in the folder on disk, a raw+jpg pair counting once when
    combineRawJpg.
*/
    int count = 0;
    dir->setPath(path);
    const QFileInfoList list = dir->entryInfoList();
    for (const QFileInfo &info : list) {
        if (!info.size()) continue;
        if (combineRawJpg && metadata->hasJpg.contains(info.suffix().toLower())) {
            const QString jpgPath = info.path() + "/" + info.baseName() + ".jpg";
            if (list.contains(QFileInfo(jpgPath))) continue;
        }
        count++;
    }
    return count;
}

void BookMarks::updateCount()
{
/*
     Update the image count for all folders in BookMarks: the folder on disk in Folders,
     the Library's images at or beneath it in the Library -- the number the LibTree row
     for that folder shows. Blank in the Library until its folders have been read.
*/
    if (G::isLogger) G::log("BookMarks::count");
     QTreeWidgetItemIterator it(this);
     while (*it) {
         const QString path = pathOf(*it);
         QString text;
         if (!libraryMode) text = QString::number(diskCount(path));
         else if (libraryKnown) {
             int n = 0;
             const QStringList f = libraryFolders(path);
             for (const QString &p : f) n += libraryTotals.value(p);
             text = QString::number(n);
         }
         (*it)->setText(1, text);
         styleItem(*it);
         ++it;
     }
}

void BookMarks::updateCount(QString dPath)
{
/*
    Only update the image count for the folder dPath
    Is this being used? 2025-06-30
*/
     if (G::isLogger) G::log("BookMarks::count(fPath)");
     if (libraryMode) { updateCount(); return; }
     QTreeWidgetItemIterator it(this);
     while (*it) {
         if (pathOf(*it) == dPath) {
             (*it)->setText(1, QString::number(diskCount(dPath)));
             return;
         }
         ++it;
     }
}

QStringList BookMarks::bookmarksWithImages()
{
/*
    Returns the bookmark paths where the image count shown in column 1 of the
    QTreeWidget is >= 2.  The count is maintained by updateCount().
*/
    if (G::isLogger) G::log("BookMarks::bookmarksWithImages");
    QStringList paths;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->text(1).toInt() >= 2) {
            paths << pathOf(*it);
        }
        ++it;
    }
    return paths;
}

void BookMarks::select(QString fPath)
{
/*
    This is called from MW::folderSelectionChange to attempt to sync bookmarks with
    the FSTree folders view.

    It is also called when there is a drop event to another folder, which changes the
    selection, to reestablish the bookmark that was selected that is consistent with the
    datamodel current folder.
*/
    if (G::isLogger || G::isFlowLogger) G::log("BookMarks::select", fPath);
    // return;

    // qDebug() << "BookMarks::select" << fPath;

    // the Library's highlight follows its folder filter (syncFromLibraryFilter)
    if (libraryMode) return;

    // ignore if already selected path
    if (selectedItems().size())
        if (fPath == pathOf(selectedItems().at(0))) return;

    /*  Matched by PATH: two bookmarks can share a folder name ("2024" on two drives),
        and matching the name selected whichever came first. */
    if (bookmarkPaths.contains(fPath)) {
        QTreeWidgetItemIterator it(this);
        while (*it) {
            if (pathOf(*it) == fPath) {
                setCurrentItem(*it);
                return;
            }
            ++it;
        }
    }
    else {
        selectionModel()->clear();
    }
}

void BookMarks::resizeColumns()
{
    if (G::isLogger) G::log("BookMarks::resizeColumns");
    if (showImageCount) {
        QFont font = this->font();
        font.setPointSize(G::strFontSize.toInt());
        QFontMetrics fm(font);
        imageCountColumnWidth = fm.boundingRect("99999").width();
        showColumn(1);
        setColumnWidth(1, imageCountColumnWidth);
    }
    else {
        imageCountColumnWidth = 0;
        hideColumn(1);
    }
    // have to include the width of the decoration foler png
    setColumnWidth(0, width() - G::scrollBarThickness - imageCountColumnWidth - 15);
}

void BookMarks::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log("BookMarks::resizeEvent");
    resizeColumns();
    QTreeWidget::resizeEvent(event);
}

void BookMarks::leaveEvent(QEvent *event)
{
    if (G::isLogger) G::log("BookMarks::leaveEvent");
    delegate->setHoveredIndex(QModelIndex());  // Clear highlight when mouse leaves
    QTreeWidget::leaveEvent(event);
}

void BookMarks::mouseDoubleClickEvent(QMouseEvent *)
{
    if (G::isLogger) G::log("BookMarks::mouseDoubleClickEvent");
    // ignore double mouse clicks (edit/expand)
    return;
}

void BookMarks::mousePressEvent(QMouseEvent *event)
{
/*
    Checks if the application is busy: If the global G::stop flag is set, indicating that
    the application is busy, the function ignores the mouse press event, beeps, shows a
    popup message, and returns immediately.  Note that MW::folderSelectionChange() employs
    a QSignalBlocker for BookMarks and FSTree which blocks this event.

    Checks if a background ingest is in progress: If the global
    G::isRunningBackgroundIngest flag is set, indicating that a background ingest is in
    progress, the function shows a popup message and returns immediately.

    Updates the eject drive menu item: If the mouse press event occurred on a valid item
    in the BookMarks widget, the function updates the eject drive menu item based on the
    path of the item.

    Handles right mouse button presses: If the right mouse button was pressed, the
    function stores the item that was clicked and returns immediately.

    Sets the include subfolders flag: If both the Control and Shift keys were pressed
    when the mouse button was pressed, the function sets the global G::includeSubfolders
    flag to true.

    Triggers the itemPressed event: Finally, the function calls the base class’s
    mousePressEvent function, which triggers the itemPressed event. This event is
    connected to the MW::bookmarkClicked slot, which updates the FSTree widget and
    signals the MW::folderSelectionChange event.
*/
    if (G::isLogger) G::log("BookMarks::mousePressEvent");
    // ignore rapid mouse press if still processing MW::stop
    qint64 ms = rapidClick.elapsed();
    if (ms < 500) {
        event->ignore();
        qApp->beep();
        G::popup->showPopup("Rapid clicks are verboten");
        return;
    }
    rapidClick.restart();

    if (G::stop || G::isModifyingDatamodel) {
        qApp->beep();
        G::popup->showPopup("Busy, try new folder in a sec.", 1000);
        // qApp->processEvents();
        return;
    }
    // do not allow if there is a background ingest in progress
    if (G::isRunningBackgroundIngest) {
        QString msg =
                "There is a background ingest in progress.  When it<br>"
                "has completed the progress bar on the left side of<br>"
                "the status bar will disappear and you can select another<br>"
                "folder."
                ;
        G::popup->showPopup(msg, 5000);
        if (G::useProcessEvents) qApp->processEvents();
        return;
    }

    // context menu is handled in MW::eventFilter
    if (event->button() == Qt::RightButton) {
        QModelIndex idx = indexAt(event->pos());
        QModelIndex idx0 = idx.sibling(idx.row(), 0);
        rightClickItem = itemAt(event->pos());
        rightMouseClickPath = idx0.data(PathRole).toString();
        return;
    }

    // popup to inform modifiers are not used in bookmarks
    if (event->modifiers() & Utilities::modifiersMask) {
        QString msg =
            "Modifier keys for multi-folder selection<br>"
            "only work in the Source panel."
            ;
        G::popup->showPopup(msg, 2000);
        if (G::useProcessEvents) qApp->processEvents();
    }

    /* trigger itemPressed event, connected to MW::bookmarkClicked slot, which updates
       FSTree, which signals MW::folderSelectionChange -- or, in the Library, filters
       the Library to the bookmark  */
    QTreeWidget::mousePressEvent(event);
}

void BookMarks::mouseMoveEvent(QMouseEvent *event)
{
    QModelIndex idx = indexAt(event->pos());
    // same row, column 0 (folder name)
    QModelIndex idx0 = idx.sibling(idx.row(), 0);
    /*
    qDebug() << "Bookmarks::mouseMoveEvent"
             << "idx =" << idx
             << "idx0 =" << idx0
        ;
    //*/
    if (idx0.isValid()) {
        hoverFolderName = idx0.data().toString();
        delegate->setHoveredIndex(idx0);
    } else {
        hoverFolderName = "";
        delegate->setHoveredIndex(QModelIndex());  // No row hovered
    }
    QTreeWidget::mouseMoveEvent(event);
}

void BookMarks::removeBookmark()
{
    if (G::isLogger)
        G::log("BookMarks::removeBookmark", pathOf(rightClickItem));
    if (rightClickItem) {
        bookmarkPaths.remove(pathOf(rightClickItem));
        reloadBookmarks();
    }
}

void BookMarks::contextMenuEvent(QContextMenuEvent *event)
{

}

void BookMarks::dragEnterEvent(QDragEnterEvent *event)
{
    if (G::isLogger) G::log("BookMarks::dragEnterEvent");
    QModelIndexList selectedDirs = selectionModel()->selectedRows();

    bool isInternal;
    event->source() ? isInternal = true : isInternal = false;

    if (!isInternal) {
        QString msg = "Copy to folder.";
        G::popup->showPopup(msg, 0);
    }

    if (selectedDirs.size() > 0) {
        dndOrigSelection = selectedDirs[0];
    }

    // Accept the proposed action and start the drag
    event->acceptProposedAction();

    emit status(false, "Add drag folder to bookmarks.");
}

void BookMarks::dragLeaveEvent(QDragLeaveEvent *event)
{
    if (G::isLogger) G::log("BookMarks::dragLeaveEvent");
    delegate->setHoveredIndex(QModelIndex());  // Clear highlight when mouse leaves
    QApplication::restoreOverrideCursor(); // Restore the original cursor when drag leaves
    // QWidget::dragLeaveEvent(event);
    event->accept();
    G::popup->reset();
    emit status(true);
}

void BookMarks::dragMoveEvent(QDragMoveEvent *event)
{
    if (G::isLogger) G::log("BookMarks::dragMoveEvent");
    QModelIndex idx = indexAt(event->pos());
    // same row, column 0 (folder name)
    QModelIndex idx0 = idx.sibling(idx.row(), 0);
    if (idx0.isValid()) {
        hoverFolderName = idx0.data().toString();
        delegate->setHoveredIndex(idx0);
    } else {
        hoverFolderName = "";
        delegate->setHoveredIndex(QModelIndex());  // No row hovered
    }

    event->acceptProposedAction();
    viewport()->update();  // Refresh view
}

void BookMarks::dropEvent(QDropEvent *event)
{
/*
    - add folder as a bookmark
    - copy or move image files
*/
    QString src = "BookMarks::dropEvent";
    if (G::isLogger) G::log(src);

    G::popup->reset();

    const QMimeData *mimeData = event->mimeData();
    if (!mimeData->hasUrls()) return;

    /*
    qDebug() << "BookMarks::dropEvent"
             << event
             << mimeData->hasUrls() << mimeData->urls();
    //*/

    const QModelIndex dropIdx = indexAt(event->position().toPoint());
    QString dropDir = dropIdx.sibling(dropIdx.row(), 0).data(PathRole).toString();

    QString dPath;      // path to folder
    QFileInfo fInfo = QFileInfo(mimeData->urls().at(0).toLocalFile());

    // if drag is a folder then add to bookmarks
    if (fInfo.isDir()) {
        dPath = fInfo.absoluteFilePath();
        if (dPath.length() == 0) return;
        // trim ending "/"
        int endPos = dPath.length() - 1;
        if (dPath[endPos] == '/') dPath.chop(1);
        if (!bookmarkPaths.contains(dPath)) {
            bookmarkPaths.insert(dPath);
            reloadBookmarks();
        }
        emit status(true);
        return;
    }

    // START MIRRORED CODE SECTION
    // This code section is mirrored in FSTree::dropEvent.
    // Make sure to sync any changes.

    /* Drag and Drop files:

       There are two types of source for the drag operation: Internal (Winnow) and External
       (another program ie finder/explorer).

       1. Internal: Only image files are being dragged, sidecar files are inferred.  Popup
          work.

       2. External: Any file can be dragged: image, sidecar or other.  Popup messages do not work
          because the external program has the operating system window focus.
       */

    G::stopCopyingFiles = false;
    G::isCopyingFiles = true;
    QString srcPath;
    QStringList srcPaths;
    int sidecarCount = 0;
    QString fileCategory;
    QMap<QString, int> fileCategoryCounts {
        {"image", 0},
        {"sidecar", 0},
        {"other", 0}
    };

    // Copy or Move operation
    QString operation;
    if (event->dropAction() == Qt::MoveAction) operation = "Move";
    else operation = "Copy";

    // Number of files (internal = images, external = all files selected)
    int count = event->mimeData()->urls().count();

    // popup for internal drag&drop progress reporting
    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(count);
    QString msg = operation + QString::number(count) +
                  " to " + dropDir +
                  "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popup->showPopup(msg, 0, true, 1);

    QString issue;

    /* Sidecars belonging to images in this same drop. FileOps carries a companion with
       its image, so a sidecar the user ALSO dragged explicitly must not be copied a
       second time -- that would report a spurious "already in destination folder". */
    QSet<QString> droppedCompanions;
    for (int i = 0; i < count; i++) {
        const QString p = event->mimeData()->urls().at(i).toLocalFile();
        if (!metadata->supportedFormats.contains(Utilities::getSuffix(p))) continue;
        const QStringList c = FileOps::companions(p);
        for (const QString &s : c) droppedCompanions.insert(s);
    }

    // iterate files
    for (int i = 0; i < count; i++) {
        G::popup->setProgress(i+1);
        if (G::useProcessEvents) qApp->processEvents(); // processEvents is necessary
        if (G::stopCopyingFiles) {
            break;
        }
        srcPath = event->mimeData()->urls().at(i).toLocalFile();
        if (droppedCompanions.contains(srcPath)) continue;
        QString destPath = dropDir + "/" + Utilities::getFileName(srcPath);

        /* FileOps copies the image AND its sidecars. Previously sidecars were inferred
           only for internal drags, so a file dropped from Finder/Explorer arrived
           without its develop recipe, ratings or labels. */
        bool copied = FileOps::copyFile(srcPath, destPath);

        if (copied) {
            // make list of src files to delete if Qt::MoveAction
            srcPaths << srcPath;
            sidecarCount += FileOps::companions(srcPath).count();
        }
        else if (QFile(destPath).exists()) {
            issue += "<br>" + Utilities::getFileName(srcPath) +
                     " already in destination folder";
        }
    }

    // count file categories
    foreach (srcPath, srcPaths) {
        QString cat = metadata->fileCategory(srcPath, srcPaths);
        fileCategoryCounts[cat]++;
    }

    if (G::stopCopyingFiles) {
        G::popup->setProgressVisible(false);
        G::popup->reset();
        G::popup->showPopup("Terminated " + operation + "operation", 4000);
    }
    else {
        //report on copy/move operation
        QString op;
        operation == "Copy" ? op = "Copied " : op = "Moved ";
        int totFiles = srcPaths.count();

        QString msg = op;
        int imageCount = 0;
        int otherCount = 0;
        if (event->source()) imageCount = srcPaths.count();
        else {
            imageCount = fileCategoryCounts["image"];
            sidecarCount = fileCategoryCounts["sidecar"];
            otherCount = fileCategoryCounts["other"];
        }

        if (imageCount)
            msg += QString::number(imageCount) + " images";
        if (sidecarCount) {
            if (imageCount) msg += ", ";
            msg += QString::number(sidecarCount) + " sidecars";
        }
        if (otherCount) {
            if (imageCount || sidecarCount) msg += ", ";
            msg += QString::number(otherCount) + " other";
            if (otherCount == 1) msg += " file";
            else msg += " files";
        }
        if (totFiles == 0)
            msg += " 0 files.";
        else
            msg += ".";
        if (!issue.isEmpty()) msg += "<br>";
        msg += issue;
        msg += "<p>Press \"ESC\" to close this message";

        emit status(false, msg, src);
        G::popup->setProgressVisible(false);
        G::popup->reset();
        G::popup->showPopup(msg, 4000);
    }
    G::isCopyingFiles = false;
    G::stopCopyingFiles = false;

    /* Refresh for ANY drop. Gating this on CopyAction left the counts stale after a
       move drop, which also adds files to the destination. MW::refresh updates the
       FSTree/BookMarks counts and the datamodel, so files dropped onto a bookmark
       that is a currently selected folder show up right away. */
    emit updateCounts();

    event->acceptProposedAction();

    // END MIRRORED CODE SECTION

}

void BookMarks::howThisWorks()
{
    if (G::isLogger) G::log("BookMarks::howThisWorks");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - How bookmarks work",
                   ":/Docs/bookmarkshelp.html",
                   QSize(700, 600), r, window());
}
