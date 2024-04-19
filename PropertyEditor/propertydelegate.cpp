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
    isDebug = false;
}

QWidget *PropertyDelegate::createEditor(QWidget *parent,
                                        const QStyleOptionViewItem &option,
                                        const QModelIndex &index ) const
{
    if (G::isLogger) G::log("PropertyDelegate::createEditor");
    if (isDebug)
        qDebug() << "PropertyDelegate::createEditor" << option << index;
    int type = index.data(UR_DelegateType).toInt();
    switch (type) {
        case 0: return nullptr;
        case DT_Label: {
            LabelEditor *label = new LabelEditor(index, parent);
            connect(this, &PropertyDelegate::fontSizeChange, label, &LabelEditor::fontSizeChanged);
            emit editorWidgetToDisplay(index, label);
            return label;
        }
        case DT_LineEdit: {
            LineEditor *lineEditor = new LineEditor(index, parent);
            connect(this, &PropertyDelegate::fontSizeChange, lineEditor, &LineEditor::fontSizeChanged);
            connect(lineEditor, &LineEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, lineEditor);
            return lineEditor;
        }
        case DT_Checkbox: {
            CheckBoxEditor *checkEditor = new CheckBoxEditor(index, parent);
            connect(this, &PropertyDelegate::fontSizeChange, checkEditor, &CheckBoxEditor::fontSizeChanged);
            connect(checkEditor, &CheckBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, checkEditor);
            return checkEditor;
        }
        case DT_Spinbox: {
            SpinBoxEditor *spinBoxEditor = new SpinBoxEditor(index, parent);
            connect(this, &PropertyDelegate::fontSizeChange, spinBoxEditor, &SpinBoxEditor::fontSizeChanged);
            connect(spinBoxEditor, &SpinBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, spinBoxEditor);
            return spinBoxEditor;
        }
        case DT_DoubleSpinbox: {
            DoubleSpinBoxEditor *doubleSpinBoxEditor = new DoubleSpinBoxEditor(index, parent);
            connect(this, &PropertyDelegate::fontSizeChange, doubleSpinBoxEditor, &DoubleSpinBoxEditor::fontSizeChanged);
            connect(doubleSpinBoxEditor, &DoubleSpinBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, doubleSpinBoxEditor);
            return doubleSpinBoxEditor;
        }
        case DT_Combo: {
            ComboBoxEditor *comboBoxEditor = new ComboBoxEditor(index, parent);
            connect(this, &PropertyDelegate::fontSizeChange, comboBoxEditor, &ComboBoxEditor::fontSizeChanged);
            connect(comboBoxEditor, &ComboBoxEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, comboBoxEditor);
            return comboBoxEditor;
        }
        case DT_Slider: {
            SliderEditor *sliderEditor = new SliderEditor(index, parent);
            connect(this, &PropertyDelegate::fontSizeChange, sliderEditor, &SliderEditor::fontSizeChanged);
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
            connect(this, &PropertyDelegate::fontSizeChange, colorEditor, &ColorEditor::fontSizeChanged);
            connect(colorEditor, &ColorEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, colorEditor);
            return colorEditor;
        }
        case DT_SelectFolder: {
            SelectFolderEditor *selectFolderEditor = new SelectFolderEditor(index, parent);
            connect(selectFolderEditor, &SelectFolderEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, selectFolderEditor);
            return selectFolderEditor;
        }
        case DT_SelectFile: {
            SelectFileEditor *selectFileEditor = new SelectFileEditor(index, parent);
            connect(selectFileEditor, &SelectFileEditor::editorValueChanged,
                    this, &PropertyDelegate::commitData);
            emit editorWidgetToDisplay(index, selectFileEditor);
            return selectFileEditor;
        }
        default:
            return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

QSize PropertyDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &/*index*/) const
{
    // row height = 1.7 * text height
    if (isDebug)
        qDebug() << "PropertyDelegate::sizeHint"
                 << "option.rect.width =" << option.rect.width()
                 << "option.rect.height =" << option.rect.height()
            ;

    int height = static_cast<int>(G::strFontSize.toInt() * 1.7 * G::ptToPx);
    return QSize(option.rect.width(), height);
}

void PropertyDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    if (G::isLogger) G::log("PropertyDelegate::setEditorData");
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
        case DT_SelectFolder: {
            static_cast<SelectFolderEditor*>(editor)->setValue(index.data(Qt::EditRole));
            break;
        }
        case DT_SelectFile: {
            static_cast<SelectFileEditor*>(editor)->setValue(index.data(Qt::EditRole));
            break;
        }
        case DT_BarBtns:
        case DT_PlusMinus: {
            // nothing to set
            break;
        }
    }
}

void PropertyDelegate::fontSizeChanged(int fontSize)
{
    if (G::isLogger) G::log("PropertyDelegate::fontSizeChanged");
    if (isDebug)
        qDebug() << "PropertyDelegate::fontSizeChanged";
    emit fontSizeChange(fontSize);
}

void PropertyDelegate::commit(QWidget *editor)
{
    if (G::isLogger) G::log("PropertyDelegate::commit");
    if (isDebug)
        qDebug() << "PropertyDelegate::commit";
    //    qDebug() << "PropertyDelegate::commit" << submitted;
    emit commitData(editor);
    emit closeEditor(editor);
}

void PropertyDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    if (isDebug)
        qDebug() << "PropertyDelegate::setModelData" << index;
    if (G::isLogger) G::log("PropertyDelegate::setModelData");
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
            double value = sliderEditor->value();
            model->setData(index, value, Qt::EditRole);
            // qDebug() << "PropertyDelegate::setModelData (slider)" << index << value;
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
        case DT_SelectFolder: {
            SelectFolderEditor *selectFolderEditor = static_cast<SelectFolderEditor*>(editor);
            QString value = selectFolderEditor->value();
            model->setData(index, value, Qt::EditRole);
            emit itemChanged(index);
            break;
        }
        case DT_SelectFile: {
            SelectFileEditor *selectFileEditor = static_cast<SelectFileEditor*>(editor);
            QString value = selectFileEditor->value();
//            emit setValue(index, value, Qt::EditRole);
            model->setData(index, value, Qt::EditRole);
            emit itemChanged(index);
            break;
        }
    }
}

void PropertyDelegate::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &/*index*/ ) const
{
    if (isDebug)
        qDebug() << "PropertyDelegate::updateEditorGeometry"
                 << "option.rect =" << option.rect
            ;
    editor->setGeometry(option.rect);
}

void PropertyDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
    painter->save();

    bool isSelected = option.state.testFlag(QStyle::State_Selected);
    bool isExpanded = (option.state & QStyle::State_Open) > 0;
    bool hasChildren = index.model()->hasChildren(index.model()->index(index.row(),0,index.parent()));

    /*
    static int i = 0;
    qDebug() << "PropertyDelegate::paint" << i++
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
    up, and is repainted */
    QRect r = option.rect;
    // save column widths
    static int w0 = 100, w1 = 200;
    if (index.column() == 0) w0 = r.width();
    if (index.column() == 1) w1 = r.width();
    // r0 extends the rect over the decoration to the left margin
    QRect r0 = QRect(0, r.y(), r.x() + r.width(), r.height());
    // r1 = r0 but leaves 1 pixel at the left and right margins to make room for a border
    QRect r1 = QRect(1, r.y(), r.x() + r.width() - 1, r.height());
    // r2 = r0 but leaves 1 pixel at the left, right and bottom margins to draw text
    QRect r2 = QRect(5, r.y(), r.x() + r.width() - 5, r.height()-1);
    // r3 = r but leaves 1 pixel at the bottom margins to draw text
    QRect r3 = QRect(r.x(), r.y(), r.x() + r.width(), r.height()-3);
    // r4 = entire row width
    QRect r4 = QRect(r.x(), r.y(), w0 + w1, r.height()-3);
    // r5 = entire col width less 50px for barbtns
    QRect r5 = QRect(r.x() + w1 - 50, r.y(), 50, r.height()-3);

    int a = G::backgroundShade + 5;
    int b = G::backgroundShade - 15;
    int c = G::backgroundShade + 30;
    int d = G::backgroundShade;
    int e = G::backgroundShade + 10;
    int t = G::textShade;

    QLinearGradient rootCategoryBackground;
    rootCategoryBackground.setStart(0, r.top());
    rootCategoryBackground.setFinalStop(0, r.bottom());
    rootCategoryBackground.setColorAt(0, QColor(a,a,a));
    rootCategoryBackground.setColorAt(1, QColor(b,b,b));

    QColor categoryRowBackground(QColor(d,d,d));
    QColor valueRowBackground(QColor(e,e,e));

    QFont font;
    font = painter->font();
    int fontSize = G::strFontSize.toInt();
    font.setPointSize(fontSize);
    painter->setFont(font);

    QPen catPen(QColor(t,t,t));             // root items same other items
    QPen regPen(QColor(t,t,t));             // other items have silver text
    QPen selPen("#1b8a83");                 // selected items have torquoise text
    QPen disPen(G::disabledColor.name());   // disabled items have gray text
    QPen brdPen(QColor(c,c,c));             // border color

    QString text = index.data().toString();
    QString elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, r.width());

    // replacement decorations
    QPixmap branchClosed(":/images/branch-closed-winnow.png");
    QPixmap branchOpen(":/images/branch-open-winnow.png");

    if (index.data(UR_isHeader).toBool()) {
        // header item in caption column
        if (index.column() == CapColumn) {
            // paint the gradient covering the decoration
            if (index.data(UR_isBackgroundGradient).toBool()) {
                painter->fillRect(r1, rootCategoryBackground);
            }
            // re-instate the decorations
            if (index.data(UR_isDecoration).toBool() && hasChildren) {
                int x = r.x() - 10;
                int y = 0;
                if (isExpanded) {
                    y = r0.top() + r0.height()/2 - 5;
                    painter->drawPixmap(x, y, 9, 9, branchOpen);
                }
                else {
                    y = r0.top() + r0.height()/2 - 5;
                    painter->drawPixmap(x, y, 9, 9, branchClosed);
                }
            }
            // caption text and no borders for root item
//            if (isSelected) painter->setPen(selPen);
//            else painter->setPen(catPen);
            painter->setPen(catPen);
            if (index.data(UR_isDecoration).toBool()) {
                painter->drawText(r4, Qt::AlignVCenter|Qt::TextSingleLine, text);
            }
            else {
                painter->drawText(r2, Qt::AlignVCenter|Qt::TextSingleLine, text);
            }
            // draw separator line if not gradient background
            if (!index.data(UR_isBackgroundGradient).toBool()) {
                painter->setPen(brdPen);
                painter->drawLine(r0.bottomLeft(), r0.bottomRight());
            }
        }
        // header row, but value column, so no decoration to deal with
        else {
            if (index.data(UR_isBackgroundGradient).toBool())
                painter->fillRect(r, rootCategoryBackground);
            else {
                painter->setPen(brdPen);
                painter->drawLine(r.bottomLeft(), r.bottomRight());
            }
        }
    }
    else {
        // Not a header item
        painter->setPen(regPen);

        // caption text and cell borders
        if (index.column() == CapColumn) {
            if (!isAlternatingRows) painter->fillRect(r0, valueRowBackground);
//            if (isSelected) painter->setPen(selPen);
            // disabled?
            if (index.data(UR_isEnabled).toBool() == false) painter->setPen(disPen);
            // indent the text (maybe not if not a header)
            if (index.data((UR_isIndent)).toBool())
                painter->drawText(r3, Qt::AlignVCenter|Qt::TextSingleLine, text);
            else
                painter->drawText(r2, Qt::AlignVCenter|Qt::TextSingleLine, text);
            painter->setPen(brdPen);
            // draw line between column 0 and 1
            if (!hasChildren) painter->drawLine(r0.topRight(), r0.bottomRight());
            // draw bottom line
            painter->setPen(brdPen);
            painter->drawLine(r0.bottomLeft(), r0.bottomRight());
        }
        if (index.column() == ValColumn) {
            if (!isAlternatingRows) painter->fillRect(r, valueRowBackground);
            painter->setPen(brdPen);
            // draw bottom line
            painter->setPen(brdPen);
            painter->drawLine(r.bottomLeft(), r.bottomRight());
        }

    }
    painter->restore();
}
