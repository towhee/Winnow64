#include "File/fstree.h"

QStringList mountedDrives;

/*------------------------------------------------------------------------------
CLASS FSFilter subclassing QSortFilterProxyModel
------------------------------------------------------------------------------*/

FSFilter::FSFilter(QObject *parent) : QSortFilterProxyModel(parent)
{

}

bool FSFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{

#ifdef Q_OS_WIN
    if (!sourceParent.isValid()) {      // if is a drive
        QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
//        bool okay = idx.isValid();
        QString path = idx.data(QFileSystemModel::FilePathRole).toString();
        bool mounted = mountedDrives.contains(path);
        if (!mounted) return false;     // do not accept unmounted drives
    }
    return true;
#endif

#ifdef Q_OS_MAC
    if (sourceParent.row() == -1) return true;
    if (!sourceParent.isValid()) return true;

    QString fParentPath = sourceParent.data(QFileSystemModel::FilePathRole).toString();
    QString fPath = sourceParent.child(sourceRow, 0).data(QFileSystemModel::FilePathRole).toString();
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
//   return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
#endif

    return true;
}

/*------------------------------------------------------------------------------
CLASS FSModel subclassing QFileSystemModel
------------------------------------------------------------------------------*/
FSModel::FSModel(QWidget *parent, Metadata *metadata /*, bool showImageCount */) : QFileSystemModel(parent)
{
    QStringList *fileFilters = new QStringList;
    dir = new QDir();
//    this->showImageCount = showImageCount;

    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);
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
        return QVariant();
     }
     else
        return QFileSystemModel::headerData(section, orientation, role);
}

QVariant FSModel::data(const QModelIndex &index, int role) const
{
//    QString path = qvariant_cast<QString>(QFileSystemModel::data(index, QFileSystemModel::FilePathRole));
//    qDebug() << G::t.restart() << "\t" << "FSModel::data -" << path;
//    bool mounted = mountedDrives.contains(path);
////    if (path == "O:/") {
//        bool okay = index.isValid();
//        QFileInfo info(path);
//        bool readable = info.isReadable();
////    }

//    if (!index.isValid() || index.model() != this)
//        return QVariant();

    // return image count for each folder
    if (index.column() == imageCountColumn) {
        if (role == Qt::DisplayRole && showImageCount) {
            QString fPath = qvariant_cast<QString>
                    (QFileSystemModel::data(index, QFileSystemModel::FilePathRole));
            dir->setPath(fPath);
            int count = dir->entryInfoList().size();
            QString imageCount = "";
            if (count > 0)
                imageCount = QString::number(count, 'f', 0);
            return imageCount;
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->metadata = metadata;
    fileFilters = new QStringList;
    dir = new QDir();

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
}

void FSTree::createModel()
{
/*
Create the model and filter in a separate function as it is also used to refresh
the folders by deleting the model and re-creating it.
*/
    fsModel = new FSModel(this, metadata);
    fsModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
    fsModel->setRootPath("");

#ifdef Q_OS_LINIX   // not recognized for some reason
    fsModel->setRootPath("");
//    fsModel->setRootPath("/home");
#endif

#ifdef Q_OS_WIN
    // get mounted drives only
    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
/*        qDebug() << G::t.restart() << "\t" << "FSTree::createModel  " << storage.rootPath()
                 << "storage.isValid()" << storage.isValid()
                 << "storage.isReady()" << storage.isReady()
                 << "storage.isReadOnly()" << storage.isReadOnly();
                 */
        if (storage.isValid() && storage.isReady()) {
            if (!storage.isReadOnly()) {
                mountedDrives << storage.rootPath();
            }
        }
    }
//    fsModel->setRootPath("D:/");
    fsModel->setRootPath(fsModel->myComputer().toString());
#endif

#ifdef Q_OS_MACOS  // Q_OS_MACOS
    fsModel->setRootPath("/Volumes");
    fsModel->setRootPath("/Users");
#endif

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
    delete fsModel;
    createModel();
}

bool FSTree::getShowImageCount()
{
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndex idx = getCurrentIndex();
    if (idx.isValid()) scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

void FSTree::select(QString dirPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    setCurrentIndex(fsFilter->mapFromSource(fsModel->index(dirPath)));
}

QModelIndex FSTree::getCurrentIndex()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndex idx;
    if (selectedIndexes().size() > 0)
        idx = fsFilter->mapFromSource(selectedIndexes().first());
    else idx = fsModel->index(-1, -1, QModelIndex());
    return idx;
}

void FSTree::resizeColumns()
{
    if (fsModel->showImageCount) {
        imageCountColumnWidth = 45;
        showColumn(4);
        setColumnWidth(4, imageCountColumnWidth);
    }
    else {
        imageCountColumnWidth = 0;
        hideColumn(4);
    }
    setColumnWidth(0, width() - G::scrollBarThickness - imageCountColumnWidth);
}

void FSTree::paintEvent(QPaintEvent *event)
{
    resizeColumns();
    QTreeView::paintEvent(event);
}

void FSTree::dragEnterEvent(QDragEnterEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
	if (event->source())
	{
		QString fstreeStr="FSTree";
		bool dirOp = (event->source()->metaObject()->className() == fstreeStr);
		emit dropOp(event->keyboardModifiers(), dirOp, event->mimeData()->urls().at(0).toLocalFile());
		setCurrentIndex(dndOrigSelection);
	}
}

 /*Tried to update count after tree created but no access to add to datamodel
 in QFileSystemModel.  The solution is to return data on-the-fly that is not
 stored in the datamodel.  This causes the tree to be laggy.

 Kept as comment for future reference

void FSTree::showSupportedImageCount()
{
    // do some initializing
    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    walkTree(rootIndex());
}

void FSTree::walkTree(const QModelIndex &row)
{
    if (fsModel->hasChildren(row)) {
        for (int i = 0; i < fsModel->rowCount(row); ++i){
            QModelIndex idx = fsModel->index(i, 0, row);
            QString fPath = qvariant_cast<QString>(idx.data(QFileSystemModel::FilePathRole));
            dir->setPath(fPath);
            QModelIndex idx2 = fsModel->index(i, 4, row);
            fsModel->setData(idx2, dir->entryInfoList().size(), Qt::DisplayRole);
            fsModel->setData(idx2, dir->entryInfoList().size(), Qt::EditRole);
            qDebug() << G::t.restart() << "\t" << fPath << dir->entryInfoList().size();
            walkTree(idx);
        }
    }
}
*/

