#include "Views/infoview.h"
#include "Main/global.h"

class InfoDelegate : public QStyledItemDelegate
{
public:
    explicit InfoDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &index) const
    {
        static int count = 0;
        count++;
        index.isValid();          // suppress compiler warning
        int height = qRound(G::fontSize.toInt() * 1.7 * G::ptToPx);
        return QSize(option.rect.width(), height);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
    {
        painter->save();

        int y1 = option.rect.top();
        int y2 = option.rect.bottom();

        int a = G::backgroundShade + 5;
        int b = G::backgroundShade - 15;

        QLinearGradient categoryBackground;
        categoryBackground.setStart(0, y1);
        categoryBackground.setFinalStop(0, y2);
        categoryBackground.setColorAt(0, QColor(a,a,a));
        categoryBackground.setColorAt(1, QColor(b,b,b));

        int hOffset = 11;
        int vOffset = -1;
        QPoint topLeft(option.rect.left() + hOffset, option.rect.top() - vOffset);
        QPoint bottomRight(option.rect.bottomRight());
        QRect textRect(topLeft, bottomRight);

        if (index.parent() == QModelIndex() && index.column() == 0) {
            painter->fillRect(option.rect, categoryBackground);
            QPen catPen(Qt::white);
            painter->setPen(catPen);
        }
        QString text = index.data().toString();
        QFont font = painter->font();
        font.setPointSize(G::fontSize.toInt());
        painter->setFont(font);

        QString elidedText;
        if (index.column() == 0)
            elidedText = text;
        else
            elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, textRect.width());

        painter->setPen(QColor(G::textShade,G::textShade,G::textShade));
        painter->drawText(textRect, Qt::AlignVCenter|Qt::TextSingleLine, elidedText);

        painter->setPen(QColor(75,75,75));
        painter->drawRect(option.rect);

        painter->restore();
    }
};


/*
This class shows information in a two column table.

Column 0 = Item Description
Column 1 = Item Value
Column 2 = Flag to show or hide row (hidden)

It is used to show some file, image and application state information.
*/

InfoView::InfoView(QWidget *parent, DataModel *dm, Metadata *metadata, IconView *thumbView)
    : QTreeView(parent)
{
    if (G::isLogger) G::log(__FUNCTION__);    this->dm = dm;
    this->metadata = metadata;
    this->thumbView = thumbView;        // req'd to update metadata in dm for selections

    ok = new QStandardItemModel(this);
    setupOk();
    setModel(ok);
    selectionModel()->setModel(ok);

    setRootIsDecorated(true);
//    setColumnWidth(0, 100);
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
    setColumn0Width();
    setTabKeyNavigation(false);

    setItemDelegate(new InfoDelegate(this));

   // InfoView menu
	infoMenu = new QMenu("");
    copyInfoAction = new QAction(tr("Copy item"), this);

//    connect(copyInfoAction, SIGNAL(triggered()), this, SLOT(copyEntry()));
//	infoMenu->addAction(copyAction);

    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            SLOT(showInfoViewMenu(QPoint)));

    connect(ok, &QStandardItemModel::dataChanged, this, &InfoView::dataChanged);

//    connect(itemDelegate(), &QStyledItemDelegate::closeEditor, this, &InfoView::editorClosed);
//    connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &InfoView::selectionChanged);
}

void InfoView::dataChanged(const QModelIndex &idx1, const QModelIndex, const QVector<int> &roles)
{
/*
    Updates the datamodel for info items that can be edited: title, creator, copyright, email
    and url (at present).  This is used in Embel (ie image title) and "Show shooting data".

    The dataChanged signal is triggered twice for the same edit so count is used to only
    process once.

    The signal dataEdited is emitted, which triggers Embel to update the text fields. This
    will only work if Embel::isRemote == false.
*/
    static int count = 0;
    if (count == 0) {
        bool isEditable = ok->itemFromIndex(idx1)->isEditable();
        if ( isEditable) {
            QModelIndexList selection = thumbView->selectionModel()->selectedRows();
            QModelIndex idx0 = ok->index(idx1.row(), 0, idx1.parent());
            QString field = idx0.data().toString();
//            int row = dm->currentRow;
            for (int i = 0; i < selection.count(); i++) {
                int row = selection.at(i).row();
                if (field == "Title") {
                    QString s = idx1.data().toString();
                    dm->setData(dm->sf->index(row, G::TitleColumn), s);
                    dm->setData(dm->sf->index(row, G::TitleColumn), s, Qt::ToolTipRole);
                    /*
                    qDebug() << __FUNCTION__
                             << "dm->currentRow =" << dm->currentRow
                             << "idx0.data() =" << idx0.data()
                             << "idx1.data() =" << idx1.data()
                             << "isEditable =" << isEditable
                                ;
    //                            */
                }
                if (field == "Creator") {
                    QString s = idx1.data().toString();
                    dm->setData(dm->sf->index(row, G::CreatorColumn), s);
                    dm->setData(dm->sf->index(row, G::CreatorColumn), s, Qt::ToolTipRole);
                }
                if (field == "Copyright") {
                    QString s = idx1.data().toString();
                    dm->setData(dm->sf->index(row, G::CopyrightColumn), s);
                    dm->setData(dm->sf->index(row, G::CopyrightColumn), s, Qt::ToolTipRole);
                }
                if (field == "Email") {
                    QString s = idx1.data().toString();
                    dm->setData(dm->sf->index(row, G::EmailColumn), s);
                    dm->setData(dm->sf->index(row, G::EmailColumn), s, Qt::ToolTipRole);
                }
                if (field == "Url") {
                    QString s = idx1.data().toString();
                    dm->setData(dm->sf->index(row, G::UrlColumn), s);
                    dm->setData(dm->sf->index(row, G::UrlColumn), s, Qt::ToolTipRole);
                }
            }

            emit dataEdited();
        }
    }
    count++;
    if (count > 1) count = 0;
}

void InfoView::refreshLayout()
{
    if (G::isLogger) G::log(__FUNCTION__);    setColumn0Width();
    scheduleDelayedItemsLayout();
}

void InfoView::showInfoViewMenu(QPoint pt)
{
    if (G::isLogger) G::log(__FUNCTION__);    selectedEntry = indexAt(pt);
	if (selectedEntry.isValid())
    	infoMenu->popup(viewport()->mapToGlobal(pt));
}

void InfoView::setColumn0Width()
{
    if (G::isLogger) G::log(__FUNCTION__);    QFont ft = this->font();
    ft.setPixelSize(static_cast<int>(G::fontSize.toInt() * 1.333/* * G::ptToPx*/));
    QFontMetrics fm(ft);
    #ifdef Q_OS_WIN
    setColumnWidth(0, fm.boundingRect("---Shutter speed---").width());
    #endif
    #ifdef Q_OS_MAC
    setColumnWidth(0, fm.boundingRect("Shutter speed").width());
    #endif
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

If any of the editable fields change then MW::metadataChanged is triggered.
*/
    if (G::isLogger) G::log(__FUNCTION__);    ok->setHorizontalHeaderItem(0, new QStandardItem(QString("Field")));
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
    ok->setData(ok->index(ExposureCompensationRow, 0, imageInfoIdx), "EC");
    ok->setData(ok->index(FocalLengthRow, 0, imageInfoIdx), "Focal length");
    ok->setData(ok->index(TitleRow, 0, tagInfoIdx), "Title");
    ok->setData(ok->index(CreatorRow, 0, tagInfoIdx), "Creator");
    ok->setData(ok->index(CopyrightRow, 0, tagInfoIdx), "Copyright");
    ok->setData(ok->index(EmailRow, 0, tagInfoIdx), "Email");
    ok->setData(ok->index(UrlRow, 0, tagInfoIdx), "Url");
    ok->setData(ok->index(PositionRow, 0, statusInfoIdx), "Position");
    ok->setData(ok->index(ZoomRow, 0, statusInfoIdx), "Zoom");
    ok->setData(ok->index(SelectedRow, 0, statusInfoIdx), "Selected");
    ok->setData(ok->index(PickedRow, 0, statusInfoIdx), "Picked");
    ok->setData(ok->index(CacheRow, 0, statusInfoIdx), "Cache");
    ok->setData(ok->index(MonitorRow, 0, statusInfoIdx), "Monitor");

    // tooltip for EC
    ok->setData(ok->index(ExposureCompensationRow, 0, imageInfoIdx), "Exposure Compensation", Qt::ToolTipRole);

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
//    ok->itemFromIndex(ok->index(ModelRow, 1, imageInfoIdx))->setEditable(true);
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
    if (G::isLogger) G::log(__FUNCTION__);    bool okToShow;
    for(int row = 0; row < ok->rowCount(); row++) {
        QModelIndex parentIdx = ok->index(row, 0);
        okToShow = ok->index(row, 2).data().toBool();
        setRowHidden(row, QModelIndex(), !okToShow);
        for (int childRow = 0; childRow < ok->rowCount(parentIdx); childRow++) {
           okToShow = ok->index(childRow, 2, parentIdx).data().toBool();
            setRowHidden(childRow, parentIdx, !okToShow);
        }
    }
}

void InfoView::clearInfo()
{
/*
Clear all the values but leave the keys and flags alone
*/
    if (G::isLogger) G::log(__FUNCTION__);    for(int row = 0; row < ok->rowCount(); row++) {
        QModelIndex parentIdx = ok->index(row, 0);
        for (int childRow = 0; childRow < ok->rowCount(parentIdx); childRow++) {
            ok->setData(ok->index(childRow, 1, parentIdx), "");
        }
    }
}

void InfoView::copyEntry()
{
    if (G::isLogger) G::log(__FUNCTION__);	if (selectedEntry.isValid())
        QApplication::clipboard()->setText(ok->itemFromIndex(selectedEntry)->toolTip());
}

void InfoView::updateInfo(const int &row)
{
    if (G::isLogger) G::log(__FUNCTION__);//    qDebug() << __FUNCTION__ << row;

    // flag updates so itemChanged will be ignored in MW::metadataChanged
    isNewImageDataChange = true;

    QString fPath = dm->sf->index(row, G::PathColumn).data(G::PathRole).toString();
    QFileInfo imageInfo = QFileInfo(fPath);

    // make sure there is metadata for this image
    if (!dm->sf->index(row, G::MetadataLoadedColumn).data().toBool()) {
        metadata->loadImageMetadata(fPath, true, true, false, true, __FUNCTION__);
        metadata->m.row = dm->fPathRow[fPath];
        dm->addMetadataForItem(metadata->m);
    }

    QString s;
    QVariant value;

    // update items

    ok->setData(ok->index(FolderRow, 1, fileInfoIdx), imageInfo.dir().dirName());
    ok->setData(ok->index(FileNameRow, 1, fileInfoIdx), imageInfo.fileName());
    ok->setData(ok->index(LocationRow, 1, fileInfoIdx), imageInfo.path());
    ok->setData(ok->index(SizeRow, 1, fileInfoIdx), QString::number(imageInfo.size() / 1024000.0, 'f', 2) + " MB");
    s = dm->sf->index(row, G::CreatedColumn).data().toDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ok->setData(ok->index(CreatedRow, 1, fileInfoIdx), s);
    s = dm->sf->index(row, G::ModifiedColumn).data().toString();
    ok->setData(ok->index(ModifiedRow, 1, fileInfoIdx), s);

    s = dm->sf->index(row, G::DimensionsColumn).data().toString();
    ok->setData(ok->index(DimensionsRow, 1, fileInfoIdx), s);
    s = dm->sf->index(row, G::MegaPixelsColumn).data().toString();
    ok->setData(ok->index(MegaPixelsRow, 1, fileInfoIdx), s);
    s = dm->sf->index(row, G::CameraModelColumn).data().toString();
    ok->setData(ok->index(ModelRow, 1, imageInfoIdx), s);
    s = dm->sf->index(row, G::LensColumn).data().toString();
    ok->setData(ok->index(LensRow, 1, imageInfoIdx), s);

    value = dm->sf->index(row, G::ShutterspeedColumn).data();
    if (value == 0) s = "";
    else {
        if (value < 1.0) {
            double recip = 1 / value.toDouble();
            if (recip >= 2) s = "1/" + QString::number(qRound(recip));
            else s = QString::number(value.toDouble(), 'g', 2);
        } else {
            s = QString::number(value.toInt());
        }
        s += " sec";
    }
    ok->setData(ok->index(ShutterSpeedRow, 1, imageInfoIdx), s);

    value = dm->sf->index(row, G::ApertureColumn).data();
    if (value == 0) s = "";
    else s = "f/" + QString::number(value.toDouble(), 'f', 1);
    ok->setData(ok->index(ApertureRow, 1, imageInfoIdx), s);

    s = dm->sf->index(row, G::ISOColumn).data().toString();
    ok->setData(ok->index(ISORow, 1, imageInfoIdx), s);
    s = QString::number(dm->sf->index(row, G::ExposureCompensationColumn).data().toDouble(), 'f', 1) + " EV";
    ok->setData(ok->index(ExposureCompensationRow, 1, imageInfoIdx), s);
    s = dm->sf->index(row, G::FocalLengthColumn).data().toString() + "mm";
    ok->setData(ok->index(FocalLengthRow, 1, imageInfoIdx), s);
    s = dm->sf->index(row, G::TitleColumn).data().toString();
    ok->setData(ok->index(TitleRow, 1, tagInfoIdx), s);
    s = dm->sf->index(row, G::CreatorColumn).data().toString();
    ok->setData(ok->index(CreatorRow, 1, tagInfoIdx), s);

    s = dm->sf->index(row, G::CopyrightColumn).data().toString();
    ok->setData(ok->index(CopyrightRow, 1, tagInfoIdx), s);
    s = dm->sf->index(row, G::EmailColumn).data().toString();
    ok->setData(ok->index(EmailRow, 1, tagInfoIdx), s);
    s = dm->sf->index(row, G::UrlColumn).data().toString();
    ok->setData(ok->index(UrlRow, 1, tagInfoIdx), s);

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
        << row;

    setColumn0Width();
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
