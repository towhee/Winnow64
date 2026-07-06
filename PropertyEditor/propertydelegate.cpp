#include "propertydelegate.h"
#include "Main/global.h"

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

QSize PropertyDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // row height = 1.7 * text height
    if (isDebug)
        qDebug() << "PropertyDelegate::sizeHint"
                 << "option.rect.width =" << option.rect.width()
                 << "option.rect.height =" << option.rect.height()
            ;

    int height = static_cast<int>(G::strFontSize.toInt() * 1.7 * G::ptToPx);

    /* Grow the row to fit wrapped caption text. Only the caption column wraps
    (headers stay single-line); the tree row uses the tallest column's hint. */
    if (index.column() == CapColumn && !index.data(UR_isHeader).toBool()) {
        QFont font = option.font;
        font.setPointSize(G::strFontSize.toInt());
        QFontMetrics fm(font);

        /* Match the text width paint() actually wraps within. Indented items
        (r3) draw from their indented x to the column's right edge, so subtract
        the indentation; non-indented items (r2) draw from a 5px left inset. */
        const QTreeView *view = qobject_cast<const QTreeView*>(parent());
        int colWidth = view ? view->columnWidth(CapColumn) : option.rect.width();
        int textLeft = 5;
        if (view && index.data(UR_isIndent).toBool()) {
            int depth = 0;
            for (QModelIndex p = index.parent(); p.isValid(); p = p.parent()) depth++;
            if (index.data(UR_ExtraIndent).toBool()) depth++;   // match the paint() extra level
            textLeft = (depth + 1) * view->indentation();   // rootIsDecorated
        }
        int width = colWidth - textLeft - 4;    // small safety margin vs paint
        if (width < 1) width = option.rect.width();

        QRect bounding = fm.boundingRect(QRect(0, 0, width, 0),
                                         Qt::AlignVCenter | Qt::TextWordWrap,
                                         index.data().toString());
        int padding = height - fm.height();     // keep the same vertical breathing room
        height = qMax(height, bounding.height() + padding);
    }

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
    qDebug().noquote()
        << "PropertyDelegate::paint" << i++
        << QString::number(i).rightJustified(2)
        << QString::number(index.row()).rightJustified(2)
        << index.column()
        << option.rect
        << "Value =" << index.data().toString().leftJustified(25)
        << index.data(UR_Name).toString().leftJustified(25)
        << "Delegate =" << index.data(UR_DelegateType).toInt()
        << "isSelected =" << isSelected
           ;
           // */

    /* Root rows are highlighted with a darker gradient and the decoration, which gets covered
    up, and is repainted */
    QRect r = option.rect;
    /* Full column widths, taken from the view so they do not depend on paint order. The previous
       approach cached r.width() into static w0/w1 as each column painted, but for column 0 Qt
       shrinks the cell rect by the row's indentation, so indented child rows could leave w0 holding
       a too-small value for a later full-width caption (r4 = w0 + w1). Reading columnWidth() is
       order-independent and always correct. */
    int w0 = 100, w1 = 200;
    if (const QTreeView *view = qobject_cast<const QTreeView*>(option.widget)) {
        w0 = view->columnWidth(0);
        w1 = view->columnWidth(1);
    }
    else {
        if (index.column() == 0) w0 = r.width();
        if (index.column() == 1) w1 = r.width();
    }
    // r0 extends the rect over the decoration to the left margin
    QRect r0 = QRect(0, r.y(), r.x() + r.width(), r.height());
    // r2 = r0 but leaves 1 pixel at the left, right and bottom margins to draw text
    QRect r2 = QRect(5, r.y(), r.x() + r.width() - 5, r.height()-1);
    // r3 = r but leaves a few pixels at the bottom margin to draw text
    QRect r3 = QRect(r.x(), r.y(), r.width(), r.height()-3);
    // r4 = entire row width
    QRect r4 = QRect(r.x(), r.y(), w0 + w1, r.height()-3);
    // r5 = entire col width less 50px for barbtns
    QRect r5 = QRect(r.x() + w1 - 50, r.y(), 50, r.height()-3);

    /*
    // static int i = 0;
    if (index.row() == 0 && index.parent().isValid())   // only row 1 excluding headers
    qDebug().noquote()
        << "PropertyDelegate::paint"
        // << QString::number(i).rightJustified(2)
        << QString::number(index.row()).rightJustified(2)
        << index.column()
        << "w0 =" << QString::number(w0).rightJustified(3)
        << "w1 =" << QString::number(w1).rightJustified(3)
        << "Value =" << index.data().toString().leftJustified(25)
        << index.data(UR_Name).toString().leftJustified(25)
        << "Delegate =" << index.data(UR_DelegateType).toInt()
        // << "isSelected =" << isSelected
        ;
        // */

    int a = G::backgroundShade + 5;
    int b = G::backgroundShade - 15;
    int c = G::backgroundShade + 30;
    int d = G::backgroundShade;
    int e = G::backgroundShade + 10;
    int t = G::textShade;
    int u = G::textShade - 30;          // leaf items

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

    QPen catPen(G::header2Color);           // root items l
    QPen hdrPen(G::header3Color);           // root items same other items
    QPen regPen(QColor(t,t,t));             // other items have silver text
    QPen selPen("#1b8a83");                 // selected items have torquoise text
    QPen disPen(G::disabledColor.name());   // disabled items have gray text
    // QPen brdPen(QColor(c,c,c));             // border color
    QPen brdPen(Qt::NoPen);                 // hide border

    QString text = index.data().toString();
    QString elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, r.width());

    // replacement decorations
    QPixmap branchClosed(":/images/branch-closed-winnow.png");
    QPixmap branchOpen(":/images/branch-open-winnow.png");

    if (index.data(UR_isHeader).toBool()) {
        /* The caption lives in column 0; the value-column branch below needs both the caption text
           and its role flags from this sibling (in the caption-column branch it IS index). */
        QModelIndex capIndex = index.sibling(index.row(), CapColumn);
        const QString capText = capIndex.data().toString();
        /* Header caption pen. UR_LeafSingleLine rows want the header's single-line full-width
           layout but the ordinary LEAF text colour (not category teal). */
        QPen capPen = capIndex.data(UR_LeafSingleLine).toBool() ? regPen : catPen;

        /* Right edge for a header caption that spans the row, in VIEWPORT coordinates so it is the
           same value whichever column is painting. The caption spans the full row (w0 + w1) but is
           pulled in before any right-aligned value-column widget (e.g. -/+ buttons) so a long
           caption never runs under it. */
        auto headerCapRight = [&]() -> int {
            QModelIndex valIndex = index.sibling(index.row(), ValColumn);
            QWidget *valEditor =
                static_cast<QWidget*>(valIndex.data(UR_Editor).value<void*>());
            if (valEditor && valEditor->isVisible()) {
                QRect kids = valEditor->childrenRect();
                if (!kids.isEmpty())
                    return valEditor->geometry().x() + kids.x() - 6;
            }
            return w0 + w1;
        };

        /* Caption geometry, in VIEWPORT coordinates so it is identical whichever column is
           painting. capLeft for a decoration row is its column-0 cell left (= indentation), taken
           from the view so the value-column pass can reproduce it without column-0's rect. */
        const QTreeView *tv = qobject_cast<const QTreeView*>(option.widget);
        const bool deco = capIndex.data(UR_isDecoration).toBool();
        int capLeft = deco ? (tv ? tv->visualRect(capIndex).x() : r4.x()) : r2.x();
        /* Reserve room at the right for the delegate-drawn glyphs: [-] alone, or [+][-] when both. */
        int glyphSlots = (capIndex.data(UR_DeleteBtn).toBool() ? 1 : 0)
                       + (capIndex.data(UR_AddBtn).toBool()    ? 1 : 0);
        int capRight = glyphSlots > 0
                           ? r.right() - glyphSlots * (16 + 4) - 6   // before the glyph cluster (spanned rows)
                           : headerCapRight();

        /* The caption is drawn in BOTH the caption-column and value-column passes. The painter is
           not clipped to the cell, so a single pass would suffice for a full repaint, but resizing
           the (stretch) value column triggers a value-column-ONLY repaint that refills that cell's
           background over the caption's overflow. Redrawing the same elided text (same viewport
           rect) in the value-column pass restores it. */
        auto drawHeaderCaption = [&]() {
            painter->setPen(capPen);
            QRect rCap(capLeft, r4.y(), qMax(0, capRight - capLeft), r4.height());
            const QString cap =
                painter->fontMetrics().elidedText(capText, Qt::ElideRight, rCap.width());
            painter->drawText(rCap, Qt::AlignVCenter|Qt::TextSingleLine, cap);
        };

        // header item in caption column
        if (index.column() == CapColumn) {
            // paint the gradient covering the decoration (caption column)
            if (index.data(UR_isBackgroundGradient).toBool()) {
                QRect capFill(1, r.y(), w0 - 1, r.height());
                painter->fillRect(capFill, rootCategoryBackground);
            }
            // re-instate the decorations (UR_ShowDecoration forces the arrow even with no children,
            // e.g. an unselected mask-tool row that reveals its settings only when clicked open)
            if (deco && (hasChildren || capIndex.data(UR_ShowDecoration).toBool())) {
                int x = r.x() - 10;
                int y = r0.top() + r0.height()/2 - 5;
                painter->drawPixmap(x, y, 9, 9, isExpanded ? branchOpen : branchClosed);
            }
            drawHeaderCaption();
            // draw separator line if not gradient background
            if (!index.data(UR_isBackgroundGradient).toBool()) {
                painter->setPen(brdPen);
                painter->drawLine(r0.bottomLeft(), r0.bottomRight());
            }
            /* Delegate-drawn [+] add / [-] remove glyphs at the row's right edge ([-] at the edge,
               [+] one slot to its left). Used by full-width spanned rows (e.g. Develop mask-tool
               rows) that cannot host a value-column button widget without it covering/clipping the
               single-line caption. Clicks are hit-tested by the view (see
               DevelopProperties::mousePressEvent). */
            if (index.data(UR_DeleteBtn).toBool() || index.data(UR_AddBtn).toBool()) {
                const int sz = 16;
                int gy = r.top() + (r.height() - sz)/2;
                painter->setOpacity(G::iconOpacity);
                /* [-] sits at the right edge; [+] (when present) sits one slot to its left. */
                int gx = r.right() - sz - 4;
                if (index.data(UR_DeleteBtn).toBool()) {
                    painter->drawPixmap(gx, gy, sz, sz, QPixmap(":/images/icon16/delete.png"));
                    gx -= sz + 4;
                }
                if (index.data(UR_AddBtn).toBool())
                    painter->drawPixmap(gx, gy, sz, sz, QPixmap(":/images/icon16/new.png"));
                painter->setOpacity(1.0);
            }
        }
        // header row, value column
        else {
            // fill the value-column background, then redraw the spanned caption over it
            if (index.data(UR_isBackgroundGradient).toBool())
                painter->fillRect(r, rootCategoryBackground);
            else {
                painter->setPen(brdPen);
                painter->drawLine(r.bottomLeft(), r.bottomRight());
            }
            drawHeaderCaption();
        }
    }
    else {
        // Not a header item
        if (hasChildren) painter->setPen(hdrPen);
        else painter->setPen(regPen);

        // caption text and cell borders
        if (index.column() == CapColumn) {
            if (!isAlternatingRows) painter->fillRect(r0, valueRowBackground);
//            if (isSelected) painter->setPen(selPen);
            // disabled?
            if (index.data(UR_isEnabled).toBool() == false) painter->setPen(disPen);
            // indent the text (maybe not if not a header)
            if (index.data((UR_isIndent)).toBool()) {
                QRect ri = r3;
                /* One extra indent level for root leaves that should line up with a section's
                   children (e.g. Develop's Demosaic / Denoise raw align with the Basic sliders). */
                if (index.data(UR_ExtraIndent).toBool()) {
                    const QTreeView *v = qobject_cast<const QTreeView*>(option.widget);
                    ri.adjust(v ? v->indentation() : 12, 0, 0, 0);
                }
                painter->drawText(ri, Qt::AlignVCenter|Qt::TextWordWrap, text);
            }
            else
                painter->drawText(r2, Qt::AlignVCenter|Qt::TextWordWrap, text);
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
