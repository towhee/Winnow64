#include "File/bookmarks.h"

class BookDelegate : public QStyledItemDelegate
{
public:
    explicit BookDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &index) const override
    {
        static int count = 0;
        count++;
        index.isValid();          // suppress compiler warning
        int height = qRound(G::strFontSize.toInt() * 1.5 * G::ptToPx);
        return QSize(option.rect.width(), height);
    }
};

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

BookMarks::BookMarks(QWidget *parent, Metadata *metadata, bool showImageCount,
                     bool &combineRawJpg)
                   : QTreeWidget(parent),
                     combineRawJpg(combineRawJpg)
{
    if (G::isLogger) G::log("BookMarks::BookMarks");
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

    setAcceptDrops(true);
	setDragEnabled(false);
	setDragDropMode(QAbstractItemView::DropOnly);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setRootIsDecorated(false);
    setColumnCount(2);
    setHeaderHidden(true);
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);

    setItemDelegate(new BookDelegate(this));
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
    count();
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

void BookMarks::update()
{
/*
    Update the image count for each folder bookmarked;
*/
    if (G::isLogger)
        G::log("BookMarks::update");
    count();
    /* update only changed folder (to do if required)
    // pass dPath and make FSModel::count and FSModel::combineCount public
    QTreeWidgetItemIterator it(this);
    while (*it) {
        QString path = (*it)->toolTip(0);
        qDebug() << "BookMarks::update" << path << dPath;
        QString countStr;
        if (combineRawJpg)
            countStr = QString::number(fsTree->fsModel->combineCount[dPath]);
        else
            countStr = QString::number(fsTree->fsModel->count[dPath]);
        (*it)->setText(1, countStr);
        ++it;
    }
    //*/
}

void BookMarks::count()
{
    if (G::isLogger) G::log("BookMarks::count");
     QTreeWidgetItemIterator it(this);
     while (*it) {
         QString path = (*it)->toolTip(0);
         int count = 0;
         dir->setPath(path);
         if (combineRawJpg) {
             QListIterator<QFileInfo> i(dir->entryInfoList());
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
         else count = dir->entryInfoList().size();
         (*it)->setText(1, QString::number(count));
         ++it;
     }
}

void BookMarks::select(QString fPath)
{
/*
    This is called from MW::folderSelectionChange to attempt to sync bookmarks with
    the FSTree folders view.
*/
    if (G::isLogger)
         G::log("BookMarks::select");

    // ignore if already selected path (in tooltip)
    if (selectedItems().size())
        if (fPath == selectedItems().at(0)->toolTip(0)) return;

    if (bookmarkPaths.contains(fPath)) {
        QList <QTreeWidgetItem *> items;
        items = findItems(QFileInfo(fPath).fileName(), Qt::MatchExactly);
        if (items.length() > 0) {
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

void BookMarks::mouseDoubleClickEvent(QMouseEvent *)
{
    if (G::isLogger) G::log("BookMarks::mouseDoubleClickEvent");
    // ignore double mouse clicks (edit/expand)
    return;
}

void BookMarks::mousePressEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log("BookMarks::mousePressEvent");
    // rapid mouse press ignore if still running MW::stopAndClearAll
    if (G::stop) {
        qApp->beep();
        G::popUp->showPopup("Busy, try new bookmark in a sec.", 1000);
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
        G::popUp->showPopup(msg, 5000);
        return;
    }

    // update eject drive menu item if ejectable
    QModelIndex idx = indexAt(event->pos());
    QString path = "";
    if (idx.isValid()) {
        path = idx.data(Qt::ToolTipRole).toString();
        // change selection, does not trigger anything
        setCurrentIndex(idx);
        qApp->processEvents();
    }
    emit renameEjectAction(path);

    if (event->button() == Qt::RightButton) {
        return;
    }

    // load all subfolders images
    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier)) {
        G::includeSubfolders = true;
    }

    // trigger itemPressed event, connected to MW::bookmarkClicked slot, which updates
    // FSTree, which signals MW::folderSelectionChange
    QTreeWidget::mousePressEvent(event);
}

void BookMarks::removeBookmark()
{
    if (G::isLogger) G::log("BookMarks::removeBookmark");
    if (rightClickItem) {
//        qDebug() << "BookMarks::removeBookmark"<< rightClickItem->toolTip(0);
        bookmarkPaths.remove(rightClickItem->toolTip(0));
        reloadBookmarks();
    }

//	if (selectedItems().size() == 1) {
//        bookmarkPaths.remove(selectedItems().at(0)->toolTip(0));
//		reloadBookmarks();
//	}
}

void BookMarks::contextMenuEvent(QContextMenuEvent *event)
{

}

void BookMarks::dragEnterEvent(QDragEnterEvent *event)
{
    if (G::isLogger) G::log("BookMarks::dragEnterEvent");
    QModelIndexList selectedDirs = selectionModel()->selectedRows();

	if (selectedDirs.size() > 0) {
		dndOrigSelection = selectedDirs[0];
	}
	event->acceptProposedAction();
}

void BookMarks::dragMoveEvent(QDragMoveEvent *event)
{
    if (G::isLogger) G::log("BookMarks::dragMoveEvent");
    setCurrentIndex(indexAt(event->pos()));
}

void BookMarks::dropEvent(QDropEvent *event)
{
/*
    - add folder as a bookmark
    - copy or move image files
*/
    if (G::isLogger) G::log("BookMarks::dropEvent");

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
        return;
    }

    // if drag is files: move or copy to bookmark folder

    /* This code section is mirrored in FSTREE::dropEvent.  Make sure to sync any
       changes. */
    G::stopCopyingFiles = false;
    G::isCopyingFiles = true;
    QStringList srcPaths;

    // popup
    int count = event->mimeData()->urls().count();
    QString operation = "Copying ";
    if (event->source() && event->dropAction() == Qt::MoveAction) operation = "Moving ";
    G::popUp->setProgressVisible(true);
    G::popUp->setProgressMax(count);
    QString txt = operation + QString::number(count) +
                  " to " + dropDir +
                  "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popUp->showPopup(txt, 0, true, 1);

    for (int i = 0; i < count; i++) {
        G::popUp->setProgress(i+1);
        qApp->processEvents();        // processEvents is necessary
        if (G::stopCopyingFiles) {
            break;
        }
        QString srcPath = event->mimeData()->urls().at(i).toLocalFile();
        QString destPath = dropDir + "/" + Utilities::getFileName(srcPath);
        bool copied = QFile::copy(srcPath, destPath);
        /*
        qDebug() << "BookMarks::dropEvent"
                 << "Copy" << srcPath
                 << "to" << destPath << "Copied:" << copied
                 << event->dropAction();  //*/
        if (copied) {
            // make list of src files to delete if Qt::MoveAction
            srcPaths << srcPath;
            // copy any sidecars if internal drag operation
            if (event->source()) {
                QStringList srcSidecarPaths = Utilities::getPossibleSidecars(srcPath);
                foreach (QString srcSidecarPath, srcSidecarPaths) {
                    if (QFile(srcSidecarPath).exists()) {
                        QString destSidecarPath = dropDir + "/" + Utilities::getFileName(srcSidecarPath);
                        QFile::copy(srcSidecarPath, destSidecarPath);
                    }
                }
            }
        }
    }
    if (G::stopCopyingFiles) {
        G::popUp->setProgressVisible(false);
        G::popUp->end();
        G::popUp->showPopup("Terminated " + operation + "operation", 2000);
    }
    else {
        G::popUp->setProgressVisible(false);
        G::popUp->end();
    }
    G::isCopyingFiles = false;
    G::stopCopyingFiles = false;

    // if Winnow source and QMoveAction
    if (event->source() && event->dropAction() == Qt::MoveAction) {
        setCurrentIndex(dndOrigSelection);
        if (srcPaths.count()) {
            // deleteFiles also deletes sidecars
            emit deleteFiles(srcPaths);
        }
    }
    // End mirrored code section

    if (G::currRootFolder == dropDir) {
        QString firstPath = event->mimeData()->urls().at(0).toLocalFile();
        emit folderSelection();
    }

    // update bookmarks folder count
    this->count();
}
