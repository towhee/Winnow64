
#include "fstree.h"

bool FSModel::hasChildren(const QModelIndex &parent) const
{
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

FSTree::FSTree(QWidget *parent) : QTreeView(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "FSTree::FSTree";
    #endif
    }
	setAcceptDrops(true);
	setDragEnabled(true);
	setDragDropMode(QAbstractItemView::InternalMove);

	fsModel = new FSModel();
//    fsModel->setRootPath(fileSystemModel.myComputer().toString());
    fsModel->setRootPath("/Volumes");

	setModelFlags();
	setModel(fsModel);

    QModelIndex rootIdx = fsModel->index(0, 0, QModelIndex().parent());
    setRootIndex(rootIdx);

    for (int i = 1; i <= 3; ++i) {
        hideColumn(i);
    }
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
	if (G::showHiddenFiles)
		fsModel->setFilter(fsModel->filter() | QDir::Hidden);
}

