#include "propertyeditor.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include <QDebug>

/* DELEGATE **********************************************************************************/

PropertyDelegate::PropertyDelegate(QObject *parent): QStyledItemDelegate(parent)
{

}

QWidget *PropertyDelegate::createEditor(QWidget *parent,
                                              const QStyleOptionViewItem &option,
                                              const QModelIndex &index ) const
{
    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0: return nullptr;
    //    case DT_Text:
    //        return new QLinedEdit;
        case DT_Checkbox: {
            CheckEditor *checkEditor = new CheckEditor(index, parent);
            connect(checkEditor, &CheckEditor::editorValueChanged,
                    this, &PropertyDelegate::editorValueChanged);
            emit editorWidgetToDisplay(index, checkEditor);
            return new QCheckBox;
        }
        case DT_Combo: {
            ComboBoxEditor *comboBoxEditor = new ComboBoxEditor(index, parent);
            connect(comboBoxEditor, &ComboBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::editorValueChanged);
            emit editorWidgetToDisplay(index, comboBoxEditor);
            return comboBoxEditor;
        }
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
    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0:
        case DT_Text:
    //        return new QLinedEdit;
        case DT_Checkbox: return CheckEditor(index, nullptr).sizeHint();
        case DT_Combo: return ComboBoxEditor(index, nullptr).sizeHint();
        case DT_Slider: return SliderEditor(index, nullptr).sizeHint();
    }

}

void PropertyDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0:
        case DT_Text:
    //        return new QLinedEdit;
        case DT_Checkbox: {
            int value = index.model()->data(index, Qt::EditRole).toBool();
            static_cast<CheckEditor*>(editor)->setValue(value);
        }
        case DT_Combo: {
            int value = index.model()->data(index, Qt::EditRole).toInt();
            static_cast<ComboBoxEditor*>(editor)->setValue(value);
        }
        case DT_Slider: {
            int value = index.model()->data(index, Qt::EditRole).toInt();
            static_cast<SliderEditor*>(editor)->setValue(value);
        }
    }
}

void PropertyDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
//    qDebug() << __FUNCTION__ << index;
    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0:
        case DT_Text:
//        return new QLinedEdit;
        case DT_Checkbox: {
            CheckEditor *checkEditor = static_cast<CheckEditor*>(editor);
            bool value = checkEditor->value();
            model->setData(index, value, Qt::EditRole);
        }
        case DT_Combo: {
            ComboBoxEditor *comboBoxEditor = static_cast<ComboBoxEditor*>(editor);
            QString value = comboBoxEditor->value();
            model->setData(index, value, Qt::EditRole);
        }
        case DT_Slider: {
            SliderEditor *sliderEditor = static_cast<SliderEditor*>(editor);
            int value = sliderEditor->value();
            model->setData(index, value, Qt::EditRole);
        }
    }
}

//bool PropertyDelegate::eventFilter(QObject *editor, QEvent *event)
//{
////    qDebug() << __FUNCTION__ << editor << event;
//    qApp->processEvents();
//    return event;
//}

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

/* PROPERTY EDITOR ***************************************************************************/

// this works because propertyeditor is a friend class of MW
MW *mw;

PropertyEditor::PropertyEditor(QWidget *parent) : QTreeView(parent)
{
    // this works because propertyeditor is a friend class of MW
    mw = qobject_cast<MW*>(parent);

    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::AllEditTriggers);
//    setStyleSheet("QTreeView::item {border:1px solid rgb(75,75,75)}");

    model = new QStandardItemModel;
    setModel(model);
    propertyDelegate = new PropertyDelegate(this);
    setItemDelegateForColumn(1, propertyDelegate);
    styleOptionViewItem = new QStyleOptionViewItem;

    connect(propertyDelegate, &PropertyDelegate::editorValueChanged,
            this, &PropertyEditor::editorValueChange);
    connect(propertyDelegate, &PropertyDelegate::editorWidgetToDisplay,
            this, &PropertyEditor::editorWidgetToDisplay);

    addItems();
    expandAll();
}

void PropertyEditor::editorWidgetToDisplay(QModelIndex idx, QWidget *editor)
/*

*/
{
    setIndexWidget(idx, editor);
    emit propertyDelegate->closeEditor(editor);

//    int widgetType = idx.data(UR_DelegateType).toInt();
////    qDebug() << __FUNCTION__ << idx << editor << widgetType;
//    switch (widgetType) {
//        case DT_Checkbox:  {
//            setIndexWidget(idx, editor);
//            emit propertyDelegate->closeEditor(editor);
//        }
//        case DT_Slider:  {
//            setIndexWidget(idx, editor);
//            emit propertyDelegate->closeEditor(editor);
//        }
//    }
}

void PropertyEditor::editorValueChange(QVariant v, QString source)
{
//    qDebug() << __FUNCTION__ << v << source;
    if (source == "classificationBadgeInImageDiameter") {
        int value = v.toInt();
        mw->setClassificationBadgeImageDiam(value);
    }
    if (source == "classificationBadgeInThumbDiameter") {
        int value = v.toInt();
        mw->setClassificationBadgeThumbDiam(value);
    }
    if (source == "rememberLastDir") {
        mw->rememberLastDir = v.toBool();
    }
}

void PropertyEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) return;
    QTreeView::mousePressEvent(event);
    if (event->modifiers() == Qt::NoModifier) {
        QModelIndex idx = indexAt(event->pos());
        // setCurrentIndex for the value cell (col 1) if user clicked on the caption cell (col 0)
        QModelIndex idxVal = model->index(idx.row(), 1, idx.parent());
        setCurrentIndex(idxVal);
    }
}

void PropertyEditor::addItems()
{
    int row;
    QModelIndex idxVal;
    QModelIndex idxCat;

    // General category
    QStandardItem *generalItem = new QStandardItem;
    generalItem->setText("General");
    generalItem->setEditable(false);
    generalItem->setData(DT_None, UR_DelegateType);
    model->appendRow(generalItem);
    model->insertColumns(1, 1);
    idxCat = generalItem->index();
    setColumnWidth(0, 300);
    setColumnWidth(1, 200);
    row = -1;

    row++;
    // name = thumbBadgeSize (for search and replace)
    QStandardItem *imBadgeSizeCaption = new QStandardItem;
    imBadgeSizeCaption->setText("Loupe classification badge size");
    imBadgeSizeCaption->setEditable(false);
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
    propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

    row++;
    // name = thumbBadgeSize (for search and replace)
    QStandardItem *thumbBadgeSizeCaption = new QStandardItem;
    thumbBadgeSizeCaption->setText("Thumbnail classification badge size");
    thumbBadgeSizeCaption->setEditable(false);
    QStandardItem *thumbBadgeSizeValue = new QStandardItem;
    thumbBadgeSizeValue->setToolTip("The image badge is a circle showing the colour classification, rating and pick status.  It is located in the top right corner of the thumbnail in the thumb and grid views.  This property allows you to adjust its size.");
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
    propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

    row++;
    // name = rememberFolder (for search and replace)
    QStandardItem *rememberFolderCaption = new QStandardItem;
    rememberFolderCaption->setText("Remember folder from last session");
    rememberFolderCaption->setEditable(false);
    QStandardItem *rememberFolderValue = new QStandardItem;
    rememberFolderValue->setToolTip("Remember the last folder used in Winnow from the previous session.");
    rememberFolderValue->setData(mw->rememberLastDir, Qt::EditRole);
    rememberFolderValue->setData(DT_Checkbox, UR_DelegateType);
    rememberFolderValue->setData("rememberLastDir", UR_Source);
    rememberFolderValue->setData("bool", UR_Type);
    generalItem->setChild(row, 0, rememberFolderCaption);
    generalItem->setChild(row, 1, rememberFolderValue);
    idxVal = rememberFolderValue->index();
    propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

    row++;
    // name = useWheelToScroll (for search and replace)
    QStandardItem *useWheelToScrollCaption = new QStandardItem;
    useWheelToScrollCaption->setText("Use wheel/trackpad to scroll");
    useWheelToScrollCaption->setEditable(false);
    QStandardItem *useWheelToScrollValue = new QStandardItem;
    useWheelToScrollValue->setToolTip("Remember the last folder used in Winnow from the previous session.");
    useWheelToScrollValue->setData(mw->rememberLastDir, Qt::EditRole);
    useWheelToScrollValue->setData(DT_Checkbox, UR_DelegateType);
    useWheelToScrollValue->setData("useWheelToScroll", UR_Source);
    useWheelToScrollValue->setData("QString", UR_Type);
    QStringList list = {"Next/previous image", "Scroll current image when zoomed"};
    useWheelToScrollValue->setData(list, UR_StringList);
    generalItem->setChild(row, 0, useWheelToScrollCaption);
    generalItem->setChild(row, 1, useWheelToScrollValue);
    idxVal = useWheelToScrollValue->index();
    propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

}
