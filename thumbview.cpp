#include "thumbview.h"

/*  ThumbView Overview

ThumbView manages the list of images within a folder and it's children
(optional). The thumbView and either be a list of file names or thumbnails.
When a list item is selected the image is shown in imageView.

The thumbView is inside a QDockWidget which allows it to dock and be moved and
resized.

ThumbView does the following:

    Manages the file list of eligible images, including attributes for
    selected and picked. Picked files are shown in green and can be filtered
    and copied to another folder via the copyPickDlg class.

    The thumbViewDelegate class formats the look of the thumbnails.

    Sorts the list based on date acquired, filename and forward/reverse

    Provides functions to navigate the list, including current index, next item,
    previous item, first item, last item and random item.

    Changes in selection trigger the MW::fileSelectionChange which loads the new
    selection in imageView and updates status.

    The mouse click location within the thumb is used in ImageView to pan a
    zoomed image.

QStandardItemModel roles used:

    1   DecorationRole - holds the thumb as an icon
    3   ToolTipRole - the file path
    8   BackgroundRole - the color of the background
   13   SizeHintRole - the QSize of the icon

   User defined roles include:

        LoadedRole - is loaded bool
        FileNameRole - file path qstring
        SortRole - int
        PickRole - bool is picked

Note that a "row" in this class refers to the row in the model, which has one
thumb per row, not the view in the dock, where there can be many thumbs per row

  */

ThumbView::ThumbView(QWidget *parent, Metadata *metadata, bool iconDisplay) : QListView(parent)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::ThumbView";
    #endif
    }
    mw = parent;
    this->metadata = metadata;

    isIconDisplay = iconDisplay;
//    thumbHeight = G::thumbHeight;
    pickFilter = false;
    if (isIconDisplay) setViewMode(QListView::IconMode);
    else setViewMode(QListView::ListMode);

    setSelectionMode(QAbstractItemView::ExtendedSelection);
    if (isIconDisplay) setResizeMode(QListView::Adjust);
    this->setLayoutMode(QListView::Batched);
//    setBatchSize(2);
    setWordWrap(true);
    setDragEnabled(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformItemSizes(false);
    this->setContentsMargins(0,0,0,0);

    thumbViewModel = new QStandardItemModel(this);
    thumbViewModel->setSortRole(SortRole);
    thumbViewFilter = new QSortFilterProxyModel;
    thumbViewFilter->setSourceModel(thumbViewModel);
    thumbViewFilter->setFilterRole(PickedRole);
//    thumbViewFilter->setSortRole(SortRole);     // rgh 2017-05-31
    setModel(thumbViewFilter);

    thumbViewSelection = selectionModel();

    thumbViewDelegate = new ThumbViewDelegate(this);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbPadding, labelFontSize, showThumbLabels);
    setItemDelegate(thumbViewDelegate);

    connect(this->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            parent, SLOT(fileSelectionChange()));

//    connect(this, SIGNAL(activated(QModelIndex)),
//            this, SLOT(activate(QModelIndex)));

//    connect(this, SIGNAL(doubleClicked(const QModelIndex &)), parent,
//            SLOT(loadImagefromThumb(const QModelIndex &)));

    // used to provide iconRect info to zoom to point clicked on thumb
    // in imageView
    connect(thumbViewDelegate, SIGNAL(update(QModelIndex, QRect)),
            this, SLOT(updateThumbRectRole(QModelIndex, QRect)));

    thumbsDir = new QDir();
    fileFilters = new QStringList;

    if (isIconDisplay) emptyImg.load(":/images/no_image.png");

    QTime time = QTime::currentTime();
}

bool ThumbView::event(QEvent *event)
{
/* Just in case we need to override a keystroke in the thumbview */
    bool override = false;
//    qDebug() << "treeView events:" << event;
    if (event->type() == QEvent::UpdateLater ||
        event->type() == QEvent::Paint) {
//        forceScroll(0);
    }
    if (event->type() == QEvent::Resize) {
//        forceScroll(40);
    }
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
//        if (keyEvent->key() == Qt::Key_Right) {
//            override = true;
//            selectNext();
//        }
//        if (keyEvent->key() == Qt::Key_Left) {
//            override = true;
//            selectPrev();
//        }
    }
    if (!override) return QListView::event(event);
}

void ThumbView::reportThumbs()
{
//    QModelIndex idx;
//    qDebug() << "List all thumbs";
//    for (int i=0; i<thumbViewFilter->rowCount(); i++) {
//        idx = thumbViewFilter->index(i, 0, QModelIndex());
//        qDebug() << i << idx.data(FileNameRole).toString();
//    }
}

void ThumbView::refreshThumbs() {
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::refreshThumbs";
    #endif
    }
    // if workspace invoked with diff thumb parameters
//    setThumbParameters();
    this->dataChanged(thumbViewFilter->index(0, 0, QModelIndex()),
      thumbViewFilter->index(getLastRow(), 0, QModelIndex()));
}

void ThumbView::setThumbParameters(
        int thumbWidth, int thumbHeight,
        int thumbSpacing,int thumbPadding, int labelFontSize,
        bool showThumbLabels)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::setThumbSize";
    #endif
    }
    // rgh are thumbWidth/Height needed in thumbView since thumbViewDelegate
    // used G::thumbWidth ...
//    thumbWidth = G::thumbWidth;
//    thumbHeight = G::thumbHeight;
    this->setSpacing(thumbSpacing);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbPadding, labelFontSize, showThumbLabels);
//    qDebug() << "Changed thumbs to" << thumbWidth << "x" << thumbHeight;
}

void ThumbView::setThumbParameters()
/*
Helper function for in class calls where thumb parameters already defined
*/
{
    this->setSpacing(thumbSpacing);
    thumbViewDelegate->setThumbDimensions(thumbWidth, thumbHeight,
        thumbPadding, labelFontSize, showThumbLabels);
}

// debugging
void ThumbView::reportThumb()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::reportThumb";
    #endif
    }
    int currThumb = currentIndex().row();
    qDebug() << "\n ***** THUMB INFO *****";
    qDebug() << "LoadedRole " << LoadedRole << thumbViewModel->item(currThumb)->data(LoadedRole).toBool();
    qDebug() << "FileNameRole " << FileNameRole << thumbViewModel->item(currThumb)->data(FileNameRole).toString();
    qDebug() << "SortRole " << SortRole << thumbViewModel->item(currThumb)->data(SortRole).toInt();
//    qDebug() << thumbViewModel->item(currThumb)->data(DisplayRole).toString();
    qDebug() << "\nAll roles:";
    for (int i=0; i<15; ++i) {
        qDebug() << i << ":  " << thumbViewModel->item(currThumb)->data(i);
    }
}

int ThumbView::getCurrentRow()
{
    return currentIndex().row();
}

int ThumbView::getNextRow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getNextRow";
    #endif
    }
    int row = currentIndex().row();
    if (row == thumbViewFilter->rowCount() - 1)
        return row;
    return row + 1;
}

int ThumbView::getPrevRow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getPrevRow";
    #endif
    }
    int row = currentIndex().row();
    if (row == 0)
        return 0;
    return row - 1;
}

int ThumbView::getLastRow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getLastRow";
    #endif
    }
    return thumbViewFilter->rowCount() - 1;
}

int ThumbView::getRandomRow()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getRandomRow";
    #endif
    }
    return qrand() % (thumbViewFilter->rowCount());
}

bool ThumbView::isSelectedItem()
{
    // call before getting current row or index
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::isSelectedItem";
    #endif
    }
    if (selectionModel()->selectedIndexes().size() == 1)
        return true;
    else
        return false;
}

// not used but might be useful
QString ThumbView::getCurrentFilename()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getSingleSelectionFilename";
    #endif
    }
    return currentIndex().data(FileNameRole).toString();
}

// PICKS: Items that have been picked

// used in MW::copyPicks
bool ThumbView::isPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::isPick";
    #endif
    }
    int pickCount = 0;
    for (int row = 0; row < thumbViewFilter->rowCount(); ++row) {
        QModelIndex idx = thumbViewFilter->index(row, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") ++pickCount;
    }
    return (pickCount > 0);
}

QFileInfoList ThumbView::getPicks()
{
/* Returns a list of all the files that have been picked.  It is used in
MW, passing the list on to the copyPicksDlg for ingestion/copying to another
folder.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getPicks";
    #endif
    }
    QFileInfoList fileInfoList;
    for (int row = 0; row < thumbViewFilter->rowCount(); ++row) {
        QModelIndex idx = thumbViewFilter->index(row, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") {
            QFileInfo fileInfo(idx.data(FileNameRole).toString());
            fileInfoList.append(fileInfo);
        }
    }
    return fileInfoList;
}

int ThumbView::getNextPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getNextPick";
    #endif
    }
    int frwd = currentIndex().row() + 1;
    int rowCount = thumbViewFilter->rowCount();
    QModelIndex idx;
    while (frwd < rowCount) {
        idx = thumbViewFilter->index(frwd, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") return frwd;
        ++frwd;
    }
    return -1;
}

int ThumbView::getPrevPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getNextPick";
    #endif
    }
    int back = currentIndex().row() - 1;
    int rowCount = thumbViewFilter->rowCount();
    QModelIndex idx;
    while (back >= 0) {
        idx = thumbViewFilter->index(back, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") return back;
        --back;
    }
    return -1;
}

int ThumbView::getNearestPick()
{
/* Returns the model row of the nearest pick, used in toggleFilterPick */
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getNearestPick";
    #endif
    }
    int frwd = currentIndex().row();
    int back = frwd;
    int rowCount = thumbViewFilter->rowCount();
    QModelIndex idx;
    while (back >=0 || frwd < rowCount) {
        if (back >=0) idx = thumbViewFilter->index(back, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") return back;
        if (frwd < rowCount) idx = thumbViewFilter->index(frwd, 0, QModelIndex());
        if (idx.data(PickedRole).toString() == "true") return frwd;
        --back;
        ++frwd;
    }
    return 0;
}

void ThumbView::toggleFilterPick(bool isFilter)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::toggleFilterPick";
    #endif
    }
    pickFilter = isFilter;
    if (pickFilter) {
        int row = getNearestPick();
        selectThumb(row);
        thumbViewFilter->setFilterRegExp("true");   // show only picked items
    }
    else
        thumbViewFilter->setFilterRegExp("");       // no filter - show all
}

QStringList ThumbView::getSelectedThumbsList()
{
/* This was used by the eliminated tags class and is not used but looks
useful.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::getSelectedThumbsList";
    #endif
    }
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    QStringList SelectedThumbsPaths;

    for (int tn = indexesList.size() - 1; tn >= 0 ; --tn) {
        SelectedThumbsPaths << indexesList[tn].data(FileNameRole).toString();
    }
    return SelectedThumbsPaths;
}

bool ThumbView::load(QString &dir, bool includeSubfolders)
{
/* When a new folder is selected load it into the thumbView.  This clears the
model and populates the QListView with all the cached thumbnail pixmaps from
thumbCache.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::load";
    #endif
    }
    currentViewDir = dir;
    if (!pickFilter) {
        loadPrepare();
        // exit if no images, otherwise get thumb info
        if (!initThumbs() && !includeSubfolders) return false;
    }

    // exit here if just display file list instead of icons
    if (!isIconDisplay) return true;

//    setWrapping(true);
    if (includeSubfolders) {
        qDebug() << "ThumbView::load including subfolders";
        QDirIterator iterator(currentViewDir, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            iterator.next();
            if (iterator.fileInfo().isDir() && iterator.fileName() != "." && iterator.fileName() != "..") {
                thumbsDir->setPath(iterator.filePath());
                qDebug() << "ITERATING FOLDER" << iterator.filePath();
                initThumbs();
            }
        }
    }
    if (thumbFileInfoList.size() && selectionModel()->selectedIndexes().size() == 0) {
        selectThumb(0);
        return true;
    }
    return false;   // no images found in any folder
}

void ThumbView::loadPrepare()
{

/*  Prepares the thumbview for new data
      * thumb layout
      * thumb size
      * file filter
      * sorting
      * clear model and run parameters      */

    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::loadPrepare";
    #endif
    }
    if (!isIconDisplay) thumbHeight = 0;

    fileFilters->clear();
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters->append("*." + str);
    thumbsDir->setNameFilters(*fileFilters);
    thumbsDir->setFilter(QDir::Files);
//	if (GData::showHiddenFiles) {
//		thumbsDir->setFilter(thumbsDir->filter() | QDir::Hidden);
//	}

    thumbsDir->setPath(currentViewDir);

//    QDir::SortFlags tempThumbsSortFlags = thumbsSortFlags;
//    if (tempThumbsSortFlags & QDir::Size || tempThumbsSortFlags & QDir::Time) {
//        tempThumbsSortFlags ^= QDir::Reversed;
//    }
//    thumbsDir->setSorting(tempThumbsSortFlags);
    thumbsDir->setSorting(QDir::Name);

    thumbViewModel->clear();
    thumbFileInfoList.clear();
}

bool ThumbView::initThumbs()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::initThumbs";
    #endif
    }
    dirFileInfoList = thumbsDir->entryInfoList();
    if (dirFileInfoList.size() == 0) return false;
    static QStandardItem *item;
    static int fileIndex;
    static QPixmap emptyPixMap;

    // rgh not working
    emptyPixMap = QPixmap::fromImage(emptyImg).scaled(thumbWidth, thumbHeight);

    for (fileIndex = 0; fileIndex < dirFileInfoList.size(); ++fileIndex) {
        thumbFileInfo = dirFileInfoList.at(fileIndex);
        thumbFileInfoList.append(thumbFileInfo);
        item = new QStandardItem();
        item->setData(false, LoadedRole);
        item->setData(fileIndex, SortRole);
//        item->setData(thumbFileInfo.created(), SortRole);
        item->setData(thumbFileInfo.filePath(), FileNameRole);
        item->setData(thumbFileInfo.absoluteFilePath(), Qt::ToolTipRole);
        item->setData("False", PickedRole);
        item->setData(QRect(), ThumbRectRole);     // define later when read
        item->setData(thumbFileInfo.fileName(), Qt::DisplayRole);
        thumbViewModel->appendRow(item);
    }
    return true;
}

// Used by thumbnail navigation (left, right, up, down etc)
void ThumbView::selectThumb(QModelIndex idx)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectThumb(index)" << idx;
    #endif
    }
    qDebug() << "ThumbView::selectThumb(index)" << idx;
    if (idx.isValid()) {
        setCurrentIndex(idx);
        scrollTo(idx, ScrollHint::PositionAtCenter);
    }
}

void ThumbView::selectThumb(int row)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectThumb(row)" << row;
    #endif
    }
    // some operations assign row = -1 if not found
    if (row < 0) return;
    setFocus();
//    qDebug() << "ThumbView::selectThumb(row)" << row;
    QModelIndex idx = thumbViewFilter->index(row, 0, QModelIndex());
    setCurrentIndex(idx);
//    forceScroll(10);
    scrollTo(idx, ScrollHint::PositionAtCenter);
//    if (idx.isValid()) scrollTo(idx, ScrollHint::PositionAtCenter);
}

void ThumbView::selectThumb(QString &fName)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectThumb(filename)";
    #endif
    }
    QModelIndexList idxList =
        thumbViewModel->match(thumbViewModel->index(0, 0), FileNameRole, fName);
    QModelIndex idx = idxList[0];
    QItemSelection selection(idx, idx);
    thumbViewSelection->select(selection, QItemSelectionModel::Select);
    if (idx.isValid()) scrollTo(idx, ScrollHint::PositionAtCenter);
}

void ThumbView::selectNext()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectNext";
    #endif
    }
    if (isSelectedItem()) selectThumb(getNextRow());
}

void ThumbView::selectPrev()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectPrev";
    #endif
    }
    if (isSelectedItem()) selectThumb(getPrevRow());
//    setCurrentIndex(moveCursor(MovePrevious, Qt::NoModifier));
}

void ThumbView::selectUp()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectUp";
    #endif
    }
    setCurrentIndex(moveCursor(QAbstractItemView::MoveUp, Qt::NoModifier));
}

void ThumbView::selectDown()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectDown";
    #endif
    }
    setCurrentIndex(moveCursor(QAbstractItemView::MoveDown, Qt::NoModifier));
}

void ThumbView::selectFirst()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectFirst";
    #endif
    }
    if (isSelectedItem()) selectThumb(0);
}

void ThumbView::selectLast()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectLast";
    #endif
    }
    if (isSelectedItem()) selectThumb(getLastRow());
}

void ThumbView::selectRandom()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectRandom";
    #endif
    }
    if (isSelectedItem()) selectThumb(getRandomRow());
}

void ThumbView::selectNextPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectNextPick";
    #endif
    }
    if (isSelectedItem()) selectThumb(getNextPick());
}

void ThumbView::selectPrevPick()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::selectPrevPick";
    #endif
    }
    if (isSelectedItem()) selectThumb(getPrevPick());
}

void ThumbView::thumbsEnlarge()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::thumbsEnlarge";
    #endif
    }
    if (thumbWidth == 0) thumbWidth = 40;
    if (thumbHeight ==0) thumbHeight = 40;
    if (thumbWidth < 160 && thumbHeight < 160)
    {
        thumbWidth *= 1.1;
        thumbHeight *= 1.1;
//        thumbsEnlargeAction->setEnabled(true);
        if (thumbWidth > 160) thumbWidth = 160;
        if (thumbHeight > 160) thumbHeight = 160;
        refreshThumbs();
    }
}

void ThumbView::thumbsShrink()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::thumbsShrink";
    #endif
    }
    if (thumbWidth > 40  && thumbHeight > 40) {
        thumbWidth *= 0.9;
        thumbHeight *= 0.9;
//        thumbsEnlargeAction->setEnabled(true);
        if (thumbWidth < 40) thumbWidth = 40;
        if (thumbHeight < 40) thumbHeight = 40;
        refreshThumbs();
    }
}

void ThumbView::thumbsFit()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::thumbsFit";
    #endif
    }
//    int imthumbWidth = imageView->getImagethumbWidth();
//    int imthumbHeight = imageView->getImagethumbHeight();
//    float aspect = (float)imthumbHeight/imthumbWidth;
//    if (thumbHeight < thumbWidth)
//        thumbHeight = thumbWidth * aspect;
//    else
//        thumbWidth = (float)thumbHeight / aspect;
//    if (thumbWidth > 160) {
//        float adj = 160 / thumbWidth;
//        thumbWidth = 160;
//        thumbHeight *= adj;
//    }
//    if (thumbHeight > 160) {
//        float adj = 160 / thumbHeight;
//        thumbHeight = 160;
//        thumbWidth *= adj;
//    }
//    thumbView->refreshThumbs();
}

void ThumbView::forceScroll(int row)
{

    QScrollBar *sb;
    sb = verticalScrollBar();
    int tot = thumbViewFilter->rowCount();
    int sbVal = ((float)row / tot) * sb->maximum();
    QSize tSize = thumbViewDelegate->getThumbSize();
    QSize wSize = this->size();
    int perRow = wSize.width() / tSize.width();
    int rowsReqd = ((float)tot / perRow) + 1;
    int sbMaxCalc = rowsReqd * tSize.height();
//    qDebug() << "sb->minimum" << sb->minimum()
//             << "sb->maximum" << sb->maximum()
//             << "sb->singleStep" << sb->singleStep()
//             << "sb->pageStep" << sb->pageStep()
//             << "sb->value" << sb->value()
//             << "tSize" << tSize
//             << "wSize" << size()
//             << "perRow" << perRow
//             << "rowsReqd" << rowsReqd
//             << "maxCalc" << sbMaxCalc
//             << "sbVal" << sbVal;
//    sb->setValue(sbVal);
//    selectThumb(row);
}


void ThumbView::wheelEvent(QWheelEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::wheelEvent";
    #endif
    }
    if (event->delta() < 0)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + thumbHeight);
    else
        verticalScrollBar()->setValue(verticalScrollBar()->value() - thumbHeight);
}

void ThumbView::mousePressEvent(QMouseEvent *event)
{
    /*
    Captures the position of the mouse click within the thumbnail. This is sent
    to imageView, which pans to the same center in zoom view. This is handy
    when the user wants to view a specific part of another image that is in a
    different position than the current image.
    */
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::mousePressEvent";
    #endif
    }
    QListView::mousePressEvent(event);
    QModelIndex idx = currentIndex();
    QRect iconRect = idx.data(ThumbRectRole).toRect();
    QPoint mousePt = event->pos();
    QPoint iconPt = mousePt - iconRect.topLeft();
    float xPct = (float)iconPt.x() / iconRect.width();
    float yPct = (float)iconPt.y() / iconRect.height();
/*    qDebug() << "ThumbView::mousePressEvent"
             << "xPct =" << xPct
             << "yPct =" << yPct
             << "iconRect =" << iconRect
             << "idx =" << idx;
             */
    if (xPct >= 0 && xPct <= 1 && yPct >= 0 && yPct <=1) {
        thumbClick(xPct, yPct);    //signal used in ThumbView::mousePressEvent
    }
//    reportThumb();
}

void ThumbView::mouseDoubleClickEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::mouseDoubleClickEvent";
    #endif
    }
    emit displayLoupe();
    // delay reqd
    QTimer::singleShot(100, this, SLOT(delaySelectCurrentThumb()));
}

void ThumbView::delaySelectCurrentThumb()
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::delaySelectCurrentThumb";
    #endif
    }
//    QKeyEvent *key = new QKeyEvent (QEvent::KeyRelease, Qt::Key_E, Qt::NoModifier, "E");
//    qApp->postEvent(mw, (QEvent *)key);
    selectThumb(currentIndex());
}

void ThumbView::updateThumbRectRole(const QModelIndex index, QRect iconRect)
{
/* thumbViewDelegate triggers this to provide rect data to calc thumb mouse
click position that is then sent to imageView to zoom to the same spot
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::updateThumbRectRole";
    #endif
    }
    thumbViewFilter->setData(index, iconRect, ThumbRectRole);
}

// called from MW invertSelectionAct
void ThumbView::invertSelection()
{
/* inverts/toggles which thumbs are selected.  Called from
 * MW::invertSelectionAct
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::invertSelection";
    #endif
    }
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::invertSelection";
    #endif
    }
    QItemSelection toggleSelection;
    QModelIndex firstIndex = thumbViewFilter->index(0, 0);
    QModelIndex lastIndex = thumbViewFilter->index(thumbViewFilter->rowCount() - 1, 0);
    toggleSelection.select(firstIndex, lastIndex);
    selectionModel()->select(toggleSelection, QItemSelectionModel::Toggle);
}

void ThumbView::copyThumbs()
{
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    if (indexesList.isEmpty()) {
        return;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;
    for (QModelIndexList::const_iterator it = indexesList.constBegin(),
         end = indexesList.constEnd();
         it != end; ++it)
    {
        urls << QUrl(it->data(FileNameRole).toString());
    }
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);
}

void ThumbView::startDrag(Qt::DropActions)
{
/* Drag and drop thumbs to another program.
 */
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbView::startDrag";
    #endif
    }
    QModelIndexList indexesList = selectionModel()->selectedIndexes();
    if (indexesList.isEmpty()) {
        return;
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;
    for (QModelIndexList::const_iterator it = indexesList.constBegin(),
         end = indexesList.constEnd();
         it != end; ++it)
    {
        urls << QUrl(it->data(FileNameRole).toString());
    }
    mimeData->setUrls(urls);
    drag->setMimeData(mimeData);
    QPixmap pix;
    if (indexesList.count() > 1) {
        pix = QPixmap(128, 112);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::white, 2));
        int x = 0, y = 0, xMax = 0, yMax = 0;
        for (int i = 0; i < qMin(5, indexesList.count()); ++i) {
            QPixmap pix = thumbViewModel->item(indexesList.at(i).row())->icon().pixmap(72);
            if (i == 4) {
                x = (xMax - pix.width()) / 2;
                y = (yMax - pix.height()) / 2;
            }
            painter.drawPixmap(x, y, pix);
            xMax = qMax(xMax, qMin(128, x + pix.width()));
            yMax = qMax(yMax, qMin(112, y + pix.height()));
            painter.drawRect(x + 1, y + 1, qMin(126, pix.width() - 2), qMin(110, pix.height() - 2));
            x = !(x == y) * 56;
            y = !y * 40;
        }
        painter.end();
        pix = pix.copy(0, 0, xMax, yMax);
        drag->setPixmap(pix);
    } else {
        pix = thumbViewModel->item(indexesList.at(0).row())->icon().pixmap(128);
        drag->setPixmap(pix);
    }
    drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
    drag->exec(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, Qt::IgnoreAction);
}
