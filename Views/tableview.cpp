#include "Views/tableview.h"

TableView::TableView(DataModel *dm)
{
/*

*/
    if (G::isLogger) G::log(CLASSFUNCTION);    
    this->dm = dm;
//    int ht = G::fontSize.toInt();

    setModel(dm->sf);
    setSortingEnabled(true);
    setAlternatingRowColors(true);
    horizontalHeader()->setSortIndicatorShown(false);
    horizontalHeader()->setSectionsMovable(true);
    horizontalHeader()->setStretchLastSection(true);
//    horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    verticalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setTabKeyNavigation(false);
    setWordWrap(false);
//    setIconSize(QSize(ht + 10, ht + 22));
//    setIconSize(QSize(24, 24 + 12));
//    verticalHeader()->setDefaultSectionSize(24);

    verticalScrollBar()->setObjectName("TableViewVerticalScrollBar");

    CreatedItemDelegate *createdItemDelegate = new CreatedItemDelegate;
    setItemDelegateForColumn(G::CreatedColumn, createdItemDelegate);

    RefineItemDelegate *refineItemDelegate = new RefineItemDelegate;
    setItemDelegateForColumn(G::RefineColumn, refineItemDelegate);

    PickItemDelegate *pickItemDelegate = new PickItemDelegate;
    setItemDelegateForColumn(G::PickColumn, pickItemDelegate);

    SearchItemDelegate *searchItemDelegate = new SearchItemDelegate;
    setItemDelegateForColumn(G::SearchColumn, searchItemDelegate);

    IngestedItemDelegate *ingestedItemDelegate = new IngestedItemDelegate;
    setItemDelegateForColumn(G::IngestedColumn, ingestedItemDelegate);

    VideoItemDelegate *videoItemDelegate = new VideoItemDelegate;
    setItemDelegateForColumn(G::VideoColumn, videoItemDelegate);

    DimensionItemDelegate *dimensionItemDelegate = new DimensionItemDelegate;
    setItemDelegateForColumn(G::DimensionsColumn, dimensionItemDelegate);

    ApertureItemDelegate *apertureItemDelegate = new ApertureItemDelegate;
    setItemDelegateForColumn(G::ApertureColumn, apertureItemDelegate);

    ExposureTimeItemDelegate *exposureTimeItemDelegate = new ExposureTimeItemDelegate;
    setItemDelegateForColumn(G::ShutterspeedColumn, exposureTimeItemDelegate);

    ISOItemDelegate *isoItemDelegate = new ISOItemDelegate;
    setItemDelegateForColumn(G::ISOColumn, isoItemDelegate);

    ExposureCompensationItemDelegate *exposureCompensationItemDelegate = new ExposureCompensationItemDelegate;
    setItemDelegateForColumn(G::ExposureCompensationColumn, exposureCompensationItemDelegate);

    KeywordsItemDelegate *keywordsItemDelegate = new KeywordsItemDelegate;
    setItemDelegateForColumn(G::KeywordsColumn, keywordsItemDelegate);

    FocalLengthItemDelegate *focalLengthItemDelegate = new FocalLengthItemDelegate;
    setItemDelegateForColumn(G::FocalLengthColumn, focalLengthItemDelegate);

    FileSizeItemDelegate *fileSizeItemDelegate = new FileSizeItemDelegate;
    setItemDelegateForColumn(G::SizeColumn, fileSizeItemDelegate);

    createOkToShow();
}

void TableView::setViewportParameters()
{
    if (G::isLogger) G::log(CLASSFUNCTION);        
		firstVisibleRow = rowAt(0);
    midVisibleRow = rowAt(height() / 2);
    lastVisibleRow = rowAt(height());

    int row = currentIndex().row();
    isCurrentVisible = (row >= firstVisibleRow && row <= lastVisibleRow);
}

void TableView::scrollToRow(int row, QString source)
{
    if (G::isLogger) G::log(CLASSFUNCTION);    
//    qDebug() << CLASSFUNCTION << objectName() << "row =" << row
//             << "source =" << source;
    QModelIndex idx = dm->sf->index(row, 0);
    scrollTo(idx, QAbstractItemView::PositionAtCenter);
    source = "";    // suppress compiler warning
}

bool TableView::isRowVisible(int row)
{
    return (row >= firstVisibleRow && row <= lastVisibleRow);
}

void TableView::scrollToCurrent()
{
    if (G::isLogger) G::log(CLASSFUNCTION);    		
    QModelIndex idx = dm->sf->index(currentIndex().row(), 1);
    scrollTo(idx, ScrollHint::PositionAtCenter);
}

int TableView::sizeHintForColumn(int column) const
{
    QFontMetrics fm(this->font());
    if (column == G::PathColumn) return fm.boundingRect("Icon").width();
    if (column == G::NameColumn) return fm.boundingRect("2019-02-25_0001.jpg========").width();
    if (column == G::TypeColumn) return fm.boundingRect("=JPG+NEF=").width();
    if (column == G::VideoColumn) return fm.boundingRect("=Video=").width();
    if (column == G::PermissionsColumn) return fm.boundingRect("=Permissions=").width();
    if (column == G::ReadWriteColumn) return fm.boundingRect("=false=").width();
    if (column == G::SizeColumn) return fm.boundingRect("=999,999,999=").width();
    if (column == G::CreatedColumn) return fm.boundingRect("=2019-09-09 09:09:09=").width();
    if (column == G::ModifiedColumn) return fm.boundingRect("=2019-09-09 09:09:09=").width();
    if (column == G::YearColumn) return fm.boundingRect("=2000=").width();
    if (column == G::DayColumn) return fm.boundingRect("=2000-00-00=").width();
    if (column == G::RefineColumn) return fm.boundingRect("=Refine=").width();
    if (column == G::PickColumn) return fm.boundingRect("=Pick=").width();
    if (column == G::IngestedColumn) return fm.boundingRect("=Ingested=").width();
    if (column == G::MetadataLoadedColumn) return fm.boundingRect("=Meta Loaded=").width();
    if (column == G::SearchColumn) return fm.boundingRect("=false=").width();
    if (column == G::LabelColumn) return fm.boundingRect("=Colour=").width();
    if (column == G::RatingColumn) return fm.boundingRect("=Rating=").width();
    if (column == G::WidthColumn) return fm.boundingRect("=Width=").width();
    if (column == G::HeightColumn) return fm.boundingRect("=Height=").width();
    if (column == G::CreatorColumn) return fm.boundingRect("Rory Hill=====").width();
    if (column == G::MegaPixelsColumn) return fm.boundingRect("=999.99=").width();
    if (column == G::LoadMsecPerMpColumn) return fm.boundingRect("=Msec/Mp=").width();
    if (column == G::DimensionsColumn) return fm.boundingRect("=99999x99999=").width();
    if (column == G::AspectRatioColumn) return fm.boundingRect("=Aspect Ratio=").width();
    if (column == G::RotationColumn) return fm.boundingRect("=Rot=").width();
    if (column == G::ApertureColumn) return fm.boundingRect("=Aperture=").width();
    if (column == G::ShutterspeedColumn) return fm.boundingRect("=1/8000 sec=").width();
    if (column == G::ISOColumn) return fm.boundingRect("999999").width();
    if (column == G::ExposureCompensationColumn) return fm.boundingRect("==-2 EV===").width();
    if (column == G::DurationColumn) return fm.boundingRect("=Duration=").width();
    if (column == G::CameraMakeColumn) return fm.boundingRect("Nikon ========").width();
    if (column == G::CameraModelColumn) return fm.boundingRect("Nikon D850============").width();
    if (column == G::LensColumn) return fm.boundingRect("Lens======================").width();
    if (column == G::FocalLengthColumn) return fm.boundingRect("=Focal length==").width();
    if (column == G::GPSCoordColumn) return fm.boundingRect("=49°13'13.477  N 123°57'22.22 W=").width();
    if (column == G::TitleColumn) return fm.boundingRect("=Title=========================").width();
    if (column == G::CopyrightColumn) return fm.boundingRect("=Copyright=====").width();
    if (column == G::EmailColumn) return fm.boundingRect("=Email================").width();
    if (column == G::UrlColumn) return fm.boundingRect("=Url=======================").width();
    if (column == G::_TitleColumn) return fm.boundingRect("=Title=========================").width();
    if (column == G::_CopyrightColumn) return fm.boundingRect("=Copyright=====").width();
    if (column == G::_EmailColumn) return fm.boundingRect("=Email================").width();
    if (column == G::_UrlColumn) return fm.boundingRect("=Url=======================").width();
    if (column == G::KeywordsColumn) return fm.boundingRect("=keyword, keyword, keyword, keyword=").width();
    if (column == G::MetadataLoadedColumn) return fm.boundingRect("=Metadata Loaded=").width();
    if (column == G::OffsetFullColumn) return fm.boundingRect("=OffsetFullColumn=").width();
    if (column == G::LengthFullColumn) return fm.boundingRect("=LengthFullColumn=").width();
    if (column == G::WidthPreviewColumn) return fm.boundingRect("=WidthPreview=").width();
    if (column == G::HeightPreviewColumn) return fm.boundingRect("=HeightPreview=").width();
    if (column == G::OffsetThumbColumn) return fm.boundingRect("=OffsetThumbColumn=").width();
    if (column == G::LengthThumbColumn) return fm.boundingRect("=LengthThumbColumn=").width();
//    if (column == G::OffsetSmallColumn) return fm.boundingRect("=OffsetSmallColumn=").width();
//    if (column == G::LengthSmallColumn) return fm.boundingRect("=LengthSmallColumn=").width();

//    if (column == G::bitsPerSampleColumn) return fm.boundingRect("=bitsPerSampleFullColumn=").width();
//    if (column == G::photoInterpColumn) return fm.boundingRect("=photoInterpFullColumn=").width();
    if (column == G::samplesPerPixelColumn) return fm.boundingRect("=samplesPerPixelFullColumn=").width();
//    if (column == G::compressionColumn) return fm.boundingRect("=compressionFullColumn=").width();
//    if (column == G::stripByteCountsColumn) return fm.boundingRect("=stripByteCountsFullColumn=").width();

    if (column == G::isBigEndianColumn) return fm.boundingRect("=isBigEndian=").width();
    if (column == G::ifd0OffsetColumn) return fm.boundingRect("=ifd0OffsetColumn=").width();
    if (column == G::XmpSegmentOffsetColumn) return fm.boundingRect("=XmpSegmentOffsetColumn=").width();
    if (column == G::XmpSegmentLengthColumn) return fm.boundingRect("=XmpSegmentLengthColumn=").width();
    if (column == G::IsXMPColumn) return fm.boundingRect("=IsXMPColumn=").width();
    if (column == G::ICCSegmentOffsetColumn) return fm.boundingRect("=ICCSegmentOffsetColumn=").width();
    if (column == G::ICCSegmentLengthColumn) return fm.boundingRect("=ICCSegmentLengthColumn=").width();
    if (column == G::ICCBufColumn) return fm.boundingRect("=ICCBuf=").width();
    if (column == G::ICCSpaceColumn) return fm.boundingRect("=ICCSpaceColumn=").width();
    if (column == G::OrientationOffsetColumn) return fm.boundingRect("=OrientationOffsetColumn=").width();
//    if (column == G::OrientationColumn) return fm.boundingRect("=OrientationColumn=").width();
    if (column == G::RotationDegreesColumn) return fm.boundingRect("=RotationDegreesColumn=").width();
    if (column == G::ShootingInfoColumn) return fm.boundingRect("=ShootingInfoColumn======================").width();
    if (column == G::SearchTextColumn) return fm.boundingRect("=SearchText=====================================================================================").width();
    return 50;
}

void TableView::selectPageUp()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    setCurrentIndex(moveCursor(QAbstractItemView::MovePageUp, Qt::NoModifier));
}

void TableView::selectPageDown()
{
    if (G::isLogger) G::log(CLASSFUNCTION);       
		qDebug() << CLASSFUNCTION;
    setCurrentIndex(moveCursor(QAbstractItemView::MovePageDown, Qt::NoModifier));
}

bool TableView::eventFilter(QObject *obj, QEvent *event)
{
    if((event->type() == QEvent::Paint || event->type() == QEvent::Timer)
            && scrollWhenReady
            && obj->objectName() == "QScrollBar")
    {
        scrollToCurrent();
    }
    return QWidget::eventFilter(obj, event);
}

void TableView::resizeColumns()
{
    for (int column = 0; column < G::TotalColumns; ++column) {
        setColumnWidth(column, sizeHintForColumn(column));
    }
}

void TableView::paintEvent(QPaintEvent *event)
{
    resizeColumns();      // prevents user changing column widths
    int d = static_cast<int>(G::fontSize.toInt() * G::ptToPx * 1.5);
    setIconSize(QSize(d, d));
    verticalHeader()->setDefaultSectionSize(d);
    horizontalHeader()->setFixedHeight(d);
    QTableView::paintEvent(event);
}

void TableView::mousePressEvent(QMouseEvent *event)
{
    // ignore right mouse clicks (context menu)
    if (event->button() == Qt::RightButton) return;
    G::fileSelectionChangeSource = "TableMouseClick";
    // propogate mouse press if pressed in a table row, otherwise do nothing
    if (indexAt(event->pos()).isValid()) QTableView::mousePressEvent(event);
}

void TableView::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
    if (G::isLogger) G::log(CLASSFUNCTION); 
    emit displayLoupe();
}

void TableView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    QTableView::selectionChanged(selected, deselected);
}

void TableView::createOkToShow()
{
/*
    This function is called from the tableView constructor.

    Create a datamodel called (ok) to hold all the columns available to the
    tableView, which = all the columns in the datamodel dm.  dm (datamodel class)
    holds significant information about all images in the current folder.

    Assign a default show flag = true to each column. This will be updated when
    QSettings has been loaded.

    Column 0 = TableView column name
    Column 1 = Show/hide TableView column
    Column 2 = Geek role for the TableView column
*/
    if (G::isLogger) G::log(CLASSFUNCTION); 
    ok = new QStandardItemModel;

    ok->setHorizontalHeaderItem(0, new QStandardItem(QString("Field")));
    ok->setHorizontalHeaderItem(1, new QStandardItem(QString("Show")));
    ok->setHorizontalHeaderItem(2, new QStandardItem(QString("Geek")));

    // do not include column 0 as it is used to index tableView
    for (int i = 1; i < dm->columnCount(); i++) {
        QString columnName = dm->horizontalHeaderItem(i)->text();
        ok->insertRow(i - 1);
        ok->setData(ok->index(i - 1, 0), columnName);
        bool isGeek = dm->horizontalHeaderItem(i)->data(G::GeekRole).toBool();
        ok->setData(ok->index(i - 1, 2), isGeek);
        if (G::showAllTableColumns) ok->setData(ok->index(i - 1, 1), true);
        else ok->setData(ok->index(i - 1, 1), !isGeek);
    }
    showOrHide();

    connect(ok, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
            this, SLOT(showOrHide()));
}

void TableView::showOrHide()
{
/*
The ok datamodel (table fields to show) is edited in the preferences dialog
and this slot is then signalled to update which fields are visible.
*/
    for(int i = 0; i < ok->rowCount(); i++) {
//        if (!G::showAllTableColumns && i > G::UrlColumn) {
//            hideColumn(i + 1);
//            continue;
//        }
        bool showField = ok->index(i, 1).data().toBool();
        if (showField) showColumn(i + 1);
        else hideColumn(i + 1);
        /*
        QString s = ok->index(i, 0).data().toString();
        qDebug() << CLASSFUNCTION << i << s << showField;  */
    }
}

//------------------------------------------------------------------------------
//   DELEGATES
//------------------------------------------------------------------------------

//FileItemDelegate::FileItemDelegate(QObject* parent): QStyledItemDelegate(parent)
//{
//}

//QSize FileItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
//{
////    QFontMetrics fm(option.font);
////    int width = fm.width("2019-02-25_0001.jpg    ");
////    return QSize(width, option.rect.height());
//}

CreatedItemDelegate::CreatedItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString CreatedItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return value.toDateTime().toString("yyyy-MM-dd hh:mm:ss");
}

RefineItemDelegate::RefineItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString RefineItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return (value.toBool()) ? "true" : "";
}

//QSize RefineItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
//{
////    QFontMetrics fm(option.font);
////    int width = fm.width(" Refine ");
////    return QSize(width, option.rect.height());
//}

PickItemDelegate::PickItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString PickItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    if (value == "reject") return "X";
    return (value == "true") ? "✓" : "";
}

SearchItemDelegate::SearchItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString SearchItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return (value == "true") ? "✓" : "";
}

IngestedItemDelegate::IngestedItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString IngestedItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return (value == "true") ? "✓" : "";
}

VideoItemDelegate::VideoItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString VideoItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return (value == "true") ? "✓" : "";
}

DimensionItemDelegate::DimensionItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString DimensionItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return " " + value.toString() + " ";
}

ApertureItemDelegate::ApertureItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString ApertureItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    if (value == 0) return QString();

    return "   f/" + QString::number(value.toDouble(), 'f', 1) + " ";
}

ExposureTimeItemDelegate::ExposureTimeItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString ExposureTimeItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    if (value == 0)
    return QString();

    QString exposureTime;
    if (value.toDouble() < 1.0) {
        double recip = 1 / value.toDouble();
        if (recip >= 2) exposureTime = "1/" + QString::number(qRound(recip));
        else exposureTime = QString::number(value.toDouble(), 'g', 2);
    } else {
        exposureTime = QString::number(value.toInt());
    }
    exposureTime += " sec";

    return exposureTime;
}

FocalLengthItemDelegate::FocalLengthItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString FocalLengthItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    if (value == 0)
    return QString();

    return "    " + QString::number(value.toDouble(), 'f', 0) + "mm    ";
}

ISOItemDelegate::ISOItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString ISOItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    if (value == 0)
        return QString();

    return QString::number(value.toDouble(), 'f', 0);
}

ExposureCompensationItemDelegate::ExposureCompensationItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString ExposureCompensationItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    if (value.toString() == "")
        return QString();

    return "   " + QString::number(value.toDouble(), 'f', 1) + " EV   ";
}

KeywordsItemDelegate::KeywordsItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString KeywordsItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    if (value.toStringList().count() == 0)
        return QString();

    return Utilities::stringListToString(value.toStringList());
}

FileSizeItemDelegate::FileSizeItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString FileSizeItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return QLocale(QLocale::English).toString(value.toDouble(), 'f', 0);
}

ErrItemDelegate::ErrItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString ErrItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    QStringList sl = value.toStringList();
    QString s = "";
    for (int i = 0; i < sl.size(); ++i) {
        s += sl.at(i) + " \n";
    }
    return s;
}
