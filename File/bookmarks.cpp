#include "File/bookmarks.h"

BookMarks::BookMarks(QWidget *parent, Metadata *metadata, bool showImageCount)
    : QTreeWidget(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "BookMarks::BookMarks";
    #endif
    }
    this->metadata = metadata;
    this->showImageCount = true;// showImageCount;

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
        dir->setPath(itemPath);
        int count = dir->entryInfoList().size();
        item->setText(1, QString::number(count));
        item->setTextAlignment(1, Qt::AlignRight);
    }
}

void BookMarks::select(QString fPath)
{
//    QString folderName = QFileInfo(fPath).fileName();
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

