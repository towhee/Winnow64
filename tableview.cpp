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
    this->dm = dm;

    setModel(dm->sf);
    setSortingEnabled(true);
    setAlternatingRowColors(true);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setFixedHeight(22);
    horizontalHeader()->setSortIndicatorShown(false);
    horizontalHeader()->setSectionsMovable(true);
    verticalHeader()->setVisible(false);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setTabKeyNavigation(false);
    setIconSize(QSize(24, 24 + 12));
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(24);

    RefineItemDelegate *refineItemDelegate = new RefineItemDelegate;
    setItemDelegateForColumn(G::RefineColumn, refineItemDelegate);

    PickItemDelegate *pickItemDelegate = new PickItemDelegate;
    setItemDelegateForColumn(G::PickColumn, pickItemDelegate);

    ApertureItemDelegate *apertureItemDelegate = new ApertureItemDelegate;
    setItemDelegateForColumn(G::ApertureColumn, apertureItemDelegate);

    ExposureTimeItemDelegate *exposureTimeItemDelegate = new ExposureTimeItemDelegate;
    setItemDelegateForColumn(G::ShutterspeedColumn, exposureTimeItemDelegate);

    FocalLengthItemDelegate *focalLengthItemDelegate = new FocalLengthItemDelegate;
    setItemDelegateForColumn(G::FocalLengthColumn, focalLengthItemDelegate);

    FileSizeItemDelegate *fileSizeItemDelegate = new FileSizeItemDelegate;
    setItemDelegateForColumn(G::SizeColumn, fileSizeItemDelegate);

    createOkToShow();
}

void TableView::scrollToCurrent()
{
    {
    #ifdef ISDEBUG
    qDebug() << "TableView::scrollToCurrent";
    #endif
    }
    QModelIndex idx = dm->sf->index(currentIndex().row(), 1);
    scrollTo(idx, ScrollHint::PositionAtCenter);
//    qDebug() << "TableView::scrollToCurrent" << idx;
}

bool TableView::eventFilter(QObject *obj, QEvent *event)
{
/*

*/
//    qDebug() << "TableView events" << obj << event;
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
    G::lastThumbChangeEvent = "MouseClick";    // either KeyStroke or MouseClick
    QTableView::mousePressEvent(event);
}

void TableView::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
    {
    #ifdef ISDEBUG
    qDebug() << "TableView::mouseDoubleClickEvent";
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
    qDebug() << "TableView::createOkToShow()";
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
        ok->setData(ok->index(i - 1, 1), true);
    }

//    ok->setData(ok->index(2, 1), false);    // test

    connect(ok, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
            this, SLOT(showOrHide()));
}

void TableView::showOrHide()
{
//    qDebug() << "TableView::showOrHide";
    for(int i = 0; i < ok->rowCount(); i++) {
        bool showField = ok->index(i, 1).data().toBool();
        if (showField) showColumn(i + 1);
        else hideColumn(i + 1);
    }
}

//------------------------------------------------------------------------------
//   DELEGATES
//------------------------------------------------------------------------------

RefineItemDelegate::RefineItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString RefineItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return (value.toBool()) ? "true" : "";
}

PickItemDelegate::PickItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString PickItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return (value == "true") ? "✓" : "";
}

ApertureItemDelegate::ApertureItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString ApertureItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    if (value == 0) return QString();

    return "f/" + QString::number(value.toDouble(), 'f', 1);
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

    return QString::number(value.toDouble(), 'f', 0) + "mm   ";
}

FileSizeItemDelegate::FileSizeItemDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QString FileSizeItemDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
    return QLocale(QLocale::English).toString(value.toDouble(), 'f', 0);
}
