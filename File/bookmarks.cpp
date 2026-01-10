#include "File/bookmarks.h"
#include "Main/global.h"

/*
A QStringList of paths to bookmarked folders is displayed as top level items
in a QWidgetTree in column 0.  Column 1 holds a count of the readable image files
in the folder.

When the user mouse clicks on one of the folders the itemClicked signal is sent
to the MW slot bookmarkClicked along with the tooltip, which holds the path
string.  Since the user could also click in the count column, it's tooltip is
also the path string.

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
    item->setToolTip(0, itemPath);
    insertTopLevelItem(0, item);
    dir->setPath(itemPath);
    item->setToolTip(1, itemPath);
    item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    if (!QFileInfo::exists(itemPath)) {
        item->setForeground(0, QBrush(QColor(G::disabledColor)));
    }
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

void BookMarks::updateCount()
{
/*
     Update the image count for all folders in BookMarks
*/
    if (G::isLogger) G::log("BookMarks::count");
     QTreeWidgetItemIterator it(this);
     while (*it) {
         QString path = (*it)->toolTip(0);
         int count = 0;
         dir->setPath(path);
         QListIterator<QFileInfo> i(dir->entryInfoList());
         if (combineRawJpg) {
             while (i.hasNext()) {
                 QFileInfo info = i.next();
                 if (!info.size()) continue;
                 QString fPath = info.path();
                 QString baseName = info.baseName();
                 QString suffix = info.suffix().toLower();
                 QString jpgPath = fPath + "/" + baseName + ".jpg";
                 if (metadata->hasJpg.contains(suffix)) {
                     if (dir->entryInfoList().contains(QFileInfo(jpgPath))) continue;
                 }
                 count++;
             }
         }
         else {
             while (i.hasNext()) {
                 QFileInfo info = i.next();
                 if (info.size()) count++;
             }
         }
         (*it)->setText(1, QString::number(count));
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
     QTreeWidgetItemIterator it(this);
     while (*it) {
         QString path = (*it)->toolTip(0);
         if (path == dPath) {
             int count = 0;
             dir->setPath(path);
             QListIterator<QFileInfo> i(dir->entryInfoList());
             if (combineRawJpg) {
                 while (i.hasNext()) {
                     QFileInfo info = i.next();
                     QString fPath = info.path();
                     QString baseName = info.baseName();
                     QString suffix = info.suffix().toLower();
                     QString jpgPath = fPath + "/" + baseName + ".jpg";
                     if (metadata->hasJpg.contains(suffix)) {
                         if (dir->entryInfoList().contains(QFileInfo(jpgPath))) continue;
                     }
                     count++;
                 }
             }
             else {
                 while (i.hasNext()) {
                     QFileInfo info = i.next();
                     if (info.size()) count++;
                 }
             }
             (*it)->setText(1, QString::number(count));
             return;
         }
         ++it;
     }
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

    // ignore if already selected path (in tooltip)
    if (selectedItems().size())
        if (fPath == selectedItems().at(0)->toolTip(0)) return;

    if (bookmarkPaths.contains(fPath)) {
        QList <QTreeWidgetItem *> items;
        items = findItems(QFileInfo(fPath).fileName(), Qt::MatchExactly);
        if (items.length() > 0) {
            // qDebug() << "BookMarks::select" << fPath;
            setCurrentItem(items[0]);
            setCurrentIndex(selectedIndexes().at(0));
            //count();  // big slowdown
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
        rightMouseClickPath = idx0.data(Qt::ToolTipRole).toString();
        return;
    }

    // popup to inform modifiers are not used in bookmarks
    if (event->modifiers() & Utilities::modifiersMask) {
        QString msg =
            "Modifier keys for multi-folder selection<br>"
            "only works in the Folder panel."
            ;
        G::popup->showPopup(msg, 2000);
        if (G::useProcessEvents) qApp->processEvents();
    }

    /* trigger itemPressed event, connected to MW::bookmarkClicked slot, which updates
       FSTree, which signals MW::folderSelectionChange  */
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
        G::log("BookMarks::removeBookmark", rightClickItem->toolTip(0));
    if (rightClickItem) {
        bookmarkPaths.remove(rightClickItem->toolTip(0));
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

    QString dropDir = indexAt(event->pos()).data(Qt::ToolTipRole).toString();

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
    bool isInternal;
    event->source() ? isInternal = true : isInternal = false;
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
    // iterate files
    for (int i = 0; i < count; i++) {
        G::popup->setProgress(i+1);
        if (G::useProcessEvents) qApp->processEvents();        // processEvents is necessary
        if (G::stopCopyingFiles) {
            break;
        }
        srcPath = event->mimeData()->urls().at(i).toLocalFile();
        QString destPath = dropDir + "/" + Utilities::getFileName(srcPath);

        // copy the files
        bool copied = QFile::copy(srcPath, destPath);

        if (copied) {
            // make list of src files to delete if Qt::MoveAction
            srcPaths << srcPath;
            // infer and copy/move any sidecars if internal drag operation
            if (isInternal) {
                QStringList srcSidecarPaths = Utilities::getSidecarPaths(srcPath);
                sidecarCount += srcSidecarPaths.count();
                foreach (QString srcSidecarPath, srcSidecarPaths) {
                    if (QFile(srcSidecarPath).exists()) {
                        QString destSidecarPath = dropDir + "/" + Utilities::getFileName(srcSidecarPath);
                        QFile::copy(srcSidecarPath, destSidecarPath);
                    }
                }
            }
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

    // update folder image counts if only copy
    if (event->dropAction() == Qt::CopyAction) emit updateCounts();

    event->acceptProposedAction();

    // END MIRRORED CODE SECTION

}
