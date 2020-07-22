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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0: return nullptr;
        case DT_Label: {
            LabelEditor *label = new LabelEditor(index, parent);
            emit editorWidgetToDisplay(index, label);
            return label;
        }
        case DT_LineEdit: {
            LineEditor *lineEditor = new LineEditor(index, parent);
            connect(lineEditor, &LineEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, lineEditor);
            return lineEditor;
        }
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
        case DT_DoubleSpinbox: {
            DoubleSpinBoxEditor *doubleSpinBoxEditor = new DoubleSpinBoxEditor(index, parent);
            connect(doubleSpinBoxEditor, &DoubleSpinBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, doubleSpinBoxEditor);
            return doubleSpinBoxEditor;
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
        case DT_BarBtns: {
            BarBtnEditor *barBtnEditor = new BarBtnEditor(index, parent);
            emit editorWidgetToDisplay(index, barBtnEditor);
            return barBtnEditor;
        }
        case DT_Color: {
            ColorEditor *colorEditor = new ColorEditor(index, parent);
            connect(colorEditor, &ColorEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, colorEditor);
            return colorEditor;
        }
        default:
            return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

QSize PropertyDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int height = static_cast<int>(G::fontSize.toInt() * 1.7 * G::ptToPx);
    return QSize(option.rect.width(), height);
    /*
    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0:
        case DT_Label: return LabelEditor(index, nullptr).sizeHint();
        case DT_LineEdit: return LineEditor(index, nullptr).sizeHint();
        case DT_Checkbox: return CheckBoxEditor(index, nullptr).sizeHint();
        case DT_Spinbox: return SpinBoxEditor(index, nullptr).sizeHint();
        case DT_Combo: return ComboBoxEditor(index, nullptr).sizeHint();
        case DT_Slider: return SliderEditor(index, nullptr).sizeHint();
        case DT_PlusMinus: return PlusMinusEditor(index, nullptr).sizeHint();
    }*/
}

void PropertyDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    switch (index.data(UR_DelegateType).toInt()) {
        case DT_None:
        case DT_Label: {
            static_cast<LabelEditor*>(editor)->setValue(index.data(Qt::EditRole));
            break;
        }
        case DT_LineEdit: {
            static_cast<LineEditor*>(editor)->setValue(index.data(Qt::EditRole));
            break;
        }
        case DT_Checkbox: {
            static_cast<CheckBoxEditor*>(editor)->setValue(index.data(Qt::EditRole));
            break;
        }
        case DT_Spinbox: {
            static_cast<SpinBoxEditor*>(editor)->setValue(index.data(Qt::EditRole));
            break;
        }
        case DT_DoubleSpinbox: {
            static_cast<DoubleSpinBoxEditor*>(editor)->setValue(index.data(Qt::EditRole));
            break;
        }
        case DT_Combo: {
            static_cast<ComboBoxEditor*>(editor)->setValue(index.data(Qt::EditRole));
            break;
        }
        case DT_Slider: {
            static_cast<SliderEditor*>(editor)->setValue(index.data(Qt::EditRole));
            break;
        }
        case DT_Color: {
            static_cast<ColorEditor*>(editor)->setValue(index.data(Qt::EditRole));
            break;
        }
        case DT_BarBtns:
        case DT_PlusMinus: {
            // nothing to set
            break;
        }
    }
}

//bool PropertyDelegate::eventFilter(QObject *editorObject, QEvent *event)
//{
////    bool ok = QStyledItemDelegate::eventFilter(editor, event);
////    qDebug() << __FUNCTION__ << editor << event << event->type();
////    return ok;
//    if (event->type() == QEvent::KeyPress)
//    {
////        const auto key = static_cast<QKeyEvent *>(event)->key();
////        if (key == Qt::Key_Enter || key == Qt::Key_Return || key == Qt::Key_Tab)
////            submitted = true;
////        else
////            submitted = true;

//        qDebug() << __FUNCTION__ << submitted;
//        QWidget *editor = qobject_cast<QWidget *>(editorObject);
//        emit commitData(editor);
//        return true;
//    }
//    else if (event->type() == QEvent::Leave /*&&
//             static_cast<QFocusEvent*>(event)->reason() == Qt::MouseFocusReason*/)
//    {
//        submitted = true;
//        qDebug() << __FUNCTION__ << submitted;
//    }
//    return submitted;
//}

void PropertyDelegate::commit(QWidget *editor)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    qDebug() << __FUNCTION__ << submitted;
    emit commitData(editor);
}

void PropertyDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
//    qDebug() << __FUNCTION__ << index;
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0:
        case DT_Label: {
            LabelEditor *label = static_cast<LabelEditor*>(editor);
            QString value = label->value();
            model->setData(index, value, Qt::EditRole);
            emit itemChanged(index);
            break;
        }
        case DT_LineEdit: {
            LineEditor *lineEditor = static_cast<LineEditor*>(editor);
            QString value = lineEditor->value();
            model->setData(index, value, Qt::EditRole);
            emit itemChanged(index);
            break;
        }
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
        case DT_DoubleSpinbox: {
            DoubleSpinBoxEditor *doubleSpinBoxEditor = static_cast<DoubleSpinBoxEditor*>(editor);
            double value = doubleSpinBoxEditor->value();
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
        case DT_Color: {
            ColorEditor *colorEditor = static_cast<ColorEditor*>(editor);
            QString value = colorEditor->value();
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
//    if (index.column() == 0) {
//        QStyledItemDelegate::paint(painter, option, index);
//        return;
//    }
    painter->save();
    bool isSelected = option.state.testFlag(QStyle::State_Selected);
    bool hasChildren = index.model()->hasChildren(index.model()->index(index.row(),0,index.parent()));

    /*
    static int i =0;
    qDebug() << __FUNCTION__ << i++
             << index.row()
             << index.column()
             << "Value =" << index.data().toString().leftJustified(25)
             << index.data(UR_Name).toString().leftJustified(25)
             << "decorateGradient =" << index.data(UR_DecorateGradient).toInt()
             << "Delegate =" << index.data(UR_DelegateType).toInt()
             << "isSelected =" << isSelected
                ;
//                */

    /* Root rows are highlighted with a darker gradient and the decoration, which gets covered
    up, is repainted */
    QRect r = option.rect;
    // r0 extends the rect over the decoration to the left margin
    QRect r0 = QRect(0, r.y(), r.x() + r.width(), r.height());

    int a = G::backgroundShade + 5;
    int b = G::backgroundShade - 15;
    int c = G::backgroundShade + 40;
    int t = G::textShade;

    QLinearGradient categoryBackground;
    categoryBackground.setStart(0, r.top());
    categoryBackground.setFinalStop(0, r.bottom());
    categoryBackground.setColorAt(0, QColor(a,a,a));
    categoryBackground.setColorAt(1, QColor(b,b,b));

    QFont font;
    font = painter->font();
    int fontSize = G::fontSize.toInt();
    font.setPointSize(fontSize);
    painter->setFont(font);

    QPen catPen(Qt::white);             // root items have white text
    QPen regPen(QColor(t,t,t));         // other items have silver text
    QPen selPen("#1b8a83");             // selected items have torqouis text
    QPen brdPen(QColor(c,c,c));         // border color

    QString text = index.data().toString();
    QString elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, r.width());

    // replacement decorations
    QPixmap branchClosed(":/images/branch-closed-small.png");
    QPixmap branchOpen(":/images/branch-open-small.png");

    if (index.data(UR_DecorateGradient).toBool()) {
        // root item in caption column
        if (index.column() == 0) {
            // paint the gradient covering the decoration
            painter->fillRect(r0, categoryBackground);
            if (index.data(UR_isDecoration).toBool()) {
                // re-instate the decorations
                int x = 2;
                int y = r0.top() + r0.height()/2 - 3;
                if (isSelected) {
                    painter->drawPixmap(x, y, 9, 9, branchOpen);
                }
                else {
                    painter->drawPixmap(x, y, 9, 9, branchClosed);
                }
            }
            // caption text and no borders for root item
            if (option.state.testFlag(QStyle::State_Selected)) painter->setPen(selPen);
            else painter->setPen(catPen);
            painter->drawText(r, Qt::AlignVCenter|Qt::TextSingleLine, elidedText);
        }
        // root row, but value column, so no decoration to deal with
        else {
            painter->fillRect(r, categoryBackground);
        }
    }
    else {
        // Not a root item
        painter->setPen(regPen);

        // caption text and cell borders
        if (index.column() == 0) {
            if (isSelected) painter->setPen(selPen);
            painter->drawText(r, Qt::AlignVCenter|Qt::TextSingleLine, elidedText);
            painter->setPen(brdPen);
            if (hasChildren) {
                painter->drawLine(r0.topLeft(), r0.topRight());
            }
            else painter->drawRect(r0);
        }
        if (index.column() == 1) {
            painter->setPen(brdPen);
            if (hasChildren) {
                painter->drawLine(r.topLeft(), r.topRight());
            }
            else painter->drawRect(r);
        }
    }

    painter->restore();
}

