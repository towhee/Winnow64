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
    /*
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
//                   count(count),
//                   combineCount(combineCount),
                   metadata(metadata)
{
//    this->metadata = metadata;
    QStringList *fileFilters = new QStringList;
    dir = new QDir();

    fileFilters->clear();
    foreach (const QString &str, metadata.supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    count.clear();
    combineCount.clear();
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
//    qDebug() << CLASSFUNCTION << event;
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

    QList<int> roles;
    roles << Qt::DisplayRole;
    emit dataChanged(index, index, roles);
//    emit dataChanged(testIdx, testIdx);
    qDebug() << "FSModel::refresh" << index << testIdx;
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
    if (G::isLogger) G::log(CLASSFUNCTION);
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

    setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::InternalMove);

    QStringList *fileFilters = new QStringList;
    dir = new QDir();
//    this->showImageCount = showImageCount;

    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    connect(this, SIGNAL(expanded(const QModelIndex&)), this, SLOT(expand(const QModelIndex&)));
}

void FSTree::createModel()
{
/*
    Create the model and filter in a separate function as it is also used to refresh
    the folders by deleting the model and re-creating it.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    fsModel = new FSModel(this, *metadata, /*count, combineCount,*/ combineRawJpg);
    fsModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
    fsModel->setRootPath(fsModel->myComputer().toString());

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
}

void FSTree::refreshModel()
{
/*
    Most common use is to refresh the folder panel after inserting a USB connected
    media card.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    setFocus();
    select(G::currRootFolder);
}

bool FSTree::isShowImageCount()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    QModelIndex idx = getCurrentIndex();
    if (idx.isValid()) scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

bool FSTree::select(QString dirPath)
{
    if (G::isLogger) G::log(CLASSFUNCTION);

    if (dirPath == "") return false;

    QDir test(dirPath);
    if (test.exists()) {
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
//    if (G::isLogger) G::log(CLASSFUNCTION);
//    return fsFilter->mapFromSource(fsModel->index(dirPath));
//}

QModelIndex FSTree::getCurrentIndex()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QModelIndex idx;
    if (selectedIndexes().size() > 0)
//        idx = fsFilter->mapFromSource(selectedIndexes().first());
        idx = selectedIndexes().first();
    else idx = fsModel->index(-1, -1, QModelIndex());
    return idx;
}

void FSTree::resizeColumns()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (fsModel->showImageCount) {
        QFont font = this->font();
        font.setPointSize(G::fontSize.toInt());
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
    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << idx << t.elapsed();
//    if (t.elapsed() > 25) {
//        QString src = CLASSFUNCTION;
//        QTimer::singleShot(50, [this, src]() {getVisibleImageCount(src);});
//    }
//    t.restart();
}

void FSTree::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    resizeColumns();
}

void FSTree::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QTreeView::selectionChanged(selected, deselected);
//    emit abortLoadDataModel();
    emit selectionChange();
//    QtConcurrent::run(this, &FSTree::updateVisibleImageCount);
//    qDebug() << CLASSFUNCTION << count;
}

void FSTree::mousePressEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);

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

    // ignore right mouse clicks (context menu)
//    if (event->button() == Qt::RightButton) return;
    if (event->button() == Qt::RightButton) {
        rightClickIndex = indexAt(event->pos());
        if (rightClickIndex.isValid()) {
            rightMouseClickPath = rightClickIndex.data(Qt::ToolTipRole).toString();
        }
        return;
    }

    QTreeView::mousePressEvent(event);
}

void FSTree::mouseReleaseEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QTreeView::mouseReleaseEvent(event);
}

void FSTree::mouseMoveEvent(QMouseEvent *event)
{
    QTreeView::mouseMoveEvent(event);
//    QModelIndex idx = indexAt(event->pos());
}

void FSTree::dragEnterEvent(QDragEnterEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
//    qDebug() << CLASSFUNCTION << event-> dropAction() << event->modifiers();

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
    if (G::isLogger) G::log(CLASSFUNCTION);
    const QMimeData *mimeData = event->mimeData();
    /*
    qDebug() << CLASSFUNCTION
             << "event->source() =" << event->source()
             << event
             << mimeData->hasUrls() << mimeData->urls();
    //*/

    /*  This code section is mirrored in BookMarks::dropEvent.  Make sure to sync any
        changes. */
    G::stopCopyingFiles = false;
    G::isCopyingFiles = true;
    QString dropDir = indexAt(event->pos()).data(QFileSystemModel::FilePathRole).toString();
    QStringList srcPaths;
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
        // processEvents is necessary
        qApp->processEvents();
        if (G::stopCopyingFiles) {
            break;
        }
        QString srcPath = event->mimeData()->urls().at(i).toLocalFile();
        QString destPath = dropDir + "/" + Utilities::getFileName(srcPath);
        bool copied = QFile::copy(srcPath, destPath);
        /*
        qDebug() << CLASSFUNCTION
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

    // if external source
    if (!event->source()) {
        refreshModel();

        // if the drag is into the current FSTree folder then need to reload
//        QString currDir = currentIndex().data(Qt::ToolTipRole).toString();
        if (G::currRootFolder == dropDir) {
//            QString firstPath = event->mimeData()->urls().at(0).toLocalFile();
            emit folderSelection();
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

