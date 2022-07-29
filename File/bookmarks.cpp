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
        int height = qRound(G::fontSize.toInt() * 1.7 * G::ptToPx);
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

BookMarks::BookMarks(QWidget *parent, Metadata *metadata, bool showImageCount,
                     bool &combineRawJpg)
                   : QTreeWidget(parent),
                     combineRawJpg(combineRawJpg)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setRootIsDecorated(false);
    setColumnCount(2);
    setHeaderHidden(true);
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);

    setItemDelegate(new BookDelegate(this));
}

void BookMarks::reloadBookmarks()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
	clear();
    QSetIterator<QString> it(bookmarkPaths);
	while (it.hasNext()) {
		QString itemPath = it.next();
        addBookmark(itemPath);
    }
    count();
}

void BookMarks::addBookmark(QString itemPath)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(this);
    item->setText(0, QFileInfo(itemPath).fileName());
    item->setIcon(0, QIcon(":/images/bookmarks.png"));
    item->setToolTip(0, itemPath);
    insertTopLevelItem(0, item);
    dir->setPath(itemPath);
    item->setToolTip(1, itemPath);
    item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
}

void BookMarks::count()
{
     QTreeWidgetItemIterator it(this);
     while (*it) {
         QString path = (*it)->toolTip(0);
         int count = 0;
         dir->setPath(path);
         if (combineRawJpg) {
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
                 count++;
             }
         }
         else count = dir->entryInfoList().size();
         (*it)->setText(1, QString::number(count));
         ++it;
     }
}

void BookMarks::select(QString fPath)
{
/*
    This is called from MW::folderSelectionChange to attempt to sync bookmarks with
    the FSTree folders view.
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (bookmarkPaths.contains(fPath)) {
        QList <QTreeWidgetItem *> items;
        items = findItems(QFileInfo(fPath).fileName(), Qt::MatchExactly);
        if (items.length() > 0) {
            setCurrentItem(items[0]);
            setCurrentIndex(selectedIndexes().at(0));
//            count();  // big slowdown
        }

    }
    else {
        selectionModel()->clear();
    }
}

void BookMarks::resizeColumns()
{
    if (showImageCount) {
        QFont font = this->font();
        font.setPointSize(G::fontSize.toInt());
        QFontMetrics fm(font);
        imageCountColumnWidth = fm.boundingRect("99999").width();
        showColumn(1);
        setColumnWidth(1, imageCountColumnWidth);
    }
    else {
        imageCountColumnWidth = 0;
        hideColumn(1);
    }
    // have to include the width of the decoration foler png
    setColumnWidth(0, width() - G::scrollBarThickness - imageCountColumnWidth - 15);
}

void BookMarks::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    resizeColumns();
    QTreeWidget::resizeEvent(event);
}

void BookMarks::mouseDoubleClickEvent(QMouseEvent *)
{
    // ignore double mouse clicks (edit/expand)
    return;
}

void BookMarks::mousePressEvent(QMouseEvent *event)
{
    // rapid mouse press ignore if still running MW::stopAndClearAll
    if (G::stop) {
        qDebug() << CLASSFUNCTION << "G::stop =" << G::stop;
        qApp->beep();
        G::popUp->showPopup("Busy, try new bookmark in a sec.", 1000);
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
        G::popUp->showPopup(msg, 5000);
        return;
    }

    // ignore right mouse clicks (context menu)
    if (event->button() == Qt::RightButton) {
        rightClickItem = itemAt(event->pos());
        if (rightClickItem) {
            rightMouseClickPath = rightClickItem->toolTip(0);
        }
        return;
    }
    qDebug() << CLASSFUNCTION << "G::stop =" << G::stop;
    QTreeWidget::mousePressEvent(event);
}

void BookMarks::removeBookmark()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (rightClickItem) {
//        qDebug() << CLASSFUNCTION<< rightClickItem->toolTip(0);
        bookmarkPaths.remove(rightClickItem->toolTip(0));
        reloadBookmarks();
    }

//	if (selectedItems().size() == 1) {
//        bookmarkPaths.remove(selectedItems().at(0)->toolTip(0));
//		reloadBookmarks();
//	}
}

void BookMarks::contextMenuEvent(QContextMenuEvent *event)
{

}

void BookMarks::dragEnterEvent(QDragEnterEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
	QModelIndexList selectedDirs = selectionModel()->selectedRows();

	if (selectedDirs.size() > 0) {
		dndOrigSelection = selectedDirs[0];
	}
	event->acceptProposedAction();
}

void BookMarks::dragMoveEvent(QDragMoveEvent *event)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    setCurrentIndex(indexAt(event->pos()));
}

void BookMarks::dropEvent(QDropEvent *event)
{
/*
    - add folder as a bookmark
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    /*
    if (event->source()) {
        qDebug() << CLASSFUNCTION << "event->source()";
        QString fstreeStr("FSTree");
        bool dirOp = (event->source()->metaObject()->className() == fstreeStr);
        emit dropOp(event->keyboardModifiers(), dirOp,
                    event->mimeData()->urls().at(0).toLocalFile());
    }
    //*/
    const QMimeData *mimeData = event->mimeData();
    qDebug() << "BookMarks::dropEvent"
             << event
             << mimeData->hasUrls() << mimeData->urls();
    if (mimeData->hasUrls())
    {
        QString dPath;      // path to folder
        QFileInfo fInfo = QFileInfo(mimeData->urls().at(0).toLocalFile());
        if (fInfo.isDir()) dPath = fInfo.absoluteFilePath();
        else dPath = fInfo.absoluteDir().absolutePath();
        if (dPath.length() == 0) return;
        // trim ending "/"
        int endPos = dPath.length() - 1;
        if (dPath[endPos] == '/') dPath.chop(1);
        if (!bookmarkPaths.contains(dPath)) {
            bookmarkPaths.insert(dPath);
            reloadBookmarks();
        }
        // no popup as focus will be on drag app
    }
}
