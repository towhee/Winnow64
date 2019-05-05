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



//    FileItemDelegate *fileItemDelegate = new FileItemDelegate;
//    setItemDelegateForColumn(G::PathColumn, fileItemDelegate);

    CreatedItemDelegate *createdItemDelegate = new CreatedItemDelegate;
    setItemDelegateForColumn(G::CreatedColumn, createdItemDelegate);

    RefineItemDelegate *refineItemDelegate = new RefineItemDelegate;
    setItemDelegateForColumn(G::RefineColumn, refineItemDelegate);

    PickItemDelegate *pickItemDelegate = new PickItemDelegate;
    setItemDelegateForColumn(G::PickColumn, pickItemDelegate);

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
    firstVisibleRow = rowAt(0);
    midVisibleRow = rowAt(height() / 2);
    lastVisibleRow = rowAt(height());

    int row = currentIndex().row();
    isCurrentVisible = (row >= firstVisibleRow && row <= lastVisibleRow);
}

void TableView::scrollToCurrent()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // G::scrollToCenter needed?
    QModelIndex idx = dm->sf->index(currentIndex().row(), 1);
    scrollTo(idx, ScrollHint::PositionAtCenter);
}

int TableView::sizeHintForColumn(int column) const
{
    QFontMetrics fm(this->font());
    if (column == G::NameColumn) return fm.width("2019-02-25_0001.jpg========");
    if (column == G::RefineColumn) return fm.width("=Refine=");
    if (column == G::PickColumn) return fm.width("=Pick=");
    if (column == G::IngestedColumn) return fm.width("=Ingested=");
    if (column == G::LabelColumn) return fm.width("=Colour=");
    if (column == G::RatingColumn) return fm.width("=Rating=");
    if (column == G::TypeColumn) return fm.width("=Type=");
    if (column == G::SizeColumn) return fm.width("=999,999,999=");
    if (column == G::CreatedColumn) return fm.width("=2019-09-09 09:09:09=");
    if (column == G::ModifiedColumn) return fm.width("=2019-09-09 09:09:09=");
    if (column == G::YearColumn) return fm.width("=2000=");
    if (column == G::DayColumn) return fm.width("=2000-00-00=");
    if (column == G::CreatorColumn) return fm.width("Rory Hill=====");
    if (column == G::MegaPixelsColumn) return fm.width("=999.99=");
    if (column == G::DimensionsColumn) return fm.width("=99999x99999=");
    if (column == G::RotationColumn) return fm.width("=Rot=");
    if (column == G::ApertureColumn) return fm.width("=Aperture=");
    if (column == G::ShutterspeedColumn) return fm.width("=1/8000 sec=");
    if (column == G::ISOColumn) return fm.width("999999");
    if (column == G::CameraMakeColumn) return fm.width("Nikon ========");
    if (column == G::CameraModelColumn) return fm.width("Nikon D850============");
    if (column == G::LensColumn) return fm.width("Lens======================");
    if (column == G::FocalLengthColumn) return fm.width("=Focal length=");
    if (column == G::TitleColumn) return fm.width("=Title=");
    if (column == G::CopyrightColumn) return fm.width("=Copyright=");
    if (column == G::EmailColumn) return fm.width("=Email=");
    if (column == G::UrlColumn) return fm.width("=Url=");
    return 50;
}

bool TableView::eventFilter(QObject *obj, QEvent *event)
{
    if((event->type() == QEvent::Paint || event->type() == QEvent::Timer)
            && readyToScroll
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
    G::source = "TableMouseClick";
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
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    ok = new QStandardItemModel;

    ok->setHorizontalHeaderItem(0, new QStandardItem(QString("Field")));
    ok->setHorizontalHeaderItem(1, new QStandardItem(QString("Show")));

    // do not include column 0 as it is used to index tableView
    for(int i = 1; i < dm->columnCount(); i++) {
        QString columnName = dm->horizontalHeaderItem(i)->text();
        ok->insertRow(i - 1);
        ok->setData(ok->index(i - 1, 0), columnName);
        bool show = i <= G::UrlColumn;
        ok->setData(ok->index(i - 1, 1), show);
        /*
        qDebug() << __FUNCTION__
                 << i
                 << columnName
                 << show;  */
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
