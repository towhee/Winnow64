#include "Views/infoview.h"
#include "Main/global.h"

class InfoDelegate : public QStyledItemDelegate
{
public:
    explicit InfoDelegate(QObject *parent = 0) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &index) const
    {
        index.isValid();          // suppress compiler warning
        return QSize(option.rect.width(), 24);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
    {
        painter->save();


        int y1 = option.rect.top();
        int y2 = option.rect.bottom();

        QLinearGradient categoryBackground;
        categoryBackground.setStart(0, y1);
        categoryBackground.setFinalStop(0, y2);
        categoryBackground.setColorAt(0, QColor(88,88,88));
        categoryBackground.setColorAt(1, QColor(66,66,66));

        int offset = 5;
        QPoint topLeft(option.rect.left() + offset, option.rect.top());
        QPoint bottomRight(option.rect.bottomRight());
        QRect textRect(topLeft, bottomRight);

        if (index.parent() == QModelIndex() && index.column() == 0) {
            painter->fillRect(option.rect, categoryBackground);
            QPen catPen(Qt::white);
            painter->setPen(catPen);
        }
        QString text = index.data().toString();

        QString elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, textRect.width());
        painter->drawText(textRect, Qt::AlignVCenter|Qt::TextSingleLine, elidedText);

        painter->setPen(QColor(75,75,75));
        painter->drawRect(option.rect);

        painter->restore();

//        QStyledItemDelegate::paint(painter, option, index);
    }
};


/*
This class shows information in a two column table.

Column 0 = Item Description
Column 1 = Item Value
Column 2 = Flag to show or hide row

It is used to show some file, image and application state information.
*/

InfoView::InfoView(QWidget *parent, Metadata *metadata) : QTreeView(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->metadata = metadata;

    ok = new QStandardItemModel(this);
    setupOk();
    setModel(ok);

    setRootIsDecorated(true);
    setColumnWidth(0, 100);
    setIndentation(0);
    setExpandsOnDoubleClick(true);
    setHeaderHidden(true);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    setFirstColumnSpanned(0, QModelIndex(), true);
    setFirstColumnSpanned(1, QModelIndex(), true);
    setFirstColumnSpanned(2, QModelIndex(), true);
    setFirstColumnSpanned(3, QModelIndex(), true);
    expandAll();
    hideColumn(2);
    setTabKeyNavigation(false);

    setItemDelegate(new InfoDelegate(this));

   // InfoView menu
	infoMenu = new QMenu("");
    copyInfoAction = new QAction(tr("Copy item"), this);

    connect(copyInfoAction, SIGNAL(triggered()), this, SLOT(copyEntry()));
//	infoMenu->addAction(copyAction);
//	setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            SLOT(showInfoViewMenu(QPoint)));

//    connect(ok, SIGNAL(itemChanged(QStandardItem*)),
//            this, SLOT(itemChanged(QStandardItem*)));
}

void InfoView::showInfoViewMenu(QPoint pt)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    selectedEntry = indexAt(pt);
	if (selectedEntry.isValid())
    	infoMenu->popup(viewport()->mapToGlobal(pt));
}

void InfoView::tweakHeaders()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    horizontalHeader()->setFixedHeight(1);
}

void InfoView::setupOk()
{
/*
The datamodel called (ok) holds all metadata items shown in the InfoView
QTableView. It contains three columns:

    ● (0) The name of the information item
    ● (1) The value of the information item
    ● (2) A boolean flag to show or hide a row of the table

The information items are metadata about the file, such as name or path;
information about the image, such as aperture or dimensions; and application
status information, such as number of items picked or current item selected.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    ok->setHorizontalHeaderItem(0, new QStandardItem(QString("Field")));
    ok->setHorizontalHeaderItem(1, new QStandardItem(QString("Value")));
    ok->setHorizontalHeaderItem(2, new QStandardItem(QString("Show")));

    // create all the category rows req'd using global enum categories
    ok->insertRows(0, categoriesEnd);

    ok->setData(ok->index(fileInfoCat, 0), "File:");
    ok->setData(ok->index(imageInfoCat, 0), "Camera:");
    ok->setData(ok->index(tagInfoCat, 0), "Tags:");
    ok->setData(ok->index(statusInfoCat, 0), "Status:");

    fileInfoIdx = ok->index(fileInfoCat, 0, QModelIndex());
    imageInfoIdx = ok->index(imageInfoCat, 0);
    tagInfoIdx = ok->index(tagInfoCat, 0);
    statusInfoIdx = ok->index(statusInfoCat, 0);

    // create the data rows for each category
    ok->insertRows(0, fileInfoRowsEnd, fileInfoIdx);
    ok->insertColumns(0, 3, fileInfoIdx);

    ok->insertRows(0, imageInfoRowsEnd, imageInfoIdx);
    ok->insertColumns(0, imageInfoRowsEnd, imageInfoIdx);

    ok->insertRows(0, tagInfoRowsEnd, tagInfoIdx);
    ok->insertColumns(0, tagInfoRowsEnd, tagInfoIdx);

    ok->insertRows(0, statusInfoRowsEnd, statusInfoIdx);
    ok->insertColumns(0, statusInfoRowsEnd, statusInfoIdx);

    // Set field description
    ok->setData(ok->index(FolderRow, 0, fileInfoIdx), "Folder");
    ok->setData(ok->index(FileNameRow, 0, fileInfoIdx), "File name");
    ok->setData(ok->index(LocationRow, 0, fileInfoIdx), "Location");
    ok->setData(ok->index(SizeRow, 0, fileInfoIdx), "Size");
    ok->setData(ok->index(CreatedRow, 0, fileInfoIdx), "Created");
    ok->setData(ok->index(ModifiedRow, 0, fileInfoIdx), "Modified");
    ok->setData(ok->index(DimensionsRow, 0, fileInfoIdx), "Dimensions");
    ok->setData(ok->index(MegaPixelsRow, 0, fileInfoIdx), "Megapixels");
    ok->setData(ok->index(ModelRow, 0, imageInfoIdx), "Model");
    ok->setData(ok->index(LensRow, 0, imageInfoIdx), "Lens");
    ok->setData(ok->index(ShutterSpeedRow, 0, imageInfoIdx), "Shutter speed");
    ok->setData(ok->index(ApertureRow, 0, imageInfoIdx), "Aperture");
    ok->setData(ok->index(ISORow, 0, imageInfoIdx), "ISO");
    ok->setData(ok->index(FocalLengthRow, 0, imageInfoIdx), "Focal length");
    ok->setData(ok->index(TitleRow, 0, tagInfoIdx), "Title");
    ok->setData(ok->index(CreatorRow, 0, tagInfoIdx), "Creator");
    ok->setData(ok->index(CopyrightRow, 0, tagInfoIdx), "Copyright");
    ok->setData(ok->index(EmailRow, 0, tagInfoIdx), "Email");
    ok->setData(ok->index(UrlRow, 0, tagInfoIdx), "Url");
    ok->setData(ok->index(PositionRow, 0, statusInfoIdx), "Position");
    ok->setData(ok->index(ZoomRow, 0, statusInfoIdx), "Zoom");
    ok->setData(ok->index(PickedRow, 0, statusInfoIdx), "Picked");
    ok->setData(ok->index(MonitorRow, 0, statusInfoIdx), "Monitor");

    // set default to show all rows - overridden in preferences
    // set all items not editable
    for(int row = 0; row < ok->rowCount(); row++) {
        ok->setData(ok->index(row, 2), true);
        ok->itemFromIndex(ok->index(row, 0))->setEditable(false);
        ok->itemFromIndex(ok->index(row, 1))->setEditable(false);
        for (int childRow = 0; childRow < ok->rowCount(ok->index(row, 0)); childRow++) {
            ok->setData(ok->index(childRow, 2, ok->index(row, 0)), true);
            ok->itemFromIndex(ok->index(childRow, 0, ok->index(row, 0)))->setEditable(false);
            ok->itemFromIndex(ok->index(childRow, 1, ok->index(row, 0)))->setEditable(false);
        }
    }

    // set editable fields
    ok->itemFromIndex(ok->index(TitleRow, 1, tagInfoIdx))->setEditable(true);
    ok->itemFromIndex(ok->index(CreatorRow, 1, tagInfoIdx))->setEditable(true);
    ok->itemFromIndex(ok->index(CopyrightRow, 1, tagInfoIdx))->setEditable(true);
    ok->itemFromIndex(ok->index(EmailRow, 1, tagInfoIdx))->setEditable(true);
    ok->itemFromIndex(ok->index(UrlRow, 1, tagInfoIdx))->setEditable(true);

    connect(ok, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
            this, SLOT(showOrHide()));
}

void InfoView::showOrHide()
{
/*
Shows or hides each metadata item in the metadata panel based on the boolean
flag in the datamodel ok.  The show/hide is set in the prefdlg, which is in
sync with ok.

When called, the function iterates through all the metadata items in ok and
looks for the field in ok. It then shows or hides the table row based on the ok
show flag.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << G::t.restart() << "\t" << "ShorOrHide List:";
    bool okToShow;
    for(int row = 0; row < ok->rowCount(); row++) {
        QModelIndex parentIdx = ok->index(row, 0);
        okToShow = ok->index(row, 2).data().toBool();
//        qDebug() << G::t.restart() << "\t" << parentIdx.data().toString() << okToShow;
        setRowHidden(row, QModelIndex(), !okToShow);
        for (int childRow = 0; childRow < ok->rowCount(parentIdx); childRow++) {
            okToShow = ok->index(childRow, 2, parentIdx).data().toBool();
//            qDebug() << G::t.restart() << "\t" << ok->index(childRow, 0, parentIdx).data().toString() << okToShow;
            setRowHidden(childRow, parentIdx, !okToShow);
        }
    }
//    qDebug() << G::t.restart() << "\t" << "ShorOrHide completed\n";
}

void InfoView::clearInfo()
{
/*
Clear all the values but leave the keys and flags alone
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    for(int row = 0; row < ok->rowCount(); row++) {
        QModelIndex parentIdx = ok->index(row, 0);
        for (int childRow = 0; childRow < ok->rowCount(parentIdx); childRow++) {
            ok->setData(ok->index(childRow, 1, parentIdx), "");
        }
    }
}

void InfoView::copyEntry()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
	if (selectedEntry.isValid())
        QApplication::clipboard()->setText(ok->itemFromIndex(selectedEntry)->toolTip());
}

void InfoView::updateInfo(const QString &fPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    // flag updates so itemChanged be ignored in MW::metadataChanged
    isNewImageDataChange = true;

    QFileInfo imageInfo = QFileInfo(fPath);

    // make sure there is metadata for this image
    if (!metadata->isLoaded(fPath)) {
        metadata->loadImageMetadata(fPath, true, true);
    }

    uint width = metadata->getWidth(fPath);
    uint height = metadata->getHeight(fPath);
    QString modified = imageInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");

    // update items
    ok->setData(ok->index(FolderRow, 1, fileInfoIdx), imageInfo.dir().dirName());
    ok->setData(ok->index(FileNameRow, 1, fileInfoIdx), imageInfo.fileName());
    ok->setData(ok->index(LocationRow, 1, fileInfoIdx), imageInfo.path());
    ok->setData(ok->index(SizeRow, 1, fileInfoIdx), QString::number(imageInfo.size() / 1024000.0, 'f', 2) + " MB");
    ok->setData(ok->index(CreatedRow, 1, fileInfoIdx), metadata->getCreatedDate(fPath).toString("yyyy-MM-dd hh:mm:ss"));
    ok->setData(ok->index(ModifiedRow, 1, fileInfoIdx), modified);
    ok->setData(ok->index(DimensionsRow, 1, fileInfoIdx), metadata->getDimensions(fPath));
    ok->setData(ok->index(MegaPixelsRow, 1, fileInfoIdx), QString::number((width * height) / 1000000.0, 'f', 1));
    ok->setData(ok->index(ModelRow, 1, imageInfoIdx), metadata->getModel(fPath));
    ok->setData(ok->index(LensRow, 1, imageInfoIdx), metadata->getLens(fPath));
    ok->setData(ok->index(ShutterSpeedRow, 1, imageInfoIdx), metadata->getExposureTime(fPath));
    ok->setData(ok->index(ApertureRow, 1, imageInfoIdx), metadata->getAperture(fPath));
    ok->setData(ok->index(ISORow, 1, imageInfoIdx), metadata->getISO(fPath));
    ok->setData(ok->index(FocalLengthRow, 1, imageInfoIdx), metadata->getFocalLength(fPath));
    ok->setData(ok->index(TitleRow, 1, tagInfoIdx), metadata->getTitle(fPath));
    ok->setData(ok->index(CreatorRow, 1, tagInfoIdx), metadata->getCreator(fPath));
    ok->setData(ok->index(CopyrightRow, 1, tagInfoIdx), metadata->getCopyright(fPath));
    ok->setData(ok->index(EmailRow, 1, tagInfoIdx), metadata->getEmail(fPath));
    ok->setData(ok->index(UrlRow, 1, tagInfoIdx), metadata->getUrl(fPath));

    this->fPath = fPath;        // not used, convenience value for future use

    // set tooltips
    for(int row = 0; row < ok->rowCount(); row++) {
        QModelIndex parentIdx = ok->index(row, 0);
        for (int childRow = 0; childRow < ok->rowCount(parentIdx); childRow++) {
            QModelIndex idx = ok->index(childRow, 1, parentIdx);
            QString value = qvariant_cast<QString>(idx.data());
            ok->setData(idx, value, Qt::ToolTipRole);
        }
    }

    if (G::isThreadTrackingOn) qDebug()
        << "ThumbView::updateExifInfo - loaded metadata display info for"
        << fPath;

    tweakHeaders();
    showOrHide();

    isNewImageDataChange = false;
}

void InfoView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QModelIndex index = indexAt(event->pos());
        if (index.column() == 1) { // column you want to use for one click
            edit(index);
        }
    }
    QTreeView::mousePressEvent(event);
}
