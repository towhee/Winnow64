#include "tableview.h"

TableView::TableView(DataModel *dm, ThumbView *thumbView)
{
/*

*/
    {
    #ifdef ISDEBUG
    qDebug() << "TableView::TableView";
    #endif
    }

    this->thumbView = thumbView;

    setModel(dm->sf);
    setSortingEnabled(true);
    setAlternatingRowColors(true);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setFixedHeight(22);
    horizontalHeader()->setSortIndicatorShown(false);
    verticalHeader()->setVisible(false);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setTabKeyNavigation(false);
    setIconSize(QSize(24,24+12));   // no effect on thumbView scroll issue
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(24);
//    setSelectionModel(thumbView->thumbViewSelection);

    PickItemDelegate *pickItemDelegate = new PickItemDelegate;
    setItemDelegateForColumn(G::PickedColumn, pickItemDelegate);

    ApertureItemDelegate *apertureItemDelegate = new ApertureItemDelegate;
    setItemDelegateForColumn(G::ApertureColumn, apertureItemDelegate);

    ExposureTimeItemDelegate *exposureTimeItemDelegate = new ExposureTimeItemDelegate;
    setItemDelegateForColumn(G::ShutterspeedColumn, exposureTimeItemDelegate);

    FocalLengthItemDelegate *focalLengthItemDelegate = new FocalLengthItemDelegate;
    setItemDelegateForColumn(G::FocalLengthColumn, focalLengthItemDelegate);

    FileSizeItemDelegate *fileSizeItemDelegate = new FileSizeItemDelegate;
    setItemDelegateForColumn(G::SizeColumn, fileSizeItemDelegate);
}

void TableView::mouseDoubleClickEvent(QMouseEvent *event)
{
    {
    #ifdef ISDEBUG
    qDebug() << "TableView::mouseDoubleClickEvent";
    #endif
    }
    QTableView::mouseDoubleClickEvent(event);
    emit displayLoupe();
    // delay reqd
    QTimer::singleShot(100, this, SLOT(delaySelectCurrentThumb()));
}

void TableView::delaySelectCurrentThumb()
{
    {
    #ifdef ISDEBUG
    qDebug() << "TableView::delaySelectCurrentThumb";
    #endif
    }
    thumbView->selectThumb(thumbView->currentIndex());
}

//------------------------------------------------------------------------------
//   DELEGATES
//------------------------------------------------------------------------------

PickItemDelegate::PickItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString PickItemDelegate::displayText(const QVariant& value, const QLocale& locale) const
{
    return (value == "true") ? "✓" : "";
}

ApertureItemDelegate::ApertureItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString ApertureItemDelegate::displayText(const QVariant& value, const QLocale& locale) const
{
    if (value == 0) return QString();

    return "f/" + QString::number(value.toDouble(), 'f', 1);
}

ExposureTimeItemDelegate::ExposureTimeItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString ExposureTimeItemDelegate::displayText(const QVariant& value, const QLocale& locale) const
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

QString FocalLengthItemDelegate::displayText(const QVariant& value, const QLocale& locale) const
{
    if (value == 0)
    return QString();

    return QString::number(value.toDouble(), 'f', 0) + "mm   ";
}

FileSizeItemDelegate::FileSizeItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString FileSizeItemDelegate::displayText(const QVariant& value, const QLocale& locale) const
{
    return QLocale(QLocale::English).toString(value.toDouble(), 'f', 0);
}
