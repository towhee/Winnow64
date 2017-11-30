
#include "fstree.h"

/*------------------------------------------------------------------------------
CLASS FSModel subclassing QFileSystemModel
------------------------------------------------------------------------------*/
FSModel::FSModel(QWidget *parent, Metadata *metadata, bool showImageCount) : QFileSystemModel(parent)
{
    QStringList *fileFilters = new QStringList;
    dir = new QDir();

    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);
}

bool FSModel::hasChildren(const QModelIndex &parent) const
{
    {
    #ifdef ISDEBUG
//    qDebug() << "FSTree::hasChildren";
    #endif
    }
    if (parent.column() > 0)
		return false;

	if (!parent.isValid()) // drives
		return true;

	// return false if item cant have children
	if (parent.flags() &  Qt::ItemNeverHasChildren) {
		return false;
	}

	// return if at least one child exists
	return QDirIterator(filePath(parent), filter() | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags).hasNext();
}

int FSModel::columnCount(const QModelIndex &parent) const
{
    return QFileSystemModel::columnCount(parent) + 1;
}

QVariant FSModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && section == imageCountColumn)
    {
        if(role == Qt::DisplayRole)
            return QVariant("Images");
        return QVariant();
     }
     else
        return QFileSystemModel::headerData(section, orientation, role);
}

Qt::ItemFlags FSModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

QVariant FSModel::data(const QModelIndex & index, int role) const
{
    if (index.column() == imageCountColumn) {
        if (role == Qt::DisplayRole) {
            return QVariant();
//            QString fPath = qvariant_cast<QString>(QFileSystemModel::data(index, QFileSystemModel::FilePathRole));
//            dir->setPath(fPath);
//            int count = dir->entryInfoList().size();
//            QString imageCount = "";
//            if (count > 0)
//              imageCount = QString::number(count, 'f', 0);
//            return imageCount;
        }
        if (role == Qt::TextAlignmentRole)
            return static_cast<QVariant>(Qt::AlignRight | Qt::AlignVCenter);
        else
            return QVariant();
    }
    else
        return QFileSystemModel::data(index, role);
}

/*------------------------------------------------------------------------------
CLASS FSTree subclassing QTreeView
------------------------------------------------------------------------------*/

FSTree::FSTree(QWidget *parent, Metadata *metadata, bool showImageCount) : QTreeView(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "FSTree::FSTree";
    #endif
    }
	setAcceptDrops(true);
	setDragEnabled(true);
	setDragDropMode(QAbstractItemView::InternalMove);

    fsModel = new FSModel(this, metadata, showImageCount);
    this->metadata = metadata;

    fileFilters = new QStringList;
    dir = new QDir();

#ifdef Q_OS_LINIX
    fsModel->setRootPath("/Volumes");
#endif
#ifdef Q_OS_WIN
    fsModel->setRootPath(fileSystemModel.myComputer().toString());
#endif
#ifdef Q_OS_MAC
    fsModel->setRootPath("/Volumes");
#endif

	setModelFlags();
	setModel(fsModel);

    QModelIndex rootIdx = fsModel->index(0, 0, QModelIndex().parent());
    setRootIndex(rootIdx);

    for (int i = 1; i <= 3; ++i) {
        hideColumn(i);
    }

    setColumnWidth(4, 30);

    setHeaderHidden(true);
    setIndentation(12);

    connect(this, SIGNAL(expanded(const QModelIndex &)), this,
            SLOT(resizeTreeColumn(const QModelIndex &)));
    connect(this, SIGNAL(collapsed(const QModelIndex &)), this,
            SLOT(resizeTreeColumn(const QModelIndex &)));
}

QModelIndex FSTree::getCurrentIndex()
{
    {
    #ifdef ISDEBUG
    qDebug() << "FSTree::getCurrentIndex";
    #endif
    }
	return selectedIndexes().first();
}

void FSTree::resizeTreeColumn(const QModelIndex &)
{
    {
    #ifdef ISDEBUG
    qDebug() << "FSTree::resizeTreeColumn";
    #endif
    }
	resizeColumnToContents(0);
}

void FSTree::dragEnterEvent(QDragEnterEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "FSTree::dragEnterEvent";
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
    qDebug() << "FSTree::dropEvent";
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

void FSTree::setModelFlags()
{
    {
    #ifdef ISDEBUG
    qDebug() << "FSTree::setModelFlags";
    #endif
    }
	fsModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
//	if (G::showHiddenFiles)
//		fsModel->setFilter(fsModel->filter() | QDir::Hidden);
}

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
            qDebug() << fPath << dir->entryInfoList().size();
            walkTree(idx);
        }
    }
}
