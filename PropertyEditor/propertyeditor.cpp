#include "propertyeditor.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include <QDebug>

// this works because propertyeditor is a friend class of MW
MW *m;

PropertyEditor::PropertyEditor(QWidget *parent) : QTreeWidget(parent)
{
    // this works because propertyeditor is a friend class of MW
    m = qobject_cast<MW*>(parent);

    setRootIsDecorated(true);
//    setIndentation(4);
//    setSelectionMode(QAbstractItemView::NoSelection);
//    setSelectionBehavior(QAbstractItemView::SelectRows);
    setColumnCount(2);
//    setHeaderHidden(false);
    setColumnWidth(0, 250);
    setColumnWidth(1, 250);
    setHeaderLabels({"Property", "Value"});
//    this->model->headerData();
//    header()->setDefaultAlignment(Qt::AlignLeft);
    header()->setVisible(true);
//    setStyleSheet("QTreeWidget {"
//                  "alternate-background-color: rgb(90,90,90);"
//                  "border: 2px solid rgb(95,95,95);"
//                  "color: lightgray;"
//                  "}"
//                  "QTreeView::item { "
//                  "height: 24px;"
////                  "border: 1px solid gray;"
//                  "}");
//    setEditTriggers(QAbstractItemView::DoubleClicked);
//    setEditTriggers(QAbstractItemView::AllEditTriggers);

    connect(this, &PropertyEditor::itemClicked, this, &PropertyEditor::editCheck);
    connect(this, &PropertyEditor::itemDoubleClicked, this, &PropertyEditor::editCheck);

    int row;
    QModelIndex idx;

    general = new QTreeWidgetItem(this);
    general->setText(0, "General");

//    setItemDelegateForRow(row, new SliderLineEditDelegate);
    generalFontSize = new QTreeWidgetItem(general);
    generalFontSize->setFlags(Qt::ItemIsEditable|Qt::ItemIsEnabled);
    generalFontSize->setText(0, "Font size");
    generalFontSize->setData(1, Qt::EditRole, 14);
    SliderLineEdit *customControl = new SliderLineEdit;
    setItemWidget(generalFontSize, 1, customControl);
    idx = indexFromItem(generalFontSize, 1);

    setItemDelegate(new SliderLineEditDelegate/*(idx, 50)*/);


    expandAll();
}

void PropertyEditor::editCheck(QTreeWidgetItem *item, int column)
{
    if (column == 1) editItem(item, column);
}
