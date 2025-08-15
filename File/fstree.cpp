#include "File/fstree.h"
#include "Main/global.h"

extern QStringList mountedDrives;
QStringList mountedDrives;

/*------------------------------------------------------------------------------
CLASS ImageCounter worker thread
------------------------------------------------------------------------------*/

ImageCounter::ImageCounter(const QString &path, Metadata &metadata,
                           bool &combineRawJpg, QStringList *fileFilters,
                           QObject *parent)
    : QThread(parent),
    dPath(path),
    metadata(metadata),
    combineRawJpg(combineRawJpg),
    fileFilters(fileFilters)
{}

void ImageCounter::run()
{
    int count = computeImageCount(dPath);
    // qDebug() << "ImageCounter::run count =" << count << dPath;
    emit countReady(dPath, count);
}

int ImageCounter::computeImageCount(const QString &path)
{
    QDirIterator it(path, *fileFilters, QDir::Files);
    int count = 0;
    QSet<QString> rawBaseNames;     // Stores base names of RAW files
    QStringList jpgBaseNames;       // Stores all valid file names

    if (combineRawJpg) {
        while (it.hasNext()) {
            it.next();
            if (!it.fileInfo().size()) continue;
            QString fileName = it.fileName().toLower();
            int dotIndex = fileName.lastIndexOf('.');
            if (dotIndex == -1) {
                continue;  // No extension, skip
            }

            QString baseName = fileName.left(dotIndex);
            QString ext = fileName.mid(dotIndex + 1);

            bool isRaw = metadata.hasJpg.contains(ext);
            bool isJpg = ext == "jpeg" || ext == "jpg";

            // count raw files
            if (isRaw) {
                rawBaseNames.insert(baseName);
                count++;
            }
            // do not count jpg files yet
            else if (isJpg) jpgBaseNames.append(baseName);
            // count all other image files
            else count++;
        }

        // check for jpg/raw matching pairs
        for (const QString &baseName : jpgBaseNames) {
            // count jpg files if no matching raw file
            if (!rawBaseNames.contains(baseName)) {
                count++;
            }
        }
    }
    else {
        while (it.hasNext()) {
            it.next();
            if (!it.fileInfo().size()) continue;
            count++;
        }
    }

    /*
    qDebug() << "ImageCounter::computeImageCount"
             << "count = " << count << path;
    */

    return count;
}

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
    qDebug().noquote()
        << "size" << info.size()
        << "isHidden" << info.isHidden()
        << "fPath" << fPath
        << "fParentPath" << fParentPath
        << "absolutePath" << info.absolutePath()
        << "absoluteFilePath" << info.absoluteFilePath()
        ;
    //*/
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
    fileFilters = new QStringList;
    // QStringList *fileFilters = new QStringList;
    dir = new QDir();

    fileFilters->clear();
    foreach (const QString &str, metadata.supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    count.clear();

    this->iconProvider()->setOptions(QFileIconProvider::DontUseCustomDirectoryIcons);
}

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

void FSModel::clearCount()
{
    count.clear();
}

void FSModel::updateCount(const QString &dPath)
{
    // remove count for folder dPath
    count.remove(dPath);

    // update data
    const QModelIndex idx = index(dPath, imageCountColumn);
    QList<int> roles;
    roles << Qt::DisplayRole;
    emit dataChanged(idx, idx, roles);
}

QVariant FSModel::data(const QModelIndex &index, int role) const
{
/*
    Return image count for each folder by looking it up in the QHash count which is built
    in FSTree::getImageCount and referenced here. This is much faster than performing the
    image count "on-the-fly" here, which causes scroll latency.
*/
    if (index.column() == imageCountColumn /*&& showImageCount*/) {
        if (role == Qt::DisplayRole) {
            QString dPath = filePath(index);

            if (count.contains(dPath)) {
                return count.value(dPath);  // Return cached value
            }

            // Cache folder eligible image count
            ImageCounter *worker = new ImageCounter(dPath, metadata, combineRawJpg, fileFilters);

            connect(worker, &ImageCounter::countReady,
                    this, [this](const QString &path, int countValue)
            {
                // Convert 'this' from 'const FSModel*' to 'FSModel*'
                auto *nc = const_cast<FSModel*>(this);
                nc->count.insert(path, QString::number(countValue));

                QModelIndex srcIndex = nc->index(path, imageCountColumn);
                if (srcIndex.isValid()) {
                    emit nc->dataChanged(srcIndex, srcIndex, { Qt::DisplayRole });
                }

            });

            worker->start(QThread::LowPriority);
        }

        if (role == Qt::TextAlignmentRole) {
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        }

        return QVariant();  // Show nothing until count is ready
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

    return QVariant();
}

/*------------------------------------------------------------------------------
CLASS FSTree subclassing QTreeView
------------------------------------------------------------------------------*/

FSTree::FSTree(QWidget *parent, DataModel *dm, Metadata *metadata)
        : QTreeView(parent), delegate(new HoverDelegate(this))
{
    if (G::isLogger) G::log("FSTree::FSTree");
    this->dm = dm;
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

    rapidClick.start();

    setAcceptDrops(true);
    setDragEnabled(true);
    setDropIndicatorShown(true);
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
    fsModel->clearCount();
    fsFilter->refresh();
    setFocus();
    select(currentFolderPath());
}

bool FSTree::isShowImageCount()
{
    if (G::isLogger) G::log("FSTree::isShowImageCount");
    return fsModel->showImageCount;
}

int FSTree::imageCount(QString path)
{
    QModelIndex idx0 = fsFilter->mapFromSource(fsModel->index(path));
    int row = idx0.row();
    int col = fsModel->imageCountColumn;
    QModelIndex par = idx0.parent();
    QModelIndex idx4 = fsFilter->index(row, col, par);
    int count = idx4.data().toInt();
    // qDebug() << row << col << idx0.data() << idx4.data().toString() << idx0 << idx4;
    return count;
}

void FSTree::updateCount()
{
/*
    Updates all visible image counts.
    Forces repaint of the image count column by emitting dataChanged
    on visible rows. This ensures the count is shown without requiring
    hover or scroll.
*/
    if (G::isLogger) G::log("FSTree::updateCount");

    fsModel->clearCount();
    fsFilter->refresh();

    // Force update on visible rows
    const int countCol = fsModel->imageCountColumn;

    int y = 0;
    while (y < viewport()->height()) {
        QModelIndex visualIndex = indexAt(QPoint(0, y));
        if (!visualIndex.isValid()) break;

        QModelIndex countIndex = fsFilter->index(
            visualIndex.row(), countCol, visualIndex.parent());

        if (countIndex.isValid()) {
            QModelIndex srcIndex = fsFilter->mapToSource(countIndex);
            emit fsModel->dataChanged(srcIndex, srcIndex, { Qt::DisplayRole });
        }

        y += rowHeight(visualIndex);
    }

    viewport()->update();
    updateGeometries();
/*
    Updates all visible image counts
*/
    // // qDebug() << "FSTree::updateCount";
    // fsModel->clearCount();
    // fsFilter->refresh();
    // viewport()->update();
    // // setFocus();
    // // QModelIndex firstVisible = indexAt(QPoint(0, 0));
    // // QModelIndex lastVisible = indexAt(QPoint(0, viewport()->height() - 1));
    // // emit fsModel->dataChanged(firstVisible, lastVisible);
}

void FSTree::updateAFolderCount(const QString &dPath)
{
/*
    Updates image count for the dPath folder
*/
    fsModel->updateCount(dPath);
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

bool FSTree::select(QString folderPath , QString modifier, QString src)
{
    if (G::isLogger || G::isFlowLogger) {
        G::log();
        G::log("FSTree::select", folderPath);
    }
    // qDebug() << "FSTree::select" << dirPath;

    if (folderPath == "") return false;

    selectSrc = src;
    QDir test(folderPath);
    if (!test.exists()) {
        G::popup->showPopup("The folder path " + folderPath + " was not found.", 2000);
        return false;
    }

    bool recurse = false;
    bool resetDataModel = false;
    QPersistentModelIndex index = fsFilter->mapFromSource(fsModel->index(folderPath));

    if (modifier == "" || modifier == "None") {
        resetDataModel = true;
        recurse = false;
        emit folderSelectionChange(folderPath, "Add", resetDataModel, recurse);
        setCurrentIndex(index);
        scrollToCurrent();
        selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        return true;
    }

    if (modifier == "Recurse") {
        resetDataModel = true;
        recurse = true;
        emit folderSelectionChange(folderPath, "Add", resetDataModel, recurse);
        setCurrentIndex(index);
        scrollToCurrent();
        selectionModel()->clearSelection();
        selectRecursively(folderPath);
        return true;
    }

    if (modifier == "USBDrive") {
        resetDataModel = true;
        recurse = true;
        emit folderSelectionChange(folderPath, "Add", resetDataModel, recurse);
        setCurrentIndex(index);
        scrollToCurrent();
        selectionModel()->clearSelection();
        selectRecursively(folderPath);
        setExpanded(index, false);
        return true;
    }

    return false;
}

void FSTree::selectRecursively(QString folderPath, bool toggle)
{
/*
    This is done in two steps:
    1. expand all subfolders using QDirIterator and setCurrentIndex
    2. iterate and select or toggle each folder
*/
    // turn off image count for performance
    setShowImageCount(false);

    QStringList recursedFolders;
    recursedFolders.append(folderPath);

    // recurse and expand all subfolders
    QDirIterator it(folderPath, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString dPath = it.next();
        if (it.fileInfo().isDir() && it.fileName() != "." && it.fileName() != "..") {
            recursedFolders.append(dPath);
            // use setCurrentIndex to force tree to expand
            QPersistentModelIndex index = fsFilter->mapFromSource(fsModel->index(dPath));
            // setCurrentIndex rapidly expands the treeview without selection
            selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        }
    }

    // select/deselect all recursed folders
    foreach (QString path, recursedFolders) {
        QModelIndex index = fsFilter->mapFromSource(fsModel->index(path));
        if (toggle)
            selectionModel()->select(index, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
        else
            selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }

    // turn image count back on
    setShowImageCount(true);
}

QStringList FSTree::selectVisibleBetween(const QModelIndex &idx1, const QModelIndex &idx2, bool recurse)
{
    QList<QPersistentModelIndex> visibleIndexes;
    QStringList visibleFolders;

    if (!idx1.isValid() || !idx2.isValid()) {
        return visibleFolders;
    }

    // Get the top and bottom visual rectangles
    QRect topRect = visualRect(idx1);
    QRect bottomRect = visualRect(idx2);

    // Ensure idx1 is above idx2
    if (topRect.top() > bottomRect.top()) {
        std::swap(topRect, bottomRect);
    }

    // Calculate row height based on the distance between the top and bottom of the topRect
    int rowHeight = topRect.height();

    // Calculate the starting and ending Y positions
    int yStart = topRect.top();
    int yEnd = bottomRect.bottom();

    // Iterate through each row within the viewport rectangle range
    for (int y = yStart; y <= yEnd; y += rowHeight) {
        QPoint pointInRow(viewport()->rect().left(), y);
        QModelIndex index = indexAt(pointInRow);

        // Check if the index is valid, then create an index in column 0
        if (index.isValid()) {
            QModelIndex idx0 = index.siblingAtColumn(0);
            visibleIndexes.append(idx0);

        }
    }

    // Select the unselected indexes
    for (const QPersistentModelIndex &index : visibleIndexes) {
        if (!index.isValid()) continue;
        if (!selectionModel()->isSelected(index)) {
            selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            visibleFolders.append(index.data(QFileSystemModel::FilePathRole).toString());
        }
    }

    // Recurse
    if (recurse) {
        // setShowImageCount(false);   // improve performance
        for (const QPersistentModelIndex &index : visibleIndexes) {
            if (!index.isValid()) continue;
            G::log("FSTree::selectVisibleBetween  Recurse folder", index.data().toString());
            if (recurse) {
                QString folderPath = index.data(QFileSystemModel::FilePathRole).toString();
                selectRecursively(folderPath);
            }
        }
        // setShowImageCount(true);
    }

    return visibleFolders;
}

QModelIndex FSTree::getCurrentIndex()
{
/*
    Used by scrollToCurrent
*/
    if (G::isLogger) G::log("FSTree::getCurrentIndex");
    QModelIndex idx;
    if (selectedIndexes().size() > 0)
        idx = selectedIndexes().first();
    else idx = fsModel->index(-1, -1, QModelIndex());
    return idx;
}

QString FSTree::currentFolderPath()
{
/*
    Used by MW::addNewBookmarkFromMenu
*/
    if (G::isLogger) G::log("FSTree::getCurrentIndex");
    return getCurrentIndex().data(QFileSystemModel::FilePathRole).toString();
}

QStringList FSTree::selectedFolderPaths() const
{
/*
    Used by toggle folder in mousePressEvent
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

    /*
    qDebug() << "FSTree::onRowsAboutToBeRemoved"
             << "Current folder =" << currentFolderPath()
             << "Current FSTree folder =" << this->currentIndex().data().toString()
             << "parent =" << parent
             << "start =" << start
             << "end =" << end
             << "To be deleted folder =" << toBeRemoved
        ; //*/

    // Clear the selection to prevent automatic selection of the next folder
    if (selectedFolderPaths().contains(toBeRemoved)) {
        // qDebug() << "FSTree::onRowsAboutToBeRemoved clear selection";
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

// void FSTree::expandAndWait(const QPersistentModelIndex &index)
// {
//     if (isDebug || G::isLogger)
//         G::log("FSTree::expandAndWait", index.data().toString());

//     justExpandedIndex = index;  // Set the index to wait for

//     // Connect the expanded signal to the hasExpanded slot
//     connect(this, &QTreeView::expanded, this, &FSTree::hasExpanded);

//     // Start the elapsed timer
//     QElapsedTimer elapsedTimer;
//     elapsedTimer.start();

//     QEventLoop loop;

//     // Set up a timeout timer for 1500 ms to avoid indefinite waiting
//     QTimer timeoutTimer;
//     timeoutTimer.setSingleShot(true);
//     timeoutTimer.start(5000);

//     // Connect the timeout and custom signal to quit the loop
//     connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
//     connect(this, &FSTree::indexExpanded, &loop, &QEventLoop::quit);

//     // Delay the `setExpanded` call to ensure `loop.exec()` starts first
//     QTimer::singleShot(0, this, [this, index]() {
//         setExpanded(index, true);
//     });

//     // Start the event loop and wait until either expansion or timeout occurs
//     loop.exec();

//     // Log the elapsed time
//     if (isDebug || G::isLogger)
//         G::log("FSTree::expandAndWait", index.data().toString() + " expanded");

//     // Clean up connections
//     disconnect(this, &QTreeView::expanded, this, &FSTree::hasExpanded);
// }

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
    QTreeView::selectionChanged(selected, deselected);
}

void FSTree::saveState(ViewState& state) const
{
    // // Save the current selection
    // const auto& selectionModel = this->selectionModel();
    // if (selectionModel) {
    //     const auto selectedIndexes = selectionModel->selectedIndexes();
    //     state.selectedIndexes.clear();
    //     for (const auto& index : selectedIndexes) {
    //         if (index.isValid()) {
    //             state.selectedIndexes << index;
    //         }
    //     }
    // }
}

bool FSTree::restoreState(const ViewState& state) const
{
    // // Restore the selection
    // const auto& selectionModel = treeSelectionModel;
    // if (selectionModel) {
    //     selectionModel->clearSelection(); // Clear any existing selection
    //     QItemSelection selection;
    //     for (const auto& index : state.selectedIndexes) {
    //         if (index.isValid()) { // Ensure the index is still valid before selecting it
    //             selection.select(index); // too few arguments
    //         }
    //     }
    //     selectionModel->select(selection, QItemSelectionModel::Select);
    // }

    return true; // Return success
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

    QTreeView::leaveEvent(event);
    viewport()->update();
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
    When a folder is mouse clicked this initiates the Winnow pipeline:
    - populate datamodel with file system info for each eligible image
    - add metadata for each row in datamodel
    - build the image cache

    Options
    - no modifier clears the selection and datamodel and starts pipeline for folder
    - ctrl/cmd modifier toggles the folder and adds or removes from datamodel
    - alt/opt modifier recursively selects all subfolders and adds to datamodel
    - shift modifier selects all unselected visible folders between the current index
      and the shift clicked index
*/

    // if (G::isLogger) G::log("FSTree::mousePressEvent");
    // qDebug() << "FSTree::mousePressEvent" << event;

    if (G::stop || G::isModifyingDatamodel) {
        // G::popUp->showPopup("Busy, try new folder in a sec.", 1000);
        qDebug() << "FSTree::mousePressEvent busy so ignore";
        return;
    }

    // ignore rapid mouse press if still processing MW::stop
    qint64 ms = rapidClick.elapsed();
    if (ms < 500) {
        event->ignore();
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
        return;
    }

    static QModelIndex prevIdx = QModelIndex();

    QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) return;

    // decoration clicked
    QRect rect = visualRect(index);
    int level = 0;
    QModelIndex current = index;
    while (current.parent().isValid()) {
        current = current.parent();
        ++level;
    }
    int indentationOffset = level * indentation();
    int decorationWidth = style()->pixelMetric(QStyle::PM_IndicatorWidth);
    QRect decorationRect = QRect(indentationOffset, rect.top(), decorationWidth, rect.height());
    /*
    qDebug() << "FSTree::mousePressEvent"
             << "level =" << level
             << "rect =" << rect
             << "decorationRect =" << decorationRect
             << "pos =" << event->pos()
             << "decorationRect.contains(event->pos()) ="
             << decorationRect.contains(event->pos()); //*/
    if (decorationRect.contains(event->pos())) {
        QTreeView::mousePressEvent(event);
        return;
    }

    // context menu is handled in MW::eventFilter
    if (event->button() == Qt::RightButton) {
        return;
    }

    // index
    QModelIndex idx0 = index.sibling(index.row(), 0);
    QString dPath = idx0.data(QFileSystemModel::FilePathRole).toString();
    bool recurse = false;
    bool resetDataModel = false;
    bool isAlt = event->modifiers() & Qt::AltModifier;
    bool isCtrl = event->modifiers() & Qt::ControlModifier;
    bool isShift = event->modifiers() & Qt::ShiftModifier;
    bool isMeta = event->modifiers() & Qt::MetaModifier;
    bool isNoModifier = event->modifiers() == Qt::NoModifier;

    if (isDebug) {
        qDebug() << " ";
        qDebug().noquote()
            << "FSTree::mousePressEvent"
            << "isAlt =" << isAlt
            << "isCtrl =" << isCtrl
            << "isShift =" << isShift
            << "isMeta =" << isMeta
            << "isNone =" << isNoModifier
            ;
    }

    if (isMeta) return;

    // New selection (primary folder)
    if (isNoModifier) {
        // qDebug() << "FSTree::mousePressEvent NEW SELECTION" << path;
        if (G::isLogger || G::isFlowLogger) G::log("FSTree::mousePressEvent", "No modifiers, new instance");
        QTreeView::mousePressEvent(event);
        resetDataModel = true;
        recurse = false;
        emit folderSelectionChange(dPath, "Add", resetDataModel, recurse);
        prevIdx = index;
        return;
    }

    // recurse load all subfolders images
    if (isAlt && !isShift) {
        if (isCtrl) {
            // to do: deal with toggle
            if (G::isLogger || G::isFlowLogger) G::log("FSTree::mousePressEvent", "Modifers: Opt + Cmd, Recurse");
            resetDataModel = false;
            // resetDataModel = true;
            recurse = true;
            emit folderSelectionChange(dPath, "Toggle", resetDataModel, recurse);
            bool toggle = true;
            selectRecursively(dPath, toggle);
            prevIdx = index;
            return;
        }
        if (G::isLogger || G::isFlowLogger) G::log("FSTree::mousePressEvent", "Modifiers: Opt, New instance and Recurse");
        resetDataModel = true;
        recurse = true;
        emit folderSelectionChange(dPath, "Add", resetDataModel, recurse);
        selectionModel()->clearSelection();
        selectRecursively(dPath);
        return;
    }

    // toggle folder
    if (isCtrl) {
        int folders = selectedFolderPaths().count();
        bool folderWasSelected = selectedFolderPaths().contains(dPath);
        resetDataModel = false;
        recurse = false;
        // qDebug() << "FSTree::mousePressEvent  isCtrl =" << isCtrl << "folderWasSelected =" << folderWasSelected;
        // ignore if click on only folder selected
        if (folderWasSelected) {
            // do not toggle if only one folder selected
            if (folders < 2) return;
            // if (G::isLogger)
                G::log("FSTree::mousePressEvent", "Cmd, Toggle Remove");
            emit folderSelectionChange(dPath, "Remove", resetDataModel, recurse);
        }
        else {
            // if (G::isLogger)
                G::log("FSTree::mousePressEvent", "Cmd, Toggle Add");
            emit folderSelectionChange(dPath, "Add", resetDataModel, recurse);
        }
        QModelIndex index = fsFilter->mapFromSource(fsModel->index(dPath));
        selectionModel()->select(index, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
        prevIdx = index;
        return;
    }

    // Select visible unselected between previous selected folder to idx0
    if (isShift) {
        recurse = false;
        if (isAlt) recurse = true;
        QStringList foldersToAdd = selectVisibleBetween(prevIdx, idx0, recurse);
        if (G::isLogger || G::isFlowLogger)
            G::log("FSTree::mousePressEvent", "Modifiers: Shift, Select All Between");

        foreach(QString path, foldersToAdd) {
            resetDataModel = false;
            emit folderSelectionChange(path, "Add", resetDataModel, recurse);
            QModelIndex index = fsFilter->mapFromSource(fsModel->index(dPath));
            selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        prevIdx = index;
    }
    // QTreeView::mousePressEvent(event);
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
    if (idx0.isValid()) {
        hoverFolderName = idx0.data().toString();
        delegate->setHoveredIndex(idx0);
    } else {
        hoverFolderName = "";
        delegate->setHoveredIndex(QModelIndex());  // No row hovered
    }

    if (!isEnabled()) setEnabled(true);

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
    qDebug() << "FSTree::dragEnterEvent" << event-> dropAction() << event->modifiers();

    QModelIndexList selectedDirs = selectionModel()->selectedRows();
    if (selectedDirs.size() > 0) {
        dndOrigSelection = selectedDirs[0];
    }
    event->acceptProposedAction();
}

void FSTree::dragLeaveEvent(QDragLeaveEvent *event)
{
    delegate->setHoveredIndex(QModelIndex());  // Clear highlight when mouse leaves
    QApplication::restoreOverrideCursor(); // Restore the original cursor when drag leaves
    event->accept();
    viewport()->update();
}

void FSTree::dragMoveEvent(QDragMoveEvent *event)
{
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


void FSTree::dropEvent(QDropEvent *event)
{
/*
    Copy files to FSTree folder.  If Qt::MoveAction then delete the source files after
    the copy operation.  If DnD is internal then also copy/move any sidecar files.
*/
    QString src = "FSTree::dropEvent";
    if (G::isLogger) G::log(src);
    qDebug() << "FSTree::dropEvent";

    // QString dropDir = indexAt(event->position().toPoint()).data(QFileSystemModel::FilePathRole).toString();

    // HandleDropOnFolder handle(event, dropDir);

    if (QMessageBox::question(nullptr, "Confirm", "Accept drop operation?",
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        event->ignore();
        return;
    }

    const QMimeData *mimeData = event->mimeData();
    if (!mimeData->hasUrls()) {
        event->ignore();
        return;
    }

    QString dropDir = indexAt(event->position().toPoint()).data(QFileSystemModel::FilePathRole).toString();

    // START MIRRORED CODE SECTION
    // This code section is mirrored in BookMarks::dropEvent.::dropEvent.
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

        // copy thew files
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

void FSTree::debugSelectedFolders(QString msg)
{
    qDebug() << "SELECTION" << msg;
    for (int i = 0; i < selectionModel()->selectedRows().count(); i++) {
        QModelIndex index = selectionModel()->selectedRows().at(i);
        QString path = index.data(QFileSystemModel::FilePathRole).toString();
        qDebug() << "SELECTION" << path;
    }
}

void FSTree::test()
{
    QList<QPersistentModelIndex> visibleIndexes;
    QStringList visibleFolders;

    QModelIndex idx1 = indexAt(viewport()->rect().topLeft());
    QModelIndex idx2 = indexAt(viewport()->rect().bottomLeft());

    // Get the top and bottom visual rectangles
    QRect topRect = visualRect(idx1);
    QRect bottomRect = visualRect(idx2);

    // Ensure idx1 is above idx2
    if (topRect.top() > bottomRect.top()) {
        std::swap(topRect, bottomRect);
    }

    // Calculate row height based on the distance between the top and bottom of the topRect
    int rowHeight = topRect.height();

    // Calculate the starting and ending Y positions
    int yStart = topRect.top();
    int yEnd = bottomRect.bottom();

    // Iterate through each row within the viewport rectangle range
    for (int y = yStart; y <= yEnd; y += rowHeight) {
        QPoint pointInRow(viewport()->rect().left(), y);
        QModelIndex index = indexAt(pointInRow);

        // Check if the index is valid, then create an index in column 0
        if (index.isValid()) {
            QModelIndex idx0 = index.siblingAtColumn(0);
            visibleIndexes.append(idx0);

        }
    }

    // Select the unselected indexes
    for (const QPersistentModelIndex &index : visibleIndexes) {
        qDebug().noquote()
                 << "FSTree::test"
                 << "path =" << index.data(QFileSystemModel::FilePathRole).toString().leftJustified(60)
                 << "isValid =" << QVariant(index.isValid()).toString().leftJustified(5)
                 << "isSelected =" << QVariant(selectionModel()->isSelected(index)).toString().leftJustified(5)
            ;

        if (!index.isValid()) continue;
        // if (!selectionModel()->isSelected(index)) {
        //     selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        //     visibleFolders.append(index.data(QFileSystemModel::FilePathRole).toString());
        // }
    }
}

void FSTree::howThisWorks()
{
    if (G::isLogger) G::log("FSTree::howThisWorks");

    QDialog *dlg = new QDialog;
    Ui::FoldersHelpDlg *ui = new Ui::FoldersHelpDlg;
    ui->setupUi(dlg);
    dlg->setWindowTitle("Folders Panel Help");
    dlg->setStyleSheet(G::css);
    dlg->exec();
}
