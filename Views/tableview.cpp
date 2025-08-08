#include "Views/tableview.h"
#include "Main/mainwindow.h"

extern MW *m5;
MW *m5;

TableView::TableView(QWidget *parent, DataModel *dm)
{
/*

*/
    if (G::isLogger) G::log("TableView::TableView");
    this->dm = dm;

    // this works because ThumbView is a friend class of MW.  It is used in the
    // mousePressEvent to call MW::Selection::select
    m5 = qobject_cast<MW*>(parent);

    setModel(dm->sf);
    setSortingEnabled(true);
    setAlternatingRowColors(true);
    horizontalHeader()->setSortIndicatorShown(false);
    horizontalHeader()->setSectionsMovable(true);
    horizontalHeader()->setStretchLastSection(true);
    //horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    verticalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setTabKeyNavigation(false);
    setWordWrap(false);
    //setIconSize(QSize(ht + 10, ht + 22));
    //setIconSize(QSize(24, 24 + 12));
    //verticalHeader()->setDefaultSectionSize(24);

    verticalScrollBar()->setObjectName("TableViewVerticalScrollBar");

    RowNumberItemDelegate *rowNumberItemDelegate = new RowNumberItemDelegate;
    setItemDelegateForColumn(G::RowNumberColumn, rowNumberItemDelegate);

    CreatedItemDelegate *createdItemDelegate = new CreatedItemDelegate;
    setItemDelegateForColumn(G::CreatedColumn, createdItemDelegate);

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

    // default column widths
    resizeColumns();

    //connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &TableView::onHorizontalScrollBarChanged);
}

void TableView::updateVisible(QString src)
{
    if (G::isLogger) G::log("TableView::updateVisible");
    int lastRow = dm->sf->rowCount() - 1;
    if (rowHeight(0)) visibleRowCount = height() / rowHeight(0);
    else visibleRowCount = 0;
    firstVisibleRow = rowAt(0);
    lastVisibleRow = firstVisibleRow + visibleRowCount - 1;
    if (lastVisibleRow > lastRow) lastVisibleRow = lastRow;
    midVisibleRow = lastVisibleRow - visibleRowCount / 2;
    dm->scrollToIcon = midVisibleRow;
    /*
    qDebug() << "TableView::updateVisible"
             << "firstVisibleRow =" << firstVisibleRow
             << "midVisibleRow =" << midVisibleRow
             << "lastVisibleRow =" << lastVisibleRow
             << "height() =" << height()
             << "rowHeight(0) =" << rowHeight(0)
             << "visibleRowCount =" << visibleRowCount
                ;
                //*/

    int row = currentIndex().row();
    isCurrentVisible = (row >= firstVisibleRow && row <= lastVisibleRow);
}

bool TableView::isRowVisible(int row)

{
    return (row >= firstVisibleRow && row <= lastVisibleRow);
}

void TableView::scrollToRow(int row, QString source)
{
    if (G::isLogger || G::isFlowLogger) G::log("TableView::scrollToRow", source);
    /* debug
    qDebug() << "TableView::scrollToRow" << objectName() << "row =" << row
             << "source =" << source;
             //*/
    QModelIndex sfIdx = dm->sf->index(row, 0);
    scrollTo(sfIdx, QAbstractItemView::PositionAtCenter);
}

void TableView::scrollToCurrent()
{
    if (G::isLogger) G::log("TableView::scrollToCurrent");
    QModelIndex sfIdx = dm->sf->index(currentIndex().row(), 1);
    G::ignoreScrollSignal = true;
    scrollTo(sfIdx, ScrollHint::PositionAtCenter);
}

int TableView::defaultColumnWidth(int column)
{
    QFontMetrics fm(this->font());
    if (column == G::PathColumn) return fm.boundingRect("-Icon-").width();
    if (column == G::RowNumberColumn) return fm.boundingRect("-999999-").width();
    if (column == G::NameColumn) return fm.boundingRect("2019-02-25_0001.jpg========").width();
    if (column == G::FolderNameColumn) return fm.boundingRect("This is the folder name========").width();
    if (column == G::MSToReadColumn) return fm.boundingRect("=Read ms=").width();
    if (column == G::PickColumn) return fm.boundingRect("===Pick===").width();
    if (column == G::IngestedColumn) return fm.boundingRect("=Ingested=").width();
    if (column == G::LabelColumn) return fm.boundingRect("=Colour=").width();
    if (column == G::RatingColumn) return fm.boundingRect("=Rating=").width();
    if (column == G::SearchColumn) return fm.boundingRect("=false=").width();
    if (column == G::TypeColumn) return fm.boundingRect("=JPG+NEF=").width();
    if (column == G::VideoColumn) return fm.boundingRect("=Video=").width();
    if (column == G::ApertureColumn) return fm.boundingRect("=Aperture=").width();
    if (column == G::ShutterspeedColumn) return fm.boundingRect("=1/8000 sec=").width();
    if (column == G::ISOColumn) return fm.boundingRect("999999").width();
    if (column == G::ExposureCompensationColumn) return fm.boundingRect("==-2 EV===").width();
    if (column == G::DurationColumn) return fm.boundingRect("=Duration=").width();
    if (column == G::CameraMakeColumn) return fm.boundingRect("Nikon ========").width();
    if (column == G::CameraModelColumn) return fm.boundingRect("Nikon D850============").width();
    if (column == G::LensColumn) return fm.boundingRect("Lens======================").width();
    if (column == G::FocalLengthColumn) return fm.boundingRect("=Focal length==").width();
    if (column == G::FocusXColumn) return fm.boundingRect("=FocusX==").width();
    if (column == G::FocusYColumn) return fm.boundingRect("=FocusY==").width();
    if (column == G::GPSCoordColumn) return fm.boundingRect("=49°13'13.477  N 123°57'22.22 W=").width();
    if (column == G::SizeColumn) return fm.boundingRect("=999,999,999=").width();
    if (column == G::WidthColumn) return fm.boundingRect("=Width=").width();
    if (column == G::HeightColumn) return fm.boundingRect("=Height=").width();
    if (column == G::ModifiedColumn) return fm.boundingRect("=2019-09-09 09:09:09=").width();
    if (column == G::CreatedColumn) return fm.boundingRect("=2019-09-09 09:09:09.999=").width();
    if (column == G::YearColumn) return fm.boundingRect("=2000=").width();
    if (column == G::DayColumn) return fm.boundingRect("=2000-00-00=").width();
    if (column == G::CreatorColumn) return fm.boundingRect("Rory Hill=====").width();
    if (column == G::MegaPixelsColumn) return fm.boundingRect("=999.99=").width();
    if (column == G::LoadMsecPerMpColumn) return fm.boundingRect("=Msec/Mp=").width();
    if (column == G::DimensionsColumn) return fm.boundingRect("=99999x99999=").width();
    if (column == G::AspectRatioColumn) return fm.boundingRect("=Aspect Ratio=").width();
    if (column == G::OrientationColumn) return fm.boundingRect("=Orientation=").width();
    if (column == G::RotationColumn) return fm.boundingRect("=Rot=").width();
    if (column == G::CopyrightColumn) return fm.boundingRect("=Copyright=====").width();
    if (column == G::TitleColumn) return fm.boundingRect("=Title=========================").width();
    if (column == G::EmailColumn) return fm.boundingRect("=Email================").width();
    if (column == G::UrlColumn) return fm.boundingRect("=Url=======================").width();
    if (column == G::KeywordsColumn) return fm.boundingRect("=keyword, keyword, keyword, keyword=").width();
    if (column == G::MetadataReadingColumn) return fm.boundingRect("=Metadata Reading=").width();
    if (column == G::MetadataAttemptedColumn) return fm.boundingRect("=Metadata Attempted=").width();
    if (column == G::MetadataLoadedColumn) return fm.boundingRect("=Meta Loaded=").width();
    if (column == G::IconLoadedColumn) return fm.boundingRect("=Icon Loaded=").width();
    if (column == G::MissingThumbColumn) return fm.boundingRect("=Missing Thumb=").width();
    if (column == G::CompareColumn) return fm.boundingRect("=Compare=").width();
    if (column == G::_RatingColumn) return fm.boundingRect("=_Rating=").width();
    if (column == G::_LabelColumn) return fm.boundingRect("=_Label=").width();
    if (column == G::_CreatorColumn) return fm.boundingRect("=_Creator=").width();
    if (column == G::_TitleColumn) return fm.boundingRect("=_Title=========================").width();
    if (column == G::_CopyrightColumn) return fm.boundingRect("=_Copyright=====").width();
    if (column == G::_EmailColumn) return fm.boundingRect("=Email================").width();
    if (column == G::_UrlColumn) return fm.boundingRect("=Url=======================").width();
    if (column == G::PermissionsColumn) return fm.boundingRect("=Permissions=").width();
    if (column == G::ReadWriteColumn) return fm.boundingRect("=false=").width();
    if (column == G::OffsetFullColumn) return fm.boundingRect("=OffsetFullColumn=").width();
    if (column == G::LengthFullColumn) return fm.boundingRect("=LengthFullColumn=").width();
    if (column == G::WidthPreviewColumn) return fm.boundingRect("=WidthPreview=").width();
    if (column == G::HeightPreviewColumn) return fm.boundingRect("=HeightPreview=").width();
    if (column == G::OffsetThumbColumn) return fm.boundingRect("=OffsetThumbColumn=").width();
    if (column == G::LengthThumbColumn) return fm.boundingRect("=LengthThumbColumn=").width();
    if (column == G::samplesPerPixelColumn) return fm.boundingRect("=samplesPerPixelFullColumn=").width();
    if (column == G::isBigEndianColumn) return fm.boundingRect("=isBigEndian=").width();
    if (column == G::ifd0OffsetColumn) return fm.boundingRect("=ifd0OffsetColumn=").width();
    if (column == G::ifdOffsetsColumn) return fm.boundingRect("=ifdOffsetColumn=").width();
    if (column == G::XmpSegmentOffsetColumn) return fm.boundingRect("=XmpSegmentOffsetColumn=").width();
    if (column == G::XmpSegmentLengthColumn) return fm.boundingRect("=XmpSegmentLengthColumn=").width();
    if (column == G::IsXMPColumn) return fm.boundingRect("=IsXMPColumn=").width();
    if (column == G::ICCSegmentOffsetColumn) return fm.boundingRect("=ICCSegmentOffsetColumn=").width();
    if (column == G::ICCSegmentLengthColumn) return fm.boundingRect("=ICCSegmentLengthColumn=").width();
    if (column == G::ICCBufColumn) return fm.boundingRect("=ICCBuf=").width();
    if (column == G::ICCSpaceColumn) return fm.boundingRect("=ICCSpaceColumn=").width();

    if (column == G::CacheSizeColumn) return fm.boundingRect("=CacheMB=").width();
    // if (column == G::IsVideoColumn) return fm.boundingRect("=IsVideo=").width();
    if (column == G::IsCachingColumn) return fm.boundingRect("=IsCaching=").width();
    if (column == G::IsCachedColumn) return fm.boundingRect("=IsCached=").width();
    if (column == G::AttemptsColumn) return fm.boundingRect("=Attempts=").width();
    if (column == G::DecoderIdColumn) return fm.boundingRect("=Decoder ID=").width();
    if (column == G::DecoderReturnStatusColumn) return fm.boundingRect("=Status=").width();
    if (column == G::DecoderErrMsgColumn) return fm.boundingRect("=Decoder Error Message==============================").width();

    if (column == G::OrientationOffsetColumn) return fm.boundingRect("=OrientationOffsetColumn=").width();
    if (column == G::RotationDegreesColumn) return fm.boundingRect("=RotationDegreesColumn=").width();
    if (column == G::IconSymbolColumn) return fm.boundingRect("=IconSymbolColumn=").width();
    if (column == G::ShootingInfoColumn) return fm.boundingRect("=ShootingInfoColumn======================").width();
    if (column == G::SearchTextColumn) return fm.boundingRect("=SearchText=====================================================================================").width();
    if (column == G::ErrColumn) return fm.boundingRect("==ErrText=========================================================").width();
    return 50;
}

QModelIndex TableView::pageDownIndex(int fromRow)
{
    if (G::isLogger) G::log("TableView::pageUpIndex");
    updateVisible("TableView::pageDownIndex");
    int max = dm->sf->rowCount() - 1;
    int pageUpRow = fromRow + visibleRowCount;
    if (pageUpRow > max) pageUpRow = max;
    qDebug() << "TableView::pageUpIndex row =" << pageUpRow << "visibleRowCount" << visibleRowCount;
    scrollToRow(pageUpRow, "TableView::pageUpIndex");
    return dm->sf->index(pageUpRow, 0);
    //return moveCursor(QAbstractItemView::MovePageUp, Qt::NoModifier);
}

QModelIndex TableView::pageUpIndex(int fromRow)
{
    if (G::isLogger) G::log("TableView::pageDownIndex");
    updateVisible("TableView::pageUpIndex");
    int pageDownRow = fromRow - visibleRowCount;
    if (pageDownRow < 0) pageDownRow = 0;
    scrollToRow(pageDownRow, "TableView::pageUpIndex");
    qDebug() << "TableView::pageDownIndex row =" << pageDownRow << "visibleRowCount" << visibleRowCount;
    return dm->sf->index(pageDownRow, 0);
    //return moveCursor(QAbstractItemView::MovePageDown, Qt::NoModifier);
}

// bool TableView::eventFilter(QObject *obj, QEvent *event)
// {
//     if ((event->type() == QEvent::Paint || event->type() == QEvent::Timer))
//     {
//         //qDebug() << "TableView::eventFilter  obj =" << obj->objectName().leftJustified(30) << "event =" << event;
//     }
// //    if ((event->type() == QEvent::Paint || event->type() == QEvent::Timer)
// //        && scrollWhenReady
// //        && obj->objectName() == "QScrollBar")
// //    {
// //        scrollToCurrent();
// //    }
//     return QWidget::eventFilter(obj, event);
// }

void TableView::onHorizontalScrollBarChanged(int value)
{
    // not working to scroll left, need to use double tableview method
    qDebug() << "TableView::onHorizontalScrollBarChanged" << value;
    if (okToFreeze) freezeFirstColumn();
    okToFreeze = true;
}

bool TableView::isColumnVisibleInViewport(int col)
{
    int x = columnViewportPosition(col);
    int hscroll = horizontalScrollBar()->value();
    int viewportWidth = viewport()->width();

    return x >= hscroll && x <= hscroll + viewportWidth;
}

 QList<int> TableView::visibleColumns()
{
    QList<int> visibleColumns;
    for (int col = 0; col < G::TotalColumns; ++col) {
        if (isColumnVisibleInViewport(col)) visibleColumns << col;
    }
    //qDebug() << "TableView::visibleColumns" << visibleColumns;
    return visibleColumns;
}

void TableView::freezeFirstColumn()
{
    // not working to scroll left, need to use double tableview method
    int col;
    for (col = 2; col < G::TotalColumns; ++col) {
        setColumnHidden(col, false);
    }
    for (col = 2; col < G::TotalColumns; ++col) {
        if (isColumnVisibleInViewport(col)) break;
    }
    int lastToHide = col;
    for (col = 2; col < lastToHide; ++col) {
        setColumnHidden(col, true);
    }
//    okToFreeze = false;
//    horizontalScrollBar()->setValue(0);
    //qDebug() << "TableView::visibleColumns" << visibleColumns;
}

void TableView::resizeColumns()
{
    // qDebug() << "TableView::resizeColumns";
    for (int column = 0; column < G::TotalColumns; ++column) {
        setColumnWidth(column, defaultColumnWidth(column));
    }
}

void TableView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    updateVisible("TableView::resizeEvent");
}

void TableView::paintEvent(QPaintEvent *event)
{
//    resizeColumns();      // prevents user changing column widths
    int d = static_cast<int>(G::strFontSize.toInt() * G::ptToPx * 1.5);
    setIconSize(QSize(d, d));
    verticalHeader()->setDefaultSectionSize(d);
    horizontalHeader()->setFixedHeight(d);
    QTableView::paintEvent(event);
}

void TableView::keyPressEvent(QKeyEvent *event){
    // navigation keys are executed in MW::eventFilter to make sure they work no matter
    // what has the focus
    if (event->key() == Qt::Key_Return) emit displayLoupe("TableView::keyPressEvent Key_Return");
}

void TableView::mousePressEvent(QMouseEvent *event)
{
    int row = indexAt(event->pos()).row();
    // only interested in rows, so set index column = 0
    QModelIndex sfIdx = dm->sf->index(row, 0);
    // check mouse click was not in area after last row
    if (!sfIdx.isValid()) return;

    // ignore right mouse clicks (context menu)
    if (event->button() == Qt::RightButton) return;

    // propogate mouse press if pressed in a table row, otherwise do nothing
    if (event->button() == Qt::LeftButton) {
        G::fileSelectionChangeSource = "TableMouseClick";
        m5->sel->select(sfIdx, event->modifiers(), "TableView::mousePressEvent");
    }
}

void TableView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (G::isLogger) G::log("TableView::mouseDoubleClickEvent");

    // check mouse click was not in area after last row
    QModelIndex sfIdx = indexAt(event->pos());
    if (!sfIdx.isValid()) return;

    if (!event->modifiers() && event->button() == Qt::LeftButton) {
        G::fileSelectionChangeSource = "TableMouseClick";
        m5->sel->select(sfIdx, event->modifiers(), "TableView::mouseDoubleClickEvent");
        qDebug() << "TableView::mouseDoubleClickEvent row =" << sfIdx.row();
        emit displayLoupe("TableView::mouseDoubleClickEvent");
        // thumbView->scrollToRow(idx.row(), "TableView::mouseDoubleClickEvent");
    }
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
    if (G::isLogger) G::log("TableView::createOkToShow");
    ok = new QStandardItemModel;

    ok->setHorizontalHeaderItem(0, new QStandardItem(QString("Field")));
    ok->setHorizontalHeaderItem(1, new QStandardItem(QString("Show")));
    ok->setHorizontalHeaderItem(2, new QStandardItem(QString("Geek")));

    // do not include column 0 as it is used to index tableView
    for (int i = 1; i < dm->columnCount(); i++) {
        QString columnName = dm->horizontalHeaderItem(i)->text();
        // qDebug() << "TableView::createOkToShow" << i << columnName;
        ok->insertRow(i - 1);
        ok->setData(ok->index(i - 1, 0), columnName);
        bool isGeek = dm->horizontalHeaderItem(i)->data(G::GeekRole).toBool();
        ok->setData(ok->index(i - 1, 2), isGeek);
        if (G::showAllTableColumns) ok->setData(ok->index(i - 1, 1), true);
        else ok->setData(ok->index(i - 1, 1), !isGeek);
    }
    showOrHide();

    connect(ok, &QStandardItemModel::dataChanged, this, &TableView::showOrHide);
}

void TableView::showOrHide()
{
/*
    The ok datamodel (table fields to show) is edited in the preferences dialog
    and this slot is then signalled to update which fields are visible.
*/
    for (int i = 0; i < ok->rowCount(); i++) {
        bool showField = ok->index(i, 1).data().toBool();
        if (showField) showColumn(i + 1);
        else hideColumn(i + 1);
        /*
        QString s = ok->index(i, 0).data().toString();
        qDebug() << "TableView::showOrHide" << i << s << showField;  //*/
    }
}

//------------------------------------------------------------------------------
//   DELEGATES
//------------------------------------------------------------------------------

RowNumberItemDelegate::RowNumberItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

void RowNumberItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->drawText(option.rect, Qt::AlignCenter | Qt::AlignVCenter, QString::number(index.row() + 1));
    painter->restore();
}

CreatedItemDelegate::CreatedItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString CreatedItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return value.toDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

PickItemDelegate::PickItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString PickItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return value.toString();
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
