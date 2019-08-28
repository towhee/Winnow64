#include "propertydelegate.h"

/*
The property editor has four components:

PropertyEditor - A subclass of QTreeView, with a data model that holds the property
    information.

PropertyDelegate - Creates the editor widgets, sets the editor and model data and does some
    custom painting to each row in the treeview.

PropertyWidgets - A collection of custom widget editors (slider, combobox etc) that are
    inserted into column 1 in the treeview (PropertyEditor) by PropertyDelegate.  The editors
    are built based on information passed in the datamodel UserRole such as the DelegateType
    (Spinbox, Checkbox, Combo, Slider, PlusMinusBtns etc).  Other information includes the
    source, datamodel index, mim value, max value, a stringlist for combos etc).

PropertyEditor subclass ie Preferences.  All the property items are defined and added to the
    treeview in addItems().  When an item value has been changed in an editor itemChanged(idx)
    is signaled and the variable is updated in Winnow and any necessary actions are executed.
    For example, if the thumbnail size is increased then the IconView function to do this is
    called.
*/

PropertyDelegate::PropertyDelegate(QWidget *parent): QStyledItemDelegate(parent)
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
            CheckBoxEditor *checkEditor = new CheckBoxEditor(index, parent);
            connect(checkEditor, &CheckBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, checkEditor);
            return checkEditor;
        }
        case DT_Spinbox: {
            SpinBoxEditor *spinBoxEditor = new SpinBoxEditor(index, parent);
            connect(spinBoxEditor, &SpinBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, spinBoxEditor);
            return spinBoxEditor;
        }
        case DT_Combo: {
            ComboBoxEditor *comboBoxEditor = new ComboBoxEditor(index, parent);
            connect(comboBoxEditor, &ComboBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, comboBoxEditor);
            return comboBoxEditor;
        }
        case DT_Slider: {
            SliderEditor *sliderEditor = new SliderEditor(index, parent);
            connect(sliderEditor, &SliderEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, sliderEditor);
            return sliderEditor;
        }
    case DT_PlusMinus: {
        PlusMinusEditor *plusMinusEditor = new PlusMinusEditor(index, parent);
        connect(plusMinusEditor, &PlusMinusEditor::editorValueChanged,
                this, &PropertyDelegate::commitData);
        emit editorWidgetToDisplay(index, plusMinusEditor);
        return plusMinusEditor;
    }
        default:
            return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

QSize PropertyDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int height = static_cast<int>(G::fontSize.toInt() * 1.5);
    return QSize(option.rect.width(), height);

    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0:
        case DT_Text:
    //        return new QLinedEdit;
        case DT_Checkbox: return CheckBoxEditor(index, nullptr).sizeHint();
        case DT_Spinbox: return SpinBoxEditor(index, nullptr).sizeHint();
        case DT_Combo: return ComboBoxEditor(index, nullptr).sizeHint();
        case DT_Slider: return SliderEditor(index, nullptr).sizeHint();
        case DT_PlusMinus: return PlusMinusEditor(index, nullptr).sizeHint();
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
            break;
        }
        case DT_Spinbox: {
            int value = index.model()->data(index, Qt::EditRole).toBool();
            static_cast<SpinBoxEditor*>(editor)->setValue(value);
            break;
        }
        case DT_Combo: {
            int value = index.model()->data(index, Qt::EditRole).toInt();
            static_cast<ComboBoxEditor*>(editor)->setValue(value);
            break;
        }
        case DT_Slider: {
            int value = index.model()->data(index, Qt::EditRole).toInt();
            static_cast<SliderEditor*>(editor)->setValue(value);
            break;
        }
        case DT_PlusMinus: {
            // nothing to set
            break;
        }
    }
}

void PropertyDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0:
        case DT_Text:
//        return new QLinedEdit;
        case DT_Checkbox: {
            CheckBoxEditor *checkBoxEditor = static_cast<CheckBoxEditor*>(editor);
            bool value = checkBoxEditor->value();
            model->setData(index, value, Qt::EditRole);
            emit itemChanged(index);
            break;
        }
        case DT_Spinbox: {
            SpinBoxEditor *spinBoxEditor = static_cast<SpinBoxEditor*>(editor);
            int value = spinBoxEditor->value();
            model->setData(index, value, Qt::EditRole);
            emit itemChanged(index);
            break;

        }
        case DT_Combo: {
            ComboBoxEditor *comboBoxEditor = static_cast<ComboBoxEditor*>(editor);
            QString value = comboBoxEditor->value();
            model->setData(index, value, Qt::EditRole);
            emit itemChanged(index);
            break;
        }
        case DT_Slider: {
            SliderEditor *sliderEditor = static_cast<SliderEditor*>(editor);
            int value = sliderEditor->value();
            model->setData(index, value, Qt::EditRole);
            emit itemChanged(index);
            break;
        }
        case DT_PlusMinus: {
            PlusMinusEditor *plusMinusEditor = static_cast<PlusMinusEditor*>(editor);
            int value = plusMinusEditor->value();
            model->setData(index, value, Qt::EditRole);
            emit itemChanged(index);
            break;
        }
    }
}

void PropertyDelegate::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &/*index*/ ) const
{
    editor->setGeometry(option.rect);
}

void PropertyDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
    painter->save();

    /* Root rows are highlighted with a darker gradient and the docarion, which gets covered
    up, is repainted */
    QRect r = option.rect;
    // r0 extends the rect over the decoration to the left margin
    QRect r0 = QRect(0, r.y(), r.x() + r.width(), r.height());

    QLinearGradient categoryBackground;
    categoryBackground.setStart(0, r.top());
    categoryBackground.setFinalStop(0, r.bottom());
    categoryBackground.setColorAt(0, QColor(88,88,88));
    categoryBackground.setColorAt(1, QColor(66,66,66));

    QPen catPen(Qt::white);             // root items have white text
    QPen regPen(QColor(190,190,190));   // other items have silver text

    // replacement decorations
    QPixmap branchClosed(":/images/branch-closed-small.png");
    QPixmap branchOpen(":/images/branch-open-small.png");

    if (index.parent() == QModelIndex()) {
        // root item in caption column
        if (index.column() == 0) {
            // paint the gradient covering the decoration
            painter->fillRect(r0, categoryBackground);
            // re-instate the decorations
            int x = 2;
            int y = r0.top() + r0.height()/2 - 3;
            if (option.state & QStyle::State_Open) {
                painter->drawPixmap(x, y, 9, 9, branchOpen);
            }
            else {
                painter->drawPixmap(x, y, 9, 9, branchClosed);
            }
        }
        // root row, but value column, so no decoration to deal with
        else painter->fillRect(r, categoryBackground);
        painter->setPen(catPen);
    }
    else if (index.column() == 0) painter->setPen(regPen);

    // caption text and cell borders
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
}

