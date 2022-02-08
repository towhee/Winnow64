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

FSModel::FSModel(QWidget *parent, Metadata *metadata, QHash<QString, QString> &count,
                 QHash<QString, QString> &combineCount, bool &combineRawJpg)
                 : QFileSystemModel(parent),
                   combineRawJpg(combineRawJpg),
                   count(count),
                   combineCount(combineCount)
{
    QStringList *fileFilters = new QStringList;
    dir = new QDir();

    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);
}

//bool FSModel::event(QEvent *event)
//{
//    qDebug() << __FUNCTION__ << event;
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

QVariant FSModel::data(const QModelIndex &index, int role) const
{
    /* Return image count for each folder by looking it up in the QHash count which is built
    in FSTree::getImageCount and referenced here. This is much faster than performing the
    image count "on-the-fly" here, which causes scroll latency. */
    if (index.column() == imageCountColumn) {
        if (role == Qt::DisplayRole && showImageCount) {
            QString path = QFileSystemModel::data(index, QFileSystemModel::FilePathRole).toString();
            if (combineRawJpg) return combineCount.value(path);
            else return count.value(path);
        }
        if (role == Qt::TextAlignmentRole)
            return static_cast<QVariant>(Qt::AlignRight | Qt::AlignVCenter);
        else
            return QVariant();
    }
    // return tooltip for folder path
    if (index.column() == 0) {
        if (role == Qt::ToolTipRole) {
            return QFileSystemModel::data(index, QFileSystemModel::FilePathRole);
        }
        else
            return QFileSystemModel::data(index, role);
    }
    // return parent class data
    return QFileSystemModel::data(index, role);
}

/*------------------------------------------------------------------------------
CLASS FSTree subclassing QTreeView
------------------------------------------------------------------------------*/

FSTree::FSTree(QWidget *parent, Metadata *metadata) : QTreeView(parent)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    this->metadata = metadata;
    fileFilters = new QStringList;
    dir = new QDir();
    viewport()->setObjectName("fsTreeViewPort");
    setObjectName("fsTree");

    // create model and filter
    createModel();

    // setup treeview
    for (int i = 1; i <= 3; ++i)
        hideColumn(i);

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
    if (G::isLogger) G::log(__FUNCTION__); 
    fsModel = new FSModel(this, metadata, count, combineCount, combineRawJpg);
    fsModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
    fsModel->setRootPath(fsModel->myComputer().toString());
//    fsModel->setRootPath("");

    // watcher
//    watch = new QFileSystemWatcher;

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
//    watch->addPaths(mountedDrives);

    /*
#ifdef Q_OS_LINIX   // not recognized for some reason
    fsModel->setRootPath("");
//    fsModel->setRootPath("/home");
#endif

#ifdef Q_OS_WIN
    fsModel->setRootPath("");
//    fsModel->setRootPath(fsModel->myComputer().toString());
//    for (int i = 0; i < mountedDrives.length(); ++i) {
//        fsModel->setRootPath(mountedDrives.at(i));
//    }
#endif

#ifdef Q_OS_MACOS
    fsModel->setRootPath("/Volumes");
    fsModel->setRootPath("/Users");
#endif
    //*/

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
    if (G::isLogger) G::log(__FUNCTION__); 
    delete fsModel;
    createModel();
}

bool FSTree::isShowImageCount()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    return fsModel->showImageCount;
}

void FSTree::setShowImageCount(bool showImageCount)
{
    fsModel->showImageCount = showImageCount;
}

void FSTree::updateFolderImageCount(QString dirPath)
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__);
    getImageCount(dirPath, true, "updateImageCount");
    fsFilter->invalidate();
}

void FSTree::updateVisibleImageCount()
{
    for (int i = 0; i < count.size(); ++i) {
        updateFolderImageCount((count.keys().at(i)));
    }
}

void FSTree::scrollToCurrent()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__); 
    QModelIndex idx = getCurrentIndex();
    if (idx.isValid()) scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

bool FSTree::select(QString dirPath)
{
    if (G::isLogger) G::log(__FUNCTION__); 
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
//    if (G::isLogger) G::log(__FUNCTION__);
//    return fsFilter->mapFromSource(fsModel->index(dirPath));
//}

QModelIndex FSTree::getCurrentIndex()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    QModelIndex idx;
    if (selectedIndexes().size() > 0)
//        idx = fsFilter->mapFromSource(selectedIndexes().first());
        idx = selectedIndexes().first();
    else idx = fsModel->index(-1, -1, QModelIndex());
    return idx;
}

void FSTree::resizeColumns()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    if (fsModel->showImageCount) {
//        imageCountColumnWidth = 45;
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
    if (G::isLogger) G::log(__FUNCTION__); 
//    qDebug() << __FUNCTION__ << idx << t.elapsed();
    if (t.elapsed() > 25) {
        QString src = __FUNCTION__;
        QTimer::singleShot(50, [this, src]() {getVisibleImageCount(src);});
    }
    t.restart();
}

void FSTree::getVisibleImageCount(QString src)
{
/*
    This function stores the image count (that winnow can read) for each folder that is
    visible (expanded) in the TreeView in a QHash. The QHash is referenced in the FSModel and
    displayed in a subclass of QFileSystemModel data, where the image count has been added as
    column 4.

    Two QHash are updated: count for the total eligible image files and combineCount for when
    raw+jpg are combined.
*/
    if (G::isLogger) G::log(__FUNCTION__, "Source: " + src);
    emit updateFileCount("Counting images in folders.");

    QModelIndex idx = indexAt(rect().topLeft());  // delta
    while (idx.isValid())
    {
        QString dirPath = idx.data(QFileSystemModel::FilePathRole).toString();
        getImageCount(dirPath, false, src);
        idx = indexBelow(idx);
    }
    emit updateFileCount("Select folder.");

}

void FSTree::getImageCount(QString const dirPath, bool changed, QString src)
{
    if (G::isLogger) G::log(__FUNCTION__, dirPath + "  Source: " + src);
//    qDebug() << __FUNCTION__ << src << dirPath;
    // counts is combineRawJpg
    if (!combineCount.contains(dirPath) || changed) {
        dir->setPath(dirPath);
        int n = 0;
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
            n++;
        }
        combineCount[dirPath] = QString::number(n, 'f', 0);
    }

    // counts if not combineRawJpg
    if (!count.contains(dirPath) || changed) {
        dir->setPath(dirPath);
        int n = dir->entryInfoList().size();
        count[dirPath] = QString::number(n, 'f', 0);
    }
}

//bool FSTree::event(QEvent *event)
//{
//    qDebug() << __FUNCTION__ << event;
//    return QTreeView::event(event);
//}

void FSTree::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    resizeColumns();
}

void FSTree::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QTreeView::selectionChanged(selected, deselected);
    emit selectionChange();
//    QtConcurrent::run(this, &FSTree::updateVisibleImageCount);
//    qDebug() << __FUNCTION__ << count;
}

void FSTree::mousePressEvent(QMouseEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__); 
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
    if (G::isLogger) G::log(__FUNCTION__); 
    QTreeView::mouseReleaseEvent(event);
}

void FSTree::mouseMoveEvent(QMouseEvent *event)
{
    QTreeView::mouseMoveEvent(event);
//    QModelIndex idx = indexAt(event->pos());
}

void FSTree::dragEnterEvent(QDragEnterEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__); 
	QModelIndexList selectedDirs = selectionModel()->selectedRows();
	if (selectedDirs.size() > 0) {
		dndOrigSelection = selectedDirs[0];
		event->acceptProposedAction();
	}
}

void FSTree::dragMoveEvent(QDragMoveEvent *event)
{
	setCurrentIndex(indexAt(event->pos()));
}

void FSTree::dropEvent(QDropEvent *event)
{
    if (G::isLogger) G::log(__FUNCTION__); 
	if (event->source())
	{
        QString fstreeStr = "FSTree";
		bool dirOp = (event->source()->metaObject()->className() == fstreeStr);
		emit dropOp(event->keyboardModifiers(), dirOp, event->mimeData()->urls().at(0).toLocalFile());
		setCurrentIndex(dndOrigSelection);
	}
}
