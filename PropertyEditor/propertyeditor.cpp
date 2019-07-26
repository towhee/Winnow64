#include "propertyeditor.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include <QDebug>

PropertyEditor::PropertyEditor(QWidget *parent) : QTreeView(parent)
{
    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::AllEditTriggers);
    indentation = 15;
    setIndentation(indentation);
    model = new QStandardItemModel;
    setModel(model);
    propertyDelegate = new PropertyDelegate(this);
    setItemDelegate(propertyDelegate);
    styleOptionViewItem = new QStyleOptionViewItem;
    connect(propertyDelegate, &PropertyDelegate::editorWidgetToDisplay,
            this, &PropertyEditor::editorWidgetToDisplay);

    connect(propertyDelegate, &PropertyDelegate::drawBranchesAgain, this, &PropertyEditor::drawBranches);
}


void PropertyEditor::editorWidgetToDisplay(QModelIndex idx, QWidget *editor)
{
    setIndexWidget(idx, editor);
    emit propertyDelegate->closeEditor(editor);
}

void PropertyEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) return;
    QTreeView::mousePressEvent(event);
    if (event->modifiers() == Qt::NoModifier) {
        QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) {
            // setCurrentIndex for the value cell (col 1) if user clicked on the caption cell (col 0)
            QModelIndex idxVal = model->index(idx.row(), 1, idx.parent());
            setCurrentIndex(idxVal);

            // try to expand / collapse if click on any part of the row
            // might have clicked on column 1
            if (idx.column() != 0) idx = model->index(idx.row(), 0, idx.parent());
            QPoint p = event->pos();
            if (p.x() >= indentation) isExpanded(idx) ? collapse(idx) : expand(idx);
        }
    }
}

void PropertyEditor::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        qDebug() << __FUNCTION__ << "Esc key";
    }
}

//void PropertyEditor::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
//{

//    QTreeView::drawRow(painter, option, index);
/*
    bool isAlt = option.features & QStyleOptionViewItem::Alternate;
    qDebug() << __FUNCTION__ << isAlt;
    bool isAlternateRow = (option.features & QStyleOptionViewItem::Alternate);

    if ( const QStyleOptionViewItem* opt = qstyleoption_cast<const QStyleOptionViewItem*>(&option) )
    {
        if (opt->features & QStyleOptionViewItem::Alternate)
            painter->fillRect(option.rect, option.palette.alternateBase());
        else
            QTreeView::drawRow(painter, option, index);
            painter->fillRect(option.rect,painter->background());
    }

    QRect r = option.rect;
    QLinearGradient categoryBackground;
    categoryBackground.setStart(0, r.top());
    categoryBackground.setFinalStop(0, r.bottom());
    categoryBackground.setColorAt(0, QColor(88,88,88));
    categoryBackground.setColorAt(1, QColor(66,66,66));
    QBrush brush(categoryBackground);

    QStyleOptionViewItem newOption(option);
    option.palette.alternateBase();
    newOption.palette.setColor( QPalette::Base, QColor(177,177,77) );
    newOption.palette.setColor( QPalette::AlternateBase, QColor(177,77,77) );

    qDebug() << __FUNCTION__ << "option" << "Alternate" << option.palette.alternateBase()
                << "newption" << "Alternate" << option.palette.alternateBase();


    if (index.parent() == QModelIndex())
    {
      QTreeView::drawRow(painter, newOption, index);
    }
    else
    {
      QTreeView::drawRow(painter, option, index);
    }

    painter->save();
    painter->setPen(QColor(75,75,75));
    painter->drawRect(option.rect);
    painter->restore();*/
//}

//void PropertyEditor::drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const
//{
//    qDebug() << __FUNCTION__ << index;
//    QRect r = rect;
//    QLinearGradient categoryBackground;
//    categoryBackground.setStart(0, r.top());
//    categoryBackground.setFinalStop(0, r.bottom());
//    categoryBackground.setColorAt(0, QColor(88,88,88));
//    categoryBackground.setColorAt(1, QColor(66,66,66));

////    painter->fillRect(r, QColor(Qt::green));
//    if (index.parent() == QModelIndex()) {
////        painter->fillRect(r, QColor(77,77,77));
//        painter->fillRect(r, categoryBackground);
//    }
//    QTreeView::drawBranches(painter, rect, index);
//}

