#include "File/fstree.h"

extern QStringList mountedDrives;
QStringList mountedDrives;

/*------------------------------------------------------------------------------
CLASS FSFilter subclassing QSortFilterProxyModel
------------------------------------------------------------------------------*/

FSFilter::FSFilter(QObject *parent) : QSortFilterProxyModel(parent)
{

}

void FSFilter::refresh()
{
    qDebug() << "FSFilter::refresh";
    this->invalidateFilter();
}

bool FSFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
#ifdef Q_OS_WIN
    ///*
    if (!sourceParent.isValid()) {      // if is a drive
        QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
        QString path = idx.data(QFileSystemModel::FilePathRole).toString();
        bool mounted = mountedDrives.contains(path);
        if (!mounted) return false;     // do not accept unmounted drives
    }
    //*/
    return true;
#endif

#ifdef Q_OS_MAC
    if (sourceParent.row() == -1) return true;
    if (!sourceParent.isValid()) return true;

    QString fParentPath = sourceParent.data(QFileSystemModel::FilePathRole).toString();
    QString fPath = sourceParent.model()->index(sourceRow, 0, sourceParent).data(QFileSystemModel::FilePathRole).toString();
    QFileInfo info(fPath);
    /*
    qDebug() << G::t.restart() << "\t" << "fPath" << fPath
             << "fParentPath" << fParentPath
             << "absolutePath" << info.absolutePath()
             << "absoluteFilePath" << info.absoluteFilePath()
             << "isHidden" << info.isHidden();
    */
    if (fParentPath == "/" && (fPath == "/Users" || fPath == "/Volumes")) return true;
    if (fParentPath == "/") return false;
    if (info.isHidden()) return false;
    return true;
#endif

#ifdef Q_OS_LINIX
    return true;
#endif
}

/*------------------------------------------------------------------------------
CLASS FSModel subclassing QFileSystemModel
------------------------------------------------------------------------------*/

/*
   We are subclassing QFileSystemModel in order to add a column for imageCount
   to the model and display the image count beside each folder in the TreeView.
*/

FSModel::FSModel(QWidget *parent, Metadata &metadata, bool &combineRawJpg)
                 : QFileSystemModel(parent),
                   combineRawJpg(combineRawJpg),
                   metadata(metadata)
{
    QStringList *fileFilters = new QStringList;
    dir = new QDir();

    fileFilters->clear();
    foreach (const QString &str, metadata.supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    count.clear();
    combineCount.clear();

    this->iconProvider()->setOptions(QFileIconProvider::DontUseCustomDirectoryIcons);
}

void FSModel::insertCount(QString dPath, QString value)
{
    count[dPath] = value;
}

void FSModel::insertCombineCount(QString dPath, QString value)
{
    combineCount[dPath] = value;
}


//bool FSModel::event(QEvent *event)
//{
//    qDebug() << "FSModel::event" << event;
//    return QFileSystemModel::event(event);
//}

bool FSModel::hasChildren(const QModelIndex &parent) const
{
    if (parent.column() > 0)
		return false;

	if (!parent.isValid()) // drives
		return true;

    // return false if item can't have children
	if (parent.flags() &  Qt::ItemNeverHasChildren) {
		return false;
	}

	// return if at least one child exists
	return QDirIterator(filePath(parent), filter() | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags).hasNext();
}

int FSModel::columnCount(const QModelIndex &parent) const
{
    // add a column for the image count
    return QFileSystemModel::columnCount(parent) + 1;
}

QVariant FSModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // add header text for the additional image count column
    if (orientation == Qt::Horizontal && section == imageCountColumn)
    {
        if (role == Qt::DisplayRole) return QVariant("#");
        if (role == Qt::EditRole) return QVariant("#");
        return QVariant();
     }
     else
        return QFileSystemModel::headerData(section, orientation, role);
}

void FSModel::refresh(const QModelIndex &index)
{
    beginResetModel();
    endResetModel();
    return;
    /*
    QList<int> roles;
    roles << Qt::DisplayRole;
    emit dataChanged(index, index, roles);
    qDebug() << "FSModel::refresh index" << index << testIdx;
    */
}

void FSModel::refresh(const QString &dPath)
{
    // not being used
    const QModelIndex idx = index(dPath, imageCountColumn);
    QList<int> roles;
    roles << Qt::DisplayRole;
    emit dataChanged(idx, idx, roles);
    // qDebug() << "FSModel::refresh dPath" << dPath;
}

QVariant FSModel::data(const QModelIndex &index, int role) const
{
/*
    Return image count for each folder by looking it up in the QHash count which is built
    in FSTree::getImageCount and referenced here. This is much faster than performing the
    image count "on-the-fly" here, which causes scroll latency.
*/
    if (index.column() == imageCountColumn) {
        if (role == Qt::DisplayRole && showImageCount) {
            QString dPath = QFileSystemModel::data(index, QFileSystemModel::FilePathRole).toString();

            static quint64 counter = 0;
//            qDebug() << "FSModel::data" << ++counter << dPath;

            /*
            How to save/cast a const variable:
            const QModelIndex *tIdx = &index;
            if (dPath == "/Users/roryhill/Pictures/_test") {
                QModelIndex ttIdx;
                ttIdx = *const_cast<QModelIndex*>(tIdx);
                testIdx = ttIdx;    // in hdr: mutable QModelIndex testIdx;
            }
            //*/
            dir->setPath(dPath);
            int n = 0;
            QString nStr = "0";
            if (combineRawJpg) {
                if (!forceRefresh) {
                    if (combineCount.contains(dPath))
                        return combineCount.value(dPath);
                }
                // iterate through files in folder
                QListIterator<QFileInfo> i(dir->entryInfoList());
                while (i.hasNext()) {
                    QFileInfo info = i.next();
                    QString fPath = info.path();
                    QString baseName = info.baseName();
                    QString suffix = info.suffix().toLower();
                    QString jpgPath = fPath + "/" + baseName + ".jpg";
                    if (metadata.hasJpg.contains(suffix)) {
                        if (dir->entryInfoList().contains(QFileInfo(jpgPath))) continue;
                    }
                    n++;
                }
                nStr = QString::number(n, 'f', 0);
                if (combineCount.contains(dPath)) {
                    // has the count changed?
                    if (combineCount.value(dPath) != nStr) {
                        // update hash value (insert adds or replaces in QHash)
                        combineCount.insert(dPath, nStr);
                        // signal changed value to bookmarks
                        emit update();
                    }
                }
                else {
                    // add hash value
                    combineCount.insert(dPath, nStr);
                }
            }
            // not combineRawJpg
            else {
                // dir is filtered to only include eligible image files
                n = dir->entryInfoList().size();
                nStr = QString::number(n, 'f', 0);
                if (count.contains(dPath)) {
                    // has the count changed?
                    if (count.value(dPath) != nStr) {
                        // update hash value (insert adds or replaces in QHash)
                        count.insert(dPath, nStr);
                        // signal changed value to bookmarks
                        emit update();
                    }
                }
                else {
                    // add hash value
                    count.insert(dPath, nStr);
                }
            }
            /*
            qDebug() << "FSModel::data image count processed" << combineCount.value(dPath)
                     << dPath << index << "count =" << nStr;
                     //*/
            return nStr;
        }
        if (role == Qt::TextAlignmentRole) {
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        }
        else {
            return QVariant();
        }
    }

    // return tooltip for folder path
    if (index.column() == 0) {
        if (role == Qt::ToolTipRole) {
            return QFileSystemModel::data(index, QFileSystemModel::FilePathRole);
        }
        /*
        else if (role == Qt::DecorationRole) {
            QFileInfo info = fileInfo(index);
            qDebug() << "FSModel::data"
                     << "isDir =" << info.isDir()
                     << "isRoot =" << info.isRoot()
                     << info.baseName();
            if (info.isDir()) {
                if (info.absoluteFilePath() == QDir::rootPath())
                    return iconProvider()->icon(QFileIconProvider::Computer);
                else if (info.isRoot())
                    return iconProvider()->icon(QFileIconProvider::Drive);
                else
                   return iconProvider()->icon(QFileIconProvider::Folder);
            }
            else if (info.isFile())
                return iconProvider()->icon(QFileIconProvider::File);
            else
                return iconProvider()->icon(QFileIconProvider::Drive);
        }
        //*/
        else {
            return QFileSystemModel::data(index, role);
        }
    }
    // return parent class data
    return QFileSystemModel::data(index, role);
}

/*------------------------------------------------------------------------------
CLASS FSTree subclassing QTreeView
------------------------------------------------------------------------------*/

FSTree::FSTree(QWidget *parent, Metadata *metadata) : QTreeView(parent)
{
    if (G::isLogger) G::log("FSTree::FSTree");
    this->metadata = metadata;
    fileFilters = new QStringList;
    dir = new QDir();
    viewport()->setObjectName("fsTreeViewPort");
    setObjectName("fsTree");

    // create model and filter
    createModel();

    // setup treeview
    for (int i = 1; i <= 3; ++i) {
        hideColumn(i);
    }

    setRootIsDecorated(true);
    setSortingEnabled(false);
    setHeaderHidden(true);
    sortByColumn(0, Qt::AscendingOrder);
    setIndentation(16);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::InternalMove);

    QStringList *fileFilters = new QStringList;
    dir = new QDir();

    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    // mouse wheel is spinning
    wheelTimer.setSingleShot(true);
    connect(&wheelTimer, &QTimer::timeout, this, &FSTree::wheelStopped);

    connect(this, SIGNAL(expanded(const QModelIndex&)), this, SLOT(expand(const QModelIndex&)));
}

void FSTree::createModel()
{
/*
    Create the model and filter in a separate function as it is also used to refresh
    the folders by deleting the model and re-creating it.
*/
    if (G::isLogger) G::log("FSTree::createModel");
    fsModel = new FSModel(this, *metadata, combineRawJpg);
    fsModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
    fsModel->setRootPath("");  //
    //fsModel->setRootPath(fsModel->myComputer().toString());

    // get mounted drives only
    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
          /*
          qDebug() << G::t.restart() << "\t" << "FSTree::createModel  " << storage.rootPath()
                 << "storage.isValid()" << storage.isValid()
                 << "storage.isReady()" << storage.isReady()
                 << "storage.isReadOnly()" << storage.isReadOnly();
                 //                 */
        if (storage.isValid() && storage.isReady()) {
            if (!storage.isReadOnly()) {
                mountedDrives << storage.rootPath();
            }
        }
    }

    fsFilter = new FSFilter(fsModel);
    fsFilter->setSourceModel(fsModel);
    fsFilter->setSortRole(QFileSystemModel::FilePathRole);

    // apply model to treeview
    setModel(fsFilter);



    QAbstractFileIconProvider *iconProvider = fsModel->iconProvider();
    QIcon icon = fsModel->iconProvider()->icon(QFileIconProvider::Folder);
    // qDebug() << icon(QFileInfo(QDir::currentPath()));
}

void FSTree::refreshModel()
{
/*
    Most common use is to refresh the folder panel after inserting a USB connected
    media card.
*/
    if (G::isLogger) G::log("FSTree::refreshModel");
    //qDebug() << "FSTree::refreshModel";
    mountedDrives.clear();
    // get mounted drives only
    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady()) {
            if (!storage.isReadOnly()) {
                mountedDrives << storage.rootPath();
            }
        }
    }
    fsModel->refresh(fsModel->index(0,0));
    setFocus();
    select(G::currRootFolder);
}

bool FSTree::isShowImageCount()
{
    if (G::isLogger) G::log("FSTree::isShowImageCount");
    return fsModel->showImageCount;
}

void FSTree::setShowImageCount(bool showImageCount)
{
    fsModel->showImageCount = showImageCount;
}

void FSTree::scrollToCurrent()
{
/*

*/
    if (G::isLogger) G::log("FSTree::scrollToCurrent");
    QModelIndex idx = getCurrentIndex();
    if (idx.isValid()) scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

bool FSTree::select(QString dirPath)
{
    if (G::isLogger) G::log("FSTree::select");

    if (dirPath == "") return false;

    QDir test(dirPath);
    if (test.exists()) {
//        QModelIndex idx = fsFilter->mapFromSource(fsModel->index(dirPath));
//        setCurrentIndex(idx);
//        selectionModel()->select(idx,QItemSelectionModel::Select);
        setCurrentIndex(fsFilter->mapFromSource(fsModel->index(dirPath)));
        scrollToCurrent();
        return true;
    }
    else {
        G::popUp->showPopup("The folder path " + dirPath + " was not found.", 2000);
        return false;
    }
}

//QModelIndex FSTree::index(QString dirPath)
//{
//    if (G::isLogger) G::log("FSTree::index");
//    return fsFilter->mapFromSource(fsModel->index(dirPath));
//}

QModelIndex FSTree::getCurrentIndex()
{
    if (G::isLogger) G::log("FSTree::getCurrentIndex");
    QModelIndex idx;
    if (selectedIndexes().size() > 0)
        idx = selectedIndexes().first();
    else idx = fsModel->index(-1, -1, QModelIndex());
    return idx;
}

void FSTree::resizeColumns()
{
    if (G::isLogger) G::log("FSTree::resizeColumns");
    if (fsModel->showImageCount) {
        QFont font = this->font();
        font.setPointSize(G::strFontSize.toInt());
        QFontMetrics fm(font);
        imageCountColumnWidth = fm.boundingRect("99999").width();
        showColumn(4);
        setColumnWidth(4, imageCountColumnWidth);
    }
    else {
        imageCountColumnWidth = 0;
        hideColumn(4);
    }
    // have to include the width of the decoration folder png
    setColumnWidth(0, width() - G::scrollBarThickness - imageCountColumnWidth - 10);
}

void FSTree::expand(const QModelIndex &/*idx*/)
{
/*
    Get the image count when a user expands the folder hierarchy.  This can also occur when a
    bookmark is selected and the matching folder is shown in the FSTree.
*/
    if (G::isLogger) G::log("FSTree::expand");
//    qDebug() << "FSTree::expand" << idx << t.elapsed();
//    if (t.elapsed() > 25) {
//        QString src = "FSTree::expand";
//        QTimer::singleShot(50, [this, src]() {getVisibleImageCount(src);});
//    }
//    t.restart();
}

//void FSTree::expandAll(const QModelIndex &idx)
//{
///*
//    .
//*/
//    if (G::isLogger) G::log("FSTree::expandAll");
//    this->expandAll(idx);
//}

void FSTree::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log("FSTree::resizeEvent");
    resizeColumns();
}

void FSTree::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    if (G::isLogger) G::log("FSTree::selectionChanged");
    QTreeView::selectionChanged(selected, deselected);
//    emit abortLoadDataModel();
    emit selectionChange();
//    QtConcurrent::run(this, &FSTree::updateVisibleImageCount);
//    qDebug() << "FSTree::selectionChanged" << count;
}

void FSTree::keyPressEvent(QKeyEvent *event){
    // prevent default key actions
}

void FSTree::enterEvent(QEnterEvent *event)
{
    wheelSpinningOnEntry = G::wheelSpinning;
    /*
    qDebug() << "ImageView::enterEvent" << objectName()
             << "G::wheelSpinning =" << G::wheelSpinning
             << "wheelSpinningOnEntry =" << wheelSpinningOnEntry
        ; //*/
}

void FSTree::wheelEvent(QWheelEvent *event)
{
/*
    Not being used.
    Mouse wheel scrolling / trackpad swiping = next/previous image.
*/
    if (wheelSpinningOnEntry && G::wheelSpinning) {
        //qDebug() << "ImageView::wheelEvent ignore because wheelSpinningOnEntry && G::wheelSpinning";
        return;
    }
    wheelSpinningOnEntry = false;
    // set spinning flag in case mouse moves to another object while still spinning
    G::wheelSpinning = true;
    // singleshot to flag when wheel has stopped spinning
    wheelTimer.start(100);

    // add code to scroll FSTree if desired
    QTreeView::wheelEvent(event);
}

void FSTree::leaveEvent(QEvent *event)
{
    // not being used
    wheelSpinningOnEntry = false;
}

void FSTree::wheelStopped()
{
    G::wheelSpinning = false;
    wheelSpinningOnEntry = false;
    //qDebug() << "ImageView::wheelStopped";
}

void FSTree::mousePressEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log("FSTree::mousePressEvent");

//    QTreeView::mousePressEvent(event);
//    qApp->processEvents();

    if (G::stop) {
        G::popUp->showPopup("Busy, try new folder in a sec.", 1000);
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
    QString folderName = "";
    if (idx.isValid()) {
        path = idx.data(Qt::ToolTipRole).toString();
        folderName = QFileInfo(path).fileName();
    }
    emit renameEjectAction(path);
    emit renameEraseMemCardContextAction(path);
    emit renamePasteContextAction(folderName);

    if (event->button() == Qt::RightButton) {
        rightMouseClickPath = path;
        return;
    }

    // load all subfolders images
    if ((event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier)) {
        G::includeSubfolders = true;
    }

    QTreeView::mousePressEvent(event);
}

void FSTree::mouseReleaseEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log("FSTree::mouseReleaseEvent");
    QTreeView::mouseReleaseEvent(event);
}

void FSTree::mouseMoveEvent(QMouseEvent *event)
{
    QTreeView::mouseMoveEvent(event);
    QModelIndex idx = indexAt(event->pos());
}

void FSTree::dragEnterEvent(QDragEnterEvent *event)
{
    if (G::isLogger) G::log("FSTree::dragEnterEvent");
//    qDebug() << "FSTree::dragEnterEvent" << event-> dropAction() << event->modifiers();

    QModelIndexList selectedDirs = selectionModel()->selectedRows();
	if (selectedDirs.size() > 0) {
		dndOrigSelection = selectedDirs[0];
	}
    event->acceptProposedAction();
}

void FSTree::dragMoveEvent(QDragMoveEvent *event)
{
    setCurrentIndex(indexAt(event->pos()));
}

void FSTree::dropEvent(QDropEvent *event)
{
/*
    Copy files to FSTree folder.  If Qt::MoveAction then delete the source files after
    the copy operation.  If DnD is internal then also copy/move any sidecar files.
*/
    QString src = "FSTree::dropEvent";
    if (G::isLogger) G::log(src);

    const QMimeData *mimeData = event->mimeData();
    if (!mimeData->hasUrls()) return;

    QString dropDir = indexAt(event->pos()).data(QFileSystemModel::FilePathRole).toString();

    // START MIRRORED CODE SECTION
    // This code section is mirrored in FSTREE::dropEvent.  Make sure to sync any changes.

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
    QString operation = "Copy";
    if (event->dropAction() == Qt::MoveAction) operation = "Move";

    // Number of files (internal = images, external = all files selected)
    int count = event->mimeData()->urls().count();

    // popup for internal drag&drop progress reporting
    G::popUp->setProgressVisible(true);
    G::popUp->setProgressMax(count);
    QString msg = operation + QString::number(count) +
                  " to " + dropDir +
                  "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popUp->showPopup(msg, 0, true, 1);

    // iterate files
    for (int i = 0; i < count; i++) {
        G::popUp->setProgress(i+1);
        if (G::useProcessEvents) qApp->processEvents();        // processEvents is necessary
        if (G::stopCopyingFiles) {
            break;
        }
        srcPath = event->mimeData()->urls().at(i).toLocalFile();
        QString destPath = dropDir + "/" + Utilities::getFileName(srcPath);
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
    }

    // count file categories
    foreach (srcPath, srcPaths) {
        QString cat = metadata->fileCategory(srcPath, srcPaths);
        fileCategoryCounts[cat]++;
    }

    if (G::stopCopyingFiles) {
        G::popUp->setProgressVisible(false);
        G::popUp->reset();
        G::popUp->showPopup("Terminated " + operation + "operation", 4000);
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
            msg += "0 files.";
        else
            msg += ".";

        emit status(false, msg, src);
        G::popUp->setProgressVisible(false);
        G::popUp->reset();
        G::popUp->showPopup(msg, 4000);
    }
    G::isCopyingFiles = false;
    G::stopCopyingFiles = false;

    // if internal (Winnow) and move then delete source files
    if (isInternal && event->dropAction() == Qt::MoveAction) {
        setCurrentIndex(dndOrigSelection);
        if (srcPaths.count()) {
            // deleteFiles also deletes sidecars
            emit deleteFiles(srcPaths);
        }
    }
    // END MIRRORED CODE SECTION

    // if external source
    if (!event->source()) {
        refreshModel();

        // if the drag is into the current FSTree folder then need to reload
        // QString currDir = currentIndex().data(Qt::ToolTipRole).toString();
        if (G::currRootFolder == dropDir) {
            // QString firstPath = event->mimeData()->urls().at(0).toLocalFile();
            emit folderSelection(dropDir);
        }
        event->acceptProposedAction();
    }
    else {
        select(G::currRootFolder);
    }

    /*
    QString fstreeStr = "FSTree";
    bool dirOp = (event->source()->metaObject()->className() == fstreeStr);
    emit dropOp(event->keyboardModifiers(), dirOp, event->mimeData()->urls().at(0).toLocalFile());
    setCurrentIndex(dndOrigSelection);
    */
}

