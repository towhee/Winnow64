#include "bookmarks.h"

BookMarks::BookMarks(QWidget *parent) : QTreeWidget(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "BookMarks::BookMarks";
    #endif
    }
	setAcceptDrops(true);
	setDragEnabled(false);
	setDragDropMode(QAbstractItemView::DropOnly);

    connect(this, SIGNAL(expanded(const QModelIndex &)), this,
            SLOT(resizeTreeColumn(const QModelIndex &)));
    connect(this, SIGNAL(collapsed(const QModelIndex &)), this,
            SLOT(resizeTreeColumn(const QModelIndex &)));

    setRootIsDecorated(false);
	setColumnCount(1);
	setHeaderHidden(true);
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);
}

void BookMarks::reloadBookmarks()
{
    {
    #ifdef ISDEBUG
    qDebug() << "BookMarks::reloadBookmarks";
    #endif
    }
	clear();
    QSetIterator<QString> it(bookmarkPaths);
	while (it.hasNext()) {
		QString itemPath = it.next();
	    QTreeWidgetItem *item = new QTreeWidgetItem(this);
        item->setText(0, QFileInfo(itemPath).fileName());
        item->setIcon(0, QIcon(":/images/bookmarks.png"));
        item->setToolTip(0, itemPath);
        insertTopLevelItem(0, item);
    }
}

void BookMarks::resizeTreeColumn(const QModelIndex &)
{
    {
    #ifdef ISDEBUG
    qDebug() << "BookMarks::resizeTreeColumn";
    #endif
    }
	resizeColumnToContents(0);
}

void BookMarks::removeBookmark()
{
    {
    #ifdef ISDEBUG
    qDebug() << "BookMarks::removeBookmark";
    #endif
    }
	if (selectedItems().size() == 1) {
        bookmarkPaths.remove(selectedItems().at(0)->toolTip(0));
		reloadBookmarks();
	}
}

void BookMarks::dragEnterEvent(QDragEnterEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "BookMarks::dragEnterEvent";
    #endif
    }
	QModelIndexList selectedDirs = selectionModel()->selectedRows();

	if (selectedDirs.size() > 0) {
		dndOrigSelection = selectedDirs[0];
	}
	event->acceptProposedAction();
}

void BookMarks::dragMoveEvent(QDragMoveEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "Bookmarks::dragMoveEvent";
    #endif
    }
    setCurrentIndex(indexAt(event->pos()));
}

void BookMarks::dropEvent(QDropEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "BookMarks::dropEvent";
    #endif
    }
	if (event->source()) {
		QString fstreeStr("FSTree");
		bool dirOp = (event->source()->metaObject()->className() == fstreeStr);
        emit dropOp(event->keyboardModifiers(), dirOp,
                    event->mimeData()->urls().at(0).toLocalFile());
	}
}

