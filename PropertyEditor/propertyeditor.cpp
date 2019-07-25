#include "propertyeditor.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include <QDebug>

//// this works because propertyeditor is a friend class of MW
//MW *mw;

PropertyEditor::PropertyEditor(QWidget *parent) : QTreeView(parent)
{
    // this works because propertyeditor is a friend class of MW
//    mw = qobject_cast<MW*>(parent);

    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::AllEditTriggers);
    indentation = 15;
    setIndentation(indentation);
//    setStyleSheet("QTreeView::item {border:1px solid rgb(75,75,75)}");

    model = new QStandardItemModel;
    setModel(model);
    propertyDelegate = new PropertyDelegate(this);
    setItemDelegate(propertyDelegate);
    styleOptionViewItem = new QStyleOptionViewItem;

    connect(propertyDelegate, &PropertyDelegate::editorWidgetToDisplay,
            this, &PropertyEditor::editorWidgetToDisplay);
}


void PropertyEditor::editorWidgetToDisplay(QModelIndex idx, QWidget *editor)
{
    setIndexWidget(idx, editor);
    emit propertyDelegate->closeEditor(editor);
}

//void PropertyEditor::editorValueChange(QVariant v, QString source, QModelIndex index)
//{
////    emit editorValueChanged(v, source, index);
//}

//void PropertyEditor::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
//{
//    QTreeView::drawRow(painter, option, index);
////    painter->save();
////    painter->setPen(QColor(75,75,75));
////    painter->drawRect(option.rect);
////    painter->restore();
//}

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

