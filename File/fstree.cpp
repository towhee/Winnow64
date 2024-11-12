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
    // qDebug() << "FSFilter::refresh";
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
// not being used
{
    count[dPath] = value;
}

void FSModel::insertCombineCount(QString dPath, QString value)
// not being used
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

void FSModel::refresh()
{
    beginResetModel();
    endResetModel();
    return;
}

void FSModel::refresh(const QString &dPath)
{
    // used in MW::pasteFiles
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
            // qDebug() << "FSModel::data" << ++counter << dPath;

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
                    QString jpgPath1 = fPath + "/" + baseName + ".jpeg";
                    if (metadata.hasJpg.contains(suffix)) {
                        if (dir->entryInfoList().contains(QFileInfo(jpgPath))) continue;
                        if (dir->entryInfoList().contains(QFileInfo(jpgPath1))) continue;
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

FSTree::FSTree(QWidget *parent, Metadata *metadata)
        : QTreeView(parent), delegate(new HoverDelegate(this))
{
    if (G::isLogger) G::log("FSTree::FSTree");
    this->metadata = metadata;
    fileFilters = new QStringList;
    dir = new QDir();
    viewport()->setObjectName("fsTreeViewPort");
    setObjectName("fsTree");

    // create model and filter
    createModel();
    treeSelectionModel = selectionModel();

    // setup treeview
    for (int i = 1; i <= 3; ++i) {
        hideColumn(i);
    }

    setItemDelegate(delegate);

    setRootIsDecorated(true);
    setSortingEnabled(false);
    setHeaderHidden(true);
    sortByColumn(0, Qt::AscendingOrder);
    setIndentation(16);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    setMouseTracking(true);

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

    // connect(this, &QTreeView::expanded, this, &FSTree::hasExpanded);
    // connect(this, &QTreeView::expanded, this, &FSTree::hasExpanded);
    // connect(this, &QTreeView::expanded, this, &FSTree::hasExpanded);

    // prevent select next folder when a folder is moved to trash/recycle
    connect(fsModel, &QFileSystemModel::rowsAboutToBeRemoved,
            this, &FSTree::onRowsAboutToBeRemoved);

    // Repaint when hover changes: Lambda function to call update
    connect(delegate, &HoverDelegate::hoverChanged, this->viewport(), [this]() {
            this->viewport()->update();});

    isDebug = true;
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
    fsModel->refresh();
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
    if (G::isLogger || G::isFlowLogger) {
        G::log();
        G::log("FSTree::select", dirPath);
    }
    // qDebug() << "FSTree::select" << dirPath;

    if (dirPath == "") return false;

    QDir test(dirPath);
    if (test.exists()) {
        QModelIndex index = fsFilter->mapFromSource(fsModel->index(dirPath));
        setCurrentIndex(index);
        scrollToCurrent();
        selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        emit folderSelectionChange(dirPath, "Add", true, false);
        return true;
    }
    else {
        G::popUp->showPopup("The folder path " + dirPath + " was not found.", 2000);
        return false;
    }
}

void FSTree::selectAndExpandRecursively(const QPersistentModelIndex &index)
{
    /*
    Expands the current row for all of its branches and selects them all. This is tricky,
    because the model is lazy loading, so it is necessary to wait for the fsModel to
    finish fetching data.

    It is also necessary to execute another setExpanded instruction.  I do not know why this
    is necessary.

    Making any selections while this is happening results in a crash, so the subfolders
    indexes are stored in a list, and the list is iterated after the recursive search is
    finished to select all the rows (subFolders).  The indexes must be persistent, as they
    can change as the model changes.

    The recursion is initiated by the mousePressEvent with the alt/opt or alt/opt + cmd/ctrl
    modifiers pressed.
*/
    if (!index.isValid()) {
        return;
    }

    QString folderName = index.data().toString();
    QString folderPath = index.data(QFileSystemModel::FilePathRole).toString();
    if (isDebug || G::isLogger || G::isFlowLogger)
        G::log("FSTree::selectAndExpandRecursively", folderPath);

    recursedForSelection.append(QPersistentModelIndex(index));

    // Check if expanded and proceed with recursive selection
    if (!isExpanded(index)) {
        expandAndWait(index);
    }

    // Delay to allow the expansion to complete before selecting children
    QModelIndex sourceIndex = fsFilter->mapToSource(index);
    setExpanded(index, true);
    fsFilter->refresh();    // necessary

    // Force lazy update to model expansion
    QElapsedTimer t;
    t.start();
    while (fsModel->canFetchMore(sourceIndex)) {
        fsModel->fetchMore(sourceIndex);
        if (t.elapsed() > 5000) {
            if (isDebug || G::isLogger) {
                QString msg = "timed out fetching more for " + folderName;
                G::log("FSTree::selectAndExpandRecursively", msg);
            }
            return;
        }
    }
    setExpanded(index, true);
    fsFilter->refresh();    // necessary

    // Delay checking for children until refresh is completed using singleShot
    bool isDebug = this->isDebug;
    QTimer::singleShot(0, this, [this, isDebug, index, folderName]() {
        int childCount = 0;
        if (index.isValid()) {
            // bool hasChildren = fsFilter->hasChildren(index);
            childCount = fsFilter->rowCount(index);
            QString msg = folderName + " has " + QString::number(childCount) + " children";
            if (isDebug || G::isLogger) G::log("FSTree::selectAndExpandRecursively", msg);
        }
        else {
            if (isDebug || G::isLogger) G::log("FSTree::selectAndExpandRecursively",  "WARNING invalid index for " + folderName);
        }

        // Recursively select all children
        // if (isDebug || G::isLogger) qDebug() << "FSTree::selectAndExpandRecursively select all children: childCount =" << childCount;
        for (int i = 0; i < childCount; ++i) {
            QModelIndex childIndex = fsFilter->index(i, 0, index);
            if (!childIndex.isValid()) {
                if (isDebug || G::isLogger) {
                    QString msg = "WARNING invalid childIndex for " + childIndex.data().toString();
                    G::log("FSTree::selectAndExpandRecursively", msg);
                }
                continue;
            }
            if (isDebug || G::isLogger) G::log("FSTree::selectAndExpandRecursively", "Recurse child " + childIndex.data().toString());
            selectAndExpandRecursively(childIndex);
        }

        // Finished recursion
        // if (isDebug)
        if (isDebug || G::isLogger)
            G::log("FSTree::selectAndExpandRecursively", "Expansion completed");
        // G::log("FSTree::selectAndExpandRecursively", "FINISHED");

        isRecursiveSelection = false;
        for (const QPersistentModelIndex &index : qAsConst(recursedForSelection)) {
            if (index.isValid()) {
                QTimer::singleShot(0, this, [this, index]() {
                    selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                });
            }
            else {
                if (isDebug) qDebug() << "FSTree::selectAndExpandRecursively invalid index";
            }
        }
        if (isDebug || G::isLogger)
            G::log("FSTree::selectAndExpandRecursively", "Selection completed");
        recursedForSelection.clear();
    });
}

// void FSTree::selectAndExpandRecursively(const QPersistentModelIndex &index)
// {
//     if (!index.isValid()) {
//         return;
//     }

//     QString folderName = index.data().toString();
//     QString folderPath = index.data(QFileSystemModel::FilePathRole).toString();
//     G::log("FSTree::selectAndExpandRecursively", folderPath);
//     recursedForSelection.append(QPersistentModelIndex(index));

//     // Check if expanded and proceed with recursive selection
//     if (!isExpanded(index)) {
//         connect(this, &QTreeView::expanded, this, [=](const QModelIndex &expandedIndex) {
//             if (expandedIndex == index) {
//                 disconnect(this, &QTreeView::expanded, this, nullptr);
//                 handleExpandedSelection(index);
//             }
//         });
//         setExpanded(index, true);
//     }
//     else {
//         handleExpandedSelection(index);
//     }
// }

// void FSTree::handleExpandedSelection(const QPersistentModelIndex &index)
// {
//     if (!isRecursiveSelection) {
//         return;
//     }

//     bool isDebug = true;

//     // Select the current node
//     QString folderName = index.data().toString();
//     if (isDebug)
//         qDebug() << "FSTree::handleExpandedSelection"
//                  << "isExpanded =" << isExpanded(index)
//                  << folderName;

//     // Delay to allow the expansion to complete before selecting children
//     QModelIndex sourceIndex = fsFilter->mapToSource(index);

//     fsFilter->refresh();    // necessary

//     // Force lazy update to model expansion
//     QElapsedTimer t;
//     t.start();
//     while (fsModel->canFetchMore(sourceIndex)) {
//         fsModel->fetchMore(sourceIndex);
//         if (t.elapsed() > 5000) {
//             if (isDebug)
//                 qDebug() << "FSTree::handleExpandedSelection timed out fetching more for " << folderName;
//             return;
//         }
//     }
//     fsFilter->refresh();    // necessary

//     // delay checking for children until refresh is completed using singleShot
//     QTimer::singleShot(0, this, [this, isDebug, index, folderName]() {
//         int childCount = 0;
//         if (index.isValid()) {
//             if (isDebug) qDebug() << "FSTree::handleExpandedSelection (after delay) valid index =" << folderName;
//             bool hasChildren = fsFilter->hasChildren(index);
//             if (isDebug) qDebug() << "FSTree::handleExpandedSelection (after delay) hasChildren =" << hasChildren;
//             childCount = fsFilter->rowCount(index);
//             if (isDebug) qDebug() << "FSTree::handleExpandedSelection (after delay) childCount =" << childCount << folderName;
//         }
//         else {
//             if (isDebug) qDebug() << "FSTree::handleExpandedSelection invalid index =" << folderName;
//         }

//         // Recursively select all child nodes
//         for (int i = 0; i < childCount; ++i) {
//             QModelIndex childIndex = fsFilter->index(i, 0, index);
//             if (!childIndex.isValid()) {
//                 if (isDebug) qDebug() << "FSTree::handleExpandedSelection invalid childIndex =" << childIndex.data().toString() << childIndex;
//                 continue;
//             }
//             if (isDebug) qDebug() << "FSTree::handleExpandedSelection Recurse child " << childIndex.data().toString();
//             selectAndExpandRecursively(childIndex);
//         }

//         // Finished recursion
//         if (isDebug) qDebug() << "FSTree::handleExpandedSelection FINISHED";
//         if (isDebug) qDebug() << "FSTree::handleExpandedSelection"
//                      << "recursedForSelection count =" << recursedForSelection.count();
//         // G::log("FSTree::handleExpandedSelection", "FINISHED");

//         isRecursiveSelection = false;
//         for (const QPersistentModelIndex &index : qAsConst(recursedForSelection)) {
//             if (index.isValid()) {
//                 QTimer::singleShot(0, this, [this, index]() {
//                     selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
//                 });
//             }
//             else {
//                 if (isDebug) qDebug() << "FSTree::handleExpandedSelection invalid index";
//             }
//         }
//         recursedForSelection.clear();
//     });
// }

void FSTree::expandedSelectRecursively(const QPersistentModelIndex &index)
{
/*
    Along with FSTree::selectRecursively, expand the current row for all of its branches
    and select them all. This is tricky, because the model is lazy loading, so it is necessary
    to wait for the fsModel to finish fetching data.

    It is also necessary to execute another setExpanded instruction.  I do not know why this
    is necessary.

    Making any selections while this is happening results in a crash, so the subfolders
    indexes are stored in a list, and the list is iterated after the recursive search is
    finished to select all the rows (subFolders).  The indexes must be persistent, as they
    can change as the model changes.

    The recursion is initiated by the mousePressEvent with the alt/opt or alt/opt + cmd/ctrl
    modifiers pressed.
*/
    if (!index.isValid() || !isRecursiveSelection) {
        return;
    }

    QString fun = "FSTree::expandedSelectRecursively";
    bool isDebug = true;

    // Select the current node
    QString folderName = index.data().toString();
    if (isDebug)
    qDebug() << "FSTree::expandedSelectRecursively"
             << "isExpanded =" << isExpanded(index)
             << folderName;

    // Delay to allow the expansion to complete before selecting children
    QModelIndex sourceIndex = fsFilter->mapToSource(index);
    // setExpanded(index, true);
    expandAndWait(index);

    fsFilter->refresh();    // necessary

    // force lazy update to model expansion
    QElapsedTimer t;
    t.start();
    while (fsModel->canFetchMore(sourceIndex)) {
        fsModel->fetchMore(sourceIndex);
        if (t.elapsed() > 5000) {
            if (isDebug)
            qDebug() << "FSTree::expandedSelectRecursively timed out fetching more for " << folderName;
            return;
        }
    }

    fsFilter->refresh();    // necessary
    /* this does not work
    QMetaObject::invokeMethod(this, [this]() {
            // fsModel->refresh();
            fsFilter->refresh();
        }, Qt::QueuedConnection);  */

    // delay checking for children until refresh is completed using singleshot
    QTimer::singleShot(0, this, [this, isDebug, index, folderName]() {
        int childCount = 0;
        if (index.isValid()) {
            if (isDebug) qDebug() << "FSTree::expandedSelectRecursively (after delay) valid index =" << folderName;
            bool hasChildren = fsFilter->hasChildren(index);
            if (isDebug) qDebug() << "FSTree::expandedSelectRecursively (after delay) hasChildren =" << hasChildren;
            childCount = fsFilter->rowCount(index);
            if (isDebug) qDebug() << "FSTree::expandedSelectRecursively (after delay) childCount =" << childCount << folderName;
        }
        else {
            if (isDebug) qDebug() << "FSTree::expandedSelectRecursively invalid index =" << folderName;
        }

        // Recursively select all child nodes
        for (int i = 0; i < childCount; ++i) {
            QModelIndex childIndex = fsFilter->index(i, 0, index);
            if (!childIndex.isValid()) {
                if (isDebug) qDebug() << "FSTree::expandedSelectRecursively invalid childIndex =" << childIndex.data().toString() << childIndex;
                continue;
            }
            if (isDebug) qDebug() << "FSTree::expandedSelectRecursively Recurse child " << childIndex.data().toString();
            selectRecursively(childIndex);
        }

        // Finished recursion
        if (isDebug) qDebug() << "FSTree::expandedSelectRecursively FINISHED";
        if (isDebug) qDebug() << "FSTree::expandedSelectRecursively"
                 << "recursedForSelection count =" << recursedForSelection.count();
        G::log("FSTree::expandedSelectRecursively", "FINISHED");

        isRecursiveSelection = false;
        for (const QPersistentModelIndex &index : qAsConst(recursedForSelection)) {
            if (index.isValid()) {
                // while (G::stop) {
                //     QThread::msleep(100);
                //     G::log("FSTree::expandedSelectRecursively", "waiting for G::stop = false");
                //     QCoreApplication::processEvents();  // keep GUI responsive
                // }
                QTimer::singleShot(0, this, [this, index]() {
                    selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                });
            }
            else {
                G::log("FSTree::expandedSelectRecursively", "invalid index");
            }
        }
        recursedForSelection.clear();
    });
}

void FSTree::selectRecursively(const QPersistentModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    QString folderName = index.data().toString();
    QString folderPath = index.data(QFileSystemModel::FilePathRole).toString();
    G::log("FSTree::selectRecursively", folderPath);
    recursedForSelection.append(QPersistentModelIndex(index));
    // while (G::stop) {
    //     QThread::msleep(100);
    //     G::log("FSTree::selectRecursively", "waiting for G::stop = false");
    //     QCoreApplication::processEvents();  // keep GUI responsive
    // }
    if (isExpanded(index)) expandedSelectRecursively(index);
    else setExpanded(index, true);
}

void FSTree::selectItemAndChildren(const QModelIndex &index)
{
/*
    Not being used.
*/
    if (G::isLogger) G::log("FSTree::selectItemAndChildren");
    QItemSelectionModel *selectionModel = this->selectionModel();
    if (!selectionModel) {
        return;
    }

    // Use a queue to traverse all children of the given index
    QList<QModelIndex> queue;
    queue.append(index);

    while (!queue.isEmpty()) {
        QModelIndex currentIndex = queue.takeFirst();
        selectionModel->select(currentIndex, QItemSelectionModel::Select);

        // Add all children of the current item to the queue
        for (int i = 0; i < model()->rowCount(currentIndex); ++i) {
            queue.append(model()->index(i, 0, currentIndex));
        }
    }
}

QModelIndex FSTree::getCurrentIndex()
{
    if (G::isLogger) G::log("FSTree::getCurrentIndex");
    QModelIndex idx;
    if (selectedIndexes().size() > 0)
        idx = selectedIndexes().first();
    else idx = fsModel->index(-1, -1, QModelIndex());
    return idx;
}

QStringList FSTree::getSelectedFolderPaths() const
{
/*
    Not being used
*/
    QStringList selectedFolderPaths;
    QItemSelectionModel *selectionModel = this->selectionModel();

    if (!selectionModel) {
        return selectedFolderPaths; // Return an empty list if there's no selection model
    }

    // Get all selected indexes from the selection model
    QModelIndexList selectedIndexes = selectionModel->selectedRows();

    // Iterate over the selected indexes and extract the folder paths
    for (const QModelIndex &index : selectedIndexes) {
        QString folderPath = index.data(QFileSystemModel::FilePathRole).toString();
        selectedFolderPaths.append(folderPath);
    }

    return selectedFolderPaths;
}

void FSTree::onRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    // prevent select next folder when a folder is moved to trash/recycle

    // Q_UNUSED(parent)
    // Q_UNUSED(start)
    // Q_UNUSED(end)

    QString toBeRemoved = fsModel->index(start, 0, parent).data(G::PathRole).toString();

    qDebug() << "FSTree::onRowsAboutToBeRemoved"
             << "Current folder =" << G::currRootFolder
             << "Current FSTree folder =" << this->currentIndex().data().toString()
             << "parent =" << parent
             << "start =" << start
             << "end =" << end
             << "To be deleted folder =" << toBeRemoved
        ;

    // Clear the selection to prevent automatic selection of the next folder
    if (G::currRootFolder == toBeRemoved) {
        qDebug() << "FSTree::onRowsAboutToBeRemoved clear selection";
        clearSelection();
        setCurrentIndex(QModelIndex());
    }
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

void FSTree::expandAndWait(const QPersistentModelIndex &index)
{
    if (isDebug || G::isLogger)
        G::log("FSTree::expandAndWait", index.data().toString());

    justExpandedIndex = index;  // Set the index to wait for

    // Connect the expanded signal to the hasExpanded slot
    connect(this, &QTreeView::expanded, this, &FSTree::hasExpanded);

    // Start the elapsed timer
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();

    QEventLoop loop;

    // Set up a timeout timer for 1500 ms to avoid indefinite waiting
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.start(1500);

    // Connect the timeout and custom signal to quit the loop
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &FSTree::indexExpanded, &loop, &QEventLoop::quit);

    // Delay the `setExpanded` call to ensure `loop.exec()` starts first
    QTimer::singleShot(0, this, [this, index]() {
        setExpanded(index, true);
    });

    // Start the event loop and wait until either expansion or timeout occurs
    loop.exec();

    // Log the elapsed time
    if (isDebug || G::isLogger)
        G::log("FSTree::expandAndWait", index.data().toString() + " expanded");

    // Clean up connections
    disconnect(this, &QTreeView::expanded, this, &FSTree::hasExpanded);
}

void FSTree::hasExpanded(const QPersistentModelIndex &index)
{
    if (isDebug || G::isLogger) {
        QString msg;
        if (justExpandedIndex == index) msg = "justExpandedIndex == index ";
        else msg = "justExpandedIndex != index ";
        msg += " Expanded = " + index.data().toString() +
                      " Just Expanded = " + index.data().toString();
        G::log("FSTree::hasExpanded", index.data().toString());
    }
    // qDebug() << "FSTree::hasExpanded" << t.elapsed() << index << justExpandedIndex;

    if (justExpandedIndex == index) {
        emit indexExpanded();
    }
}

void FSTree::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log("FSTree::resizeEvent");
    resizeColumns();
}

qlonglong FSTree::selectionCount()
{
    return selectionModel()->selectedRows().count();
    // return treeSelectionModel->selectedRows().count() == 1;
}

void FSTree::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
/*
    Changes in folders selected triggers signals to add or remove all the folder eligible
    files from the DataModel.  If only one folder is selected then the DataModel is cleared
    and the selected folder images are appended to the DataModel. As additional folders
    are selected their images are appended or removed from the DataModel.
*/
    if (G::isInitializing) return;
    // if (selected.count() == 0 && deselected.count() == 0) return;

    if (G::isLogger || G::isFlowLogger) {
        int selectionCount = treeSelectionModel->selectedRows().count();
        QString msg = "total selection = " + QVariant(selectionCount).toString() +
                      " Changed: " +
                      " selected = " + QVariant(selected.count()).toString() +
                      " deselected = " + QVariant(deselected.count()).toString();
        G::log();
        G::log("FSTree::selectionChanged", msg);
    }

    QTreeView::selectionChanged(selected, deselected);
    return;

    // Iterate through the selected rows
    for (const QModelIndex &index : selected.indexes()) {
        if (index.column() == 0) { // Ensure we're only processing the first column
            const QString folderPath = index.data(QFileSystemModel::FilePathRole).toString();
            if (folderPath.length()) {
                // emit folderSelectionChange(folderPath, "Add");
            }
        }
    }

    // selected a new primary folder, nothing to remove
    if (selected.count() < 2) return;

    /*
    qDebug() << "\nFSTree::selectionChanged before process deleted"
             << "Selection =" << treeSelectionModel->selection().count()
             << "selected.count() =" << selected.count()
             << "deselected.count() =" << deselected.count(); //*/

    // Iterate through the deselected rows
    for (const QModelIndex &index : deselected.indexes()) {
        // Only process the first column
        if (index.column() == 0) {
            QString folderPath = index.data(QFileSystemModel::FilePathRole).toString();
            if (folderPath.length()) {
                /*
                qDebug()
                    << "FSTree::selectionChanged Iterate deselected"
                    << "Deselected Path:" << folderPath
                    ; //*/

                // emit folderSelectionChange(folderPath, "Remove");
            }
        }
    }
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
    delegate->setHoveredIndex(QModelIndex());  // Clear highlight when mouse leaves
    // not being used
    wheelSpinningOnEntry = false;

    // req'd ?
    QTreeView::leaveEvent(event);
}

void FSTree::wheelStopped()
{
    G::wheelSpinning = false;
    wheelSpinningOnEntry = false;
    //qDebug() << "ImageView::wheelStopped";
}

void FSTree::mousePressEvent(QMouseEvent *event)
{
    /*
    static int count = 0;
    count++;

    QModelIndex idx = indexAt(event->pos());
    QString path = "";
    QString folderName = "";
    path = idx.data(Qt::ToolTipRole).toString();
    folderName = QFileInfo(path).fileName();
    qDebug() << "FSTree::mousePressEvent " << count
             << "idx =" << idx
             << "idx.isValid() =" << idx.isValid()
             << "event->pos() =" << event->pos()
             << "folderName =" << folderName
        ;
    QTreeView::mousePressEvent(event);
    return;
    */

    if (G::isLogger) G::log("FSTree::mousePressEvent");

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

    QModelIndex idx = indexAt(event->pos());
    if (!idx.isValid()) return;

    // update path
    QModelIndex idx0 = idx.sibling(idx.row(), 0);
    QString dPath = idx0.data(QFileSystemModel::FilePathRole).toString();
    // selectionModel()->selectedIndexes().contains(idx0);

    // context menu is handled in MW::eventFilter
    if (event->button() == Qt::RightButton) {
        return;
    }

    // New selection (primary folder)
    if (event->modifiers() == Qt::NoModifier) {
        // qDebug() << "FSTree::mousePressEvent NEW SELECTION" << path;
        QTreeView::mousePressEvent(event);
        emit folderSelectionChange(dPath, "Add", true, false);
        return;
    }

    // load all subfolders images
    if (event->modifiers() & Qt::AltModifier) {
        // ignore mac control modifier or windows window key modifier
        if (event->modifiers() & Qt::MetaModifier) return;
        // qDebug() << "FSTree::mousePressEvent ADD SUBFOLDERS SELECTION" << path;
        if ((event->modifiers() & Qt::ControlModifier)) {
            // qDebug() << "FSTree::mousePressEvent ADD SUBFOLDERS not Qt::ControlModifier" << path;
            emit folderSelectionChange(dPath, "Add", /*newInstance*/false, /*recurse*/true);
        }
        // selectRecursively(idx0);
        emit folderSelectionChange(dPath, "Add", /*newInstance*/true, /*recurse*/true);
        t.restart();
        isRecursiveSelection = true;
        selectAndExpandRecursively(idx0);
        selectionModel()->clearSelection();
        return;
    }

    // toggle folder
    if (event->modifiers() & Qt::ControlModifier) {
        int folders = getSelectedFolderPaths().count();
        bool folderWasSelected = getSelectedFolderPaths().contains(dPath);
        // ignore if click on only folder selected
        if (folderWasSelected) {
            // do not toggle if only one folder selected
            if (folders < 2) return;
            emit folderSelectionChange(dPath, "Remove", /*newInstance*/false, /*recurse*/false);
        }
        else {
            emit folderSelectionChange(dPath, "Add", /*newInstance*/false, /*recurse*/false);
        }
        QTreeView::mousePressEvent(event);
    }
}

void FSTree::mouseReleaseEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log("FSTree::mouseReleaseEvent");

    if (event->button() == Qt::RightButton) {
        return;
    }

    if (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ControlModifier) {
        QTreeView::mouseReleaseEvent(event);
    }
}

void FSTree::mouseMoveEvent(QMouseEvent *event)
{
    QModelIndex idx = indexAt(event->pos());
    // same row, column 0 (folder name)
    QModelIndex idx0 = idx.sibling(idx.row(), 0);
    /*
    qDebug() << "FSTree::mouseMoveEvent"
             << "idx.row() =" << idx.row();
    //*/
    if (idx0.isValid()) {
        hoverFolderName = idx0.data().toString();
        delegate->setHoveredIndex(idx0);
    } else {
        hoverFolderName = "";
        delegate->setHoveredIndex(QModelIndex());  // No row hovered
    }
    QTreeView::mouseMoveEvent(event);
}

void FSTree::contextMenuEvent(QContextMenuEvent *event)
{
    QModelIndex idx = indexAt(event->pos());
    QString path = "";
    QString folderName = "";
    path = idx.data(Qt::ToolTipRole).toString();
    folderName = QFileInfo(path).fileName();
    qDebug() << "FSTree::contextMenuEvent"
             << "idx =" << idx
             << "idx.isValid() =" << idx.isValid()
             << "event->pos() =" << event->pos()
             << "folderName =" << folderName
        ;
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
            // emit folderSelection(dropDir);
            select(dropDir);
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

