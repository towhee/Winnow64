#include "File/bookmarks.h"

BookMarks::BookMarks(QWidget *parent, Metadata *metadata, bool showImageCount)
    : QTreeWidget(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->metadata = metadata;
    this->showImageCount = showImageCount;
    viewport()->setObjectName("bookmarksViewPort");
    setObjectName("bookmarks");

    fileFilters = new QStringList;
    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    dir = new QDir();
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    setAcceptDrops(true);
	setDragEnabled(false);
	setDragDropMode(QAbstractItemView::DropOnly);

    setRootIsDecorated(false);
    setColumnCount(2);
    setHeaderHidden(true);
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);
}

void BookMarks::reloadBookmarks()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
        dir->setPath(itemPath);
        int count = dir->entryInfoList().size();
        item->setText(1, QString::number(count));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    }
}

void BookMarks::select(QString fPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (bookmarkPaths.contains(fPath)) {
        QList <QTreeWidgetItem *> items;
        items = findItems(QFileInfo(fPath).fileName(), Qt::MatchExactly);
        if (items.length() > 0) {
            setCurrentItem(items[0]);
            setCurrentIndex(selectedIndexes().at(0));
        }
    }
}

void BookMarks::resizeEvent(QResizeEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (showImageCount) {
        imageCountColumnWidth = 45;
        showColumn(1);
        setColumnWidth(1, imageCountColumnWidth);
    }
    else {
        imageCountColumnWidth = 0;
        hideColumn(1);
    }
    setColumnWidth(0, width() - G::scrollBarThickness - imageCountColumnWidth);
    QTreeWidget::resizeEvent(event);
}

void BookMarks::removeBookmark()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
	if (selectedItems().size() == 1) {
        bookmarkPaths.remove(selectedItems().at(0)->toolTip(0));
		reloadBookmarks();
	}
}

void BookMarks::mousePressEvent(QMouseEvent *event)
{
    qDebug() << QTime::currentTime() << "MouseButtonPress" << __FUNCTION__;
    QTreeView::mousePressEvent(event);
}

void BookMarks::mouseReleaseEvent(QMouseEvent *event)
{
    qDebug() << QTime::currentTime() << "MouseButtonRelease" << __FUNCTION__ << "\n";
    QTreeView::mouseReleaseEvent(event);
}

void BookMarks::dragEnterEvent(QDragEnterEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
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
    G::track(__FUNCTION__);
    #endif
    }
    setCurrentIndex(indexAt(event->pos()));
}

void BookMarks::dropEvent(QDropEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
	if (event->source()) {
		QString fstreeStr("FSTree");
		bool dirOp = (event->source()->metaObject()->className() == fstreeStr);
        emit dropOp(event->keyboardModifiers(), dirOp,
                    event->mimeData()->urls().at(0).toLocalFile());
	}
}

