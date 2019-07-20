#include "propertyeditor.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include <QDebug>

// this works because propertyeditor is a friend class of MW
MW *mw;

PropertyEditor::PropertyEditor(QWidget *parent) : QTreeView(parent)
{
    // this works because propertyeditor is a friend class of MW
    mw = qobject_cast<MW*>(parent);

    setRootIsDecorated(true);
//    setIndentation(4);
    setSelectionMode(QAbstractItemView::NoSelection);
//    setSelectionBehavior(QAbstractItemView::SelectRows);
//    setHeaderHidden(false);
//    setStyleSheet("QTreeView {"
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

    QStandardItemModel *m = new QStandardItemModel;
    setModel(m);
    PropertyDelegate *propertyDelegate = new PropertyDelegate;
    setItemDelegate(propertyDelegate);
    connect(propertyDelegate, &PropertyDelegate::update, this, &PropertyEditor::editorUpdate);

//    connect(this, &PropertyEditor::, this, &PropertyEditor::editCheck);
//    connect(this, &PropertyEditor::itemDoubleClicked, this, &PropertyEditor::editCheck);

    int row;
    QModelIndex idxVal;
    QModelIndex idxCat;


    QList<QStandardItem *> rowItems;
    QStandardItem *col0 = new QStandardItem;
    QStandardItem *col1 = new QStandardItem;
    rowItems.append(col0);
    rowItems.append(col1);

    QStandardItem *generalItem = new QStandardItem;
    generalItem->setText("General");
    generalItem->setData(DT_None, UR_DelegateType);
    m->appendRow(generalItem);
    m->insertColumns(1, 1);
    idxCat = generalItem->index();
//    m->setData(m->index(0, 1), "Column 1");
    setColumnWidth(0, 250);
    setColumnWidth(1, 250);

    QStandardItem *gridCellSizeCaption = new QStandardItem;
    gridCellSizeCaption->setText("Grid cell size");
    QStandardItem *gridCellSizeValue = new QStandardItem;
    gridCellSizeValue->setText("Grid cell value");
    gridCellSizeValue->setData(mw->classificationBadgeInImageDiameter, Qt::EditRole);
    gridCellSizeValue->setData(DT_Slider, UR_DelegateType);
    gridCellSizeValue->setData("classificationBadgeInImageDiameter", UR_Source);
    gridCellSizeValue->setData("int", UR_Type);
    gridCellSizeValue->setData(10, UR_Min);
    gridCellSizeValue->setData(100, UR_Max);
//    m->insertRow(1, gridCellSizeItem);      // adds another top level item
    generalItem->setChild(0, 0, gridCellSizeCaption);
    generalItem->setChild(0, 1, gridCellSizeValue);

    expandAll();
}

void PropertyEditor::editorUpdate(QVariant v, QString source)
{
    qDebug() << __FUNCTION__ << v << source;
    if (source == "classificationBadgeInImageDiameter") {
        int value = v.toInt();
        mw->classificationBadgeInImageDiameter = value;
        mw->imageView->setClassificationBadgeImageDiam(value);
    }
}
//void PropertyEditor::editCheck(QTreeWidgetItem *item, int column)
//{
//    if (column == 1) editItem(item, column);
//}

/* DELEGATE **********************************************************************************/

PropertyDelegate::PropertyDelegate(QObject *parent): QStyledItemDelegate(parent)
{
//    mw = qobject_cast<MW*>(parent);
}

QWidget *PropertyDelegate::createEditor(QWidget *parent,
                                              const QStyleOptionViewItem &option,
                                              const QModelIndex &index ) const
{
    int type = index.data(UR_DelegateType).toInt();
    qDebug() << __FUNCTION__ << index << type;

    switch (type)
    {
    case 0: return nullptr;
//    case DT_Text:
//        return new QLinedEdit;
//    case DT_Checkbox:
//        return new QCheckBox;
//    case DT_Combo:
//        return new QComboBox;
    case DT_Slider: {
        SliderEditor *sliderEditor = new SliderEditor(index, parent);
        connect(sliderEditor, &SliderEditor::update, this, &PropertyDelegate::editorUpdate);
        return sliderEditor;
//        return new SliderEditor(index, parent);
    }
    default:
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

QSize PropertyDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return SliderEditor(index, nullptr).sizeHint();
}

void PropertyDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();
    static_cast<SliderEditor*>(editor)->setValue(value);
//    SliderLineEdit *w = static_cast<SliderLineEdit*>(editor);
//    w->setValue(value);
}

void PropertyDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    SliderEditor *sliderEditor = static_cast<SliderEditor*>(editor);
//    spinBox->interpretText();
    int value = sliderEditor->value();
    qDebug() << __FUNCTION__ << value;
    model->setData(index, value, Qt::EditRole);
}

void PropertyDelegate::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}

void PropertyDelegate::editorUpdate(QVariant v, QString source)
{
    emit update(v, source);
}
















//#include "propertyeditor.h"
//#include "Main/mainwindow.h"
//#include "Main/global.h"
//#include <QDebug>

//// this works because propertyeditor is a friend class of MW
//MW *m;

//PropertyEditor::PropertyEditor(QWidget *parent) : QTreeWidget(parent)
//{
//    // this works because propertyeditor is a friend class of MW
//    m = qobject_cast<MW*>(parent);

//    setRootIsDecorated(true);
////    setIndentation(4);
////    setSelectionMode(QAbstractItemView::NoSelection);
////    setSelectionBehavior(QAbstractItemView::SelectRows);
//    setColumnCount(2);
////    setHeaderHidden(false);
//    setColumnWidth(0, 250);
//    setColumnWidth(1, 250);
//    setHeaderLabels({"Property", "Value"});
////    this->model->headerData();
////    header()->setDefaultAlignment(Qt::AlignLeft);
//    header()->setVisible(true);
////    setStyleSheet("QTreeWidget {"
////                  "alternate-background-color: rgb(90,90,90);"
////                  "border: 2px solid rgb(95,95,95);"
////                  "color: lightgray;"
////                  "}"
////                  "QTreeView::item { "
////                  "height: 24px;"
//////                  "border: 1px solid gray;"
////                  "}");
////    setEditTriggers(QAbstractItemView::DoubleClicked);
////    setEditTriggers(QAbstractItemView::AllEditTriggers);

//    connect(this, &PropertyEditor::itemClicked, this, &PropertyEditor::editCheck);
//    connect(this, &PropertyEditor::itemDoubleClicked, this, &PropertyEditor::editCheck);

//    int row;
//    QModelIndex idx;

//    general = new QTreeWidgetItem(this);
//    general->setText(0, "General");

////    setItemDelegateForRow(row, new SliderLineEditDelegate);
//    generalFontSize = new QTreeWidgetItem(general);
//    generalFontSize->setFlags(Qt::ItemIsEditable|Qt::ItemIsEnabled);
//    generalFontSize->setText(0, "Font size");
//    generalFontSize->setData(1, Qt::EditRole, 14);
//    idx = indexFromItem(generalFontSize, 1);
//    SliderEditor *customControl = new SliderEditor(idx, nullptr);
//    setItemWidget(generalFontSize, 1, customControl);

//    setItemDelegate(new PropertyDelegate/*(idx, 50)*/);


//    expandAll();
//}

//void PropertyEditor::editCheck(QTreeWidgetItem *item, int column)
//{
//    if (column == 1) editItem(item, column);
//}

///* DELEGATE **********************************************************************************/

//PropertyDelegate::PropertyDelegate(QObject *parent): QStyledItemDelegate(parent)
//{
//}

//QWidget *PropertyDelegate::createEditor(QWidget *parent,
//                                              const QStyleOptionViewItem &option,
//                                              const QModelIndex &index ) const
//{
////    int type = index.data(MyTypeRole).toInt();

////    switch (type)
////    {
////    case DT_Text:
////        return new QLinedEdit;
////    case DT_Checkbox:
////        return new QCheckBox;
////    case DT_Combo:
////        return new QComboBox;
////    default:
////        return QItemDelegate::createEditor(parent, option, index);
////    }

//    return new SliderEditor(index, parent);
//}

//QSize PropertyDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
//{
//    return SliderEditor(index, nullptr).sizeHint();
//}

//void PropertyDelegate::setEditorData(QWidget *editor,
//                                    const QModelIndex &index) const
//{
//    int value = index.model()->data(index, Qt::EditRole).toInt();
//    static_cast<SliderEditor*>(editor)->setValue(value);
////    SliderLineEdit *w = static_cast<SliderLineEdit*>(editor);
////    w->setValue(value);
//}

//void PropertyDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
//                                   const QModelIndex &index) const
//{
////    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
////    spinBox->interpretText();
////    int value = spinBox->value();

//    int value = 17;

//    model->setData(index, value, Qt::EditRole);
//}

//void PropertyDelegate::updateEditorGeometry(QWidget *editor,
//    const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
//{
//    editor->setGeometry(option.rect);
//}

