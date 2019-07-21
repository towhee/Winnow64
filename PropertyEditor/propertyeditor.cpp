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
    setAlternatingRowColors(true);
//    setIndentation(4);
//    setSelectionMode(QAbstractItemView::NoSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
//    setHeaderHidden(false);
    /*setStyleSheet("QTreeView {"
                  "alternate-background-color: rgb(90,90,90);"
                  "border: 2px solid rgb(95,95,95);"
                  "color: lightgray;"
                  "}"
                  "QTreeView::item { "
                  "height: 24px;"
                  "border: 1px solid gray;"
                  "}");*/
//    setEditTriggers(QAbstractItemView::DoubleClicked);
    setEditTriggers(QAbstractItemView::AllEditTriggers);

    QStandardItemModel *m = new QStandardItemModel;
    setModel(m);
    propertyDelegate = new PropertyDelegate(this);
    setItemDelegateForColumn(1, propertyDelegate);
    const QStyleOptionViewItem *styleOptionViewItem = new QStyleOptionViewItem;

    connect(propertyDelegate, &PropertyDelegate::editorValueChanged,
            this, &PropertyEditor::editorValueChange);
    connect(propertyDelegate, &PropertyDelegate::editorWidgetToDisplay,
            this, &PropertyEditor::editorWidgetToDisplay);

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
    setColumnWidth(0, 350);
    setColumnWidth(1, 200);
    row = -1;

    row++;
    // name = thumbBadgeSize (for search and replace)
    QStandardItem *imBadgeSizeCaption = new QStandardItem;
    imBadgeSizeCaption->setText("Loupe view classification badge size");
    QStandardItem *imageBadgeSizeValue = new QStandardItem;
    imageBadgeSizeValue->setToolTip("The image badge is a circle showing the colour classification, rating and pick status.  It is located in the lower right corner of the image.  This property allows you to adjust its size.");
    imageBadgeSizeValue->setData(mw->classificationBadgeInImageDiameter, Qt::EditRole);
    imageBadgeSizeValue->setData(DT_Slider, UR_DelegateType);
    imageBadgeSizeValue->setData("classificationBadgeInImageDiameter", UR_Source);
    imageBadgeSizeValue->setData("int", UR_Type);
    imageBadgeSizeValue->setData(10, UR_Min);
    imageBadgeSizeValue->setData(100, UR_Max);
    imageBadgeSizeValue->setData(50, UR_LabelFixedWidth);
    generalItem->setChild(row, 0, imBadgeSizeCaption);
    generalItem->setChild(row, 1, imageBadgeSizeValue);
    idxVal = imageBadgeSizeValue->index();
    qDebug() << __FUNCTION__ << "Add to model: classificationBadgeInImageDiameter"
             << idxVal;
//    edit(idxVal);
    propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

    row++;
    // name = thumbBadgeSize (for search and replace)
    QStandardItem *thumbBadgeSizeCaption = new QStandardItem;
    thumbBadgeSizeCaption->setText("Grid/Thumb view classification badge size");
    QStandardItem *thumbBadgeSizeValue = new QStandardItem;
    thumbBadgeSizeValue->setData(mw->classificationBadgeInThumbDiameter, Qt::EditRole);
    thumbBadgeSizeValue->setData(DT_Slider, UR_DelegateType);
    thumbBadgeSizeValue->setData("classificationBadgeInThumbDiameter", UR_Source);
    thumbBadgeSizeValue->setData("int", UR_Type);
    thumbBadgeSizeValue->setData(10, UR_Min);
    thumbBadgeSizeValue->setData(100, UR_Max);
    thumbBadgeSizeValue->setData(50, UR_LabelFixedWidth);
    generalItem->setChild(row, 0, thumbBadgeSizeCaption);
    generalItem->setChild(row, 1, thumbBadgeSizeValue);
    idxVal = thumbBadgeSizeValue->index();
//    setCurrentIndex(idxVal);
    qDebug() << __FUNCTION__ << "Add to model: classificationBadgeInThumbDiameter"
             << thumbBadgeSizeValue->index();
//    edit(idxVal);
    propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

    expandAll();
}

void PropertyEditor::editorWidgetToDisplay(QModelIndex idx, QWidget *editor)
/*

*/
{
    int widgetType = idx.data(UR_DelegateType).toInt();
    qDebug() << __FUNCTION__ << idx << editor << widgetType;
    switch (widgetType) {
        case DT_Slider:  {
            setIndexWidget(idx, editor);
            emit propertyDelegate->closeEditor(editor);
//            propertyDelegate->commitData(editor);
        }
    }
}

void PropertyEditor::editorValueChange(QVariant v, QString source)
{
    qDebug() << __FUNCTION__ << v << source;
    if (source == "classificationBadgeInImageDiameter") {
        int value = v.toInt();
        mw->setClassificationBadgeImageDiam(value);
    }
    if (source == "classificationBadgeInThumbDiameter") {
        int value = v.toInt();
        mw->setClassificationBadgeThumbDiam(value);
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
    qDebug() << __FUNCTION__ << "parent =" << parent
             << "option " << option
             << "index =" << index;

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
        connect(sliderEditor, &SliderEditor::editorValueChanged,
                this, &PropertyDelegate::editorValueChanged);
        emit editorWidgetToDisplay(index, sliderEditor);
        return sliderEditor;
//        return new SliderEditor(index, parent);
    }
    default:
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

QString PropertyDelegate::displayText(const QVariant& value, const QLocale& /*locale*/) const
{
//    qDebug() << __FUNCTION__ << value;
    return "";
}

QSize PropertyDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
//    qDebug() << __FUNCTION__ << index;
    return SliderEditor(index, nullptr).sizeHint();
}

void PropertyDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
//    qDebug() << __FUNCTION__ << index;
    int value = index.model()->data(index, Qt::EditRole).toInt();
    static_cast<SliderEditor*>(editor)->setValue(value);
//    SliderLineEdit *w = static_cast<SliderLineEdit*>(editor);
//    w->setValue(value);
}

void PropertyDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
//    qDebug() << __FUNCTION__ << index;
    SliderEditor *sliderEditor = static_cast<SliderEditor*>(editor);
//    spinBox->interpretText();
    int value = sliderEditor->value();
//    qDebug() << __FUNCTION__ << value << index;
    model->setData(index, value, Qt::EditRole);
}

bool PropertyDelegate::eventFilter(QObject *editor, QEvent *event)
{
    qDebug() << __FUNCTION__ << editor << event;
    qApp->processEvents();
    return event;
}

void PropertyDelegate::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
//    qDebug() << __FUNCTION__ << index;
    editor->setGeometry(option.rect);
}

void PropertyDelegate::editorValueChange(QVariant v, QString source)
{
//    qDebug() << __FUNCTION__ << v << source;
    emit editorValueChanged(v, source);
}
