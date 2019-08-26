#include "File/bookmarks.h"

class BookDelegate : public QStyledItemDelegate
{
public:
    explicit BookDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &index) const
    {
        static int count = 0;
        count++;
        index.isValid();          // suppress compiler warning
        int height = qRound(G::fontSize.toInt() * 1.5);
        return QSize(option.rect.width(), height);
    }
};

/* A QStringList of paths to bookmarked folders is displayed as top level items
in a QWidgetTree in column 0.  Column 1 holds a count of the readable image files
in the folder.

When the user mouse clicks on one of the folders the itemClicked signal is sent
to the MW slot bookmarkClicked along with the tooltip, which holds the path
string.  Since the user could also click in the count column, it's tooltip is
also the path string.

Bookmarks can be added via the context menu in FSTree or the folders can be
dragged from the FSTree to Bookmarks.  Folders or files can also be dragged
from the windows explorer or mac finder to be added as a bookmark.

As an abbreviation in the program UI bookmarks are called favs.
*/

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

    setItemDelegate(new BookDelegate(this));
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
        addBookmark(itemPath);
    }
}

void BookMarks::addBookmark(QString itemPath)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(this);
    item->setText(0, QFileInfo(itemPath).fileName());
    item->setIcon(0, QIcon(":/images/bookmarks.png"));
    item->setToolTip(0, itemPath);
    insertTopLevelItem(0, item);
    dir->setPath(itemPath);
    int count = dir->entryInfoList().size();
    item->setText(1, QString::number(count));
    item->setToolTip(1, itemPath);
//    QFont font = item->font(0);
//    font.setPixelSize(G::fontSize.toInt());
//    item->setFont(0, font);
//    item->setFont(1, font);
    item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
}

void BookMarks::count()
{
     QTreeWidgetItemIterator it(this);
     while (*it) {
         QString path = (*it)->toolTip(0);
         dir->setPath(path);
         int count = dir->entryInfoList().size();
         (*it)->setText(1, QString::number(count));
         qDebug() << path << count;
         ++it;
     }

}

void BookMarks::select(QString fPath)
{
/* This is called from MW::folderSelectionChange to attempt to sync bookmarks with
the FSTree folders view.
*/
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
    else {
        selectionModel()->clear();
    }
}

void BookMarks::resizeColumns()
{
    int w = width();
    if (showImageCount) {
//        imageCountColumnWidth = 45;
        QFont font = this->font();
        font.setPixelSize(G::fontSize.toInt());
        QFontMetrics fm(font);
        imageCountColumnWidth = fm.boundingRect("99999").width();
        showColumn(1);
        setColumnWidth(1, imageCountColumnWidth);
    }
    else {
        imageCountColumnWidth = 0;
        hideColumn(1);
    }
    setColumnWidth(0, w - G::scrollBarThickness - imageCountColumnWidth -10);
//    setColumnWidth(0, width() - G::scrollBarThickness - imageCountColumnWidth);
}

void BookMarks::resizeEvent(QResizeEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    resizeColumns();
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

void BookMarks::paintEvent(QPaintEvent *event)
{
    resizeColumns();
    QTreeWidget::paintEvent(event);
}

void BookMarks::mousePressEvent(QMouseEvent *event)
{
    QTreeView::mousePressEvent(event);
}

void BookMarks::mouseReleaseEvent(QMouseEvent *event)
{
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
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        QString fPath;      // path to folder
        QFileInfo fInfo = QFileInfo(mimeData->urls().at(0).toLocalFile());
        if (fInfo.isDir()) fPath = fInfo.absoluteFilePath();
        else fPath = fInfo.absoluteDir().absolutePath();
        addBookmark(fPath);
    }
}

