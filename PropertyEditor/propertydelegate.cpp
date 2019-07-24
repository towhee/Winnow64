#include "propertydelegate.h"

/* DELEGATE **********************************************************************************/

PropertyDelegate::PropertyDelegate(QObject *parent): QStyledItemDelegate(parent)
{
//    qDebug() << __FUNCTION__;
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
            CheckBoxEditor *checkEditor = new CheckBoxEditor(index, parent);
            connect(checkEditor, &CheckBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::editorValueChanged);
            emit editorWidgetToDisplay(index, checkEditor);
            return checkEditor;
        }
        case DT_Spinbox: {
            SpinBoxEditor *spinBoxEditor = new SpinBoxEditor(index, parent);
            connect(spinBoxEditor, &SpinBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::editorValueChanged);
            emit editorWidgetToDisplay(index, spinBoxEditor);
            return spinBoxEditor;
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
    int height = G::fontSize.toInt() + 10;
    return QSize(option.rect.width(), height);  // rgh perhaps change 24 to function of font size + 10

    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0:
        case DT_Text:
    //        return new QLinedEdit;
        case DT_Checkbox: return CheckBoxEditor(index, nullptr).sizeHint();
        case DT_Spinbox: return SpinBoxEditor(index, nullptr).sizeHint();
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
            static_cast<CheckBoxEditor*>(editor)->setValue(value);
        }
        case DT_Spinbox: {
            int value = index.model()->data(index, Qt::EditRole).toBool();
            static_cast<SpinBoxEditor*>(editor)->setValue(value);
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
            CheckBoxEditor *checkEditor = static_cast<CheckBoxEditor*>(editor);
            bool value = checkEditor->value();
            model->setData(index, value, Qt::EditRole);
        }
        case DT_Spinbox: {
            SpinBoxEditor *spinBoxEditor = static_cast<SpinBoxEditor*>(editor);
            bool value = spinBoxEditor->value();
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


void PropertyDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
    painter->save();

    QRect r = option.rect;
    QRect r0 = QRect(0, r.y(), r.x() + r.width(), r.height());

    QLinearGradient categoryBackground;
    categoryBackground.setStart(0, r.top());
    categoryBackground.setFinalStop(0, r.bottom());
    categoryBackground.setColorAt(0, QColor(88,88,88));
    categoryBackground.setColorAt(1, QColor(66,66,66));

    QPen catPen(Qt::white);
    QPen regPen(QColor(190,190,190));

    if (index.parent() == QModelIndex()) {
        if (index.column() == 0) painter->fillRect(r0, categoryBackground);
        if (index.column() == 1) painter->fillRect(r, categoryBackground);
        painter->setPen(catPen);
    }
    else if (index.column() == 0) painter->setPen(regPen);

    if (index.column() == 0) {
        QString text = index.data().toString();
        QString elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, r.width());
        painter->drawText(r, Qt::AlignVCenter|Qt::TextSingleLine, elidedText);
        painter->setPen(QColor(75,75,75));
        painter->drawRect(r0);
    }
    if (index.column() == 1) {
        painter->setPen(QColor(75,75,75));
        painter->drawRect(r);
    }
    painter->restore();

//        QStyledItemDelegate::paint(painter, option, index);
}

