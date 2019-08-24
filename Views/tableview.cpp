#include "Views/tableview.h"

TableView::TableView(DataModel *dm)
{
/*

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }

    this->dm = dm;
    int ht = G::fontSize.toInt();

    setModel(dm->sf);
    setSortingEnabled(true);
    setAlternatingRowColors(true);
    horizontalHeader()->setFixedHeight(22);
    horizontalHeader()->setSortIndicatorShown(false);
    horizontalHeader()->setSectionsMovable(true);
    horizontalHeader()->setStretchLastSection(true);
//    horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    verticalHeader()->setVisible(false);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setTabKeyNavigation(false);
    setWordWrap(false);
    setIconSize(QSize(ht + 10, ht + 22));
//    setIconSize(QSize(24, 24 + 12));
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(24);

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

    DimensionItemDelegate *dimensionItemDelegate = new DimensionItemDelegate;
    setItemDelegateForColumn(G::DimensionsColumn, dimensionItemDelegate);

    ApertureItemDelegate *apertureItemDelegate = new ApertureItemDelegate;
    setItemDelegateForColumn(G::ApertureColumn, apertureItemDelegate);

    ExposureTimeItemDelegate *exposureTimeItemDelegate = new ExposureTimeItemDelegate;
    setItemDelegateForColumn(G::ShutterspeedColumn, exposureTimeItemDelegate);

    ISOItemDelegate *isoItemDelegate = new ISOItemDelegate;
    setItemDelegateForColumn(G::ISOColumn, isoItemDelegate);

    FocalLengthItemDelegate *focalLengthItemDelegate = new FocalLengthItemDelegate;
    setItemDelegateForColumn(G::FocalLengthColumn, focalLengthItemDelegate);

    FileSizeItemDelegate *fileSizeItemDelegate = new FileSizeItemDelegate;
    setItemDelegateForColumn(G::SizeColumn, fileSizeItemDelegate);

    createOkToShow();
}

void TableView::setViewportParameters()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    firstVisibleRow = rowAt(0);
    midVisibleRow = rowAt(height() / 2);
    lastVisibleRow = rowAt(height());

    int row = currentIndex().row();
    isCurrentVisible = (row >= firstVisibleRow && row <= lastVisibleRow);
}

void TableView::scrollToRow(int row, QString source)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << __FUNCTION__ << objectName() << "row =" << row
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QModelIndex idx = dm->sf->index(currentIndex().row(), 1);
    scrollTo(idx, ScrollHint::PositionAtCenter);
}

int TableView::sizeHintForColumn(int column) const
{
    QFontMetrics fm(this->font());
    if (column == G::NameColumn) return fm.boundingRect("2019-02-25_0001.jpg========").width();
    if (column == G::RefineColumn) return fm.boundingRect("=Refine=").width();
    if (column == G::PickColumn) return fm.boundingRect("=Pick=").width();
    if (column == G::IngestedColumn) return fm.boundingRect("=Ingested=").width();
    if (column == G::LabelColumn) return fm.boundingRect("=Colour=").width();
    if (column == G::RatingColumn) return fm.boundingRect("=Rating=").width();
    if (column == G::SearchColumn) return fm.boundingRect("=false=").width();
    if (column == G::TypeColumn) return fm.boundingRect("=JPG+NEF=").width();
    if (column == G::SizeColumn) return fm.boundingRect("=999,999,999=").width();
    if (column == G::CreatedColumn) return fm.boundingRect("=2019-09-09 09:09:09=").width();
    if (column == G::ModifiedColumn) return fm.boundingRect("=2019-09-09 09:09:09=").width();
    if (column == G::YearColumn) return fm.boundingRect("=2000=").width();
    if (column == G::DayColumn) return fm.boundingRect("=2000-00-00=").width();
    if (column == G::CreatorColumn) return fm.boundingRect("Rory Hill=====").width();
    if (column == G::MegaPixelsColumn) return fm.boundingRect("=999.99=").width();
    if (column == G::DimensionsColumn) return fm.boundingRect("=99999x99999=").width();
    if (column == G::RotationColumn) return fm.boundingRect("=Rot=").width();
    if (column == G::ApertureColumn) return fm.boundingRect("=Aperture=").width();
    if (column == G::ShutterspeedColumn) return fm.boundingRect("=1/8000 sec=").width();
    if (column == G::ISOColumn) return fm.boundingRect("999999").width();
    if (column == G::CameraMakeColumn) return fm.boundingRect("Nikon ========").width();
    if (column == G::CameraModelColumn) return fm.boundingRect("Nikon D850============").width();
    if (column == G::LensColumn) return fm.boundingRect("Lens======================").width();
    if (column == G::FocalLengthColumn) return fm.boundingRect("=Focal length==").width();
    if (column == G::TitleColumn) return fm.boundingRect("=Title======================================").width();
    if (column == G::CopyrightColumn) return fm.boundingRect("=Copyright=====").width();
    if (column == G::EmailColumn) return fm.boundingRect("=Email================").width();
    if (column == G::UrlColumn) return fm.boundingRect("=Url=======================").width();
    if (column == G::OffsetFullJPGColumn) return fm.boundingRect("=OffsetFullJPGColumn=").width();
    if (column == G::LengthFullJPGColumn) return fm.boundingRect("=LengthFullJPGColumn=").width();
    if (column == G::OffsetThumbJPGColumn) return fm.boundingRect("=OffsetThumbJPGColumn=").width();
    if (column == G::LengthThumbJPGColumn) return fm.boundingRect("=LengthThumbJPGColumn=").width();
    if (column == G::OffsetSmallJPGColumn) return fm.boundingRect("=OffsetSmallJPGColumn=").width();
    if (column == G::LengthSmallJPGColumn) return fm.boundingRect("=LengthSmallJPGColumn=").width();
    if (column == G::XmpSegmentOffsetColumn) return fm.boundingRect("=XmpSegmentOffsetColumn=").width();
    if (column == G::XmpNextSegmentOffsetColumn) return fm.boundingRect("=XmpNextSegmentOffsetColumn=").width();
    if (column == G::IsXMPColumn) return fm.boundingRect("=IsXMPColumn=").width();
    if (column == G::OrientationOffsetColumn) return fm.boundingRect("=OrientationOffsetColumn=").width();
    if (column == G::OrientationColumn) return fm.boundingRect("=OrientationColumn=").width();
    if (column == G::RotationDegreesColumn) return fm.boundingRect("=RotationDegreesColumn=").width();
    if (column == G::ErrColumn) return fm.boundingRect("=ErrColumn===========================================================").width();
    if (column == G::ShootingInfoColumn) return fm.boundingRect("=ShootingInfoColumn======================").width();
    if (column == G::MetadataLoadedColumn) return fm.boundingRect("=Meta Loaded=").width();
    if (column == G::SearchTextColumn) return fm.boundingRect("=SearchText=====================================================================================").width();
    return 50;
}

void TableView::selectPageUp()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << __FUNCTION__;
    setCurrentIndex(moveCursor(QAbstractItemView::MovePageUp, Qt::NoModifier));
}

void TableView::selectPageDown()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__;
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

void TableView::mousePressEvent(QMouseEvent *event)
{
    // ignore right mouse clicks (context menu)
    if (event->button() == Qt::RightButton) return;
    G::fileSelectionChangeSource = "TableMouseClick";
    QTableView::mousePressEvent(event);
}

void TableView::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    emit displayLoupe();
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
        qDebug() << __FUNCTION__ << i << s << showField;  */
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
    if (value < 1.0) {
        uint t = qRound(1 / value.toDouble());
        exposureTime = "1/" + QString::number(t);
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

FileSizeItemDelegate::FileSizeItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString FileSizeItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return QLocale(QLocale::English).toString(value.toDouble(), 'f', 0);
}
