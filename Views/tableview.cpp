#include "Views/tableview.h"
#include "Main/mainwindow.h"

extern MW *m5;
MW *m5;

TableView::TableView(QWidget *parent, DataModel *dm)
{
    if (isDebug || G::isLogger) G::log("TableView::TableView");
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
    setItemDelegateForColumn(G::ByteSizeColumn, fileSizeItemDelegate);
    setItemDelegateForColumn(G::NSThumbColumn, fileSizeItemDelegate);
    setItemDelegateForColumn(G::NSImageColumn, fileSizeItemDelegate);

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

    isDebug = false;
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
    /* Do NOT write dm->scrollToIcon here. updateVisible runs from layout/show paths
       (resizeEvent, MW::updateIconRange) while the table may sit at a stale or zeroed
       scroll position, which would clobber the shared scroll anchor before a mode switch
       reads it. Only the user-scroll handler MW::tableHasScrolled updates scrollToIcon,
       mirroring how IconView writes it solely from updateMidVisibleCell. */
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
    // defaultColumnWidth[G::PathColumn] = (row >= firstVisibleRow && row <= lastVisibleRow);
    return (row >= firstVisibleRow && row <= lastVisibleRow);
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

void TableView::ensureRowVisible(int row, QString source)
/*
    Minimal vertical scroll to bring row into view (used to follow a shift-extended
    selection so the row just beyond the selection edge becomes visible). Unlike
    scrollToRow, which centers the row, EnsureVisible only scrolls far enough to expose
    the row at the viewport edge. The horizontal position is preserved.
*/
{
    if (isDebug || G::isLogger) G::log("TableView::ensureRowVisible", source);
    QModelIndex sfIdx = dm->sf->index(row, frozenColumns);
    if (!sfIdx.isValid()) return;
    int hPos = horizontalScrollBar()->value();
    G::ignoreScrollSignal = true;
    scrollTo(sfIdx, QAbstractItemView::EnsureVisible);
    horizontalScrollBar()->setValue(hPos);
}

void TableView::scrollToCurrent()
{
    if (isDebug || G::isLogger) G::log("TableView::scrollToCurrent");

    // QModelIndex sfIdx = dm->sf->index(currentIndex().row(), 1);
    // G::ignoreScrollSignal = true;
    // scrollTo(sfIdx, ScrollHint::PositionAtCenter);

    int hPos = horizontalScrollBar()->value();
    QModelIndex sfIdx = dm->sf->index(currentIndex().row(), frozenColumns);
    G::ignoreScrollSignal = true;
    scrollTo(sfIdx, ScrollHint::PositionAtCenter);
    horizontalScrollBar()->setValue(hPos);
}

void TableView::setDefaultColumnWidths()
{
    // if (isDebug || G::isLogger) G::log("TableView::defaultColumnWidth", QString::number(column);
    QFontMetrics fm(this->font());

    for (int column = 0; column < G::TotalColumns; ++column) {
        defaultColumnWidth[column] = 50;
    }

    defaultColumnWidth[G::PathColumn] = fm.boundingRect("-Icon-").width();
    defaultColumnWidth[G::RowNumberColumn] = fm.boundingRect("-999999-").width();
    defaultColumnWidth[G::NameColumn] = fm.boundingRect("2019-02-25_0001.jpg========").width();
    defaultColumnWidth[G::FolderNameColumn] = fm.boundingRect("This is the folder name========").width();
    defaultColumnWidth[G::NSThumbColumn] = fm.boundingRect("=NS Thumb=").width();
    defaultColumnWidth[G::NSImageColumn] = fm.boundingRect("==NS Image==").width();
    defaultColumnWidth[G::PickColumn] = fm.boundingRect("===Pick===").width();
    defaultColumnWidth[G::IngestedColumn] = fm.boundingRect("=Ingested=").width();
    defaultColumnWidth[G::LabelColumn] = fm.boundingRect("=Colour=").width();
    defaultColumnWidth[G::RatingColumn] = fm.boundingRect("=Rating=").width();
    defaultColumnWidth[G::SearchColumn] = fm.boundingRect("=false=").width();
    defaultColumnWidth[G::TypeColumn] = fm.boundingRect("=JPG+NEF=").width();
    defaultColumnWidth[G::VideoColumn] = fm.boundingRect("=Video=").width();
    defaultColumnWidth[G::SidecarColumn] = fm.boundingRect("=Sidecar=").width();
    defaultColumnWidth[G::ApertureColumn] = fm.boundingRect("=Aperture=").width();
    defaultColumnWidth[G::ShutterspeedColumn] = fm.boundingRect("=1/8000 sec=").width();
    defaultColumnWidth[G::ISOColumn] = fm.boundingRect("999999").width();
    defaultColumnWidth[G::ExposureCompensationColumn] = fm.boundingRect("==-2 EV===").width();
    defaultColumnWidth[G::DurationColumn] = fm.boundingRect("=Duration=").width();
    defaultColumnWidth[G::CameraMakeColumn] = fm.boundingRect("Nikon ========").width();
    defaultColumnWidth[G::CameraModelColumn] = fm.boundingRect("Nikon D850============").width();
    defaultColumnWidth[G::LensColumn] = fm.boundingRect("Lens======================").width();
    defaultColumnWidth[G::FocalLengthColumn] = fm.boundingRect("=Focal length==").width();
    defaultColumnWidth[G::FocusXColumn] = fm.boundingRect("=FocusX==").width();
    defaultColumnWidth[G::FocusYColumn] = fm.boundingRect("=FocusY==").width();
    defaultColumnWidth[G::GPSCoordColumn] = fm.boundingRect("=49°13'13.477  N 123°57'22.22 W=").width();
    defaultColumnWidth[G::ByteSizeColumn] = fm.boundingRect("=999,999,999=").width();
    defaultColumnWidth[G::WidthColumn] = fm.boundingRect("=Width=").width();
    defaultColumnWidth[G::HeightColumn] = fm.boundingRect("=Height=").width();
    defaultColumnWidth[G::ModifiedColumn] = fm.boundingRect("=2019-09-09 09:09:09=").width();
    defaultColumnWidth[G::CreatedColumn] = fm.boundingRect("=2019-09-09 09:09:09.999=").width();
    defaultColumnWidth[G::YearColumn] = fm.boundingRect("=2000=").width();
    defaultColumnWidth[G::DayColumn] = fm.boundingRect("=2000-00-00=").width();
    defaultColumnWidth[G::CreatorColumn] = fm.boundingRect("Rory Hill=====").width();
    defaultColumnWidth[G::MegaPixelsColumn] = fm.boundingRect("=999.99=").width();
    defaultColumnWidth[G::LoadMsecPerMpColumn] = fm.boundingRect("=Msec/Mp=").width();
    defaultColumnWidth[G::DimensionsColumn] = fm.boundingRect("=99999x99999=").width();
    defaultColumnWidth[G::AspectRatioColumn] = fm.boundingRect("=Aspect Ratio=").width();
    defaultColumnWidth[G::IconAspectRatioColumn] = fm.boundingRect("=Icon Aspect Ratio=").width();
    defaultColumnWidth[G::OrientationColumn] = fm.boundingRect("=Orientation=").width();
    defaultColumnWidth[G::RotationColumn] = fm.boundingRect("=Rot=").width();
    defaultColumnWidth[G::CopyrightColumn] = fm.boundingRect("=Copyright=====").width();
    defaultColumnWidth[G::TitleColumn] = fm.boundingRect("=Title=========================").width();
    defaultColumnWidth[G::EmailColumn] = fm.boundingRect("=Email================").width();
    defaultColumnWidth[G::UrlColumn] = fm.boundingRect("=Url=======================").width();
    defaultColumnWidth[G::KeywordsColumn] = fm.boundingRect("=keyword, keyword, keyword, keyword=").width();
    defaultColumnWidth[G::MetadataReadingColumn] = fm.boundingRect("=Metadata Reading=").width();
    defaultColumnWidth[G::MetadataStatusColumn] = fm.boundingRect("=Meta Status=").width();
    defaultColumnWidth[G::IconLoadedColumn] = fm.boundingRect("=Icon Loaded=").width();
    defaultColumnWidth[G::CompareColumn] = fm.boundingRect("=Compare=").width();
    defaultColumnWidth[G::_RatingColumn] = fm.boundingRect("=_Rating=").width();
    defaultColumnWidth[G::_LabelColumn] = fm.boundingRect("=_Label=").width();
    defaultColumnWidth[G::_CreatorColumn] = fm.boundingRect("=_Creator=").width();
    defaultColumnWidth[G::_TitleColumn] = fm.boundingRect("=_Title=========================").width();
    defaultColumnWidth[G::_CopyrightColumn] = fm.boundingRect("=_Copyright=====").width();
    defaultColumnWidth[G::_EmailColumn] = fm.boundingRect("=Email================").width();
    defaultColumnWidth[G::_UrlColumn] = fm.boundingRect("=Url=======================").width();
    defaultColumnWidth[G::PermissionsColumn] = fm.boundingRect("=Permissions=").width();
    defaultColumnWidth[G::ReadWriteColumn] = fm.boundingRect("=false=").width();
    defaultColumnWidth[G::OffsetFullColumn] = fm.boundingRect("=OffsetFullColumn=").width();
    defaultColumnWidth[G::LengthFullColumn] = fm.boundingRect("=LengthFullColumn=").width();
    defaultColumnWidth[G::WidthPreviewColumn] = fm.boundingRect("=WidthPreview=").width();
    defaultColumnWidth[G::HeightPreviewColumn] = fm.boundingRect("=HeightPreview=").width();
    defaultColumnWidth[G::OffsetThumbColumn] = fm.boundingRect("=OffsetThumbColumn=").width();
    defaultColumnWidth[G::LengthThumbColumn] = fm.boundingRect("=LengthThumbColumn=").width();
    defaultColumnWidth[G::samplesPerPixelColumn] = fm.boundingRect("=samplesPerPixelFullColumn=").width();
    defaultColumnWidth[G::isBigEndianColumn] = fm.boundingRect("=isBigEndian=").width();
    defaultColumnWidth[G::ifd0OffsetColumn] = fm.boundingRect("=ifd0OffsetColumn=").width();
    defaultColumnWidth[G::ifdOffsetsColumn] = fm.boundingRect("=ifdOffsetColumn=").width();
    defaultColumnWidth[G::XmpSegmentOffsetColumn] = fm.boundingRect("=XmpSegmentOffsetColumn=").width();
    defaultColumnWidth[G::XmpSegmentLengthColumn] = fm.boundingRect("=XmpSegmentLengthColumn=").width();
    defaultColumnWidth[G::IsXMPColumn] = fm.boundingRect("=IsXMPColumn=").width();
    defaultColumnWidth[G::ICCSegmentOffsetColumn] = fm.boundingRect("=ICCSegmentOffsetColumn=").width();
    defaultColumnWidth[G::ICCSegmentLengthColumn] = fm.boundingRect("=ICCSegmentLengthColumn=").width();
    defaultColumnWidth[G::ICCBufColumn] = fm.boundingRect("=ICCBuf=").width();
    defaultColumnWidth[G::ICCSpaceColumn] = fm.boundingRect("=ICCSpaceColumn=").width();

    defaultColumnWidth[G::CacheSizeColumn] = fm.boundingRect("=CacheMB=").width();
    defaultColumnWidth[G::IsCachingColumn] = fm.boundingRect("=IsCaching=").width();
    defaultColumnWidth[G::IsCachedColumn] = fm.boundingRect("=IsCached=").width();
    defaultColumnWidth[G::AttemptsColumn] = fm.boundingRect("=Attempts=").width();
    defaultColumnWidth[G::DecoderIdColumn] = fm.boundingRect("=Decoder ID=").width();
    // defaultColumnWidth[G::DecoderStatusColumn] = fm.boundingRect("=Status=").width();
    defaultColumnWidth[G::DecoderErrMsgColumn] = fm.boundingRect("=Decoder Error Message==============================").width();

    defaultColumnWidth[G::OrientationOffsetColumn] = fm.boundingRect("=OrientationOffsetColumn=").width();
    defaultColumnWidth[G::RotationDegreesColumn] = fm.boundingRect("=RotationDegreesColumn=").width();
    defaultColumnWidth[G::ShootingInfoColumn] = fm.boundingRect("=ShootingInfoColumn======================").width();
    defaultColumnWidth[G::SearchTextColumn] = fm.boundingRect("=SearchText=====================================================================================").width();
    defaultColumnWidth[G::ErrColumn] = fm.boundingRect("==ErrText=========================================================").width();
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

    When the datamodel is cleared this must be rerun.  MW::folderChangeCompleted
    signals this slot for this reason.
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
