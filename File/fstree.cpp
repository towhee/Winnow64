#include "File/fstree.h"
#include "Main/global.h"
#include "Main/mainwindow.h"   // or whatever your MW header is actually called

extern QStringList mountedDrives;
QStringList mountedDrives;

/*------------------------------------------------------------------------------
FSTree Subsystem Overview
------------------------------------------------------------------------------*
The FSTree subsystem implements the left-hand Folder Tree panel in Winnow.
It displays the filesystem hierarchy, shows image counts beside each folder,
supports recursive folder selection, and prevents deep recursion that could
freeze the UI.  The system is composed of several cooperating classes:

    FSTree (QTreeView)
        ├── FSModel  (QFileSystemModel subclass)
        │     └── ImageCounter (QThread worker for async counting)
        ├── FSFilter (QSortFilterProxyModel subclass)
        └── HoverDelegate (QStyledItemDelegate subclass)

Together these classes provide a responsive, filterable, and asynchronous
filesystem view integrated with the global DataModel and Metadata subsystems.

-------------------------------------------------------------------------------
FSTree  (QTreeView subclass)
-------------------------------------------------------------------------------
- Owns FSModel (source) and FSFilter (proxy).
- Manages folder selection, recursion, and drag/drop.
- Enforces a recursion depth limit (maxExpandLimit) to prevent UI stalls.
- Marks over-limit folders (orange) using OverLimitRole = 300.
- Coordinates with the DataModel through folderSelectionChange() signals.
- Displays hover highlights via HoverDelegate.
- Handles platform drive refresh, selection modifiers, and context menus.

Key details:
- countSubdirsFast() uses a non-recursive QStack traversal for safety.
- selectRecursively() disables image count updates during recursion.
- Over-limit folders cannot be manually expanded (blocked in onItemExpanded()).
- rapidClick timer prevents double activation while stop() is processing.
- onRowsAboutToBeRemoved() clears selection to prevent Qt auto-selecting next row.

Mouse / Keyboard modifiers:
  None      → clear + select single folder (reset DataModel)
  Ctrl/Cmd  → toggle folder on/off
  Alt/Opt   → recursively select subfolders
  Shift     → select all visible folders between last and clicked row

-------------------------------------------------------------------------------
FSModel  (QFileSystemModel subclass)
-------------------------------------------------------------------------------
- Adds an image count column (#) at index 4.
- Exposes custom role OverLimitRole (300) for orange over-limit highlight.
- Launches ImageCounter threads to compute image counts asynchronously.
- Caches results in QHash<QString, QString> count.
- Integrates Metadata for supported image formats and RAW+JPG pairing.
- Emits dataChanged() when a count completes to repaint the column.

Notes:
- data() triggers ImageCounter on demand (lazy evaluation per visible folder).
- setData() intercepts OverLimitRole writes to track maxRecursedRoots.
- roleNames() includes "overLimit" for debugging or QML inspection.

-------------------------------------------------------------------------------
ImageCounter  (QThread)
-------------------------------------------------------------------------------
- Worker thread that counts images in a folder without blocking the UI.
- Uses QDirIterator and Metadata to filter eligible formats.
- Handles RAW+JPG pairing if combineRawJpg is true.
- Emits countReady(path, count) → FSModel updates cache and notifies view.

-------------------------------------------------------------------------------
FSFilter  (QSortFilterProxyModel subclass)
-------------------------------------------------------------------------------
- Filters out unwanted or hidden folders.
- macOS: shows only /Users and /Volumes at root, hides system/hidden dirs.
- Windows: excludes unmounted drives using global mountedDrives list.
- Linux: allows all.
- refresh() simply calls invalidateFilter().

-------------------------------------------------------------------------------
HoverDelegate  (QStyledItemDelegate subclass)
-------------------------------------------------------------------------------
- Also used by Bookmarks
- Handles hover and over-limit highlighting.
- hoverBackground = G::backgroundShade + 20.
- Over-limit text color = orange (255,165,0).
- setHoveredIndex() updates current hover and emits hoverChanged().
- paint() fills hover background and recolors text when OverLimitRole == true.
- FSTree connects hoverChanged → viewport()->update() for repaint.

-------------------------------------------------------------------------------
Over-Limit Protection (maxExpandLimit)
-------------------------------------------------------------------------------
Purpose: prevent runaway recursion during deep folder selections.

Workflow:
1. FSTree::selectRecursively() calls countSubdirsFast().
2. If subfolder count >= maxExpandLimit:
       fsModel->setData(path, true, OverLimitRole);
       markFolderOverLimit(path, true);
       fsModel->isMaxRecurse = true;
3. HoverDelegate paints the folder name orange.
4. Manual expansion is immediately collapsed in onItemExpanded().

Data members:
    FSModel::OverLimitRole = 300
    FSModel::isMaxRecurse
    FSModel::maxRecursedRoots (QStringList)
    FSTree::maxExpandLimit (default 100)

-------------------------------------------------------------------------------
Image Count Column
-------------------------------------------------------------------------------
- Optional fifth column (“#”) showing the number of image files in each folder.
- Visible only when fsModel->showImageCount == true.
- Updated via updateCount() and updateAFolderCount().
- Width adjusted dynamically in resizeColumns() based on font metrics.

-------------------------------------------------------------------------------
Drag & Drop
-------------------------------------------------------------------------------
- Accepts both internal (Winnow) and external (Finder/Explorer) drops.
- Internal drags infer sidecar files; external drops copy any file.
- Uses mirrored code from Bookmarks::dropEvent for consistency.
- Displays progress popup with ESC cancellation.
- Emits updateCounts() after CopyAction.

-------------------------------------------------------------------------------
Diagnostics and Debugging
-------------------------------------------------------------------------------
- diagnostics() → human-readable state summary.
- diagModel(selectionOnly) → dumps selected or expanded model with roles.
- test() → qDebug() dump of entire tree.
- OverLimitRole registration can be verified via diagModel() output.

-------------------------------------------------------------------------------
Key Constants and Relationships
-------------------------------------------------------------------------------
    OverLimitRole = 300  // must match in FSModel and HoverDelegate
    imageCountColumn = 4
    maxExpandLimit   = 100
    combineRawJpg    // global flag shared with Metadata
    G::stop, G::popup, G::isLogger, etc. govern background safety.

-------------------------------------------------------------------------------
Summary
-------------------------------------------------------------------------------
FSTree is a QTreeView-based folder browser tightly integrated with
DataModel and Metadata.  It supports asynchronous image counting,
recursive selection with over-limit protection, and visual feedback for
hover and recursion states.  The design balances responsiveness,
concurrency, and user feedback for very large folder hierarchies.

*------------------------------------------------------------------------------*/

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
    Add a column for imageCount to the model and display the image count beside each
    folder in the TreeView.

    Add the role OverLimitRole to flag folders that are recursively selected and have
    more than maxExpandLimit subfolders.
*/

FSModel::FSModel(QWidget *parent,
                 Metadata &metadata,
                 DataModel *dm,
                 bool &combineRawJpg)
                 : QFileSystemModel(parent),
                   combineRawJpg(combineRawJpg),
                   metadata(metadata)
{
    this->dm = dm;
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

QHash<int, QByteArray> FSModel::roleNames() const
{
    // Start with base roles from QFileSystemModel
    QHash<int, QByteArray> roles = QFileSystemModel::roleNames();

    // Add your custom role(s)
    roles.insert(OverLimitRole, "overLimit");

    return roles;
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

bool FSModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == OverLimitRole) {
        QString path = filePath(index);
        if (value.toBool())
            maxRecursedRoots.append(path);
        else
            maxRecursedRoots.removeOne(path);
        emit dataChanged(index, index, {OverLimitRole});
        return true;
    }
    return QFileSystemModel::setData(index, value, role);
}

QVariant FSModel::data(const QModelIndex &index, int role) const
{
    if (index.column() == imageCountColumn /*&& showImageCount*/) {
        /*
        Return image count for each folder by looking it up in the QHash count which is
        built in FSTree::getImageCount and referenced here. This is much faster than
        performing the image count "on-the-fly" here, which causes scroll latency.

        If the folder has been recursed and exceeded maxExpandLimit, then the total
        images for all subfolders is showm. This is calculated by calling
        dm->recurseImageCount() and counting the images in the datamodel, which is much
        faster.
        */
        if (role == Qt::DisplayRole) {
            QString dPath = filePath(index);
            /*
            qDebug() << "FSModel::data"
                     << "isMaxRecurse =" << isMaxRecurse
                     << "maxRecursedRoots.contains(dPath) =" << maxRecursedRoots.contains(dPath)
                     << dPath; //*/

            if (isMaxRecurse && maxRecursedRoots.contains(dPath)) {
                return dm->recurseImageCount(dPath);
            }

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

        if (role == OverLimitRole) {
            QString path = filePath(index);
            return maxRecursedRoots.contains(path);
        }

        return QFileSystemModel::data(index, role);
    }

    return QVariant();
}

/*------------------------------------------------------------------------------
CLASS FSTree subclassing QTreeView
------------------------------------------------------------------------------*/

FSTree::FSTree(MW *mw, DataModel *dm, Metadata *metadata, QWidget *parent)
        : QTreeView(parent), delegate(new HoverDelegate(this))
{
    if (G::isLogger) G::log("FSTree::FSTree");
    this->mw = mw;
    this->dm = dm;
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

    // prevent expansion of folders where OverLimitRole is true
    connect(this, &QTreeView::expanded,
            this, &FSTree::onItemExpanded);

    // Repaint when hover changes: Lambda function to call update
    connect(delegate, &HoverDelegate::hoverChanged, this->viewport(), [this]() {
            this->viewport()->update();});

    isDebug = true;
}

FSTree::~FSTree() {}

void FSTree::createModel()
{
/*
    Create the model and filter in a separate function as it is also used to refresh
    the folders by deleting the model and re-creating it.
*/
    if (G::isLogger) G::log("FSTree::createModel");
    fsModel = new FSModel(this, *metadata, dm, combineRawJpg);
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
    // qDebug() << "FSTree::refreshModel";
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
    // select(currentFolderPath());
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

bool FSTree::isItemVisible(const QModelIndex idx)
{
    QRect itemRect = visualRect(idx);
    QRect viewRect = viewport()->rect();

    return viewRect.intersects(itemRect) && itemRect.isValid();
}

void FSTree::scrollToCurrent()
{
/*

*/
    if (G::isLogger) G::log("FSTree::scrollToCurrent");
    QModelIndex idx = getCurrentIndex();

    if (!idx.isValid()) return;
    if (isItemVisible(idx)) return;

    scrollTo(idx, QAbstractItemView::PositionAtCenter);
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
        emit folderSelectionChange(folderPath, G::FolderOp::Add, resetDataModel, recurse);
        setCurrentIndex(index);
        // scrollToCurrent();
        selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        return true;
    }

    if (modifier == "Recurse") {
        resetDataModel = true;
        recurse = true;
        emit folderSelectionChange(folderPath, G::FolderOp::Add, resetDataModel, recurse);
        setCurrentIndex(index);
        // scrollToCurrent();
        selectionModel()->clearSelection();
        selectRecursively(folderPath);
        return true;
    }

    if (modifier == "USBDrive") {
        resetDataModel = true;
        recurse = true;
        emit folderSelectionChange(folderPath, G::FolderOp::Add, resetDataModel, recurse);
        setCurrentIndex(index);
        // scrollToCurrent();
        selectionModel()->clearSelection();
        selectRecursively(folderPath);
        setExpanded(index, false);
        return true;
    }

    return false;
}

void FSTree::clearFolderOverLimit()
{
    // qDebug() << "FSTree::clearFolderOverLimit";
    for (QString folderPath : fsModel->maxRecursedRoots) {
        const QModelIndex src = fsModel->index(folderPath);
        if (!src.isValid()) continue;
        fsModel->setData(src, false, FSModel::OverLimitRole);
    }
    fsModel->maxRecursedRoots.clear();
    fsModel->isMaxRecurse = false;
}

void FSTree::markFolderOverLimit(const QString& folderPath, bool on)
{
    if (G::stop) return;

    const QModelIndex src = fsModel->index(folderPath);
    if (!src.isValid()) return;

    fsModel->setData(src, on, FSModel::OverLimitRole);  // highlight flag
    const QModelIndex idx = fsFilter->mapFromSource(src);
    if (idx.isValid()) viewport()->update(visualRect(idx));
    fsModel->isMaxRecurse = true;
    /*
    qDebug() << "FSTree::markFolderOverLimit overLimitColor ="
             << overLimitColor
             // << idx
             // << visualRect(idx)
             << folderPath; //*/

}

bool FSTree::isSubDirsOverLimit(const QString &root, int hardCap)
{
    if (G::isLogger)
        G::log("FSTree::countSubdirsFast", root);

    int count = 0;
    QStack<QString> stack;
    stack.push(root);

    while (!stack.isEmpty()) {
        if (G::stop || count > hardCap)
            break;

        const QString path = stack.pop();
        QDir dir(path);
        const QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs |
                                      QDir::NoDotAndDotDot |
                                      QDir::NoSymLinks);


        for (const QFileInfo &fi : subdirs) {
            if (G::stop || ++count > hardCap)
                return true;

            stack.push(fi.absoluteFilePath());
        }
    }
    return false;
}

void FSTree::selectRecursively(QString folderPath, bool toggle)
{
    if (G::isLogger)
        G::log("FSTree::selectRecursively", folderPath);

    // disable heavy UI bits
    setShowImageCount(false);

    // Too many subfolders: just select root folder and mark orange
    if (isSubDirsOverLimit(folderPath, maxExpandLimit)) {
        fsModel->isMaxRecurse = true;
        fsModel->maxRecursedRoots.append(folderPath);
        markFolderOverLimit(folderPath, true);
        QPersistentModelIndex index = fsFilter->mapFromSource(fsModel->index(folderPath));
        selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        setShowImageCount(true);
        return;
    }

    // Proceed to expansion and selection only if below limit
    QStringList recursedFolders;
    recursedFolders.append(folderPath);

    QDirIterator it(folderPath, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();

        const QFileInfo fi = it.fileInfo();
        if (!fi.isDir()) continue;

        QString dPath = fi.absoluteFilePath();
        recursedFolders.append(dPath);
        QPersistentModelIndex index = fsFilter->mapFromSource(fsModel->index(dPath));
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    }

    for (const QString &path : recursedFolders) {

        QModelIndex index = fsFilter->mapFromSource(fsModel->index(path));
        if (toggle)
            selectionModel()->select(index, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
        else
            selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }

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
        imageCountColumnWidth = fm.boundingRect("(99999").width();
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

void FSTree::onItemExpanded(const QModelIndex &index)
{
    if (index.data(FSModel::OverLimitRole).toBool()) {
        /*
        qDebug() << "Blocking manual expansion of OverLimit item:"
                 << index.data(Qt::DisplayRole).toString();
                    //*/

        // Collapse immediately
        QTreeView::setExpanded(index, false);

        // Optional: provide feedback
        G::popup->showPopup("This folder contains too many subfolders to expand automatically.");
    }
}

// void FSTree::hasExpanded(const QPersistentModelIndex &index)
// {
//     if (isDebug || G::isLogger) {
//         QString msg;
//         if (justExpandedIndex == index) msg = "justExpandedIndex == index ";
//         else msg = "justExpandedIndex != index ";
//         msg += " Expanded = " + index.data().toString() +
//                       " Just Expanded = " + index.data().toString();
//         G::log("FSTree::hasExpanded", index.data().toString());
//     }
//     // qDebug() << "FSTree::hasExpanded" << t.elapsed() << index << justExpandedIndex;

//     if (justExpandedIndex == index) {
//         emit indexExpanded();
//     }
// }

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

    // ignore rapid mouse press if still processing MW::stop
    qint64 ms = rapidClick.restart();
    if (ms < 500) {
        event->ignore();
        qApp->beep();
        G::popup->showPopup("Rapid clicks are verboten");
        return;
    }

    if (G::stop || G::isModifyingDatamodel) {
        G::popup->showPopup("Busy, try new folder in a sec.", 1000);
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
             << decorationRect.contains(event->pos())
             << "index.data(FSModel::OverLimitRole).toBool() ="
             << index.data(FSModel::OverLimitRole).toBool()
        ;
    // */
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
    // bool recurse = false;
    bool resetDataModel = false;
    bool isAlt = event->modifiers() & Qt::AltModifier;
    bool isCtrl = event->modifiers() & Qt::ControlModifier;
    bool isShift = event->modifiers() & Qt::ShiftModifier;
    bool isMeta = event->modifiers() & Qt::MetaModifier;
    bool isNoModifier = event->modifiers() == Qt::NoModifier;

    if (isMeta) return;

    bool isNoSelection = selectionModel()->selectedRows().isEmpty();
    bool isClearAdd = isNoModifier;
    bool isToggle = isCtrl;
    bool isRecurse = isAlt;

    resetDataModel = (
                        (isNoSelection && (isClearAdd || isRecurse))
                        ||
                        ((isClearAdd || isRecurse) && !(isToggle || isShift))
                     );

    if (isDebug)
    {
        qDebug() << " ";
        qDebug().noquote()
            << "FSTree::mousePressEvent"
            << "isClearAdd =" << isClearAdd
            << "isRecurse =" << isRecurse
            << "isToggle =" << isToggle
            << "isShift =" << isShift
            << "resetDataModel =" << resetDataModel
            << dPath
            ;
    }


    // reset datamodel
    if (resetDataModel) {
        QString step = "Selecting folders.\n";
        QString escapeClause = "\nPress \"Esc\" to stop.";
        mw->setCentralMessage(step + escapeClause);
        // qApp->processEvents();
        // fsModel->maxRecursedRoots.clear();
        clearFolderOverLimit();
        G::allMetadataLoaded = false;
    }

    // Clear and select folder
    if (isClearAdd) {
        // qDebug() << "FSTree::mousePressEvent NEW SELECTION" << path;
        if (G::isLogger || G::isFlowLogger) G::log("FSTree::mousePressEvent", "No modifiers, new instance");
        QTreeView::mousePressEvent(event);
        emit folderSelectionChange(dPath, G::FolderOp::Add, resetDataModel, isRecurse);
        prevIdx = index;
    }

    // recurse load all subfolders images
    else if (isRecurse) {
        if (isToggle) {
            // to do: deal with toggle
            if (G::isLogger || G::isFlowLogger) G::log("FSTree::mousePressEvent", "Modifers: Opt + Cmd, Recurse");
            selectRecursively(dPath, isToggle);
            emit folderSelectionChange(dPath, G::FolderOp::Toggle, resetDataModel, isRecurse);
        }
        else {
            if (G::isLogger || G::isFlowLogger) G::log("FSTree::mousePressEvent", "Modifiers: Opt, New instance and Recurse");
            selectionModel()->clearSelection();
            selectRecursively(dPath);
            emit folderSelectionChange(dPath, G::FolderOp::Add, resetDataModel, isRecurse);
        }
        prevIdx = index;
    }

    // toggle folder
    else if (isToggle) {
        int folders = selectedFolderPaths().count();
        bool folderWasSelected = selectedFolderPaths().contains(dPath);
        // qDebug() << "FSTree::mousePressEvent  isCtrl =" << isCtrl << "folderWasSelected =" << folderWasSelected;
        // ignore if click on only folder selected
        if (folderWasSelected) {
            // do not toggle if only one folder selected
            if (folders < 2) return;
            // if (G::isLogger)
                G::log("FSTree::mousePressEvent", "Cmd, Toggle Remove");
            emit folderSelectionChange(dPath, G::FolderOp::Remove, resetDataModel, isRecurse);
        }
        else {
            // if (G::isLogger)
                G::log("FSTree::mousePressEvent", "Cmd, Toggle Add");
            emit folderSelectionChange(dPath, G::FolderOp::Add, resetDataModel, isRecurse);
        }
        QModelIndex index = fsFilter->mapFromSource(fsModel->index(dPath));
        selectionModel()->select(index, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
        prevIdx = index;
    }

    // Select visible unselected between previous selected folder to idx0
    else if (isShift) {
        // if first mousePress includes shift (nothing has been selected yet)
        if (!prevIdx.isValid()) {
                QModelIndexList selectedIndexes = selectionModel()->selectedIndexes();
            if (!selectedIndexes.isEmpty()) {
                prevIdx = selectedIndexes.first();
            } else {
                prevIdx = indexAt(QPoint(0, 0));
            }
        }
        QStringList foldersToAdd = selectVisibleBetween(prevIdx, idx0, isRecurse);
        if (G::isLogger || G::isFlowLogger)
            G::log("FSTree::mousePressEvent", "Modifiers: Shift, Select All Between");

        foreach(QString path, foldersToAdd) {
            resetDataModel = false;
            emit folderSelectionChange(path, G::FolderOp::Add, resetDataModel, isRecurse);
            QModelIndex index = fsFilter->mapFromSource(fsModel->index(path));
            selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        prevIdx = index;
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
    /*
    qDebug() << "FSTree::contextMenuEvent"
             << "idx =" << idx
             << "idx.isValid() =" << idx.isValid()
             << "event->pos() =" << event->pos()
             << "folderName =" << folderName
        ; //*/
}

void FSTree::dragEnterEvent(QDragEnterEvent *event)
{
    if (G::isLogger) G::log("FSTree::dragEnterEvent");

    bool isInternal;
    QString msg;
    event->source() ? isInternal = true : isInternal = false;
    msg = "Copy to folder.";
    if (isInternal) {
        if (!(event->keyboardModifiers() & Qt::AltModifier)) {
            msg = "Move to folder.";
        }
    }
    G::popup->showPopup(msg, 0);

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
    G::popup->reset();
}

void FSTree::dragMoveEvent(QDragMoveEvent *event)  // not showing "View" label
{
    QModelIndex idx = indexAt(event->position().toPoint());
    // same row, column 0 (folder name)
    QModelIndex idx0 = idx.sibling(idx.row(), 0);
    if (idx0.isValid()) {
        hoverFolderName = idx0.data().toString();
        delegate->setHoveredIndex(idx0);
    } else {
        hoverFolderName = "";
        delegate->setHoveredIndex(QModelIndex());  // No row hovered
    }

    // show the dragLabel to show images from the source folder
    if (event->mimeData()->hasUrls()) {
        // viewport()->update();  // Refresh view
    }
    else {
        event->ignore();
    }
}


void FSTree::dropEvent(QDropEvent *event)
{
/*
    Copy files to FSTree folder.  If Qt::MoveAction then delete the source files after
    the copy operation.  If DnD is internal then also copy/move any sidecar files.
*/
    QString src = "FSTree::dropEvent";
    if (G::isLogger) G::log(src);
    // qDebug() << "FSTree::dropEvent";

    G::popup->reset();
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
        if (G::useProcessEvents) qApp->processEvents(); // processEvents is necessary
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

QString FSTree::diagnostics()
{
    QString reportString;
    QTextStream rpt;
    int dots = 30;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "FSTree Diagnostics");
    rpt << "\n";
    rpt << "\n" << G::sj("imageCountColumn", dots) << fsModel->imageCountColumn;
    rpt << "\n" << G::sj("imageCountColumnWidth", dots) << imageCountColumnWidth;
    rpt << "\n" << G::sj("isMaxRecurse", dots) << QVariant(fsModel->isMaxRecurse).toString();
    rpt << "\n" << G::sj("maxExpandLimit", dots) << maxExpandLimit;
    rpt << "\n" << G::sj("Qt::UserRole", dots) << Qt::UserRole;
    rpt << "\n" << G::sj("OverLimitRole", dots) << FSModel::OverLimitRole;
    rpt << "\n";
    rpt << "Selected recursive folders where subfolders exceed maxExpandLimit:\n";
    for (QString s : fsModel->maxRecursedRoots) {
        rpt << "  " << s << "\n";
    }
    rpt << "\n";

    rpt << "Selection fsModel detail:\n";
    rpt << diagModel(true);

    return reportString;
}

QString FSTree::diagModel(bool isSelectionOnly)
{
    QAbstractItemModel *model = this->model();   // ✅ use the view's model (proxy or source)
    if (!model)
        return "⚠️ No model set on FSTree.\n";

    QString out;
    QTextStream stream(&out);
    // stream << "\n--- Dumping FSTree model contents ---\n";
    stream << "Selection only: " << (isSelectionOnly ? "true\n" : "false\n");

    const QHash<int, QByteArray> roleNames = model->roleNames();

    auto dumpIndex = [&](const QModelIndex &rowIdx, int depth)
    {
        if (!rowIdx.isValid()) {
            stream << QString(depth * 2, ' ') << "⚠️ Invalid index\n";
            return;
        }

        const QString indent(depth * 2, ' ');
        const int cols = model->columnCount(rowIdx.parent());
        const int children = model->rowCount(rowIdx);

        stream << indent << "Row " << rowIdx.row() << " (" << children << " children)\n";

        for (int c = 0; c < cols; ++c) {
            const QModelIndex cIdx = rowIdx.sibling(rowIdx.row(), c); // ✅ same row, different column
            if (!cIdx.isValid()) {
                stream << indent << "  Col " << c << ": (invalid)\n";
                continue;
            }

            const QString colName = model->headerData(c, Qt::Horizontal).toString();
            const QVariant dispVar = model->data(cIdx, Qt::DisplayRole);
            const QString display = dispVar.isValid() ? dispVar.toString() : QString();

            stream << indent << "  Col " << c << ": " << colName
                   << " = \"" << display << "\"\n";

            for (auto it = roleNames.constBegin(); it != roleNames.constEnd(); ++it) {
                const QVariant v = model->data(cIdx, it.key());
                if (v.isValid() && !(v.typeId() == QMetaType::QString && v.toString().isEmpty())) {
                    stream << indent << "    Role " << it.key()
                    << " (" << QString::fromUtf8(it.value()) << ") = "
                    << v.toString() << "\n";
                }
            }
        }
    };

    // Recurse only through *expanded* branches in the view
    std::function<void(const QModelIndex&, int)> dumpExpanded =
        [&](const QModelIndex &parent, int depth)
    {
        const int rows = model->rowCount(parent);
        for (int r = 0; r < rows; ++r) {
            const QModelIndex idx = model->index(r, 0, parent);
            if (!idx.isValid()) continue;

            dumpIndex(idx, depth);

            if (this->isExpanded(idx))           // ✅ expansion state lives on the view
                dumpExpanded(idx, depth + 1);
        }
    };

    if (isSelectionOnly) {
        QItemSelectionModel *sel = this->selectionModel();
        if (!sel) {
            stream << "⚠️ No selection model available.\n";
        } else {
            const QModelIndexList rows = sel->selectedRows(0); // indexes in the *view’s* model
            if (rows.isEmpty()) {
                stream << "⚠️ No rows selected.\n";
            } else {
                for (const QModelIndex &idx : rows)
                    dumpIndex(idx, 0);           // ❗ no recursion when selection-only
            }
        }
    } else {
        dumpExpanded(QModelIndex(), 0);
    }

    // stream << "--- End of model dump ---\n";
    stream.flush();
    return out;
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
    QAbstractItemModel *model = fsModel;  // or fsFilter if you want the filtered model
    if (!model) {
        qWarning() << "No model set in FSTree.";
        return;
    }

    qDebug().noquote() << "\n--- Dumping FSTree model contents ---";

    std::function<void(const QModelIndex&, int)> dumpRow =
        [&](const QModelIndex &parent, int depth)
    {
        int rows = model->rowCount(parent);
        int cols = model->columnCount(parent);

        for (int r = 0; r < rows; ++r) {
            QModelIndex idx = model->index(r, 0, parent);
            QString indent(depth * 2, ' ');
            qDebug().noquote() << QString("%1Row %2 (%3 children)")
                                      .arg(indent)
                                      .arg(r)
                                      .arg(model->rowCount(idx));

            for (int c = 0; c < cols; ++c) {
                QModelIndex cIdx = model->index(r, c, parent);
                QString colName = model->headerData(c, Qt::Horizontal).toString();
                QString display = model->data(cIdx, Qt::DisplayRole).toString();
                qDebug().noquote() << QString("%1  Col %2: %3 = \"%4\"")
                                          .arg(indent)
                                          .arg(c)
                                          .arg(colName)
                                          .arg(display);

                // Dump all roles for this cell
                QHash<int, QByteArray> roleNames = model->roleNames();
                for (auto it = roleNames.constBegin(); it != roleNames.constEnd(); ++it) {
                    QVariant val = model->data(cIdx, it.key());
                    if (val.isValid() && !val.toString().isEmpty()) {
                        qDebug().noquote() << QString("%1    Role %2 (%3) = %4")
                        .arg(indent)
                            .arg(it.key())
                            .arg(QString::fromUtf8(it.value()))
                            .arg(val.toString());
                    }
                }
            }

            // Recurse into child rows
            dumpRow(idx, depth + 1);
        }
    };

    dumpRow(QModelIndex(), 0);
    qDebug().noquote() << "--- End of model dump ---\n";
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
