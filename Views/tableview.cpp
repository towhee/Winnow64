#include "Views/tableview.h"
#include "Main/mainwindow.h"

extern MW *m5;
MW *m5;

TableView::TableView(QWidget *parent, DataModel *dm)
{
    if (isDebug || G::isLogger) G::log("TableView::TableView");
    qDebug() << "TableView::TableView";
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
    verticalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setTabKeyNavigation(false);
    setWordWrap(false);

    verticalScrollBar()->setObjectName("TableViewVerticalScrollBar");

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

    // Setup frozenView
    frozenView = new QTableView(this);
    frozenView->setModel(dm->sf);  // Same model
    frozenView->setSelectionModel(dm->selectionModel);
    frozenView->setSortingEnabled(true); // rory
    frozenView->setAlternatingRowColors(true); // rory
    frozenView->setFocusPolicy(Qt::NoFocus);
    frozenView->verticalHeader()->hide();
    frozenView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    frozenView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    frozenView->setSelectionModel(selectionModel());
    frozenView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    frozenView->setSelectionBehavior(QAbstractItemView::SelectRows);
    frozenView->setTabKeyNavigation(false);
    frozenView->setWordWrap(false);
    frozenView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frozenView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frozenView->setFrameStyle(QFrame::NoFrame);

    // Ensure row heights are synced
    connect(verticalScrollBar(), &QScrollBar::valueChanged, frozenView->verticalScrollBar(), &QScrollBar::setValue);
    connect(frozenView->verticalScrollBar(), &QScrollBar::valueChanged, verticalScrollBar(), &QScrollBar::setValue);

    RowNumberItemDelegate *rowNumberItemDelegate = new RowNumberItemDelegate;
    frozenView->setItemDelegateForColumn(G::RowNumberColumn, rowNumberItemDelegate);

    // assign default column widths (only do this once)
    setDefaultColumnWidths();

    // how many columns to freeze
    frozenColumns = 2;

    // resize column widths to defaults
    resizeColumns();

    // Keep geometry aligned
    updateFrozenViewGeometry();

    isDebug = true;
}

void TableView::updateVisible(QString src)
{
    if (isDebug || G::isLogger) G::log("TableView::updateVisible");
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
    if (isDebug || G::isLogger) G::log("TableView::isRowVisible");
    defaultColumnWidth[G::PathColumn] = (row >= firstVisibleRow && row <= lastVisibleRow);
}

void TableView::scrollToRow(int row, QString source)
{
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("TableView::scrollToRow", source);
    /* debug
    qDebug() << "TableView::scrollToRow" << objectName() << "row =" << row
             << "source =" << source;
             //*/
    QModelIndex sfIdx = dm->sf->index(row, 0);
    scrollTo(sfIdx, QAbstractItemView::PositionAtCenter);
}

void TableView::scrollToCurrent()
{
    if (isDebug || G::isLogger) G::log("TableView::scrollToCurrent");
    QModelIndex sfIdx = dm->sf->index(currentIndex().row(), 1);
    G::ignoreScrollSignal = true;
    scrollTo(sfIdx, ScrollHint::PositionAtCenter);
}

void TableView::setDefaultColumnWidths()
{
    // if (isDebug || G::isLogger) G::log("TableView::defaultColumnWidth", QString::number(column);
    QFontMetrics fm(this->font());

    for (int column = 0; column < G::TotalColumns; ++column) {
        if (column == G::PathColumn) defaultColumnWidth[column] = fm.boundingRect("-Icon-").width();
        else if (column == G::RowNumberColumn) defaultColumnWidth[column] = fm.boundingRect("-999999-").width();
        else if (column == G::NameColumn) defaultColumnWidth[column] = fm.boundingRect("2019-02-25_0001.jpg========").width();
        else if (column == G::FolderNameColumn) defaultColumnWidth[column] = fm.boundingRect("This is the folder name========").width();
        else if (column == G::MSToReadColumn) defaultColumnWidth[column] = fm.boundingRect("=Read ms=").width();
        else if (column == G::PickColumn) defaultColumnWidth[column] = fm.boundingRect("===Pick===").width();
        else if (column == G::IngestedColumn) defaultColumnWidth[column] = fm.boundingRect("=Ingested=").width();
        else if (column == G::LabelColumn) defaultColumnWidth[column] = fm.boundingRect("=Colour=").width();
        else if (column == G::RatingColumn) defaultColumnWidth[column] = fm.boundingRect("=Rating=").width();
        else if (column == G::SearchColumn) defaultColumnWidth[column] = fm.boundingRect("=false=").width();
        else if (column == G::TypeColumn) defaultColumnWidth[column] = fm.boundingRect("=JPG+NEF=").width();
        else if (column == G::VideoColumn) defaultColumnWidth[column] = fm.boundingRect("=Video=").width();
        else if (column == G::ApertureColumn) defaultColumnWidth[column] = fm.boundingRect("=Aperture=").width();
        else if (column == G::ShutterspeedColumn) defaultColumnWidth[column] = fm.boundingRect("=1/8000 sec=").width();
        else if (column == G::ISOColumn) defaultColumnWidth[column] = fm.boundingRect("999999").width();
        else if (column == G::ExposureCompensationColumn) defaultColumnWidth[column] = fm.boundingRect("==-2 EV===").width();
        else if (column == G::DurationColumn) defaultColumnWidth[column] = fm.boundingRect("=Duration=").width();
        else if (column == G::CameraMakeColumn) defaultColumnWidth[column] = fm.boundingRect("Nikon ========").width();
        else if (column == G::CameraModelColumn) defaultColumnWidth[column] = fm.boundingRect("Nikon D850============").width();
        else if (column == G::LensColumn) defaultColumnWidth[column] = fm.boundingRect("Lens======================").width();
        else if (column == G::FocalLengthColumn) defaultColumnWidth[column] = fm.boundingRect("=Focal length==").width();
        else if (column == G::FocusXColumn) defaultColumnWidth[column] = fm.boundingRect("=FocusX==").width();
        else if (column == G::FocusYColumn) defaultColumnWidth[column] = fm.boundingRect("=FocusY==").width();
        else if (column == G::GPSCoordColumn) defaultColumnWidth[column] = fm.boundingRect("=49°13'13.477  N 123°57'22.22 W=").width();
        else if (column == G::SizeColumn) defaultColumnWidth[column] = fm.boundingRect("=999,999,999=").width();
        else if (column == G::WidthColumn) defaultColumnWidth[column] = fm.boundingRect("=Width=").width();
        else if (column == G::HeightColumn) defaultColumnWidth[column] = fm.boundingRect("=Height=").width();
        else if (column == G::ModifiedColumn) defaultColumnWidth[column] = fm.boundingRect("=2019-09-09 09:09:09=").width();
        else if (column == G::CreatedColumn) defaultColumnWidth[column] = fm.boundingRect("=2019-09-09 09:09:09.999=").width();
        else if (column == G::YearColumn) defaultColumnWidth[column] = fm.boundingRect("=2000=").width();
        else if (column == G::DayColumn) defaultColumnWidth[column] = fm.boundingRect("=2000-00-00=").width();
        else if (column == G::CreatorColumn) defaultColumnWidth[column] = fm.boundingRect("Rory Hill=====").width();
        else if (column == G::MegaPixelsColumn) defaultColumnWidth[column] = fm.boundingRect("=999.99=").width();
        else if (column == G::LoadMsecPerMpColumn) defaultColumnWidth[column] = fm.boundingRect("=Msec/Mp=").width();
        else if (column == G::DimensionsColumn) defaultColumnWidth[column] = fm.boundingRect("=99999x99999=").width();
        else if (column == G::AspectRatioColumn) defaultColumnWidth[column] = fm.boundingRect("=Aspect Ratio=").width();
        else if (column == G::OrientationColumn) defaultColumnWidth[column] = fm.boundingRect("=Orientation=").width();
        else if (column == G::RotationColumn) defaultColumnWidth[column] = fm.boundingRect("=Rot=").width();
        else if (column == G::CopyrightColumn) defaultColumnWidth[column] = fm.boundingRect("=Copyright=====").width();
        else if (column == G::TitleColumn) defaultColumnWidth[column] = fm.boundingRect("=Title=========================").width();
        else if (column == G::EmailColumn) defaultColumnWidth[column] = fm.boundingRect("=Email================").width();
        else if (column == G::UrlColumn) defaultColumnWidth[column] = fm.boundingRect("=Url=======================").width();
        else if (column == G::KeywordsColumn) defaultColumnWidth[column] = fm.boundingRect("=keyword, keyword, keyword, keyword=").width();
        else if (column == G::MetadataReadingColumn) defaultColumnWidth[column] = fm.boundingRect("=Metadata Reading=").width();
        else if (column == G::MetadataAttemptedColumn) defaultColumnWidth[column] = fm.boundingRect("=Metadata Attempted=").width();
        else if (column == G::MetadataLoadedColumn) defaultColumnWidth[column] = fm.boundingRect("=Meta Loaded=").width();
        else if (column == G::IconLoadedColumn) defaultColumnWidth[column] = fm.boundingRect("=Icon Loaded=").width();
        else if (column == G::MissingThumbColumn) defaultColumnWidth[column] = fm.boundingRect("=Missing Thumb=").width();
        else if (column == G::CompareColumn) defaultColumnWidth[column] = fm.boundingRect("=Compare=").width();
        else if (column == G::_RatingColumn) defaultColumnWidth[column] = fm.boundingRect("=_Rating=").width();
        else if (column == G::_LabelColumn) defaultColumnWidth[column] = fm.boundingRect("=_Label=").width();
        else if (column == G::_CreatorColumn) defaultColumnWidth[column] = fm.boundingRect("=_Creator=").width();
        else if (column == G::_TitleColumn) defaultColumnWidth[column] = fm.boundingRect("=_Title=========================").width();
        else if (column == G::_CopyrightColumn) defaultColumnWidth[column] = fm.boundingRect("=_Copyright=====").width();
        else if (column == G::_EmailColumn) defaultColumnWidth[column] = fm.boundingRect("=Email================").width();
        else if (column == G::_UrlColumn) defaultColumnWidth[column] = fm.boundingRect("=Url=======================").width();
        else if (column == G::PermissionsColumn) defaultColumnWidth[column] = fm.boundingRect("=Permissions=").width();
        else if (column == G::ReadWriteColumn) defaultColumnWidth[column] = fm.boundingRect("=false=").width();
        else if (column == G::OffsetFullColumn) defaultColumnWidth[column] = fm.boundingRect("=OffsetFullColumn=").width();
        else if (column == G::LengthFullColumn) defaultColumnWidth[column] = fm.boundingRect("=LengthFullColumn=").width();
        else if (column == G::WidthPreviewColumn) defaultColumnWidth[column] = fm.boundingRect("=WidthPreview=").width();
        else if (column == G::HeightPreviewColumn) defaultColumnWidth[column] = fm.boundingRect("=HeightPreview=").width();
        else if (column == G::OffsetThumbColumn) defaultColumnWidth[column] = fm.boundingRect("=OffsetThumbColumn=").width();
        else if (column == G::LengthThumbColumn) defaultColumnWidth[column] = fm.boundingRect("=LengthThumbColumn=").width();
        else if (column == G::samplesPerPixelColumn) defaultColumnWidth[column] = fm.boundingRect("=samplesPerPixelFullColumn=").width();
        else if (column == G::isBigEndianColumn) defaultColumnWidth[column] = fm.boundingRect("=isBigEndian=").width();
        else if (column == G::ifd0OffsetColumn) defaultColumnWidth[column] = fm.boundingRect("=else ifd0OffsetColumn=").width();
        else if (column == G::ifdOffsetsColumn) defaultColumnWidth[column] = fm.boundingRect("=else ifdOffsetColumn=").width();
        else if (column == G::XmpSegmentOffsetColumn) defaultColumnWidth[column] = fm.boundingRect("=XmpSegmentOffsetColumn=").width();
        else if (column == G::XmpSegmentLengthColumn) defaultColumnWidth[column] = fm.boundingRect("=XmpSegmentLengthColumn=").width();
        else if (column == G::IsXMPColumn) defaultColumnWidth[column] = fm.boundingRect("=IsXMPColumn=").width();
        else if (column == G::ICCSegmentOffsetColumn) defaultColumnWidth[column] = fm.boundingRect("=ICCSegmentOffsetColumn=").width();
        else if (column == G::ICCSegmentLengthColumn) defaultColumnWidth[column] = fm.boundingRect("=ICCSegmentLengthColumn=").width();
        else if (column == G::ICCBufColumn) defaultColumnWidth[column] = fm.boundingRect("=ICCBuf=").width();
        else if (column == G::ICCSpaceColumn) defaultColumnWidth[column] = fm.boundingRect("=ICCSpaceColumn=").width();

        else if (column == G::CacheSizeColumn) defaultColumnWidth[column] = fm.boundingRect("=CacheMB=").width();
        else if (column == G::IsCachingColumn) defaultColumnWidth[column] = fm.boundingRect("=IsCaching=").width();
        else if (column == G::IsCachedColumn) defaultColumnWidth[column] = fm.boundingRect("=IsCached=").width();
        else if (column == G::AttemptsColumn) defaultColumnWidth[column] = fm.boundingRect("=Attempts=").width();
        else if (column == G::DecoderIdColumn) defaultColumnWidth[column] = fm.boundingRect("=Decoder ID=").width();
        // else if (column == G::DecoderdefaultColumnWidth[G::PathColumn] =StatusColumn) defaultColumnWidth[G::PathColumn] = fm.boundingRect("=Status=").width();
        else if (column == G::DecoderErrMsgColumn) defaultColumnWidth[column] = fm.boundingRect("=Decoder Error Message==============================").width();

        else if (column == G::OrientationOffsetColumn) defaultColumnWidth[column] = fm.boundingRect("=OrientationOffsetColumn=").width();
        else if (column == G::RotationDegreesColumn) defaultColumnWidth[column] = fm.boundingRect("=RotationDegreesColumn=").width();
        else if (column == G::IconSymbolColumn) defaultColumnWidth[column] = fm.boundingRect("=IconSymbolColumn=").width();
        else if (column == G::ShootingInfoColumn) defaultColumnWidth[column] = fm.boundingRect("=ShootingInfoColumn======================").width();
        else if (column == G::SearchTextColumn) defaultColumnWidth[column] = fm.boundingRect("=SearchText=====================================================================================").width();
        else if (column == G::ErrColumn) defaultColumnWidth[column] = fm.boundingRect("==ErrText=========================================================").width();
        else defaultColumnWidth[column] = 50;
    }
}

void TableView::onHorizontalScrollBarChanged(int value)
{
    // not working to scroll left, need to use double tableview method
    qDebug() << "TableView::onHorizontalScrollBarChanged" << value;
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

void TableView::updateFrozenViewGeometry()
{
    if (isDebug || G::isLogger) G::log("TableView::updateFrozenViewGeometry");
    int frozenWidth = 0;
    for (int column = 0; column < frozenColumns; column++)
        frozenWidth += frozenView->columnWidth(column);

    frozenView->setGeometry(verticalHeader()->width() + frameWidth(),
                            frameWidth(),
                            frozenWidth,
                            viewport()->height() + horizontalHeader()->height());
}

void TableView::setFrozenModel(QAbstractItemModel *model) // not being used
{
    if (isDebug || G::isLogger) G::log("TableView::setFrozenModel");
    setModel(model);
    frozenView->setModel(model);

    // Sync selection models again
    frozenView->setSelectionModel(dm->selectionModel);

    // Hide non-frozen columns again
    for (int col = 3; col < model->columnCount(); ++col)
        frozenView->setColumnHidden(col, true);

    updateFrozenViewGeometry();
}

void TableView::resizeColumns()
{
    if (isDebug || G::isLogger) G::log("TableView::resizeColumns");
    // qDebug() << "TableView::resizeColumns";

    for (int column = 0; column < frozenColumns; ++column) {
        // setColumnWidth(column, 0);
        setColumnWidth(column, defaultColumnWidth[column]);
        // setColumnHidden(column, true);
        frozenView->setColumnWidth(column, defaultColumnWidth[column]);
    }
    for (int column = frozenColumns; column < G::TotalColumns; ++column) {
        resizeColumnToContents(column);
        // int defaultWidth = defaultColumnWidth[column];
        // int w = columnWidth(column);
        // int w;
        // defaultWidth > currentWidth ? w = defaultWidth : w + currentWidth;
        // if (columnWidth(column) > 80) setColumnWidth(column, 80);
        // setColumnWidth(column, w);
        // // frozenView->setColumnWidth(column, defaultColumnWidth[column]);
        frozenView->setColumnHidden(column, true);
    }

    updateFrozenViewGeometry();
}

void TableView::resizeEvent(QResizeEvent *event)
{
    if (isDebug || G::isLogger) G::log("TableView::resizeEvent");
    QTableView::resizeEvent(event);
    updateFrozenViewGeometry();
    updateVisible("TableView::resizeEvent");
}

void TableView::paintEvent(QPaintEvent *event)
{
//    resizeColumns();      // prevents user changing column widths
    int d = static_cast<int>(G::strFontSize.toInt() * G::ptToPx * 1.5);
    setIconSize(QSize(d, d));
    frozenView->setIconSize(QSize(d, d));
    verticalHeader()->setDefaultSectionSize(d);
    frozenView->verticalHeader()->setDefaultSectionSize(d);
    horizontalHeader()->setFixedHeight(d);
    frozenView->horizontalHeader()->setFixedHeight(d);
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

void TableView::test() {
    for (int c = 0; c < G::TotalColumns; ++c) {
        QString colHeader = model()->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
        qDebug().noquote() << c << colHeader << defaultColumnWidth[c] << columnWidth(c);
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
