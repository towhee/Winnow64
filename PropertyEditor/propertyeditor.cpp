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

//    qDebug() << __FUNCTION__ << index.data().toString();

    if (index.parent() == QModelIndex()) {
        qDebug() << __FUNCTION__ << "col =" << index.column() << "r =" << r << "\tr0 =" << r0
                 << index.data().toString();
        if (index.column() == 0) painter->fillRect(r0, categoryBackground);
        if (index.column() == 1) painter->fillRect(r, categoryBackground);
//        painter->fillRect(r, categoryBackground);
//        painter->fillRect(row, categoryBackground);
        painter->setPen(catPen);
    }
    else if (index.column() == 0) painter->setPen(regPen);

    if (index.column() == 0) {
        QString text = index.data().toString();
        QString elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, r.width());
        painter->drawText(r, Qt::AlignVCenter|Qt::TextSingleLine, elidedText);
        painter->setPen(QColor(75,75,75));
        painter->drawRect(r0);
//        painter->drawLine(0, r0.bottom() + 1, r0.width(), r0.bottom() + 1);
    }
    if (index.column() == 1) {
        painter->setPen(QColor(75,75,75));
        painter->drawRect(r);
//        painter->drawLine(r.x(), r.bottom() + 1, r.width(), r0.bottom() + 1);
    }
    painter->restore();

//        QStyledItemDelegate::paint(painter, option, index);
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
    indentation = -10;
//    setIndentation(indentation);
//    setStyleSheet("QTreeView::item {border:1px solid rgb(75,75,75)}");

    model = new QStandardItemModel;
    setModel(model);
    propertyDelegate = new PropertyDelegate(this);
    setItemDelegate(propertyDelegate);
//    setItemDelegateForColumn(1, propertyDelegate);
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
    if (source == "rememberLastDir") {
        mw->rememberLastDir = v.toBool();
    }
    if (source == "useWheelToScroll") {
        if (v.toString() == "Next/previous image") mw->imageView->useWheelToScroll = false;
        else mw->imageView->useWheelToScroll = true;
    }
    if (source == "globalFontSize") {
        mw->setFontSize(v.toInt());
    }
    if (source == "infoOverlayFontSize") {
        qDebug() << __FUNCTION__ << v;
        mw->imageView->infoOverlayFontSize = v.toInt();
        mw->setInfoFontSize();
    }
}

void PropertyEditor::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QTreeView::drawRow(painter, option, index);
//    painter->save();
//    painter->setPen(QColor(75,75,75));
//    painter->drawRect(option.rect);
//    painter->restore();
}

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
            QPoint p = event->pos();
            if (p.x() >= indentation) isExpanded(idx) ? collapse(idx) : expand(idx);
        }
    }
}

void PropertyEditor::addItems()
{
    int col0width = 200;
    int col1width = 200;
    int firstGenerationCount = -1;        // top items
    int secondGenerationCount;       // child items
    int thirdGenerationCount;  // child child items
    QString tooltip;
    QModelIndex idxVal;
    QModelIndex idxCat;

    firstGenerationCount++;
    // HEADER
    // General category
    QStandardItem *generalItem = new QStandardItem;
    generalItem->setText("General");
    generalItem->setData(DT_None, UR_DelegateType);
    generalItem->setData("GENERAL", UR_Source);
    generalItem->setEditable(false);
    model->appendRow(generalItem);
    model->insertColumns(1, 1);
    idxCat = generalItem->index();
    setColumnWidth(0, col0width);
    setColumnWidth(1, col1width);
    secondGenerationCount = -1;

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = rememberFolder
        tooltip = "Remember the last folder used in Winnow from the previous session.";
        QStandardItem *rememberFolderCaption = new QStandardItem;
        rememberFolderCaption->setToolTip(tooltip);
        rememberFolderCaption->setText("Remember folder");
        rememberFolderCaption->setEditable(false);
        QStandardItem *rememberFolderValue = new QStandardItem;
        rememberFolderValue->setToolTip(tooltip);
        rememberFolderValue->setData(mw->rememberLastDir, Qt::EditRole);
        rememberFolderValue->setData(DT_Checkbox, UR_DelegateType);
        rememberFolderValue->setData("rememberLastDir", UR_Source);
        rememberFolderValue->setData("bool", UR_Type);
        generalItem->setChild(secondGenerationCount, 0, rememberFolderCaption);
        generalItem->setChild(secondGenerationCount, 1, rememberFolderValue);
        idxVal = rememberFolderValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

        secondGenerationCount++;
        // Type = COMBOBOX
        // name = useWheelToScroll
        tooltip = "Use the trackpad or mouse wheel to either scroll when zoomed into an image\n"
                  "or to progress forward and backward through the images.";
        QStandardItem *useWheelToScrollCaption = new QStandardItem;
        useWheelToScrollCaption->setToolTip(tooltip);
        useWheelToScrollCaption->setText("Wheel/trackpad");
        useWheelToScrollCaption->setEditable(false);
        QStandardItem *useWheelToScrollValue = new QStandardItem;
        useWheelToScrollValue->setToolTip(tooltip);
        useWheelToScrollValue->setData(mw->imageView->useWheelToScroll, Qt::EditRole);
        useWheelToScrollValue->setData(DT_Combo, UR_DelegateType);
        useWheelToScrollValue->setData("useWheelToScroll", UR_Source);
        useWheelToScrollValue->setData("QString", UR_Type);
        QStringList list = {"Next/previous image", "Scroll current image when zoomed"};
        useWheelToScrollValue->setData(list, UR_StringList);
        generalItem->setChild(secondGenerationCount, 0, useWheelToScrollCaption);
        generalItem->setChild(secondGenerationCount, 1, useWheelToScrollValue);
        idxVal = useWheelToScrollValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

        // HEADER
        // General category::Font size subcategory
        secondGenerationCount++;
        QStandardItem *fontSizeItem = new QStandardItem;
        fontSizeItem->setText("Font size");
        fontSizeItem->setEditable(false);
        fontSizeItem->setData(DT_None, UR_DelegateType);
        generalItem->setChild(secondGenerationCount, 0, fontSizeItem);
        thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = SLIDER
            // name = globalFontSize (for search and replace)
            // parent = fontSizeItem
            tooltip = "Change the font size throughout the application.";
            QStandardItem *globalFontSizeCaption = new QStandardItem;
            globalFontSizeCaption->setToolTip(tooltip);
            globalFontSizeCaption->setText("Global");
            globalFontSizeCaption->setEditable(false);
            QStandardItem *globalFontSizeValue = new QStandardItem;
            globalFontSizeValue->setToolTip(tooltip);
            globalFontSizeValue->setData(G::fontSize, Qt::EditRole);
            globalFontSizeValue->setData(DT_Slider, UR_DelegateType);
            globalFontSizeValue->setData("globalFontSize", UR_Source);
            globalFontSizeValue->setData("int", UR_Type);
            globalFontSizeValue->setData(6, UR_Min);
            globalFontSizeValue->setData(16, UR_Max);
            globalFontSizeValue->setData(50, UR_LabelFixedWidth);
            fontSizeItem->setChild(thirdGenerationCount, 0, globalFontSizeCaption);
            fontSizeItem->setChild(thirdGenerationCount, 1, globalFontSizeValue);
            idxVal = globalFontSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

            thirdGenerationCount++;
            // Type = SLIDER
            // name = infoOverlayFontSize (for search and replace)
            // parent = fontSizeItem
            tooltip = "Change the font size for the info overlay (usually showing the shooting info in the top left of the image).";
            QStandardItem *infoOverlayFontSizeCaption = new QStandardItem;
            infoOverlayFontSizeCaption->setToolTip(tooltip);
            infoOverlayFontSizeCaption->setText("Info overlay");
            infoOverlayFontSizeCaption->setEditable(false);
            QStandardItem *infoOverlayFontSizeValue = new QStandardItem;
            infoOverlayFontSizeValue->setToolTip(tooltip);
            infoOverlayFontSizeValue->setData(mw->imageView->infoOverlayFontSize, Qt::EditRole);
            infoOverlayFontSizeValue->setData(DT_Slider, UR_DelegateType);
            infoOverlayFontSizeValue->setData("infoOverlayFontSize", UR_Source);
            infoOverlayFontSizeValue->setData("int", UR_Type);
            infoOverlayFontSizeValue->setData(6, UR_Min);
            infoOverlayFontSizeValue->setData(30, UR_Max);
            infoOverlayFontSizeValue->setData(50, UR_LabelFixedWidth);
            fontSizeItem->setChild(thirdGenerationCount, 0, infoOverlayFontSizeCaption);
            fontSizeItem->setChild(thirdGenerationCount, 1, infoOverlayFontSizeValue);
            idxVal = infoOverlayFontSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

    // HEADER
    // General category::Badge size subcategory
    secondGenerationCount++;
    QStandardItem *badgeSizeItem = new QStandardItem;
    badgeSizeItem->setText("Classification badge size");
    badgeSizeItem->setEditable(false);
    badgeSizeItem->setData(DT_None, UR_DelegateType);
    generalItem->setChild(secondGenerationCount, 0, badgeSizeItem);
    thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = SLIDER
            // name = thumbBadgeSize (for search and replace)
            // parent = badgeSizeItem
            tooltip = "The image badge is a circle showing the colour classification, rating and pick\n"
                      "status.  It is located in the lower right corner of the image.  This property \n"
                      "allows you to adjust its size.";
            QStandardItem *imBadgeSizeCaption = new QStandardItem;
            imBadgeSizeCaption->setToolTip(tooltip);
            imBadgeSizeCaption->setText("Loupe");
            imBadgeSizeCaption->setEditable(false);
            QStandardItem *imageBadgeSizeValue = new QStandardItem;
            imageBadgeSizeValue->setToolTip(tooltip);
            imageBadgeSizeValue->setData(mw->classificationBadgeInImageDiameter, Qt::EditRole);
            imageBadgeSizeValue->setData(DT_Slider, UR_DelegateType);
            imageBadgeSizeValue->setData("classificationBadgeInImageDiameter", UR_Source);
            imageBadgeSizeValue->setData("int", UR_Type);
            imageBadgeSizeValue->setData(10, UR_Min);
            imageBadgeSizeValue->setData(100, UR_Max);
            imageBadgeSizeValue->setData(50, UR_LabelFixedWidth);
            badgeSizeItem->setChild(thirdGenerationCount, 0, imBadgeSizeCaption);
            badgeSizeItem->setChild(thirdGenerationCount, 1, imageBadgeSizeValue);
            idxVal = imageBadgeSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

            thirdGenerationCount++;
            // Type = SLIDER
            // name = thumbBadgeSize (for search and replace)
            // parent = badgeSizeItem
            tooltip = "The image badge is a circle showing the colour classification, rating and pick\n"
                      "status.  It is located in the top right corner of the thumbnail in the thumb \n"
                      "and grid views.  This property allows you to adjust its size.";
            QStandardItem *thumbBadgeSizeCaption = new QStandardItem;
            thumbBadgeSizeCaption->setToolTip(tooltip);
            thumbBadgeSizeCaption->setText("Thumbnail");
            thumbBadgeSizeCaption->setEditable(false);
            QStandardItem *thumbBadgeSizeValue = new QStandardItem;
            thumbBadgeSizeValue->setToolTip(tooltip);
            thumbBadgeSizeValue->setData(mw->classificationBadgeInThumbDiameter, Qt::EditRole);
            thumbBadgeSizeValue->setData(DT_Slider, UR_DelegateType);
            thumbBadgeSizeValue->setData("classificationBadgeInThumbDiameter", UR_Source);
            thumbBadgeSizeValue->setData("int", UR_Type);
            thumbBadgeSizeValue->setData(10, UR_Min);
            thumbBadgeSizeValue->setData(100, UR_Max);
            thumbBadgeSizeValue->setData(50, UR_LabelFixedWidth);
            badgeSizeItem->setChild(thirdGenerationCount, 0, thumbBadgeSizeCaption);
            badgeSizeItem->setChild(thirdGenerationCount, 1, thumbBadgeSizeValue);
            idxVal = thumbBadgeSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

    firstGenerationCount++;
    // HEADER
    // Thumbnail category
    QStandardItem *thumbnailCatItem = new QStandardItem;
    thumbnailCatItem->setText("Thumbnails");
    thumbnailCatItem->setEditable(false);
    thumbnailCatItem->setData(DT_None, UR_DelegateType);
    model->appendRow(thumbnailCatItem);
//    model->insertColumns(1, 1);
    setColumnWidth(0, col0width);
    setColumnWidth(1, col1width);
    secondGenerationCount = -1;

        secondGenerationCount++;
        // Type = SPINBOX
        // name = maximumIconSize
        // parent = thumbnailCatItem
        tooltip = "Enter the maximum size in pixel width for thumbnails.  Icons will be \nt"
                  "created a this size.  The memory requirements increase at the square of \n"
                  "the size, and folders can contain thousands of images.\n\n"
                  "WARNING: Larger thumbnail sizes can consume huge amounts of memory.  The\n"
                  "default size is 256 pixels to a side.";
        QStandardItem *maximumIconSizeCaption = new QStandardItem;
        maximumIconSizeCaption->setToolTip(tooltip);
        maximumIconSizeCaption->setText("Maximum size");
        maximumIconSizeCaption->setEditable(false);
        QStandardItem *maximumIconSizeValue = new QStandardItem;
        maximumIconSizeValue->setToolTip(tooltip);
        maximumIconSizeValue->setData(G::maxIconSize, Qt::EditRole);
        maximumIconSizeValue->setData(DT_Spinbox, UR_DelegateType);
        maximumIconSizeValue->setData("maxIconSize", UR_Source);
        maximumIconSizeValue->setData("int", UR_Type);
        maximumIconSizeValue->setData(40, UR_Min);
        maximumIconSizeValue->setData(640, UR_Max);
        thumbnailCatItem->setChild(secondGenerationCount, 0, maximumIconSizeCaption);
        thumbnailCatItem->setChild(secondGenerationCount, 1, maximumIconSizeValue);
        idxVal = maximumIconSizeValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

        // HEADER
        // Thumbnails:: film strip subcategory
        secondGenerationCount++;
        QStandardItem *thumbnailFilmStripCatItem = new QStandardItem;
        thumbnailFilmStripCatItem->setText("Film strip");
        thumbnailFilmStripCatItem->setEditable(false);
        thumbnailFilmStripCatItem->setData(DT_None, UR_DelegateType);
        thumbnailCatItem->setChild(secondGenerationCount, 0, thumbnailFilmStripCatItem);
        thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = SLIDER
            // name = thumbViewIconSize (for search and replace)
            // parent = thumbnailFilmStripCatItem
            tooltip = "Change the display size of the thumbnails in the film strip.";
            QStandardItem *thumbViewIconSizeCaption = new QStandardItem;
            thumbViewIconSizeCaption->setToolTip(tooltip);
            thumbViewIconSizeCaption->setText("Size");
            thumbViewIconSizeCaption->setEditable(false);
            QStandardItem *thumbViewIconSizeValue = new QStandardItem;
            thumbViewIconSizeValue->setToolTip(tooltip);
            thumbViewIconSizeValue->setData(120, Qt::EditRole);  // figure this out
            thumbViewIconSizeValue->setData(DT_Slider, UR_DelegateType);
            thumbViewIconSizeValue->setData("thumbViewIconSize", UR_Source);
            thumbViewIconSizeValue->setData("int", UR_Type);
            thumbViewIconSizeValue->setData(6, UR_Min);
            thumbViewIconSizeValue->setData(16, UR_Max);
            thumbViewIconSizeValue->setData(50, UR_LabelFixedWidth);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 0, thumbViewIconSizeCaption);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 1, thumbViewIconSizeValue);
            idxVal = thumbViewIconSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

            thirdGenerationCount++;
            // Type = SLIDER
            // name = thumbViewLabelSize (for search and replace)
            // parent = thumbnailFilmStripCatItem
            tooltip = "Change the display size of the thumbnails in the film strip.";
            QStandardItem *thumbViewLabelSizeCaption = new QStandardItem;
            thumbViewLabelSizeCaption->setToolTip(tooltip);
            thumbViewLabelSizeCaption->setText("Label size");
            thumbViewLabelSizeCaption->setEditable(false);
            QStandardItem *thumbViewLabelSizeValue = new QStandardItem;
            thumbViewLabelSizeValue->setToolTip(tooltip);
            thumbViewLabelSizeValue->setData(mw->thumbView->labelFontSize, Qt::EditRole);  // figure this out
            thumbViewLabelSizeValue->setData(DT_Slider, UR_DelegateType);
            thumbViewLabelSizeValue->setData("thumbViewLabelSize", UR_Source);
            thumbViewLabelSizeValue->setData("int", UR_Type);
            thumbViewLabelSizeValue->setData(6, UR_Min);
            thumbViewLabelSizeValue->setData(16, UR_Max);
            thumbViewLabelSizeValue->setData(50, UR_LabelFixedWidth);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 0, thumbViewLabelSizeCaption);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 1, thumbViewLabelSizeValue);
            idxVal = thumbViewLabelSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

            thirdGenerationCount++;
            // Type = CHECKBOX
            // name = thumbViewShowLabel
            tooltip = "Remember the last folder used in Winnow from the previous session.";
            QStandardItem *thumbViewShowLabelCaption = new QStandardItem;
            thumbViewShowLabelCaption->setToolTip(tooltip);
            thumbViewShowLabelCaption->setText("Show label");
            thumbViewShowLabelCaption->setEditable(false);
            QStandardItem *thumbViewShowLabelValue = new QStandardItem;
            thumbViewShowLabelValue->setToolTip(tooltip);
            thumbViewShowLabelValue->setData(mw->thumbView->showIconLabels, Qt::EditRole);
            thumbViewShowLabelValue->setData(DT_Checkbox, UR_DelegateType);
            thumbViewShowLabelValue->setData("thumbViewShowLabel", UR_Source);
            thumbViewShowLabelValue->setData("bool", UR_Type);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 0, thumbViewShowLabelCaption);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 1, thumbViewShowLabelValue);
            idxVal = thumbViewShowLabelValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

            // HEADER
            // Thumbnails:: grid subcategory
            secondGenerationCount++;
            QStandardItem *thumbnailGridCatItem = new QStandardItem;
            thumbnailGridCatItem->setText("Grid");
            thumbnailGridCatItem->setEditable(false);
            thumbnailGridCatItem->setData(DT_None, UR_DelegateType);
            thumbnailCatItem->setChild(secondGenerationCount, 0, thumbnailGridCatItem);
            thirdGenerationCount = -1;

                thirdGenerationCount++;
                // Type = SLIDER
                // name = gridViewIconSize (for search and replace)
                // parent = thumbnailGridCatItem
                tooltip = "Change the display size of the thumbnails in the film strip.";
                QStandardItem *gridViewIconSizeCaption = new QStandardItem;
                gridViewIconSizeCaption->setToolTip(tooltip);
                gridViewIconSizeCaption->setText("Size");
                gridViewIconSizeCaption->setEditable(false);
                QStandardItem *gridViewIconSizeValue = new QStandardItem;
                gridViewIconSizeValue->setToolTip(tooltip);
                gridViewIconSizeValue->setData(120, Qt::EditRole);  // figure this out
                gridViewIconSizeValue->setData(DT_Slider, UR_DelegateType);
                gridViewIconSizeValue->setData("gridViewIconSize", UR_Source);
                gridViewIconSizeValue->setData("int", UR_Type);
                gridViewIconSizeValue->setData(6, UR_Min);
                gridViewIconSizeValue->setData(16, UR_Max);
                gridViewIconSizeValue->setData(50, UR_LabelFixedWidth);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 0, gridViewIconSizeCaption);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 1, gridViewIconSizeValue);
                idxVal = gridViewIconSizeValue->index();
                propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

                thirdGenerationCount++;
                // Type = SLIDER
                // name = gridViewLabelSize (for search and replace)
                // parent = thumbnailGridCatItem
                tooltip = "Change the display size of the thumbnails in the film strip.";
                QStandardItem *gridViewLabelSizeCaption = new QStandardItem;
                gridViewLabelSizeCaption->setToolTip(tooltip);
                gridViewLabelSizeCaption->setText("Label size");
                gridViewLabelSizeCaption->setEditable(false);
                QStandardItem *gridViewLabelSizeValue = new QStandardItem;
                gridViewLabelSizeValue->setToolTip(tooltip);
                gridViewLabelSizeValue->setData(mw->gridView->labelFontSize, Qt::EditRole);  // figure this out
                gridViewLabelSizeValue->setData(DT_Slider, UR_DelegateType);
                gridViewLabelSizeValue->setData("gridViewLabelSize", UR_Source);
                gridViewLabelSizeValue->setData("int", UR_Type);
                gridViewLabelSizeValue->setData(6, UR_Min);
                gridViewLabelSizeValue->setData(16, UR_Max);
                gridViewLabelSizeValue->setData(50, UR_LabelFixedWidth);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 0, gridViewLabelSizeCaption);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 1, gridViewLabelSizeValue);
                idxVal = gridViewLabelSizeValue->index();
                propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

                thirdGenerationCount++;
                // Type = CHECKBOX
                // name = gridViewShowLabel
                tooltip = "Remember the last folder used in Winnow from the previous session.";
                QStandardItem *gridViewShowLabelCaption = new QStandardItem;
                gridViewShowLabelCaption->setToolTip(tooltip);
                gridViewShowLabelCaption->setText("Show label");
                gridViewShowLabelCaption->setEditable(false);
                QStandardItem *gridViewShowLabelValue = new QStandardItem;
                gridViewShowLabelValue->setToolTip(tooltip);
                gridViewShowLabelValue->setData(mw->gridView->showIconLabels, Qt::EditRole);
                gridViewShowLabelValue->setData(DT_Checkbox, UR_DelegateType);
                gridViewShowLabelValue->setData("gridViewShowLabel", UR_Source);
                gridViewShowLabelValue->setData("bool", UR_Type);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 0, gridViewShowLabelCaption);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 1, gridViewShowLabelValue);
                idxVal = gridViewShowLabelValue->index();
                propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

    firstGenerationCount++;
    // HEADER
    // Cache category
    QStandardItem *cacheCatItem = new QStandardItem;
    cacheCatItem->setText("Cache");
    cacheCatItem->setEditable(false);
    cacheCatItem->setData(DT_None, UR_DelegateType);
    model->appendRow(cacheCatItem);
    //    model->insertColumns(1, 1);
    setColumnWidth(0, col0width);
    setColumnWidth(1, col1width);
    secondGenerationCount = -1;

        // HEADER
        // Cache:: Metadata subcategory
        secondGenerationCount++;
        QStandardItem *MetadataCatItem = new QStandardItem;
        MetadataCatItem->setText("Metadata");
        MetadataCatItem->setEditable(false);
        MetadataCatItem->setData(DT_None, UR_DelegateType);
        cacheCatItem->setChild(secondGenerationCount, 0, MetadataCatItem);
        thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = COMBO
            // name = metadataCacheStrategy (for search and replace)
            tooltip = "If you cache the metadata for all the images in the folder(s) it will take\n"
                      "longer to initially to get started but performance might be better.  Alternatively\n"
                      "you can incrementally load the metadata as needed, and for larger folders with\n"
                      "thousands of images this might be quicker.";
            QStandardItem *metadataCacheStrategyCaption = new QStandardItem;
            metadataCacheStrategyCaption->setToolTip(tooltip);
            metadataCacheStrategyCaption->setText("Strategy");
            metadataCacheStrategyCaption->setEditable(false);
            QStandardItem *metadataCacheStrategyValue = new QStandardItem;
            metadataCacheStrategyValue->setToolTip(tooltip);
            metadataCacheStrategyValue->setData("Incremental", Qt::EditRole); // ** no MW variable yet
            metadataCacheStrategyValue->setData(DT_Combo, UR_DelegateType);
            metadataCacheStrategyValue->setData("metadataCacheStrategy", UR_Source);
            metadataCacheStrategyValue->setData("QString", UR_Type);
            QStringList list1 = {"All", "Incremental"};
            metadataCacheStrategyValue->setData(list1, UR_StringList);
            MetadataCatItem->setChild(thirdGenerationCount, 0, metadataCacheStrategyCaption);
            MetadataCatItem->setChild(thirdGenerationCount, 1, metadataCacheStrategyValue);
            idxVal = metadataCacheStrategyValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

        // HEADER
        // Cache:: CacheThumbnail subcategory
        secondGenerationCount++;
        QStandardItem *cacheThumbnailCatItem = new QStandardItem;
        cacheThumbnailCatItem->setText("Thumbnails");
        cacheThumbnailCatItem->setEditable(false);
        cacheThumbnailCatItem->setData(DT_None, UR_DelegateType);
        cacheCatItem->setChild(secondGenerationCount, 0, cacheThumbnailCatItem);
        thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = COMBO
            // name = thumbnailCacheStrategy
            tooltip = "If you cache the thumbnails for all the images in the folder(s) it will take\n"
                      "longer to initially to get started and can consume huge amounts of memory \n"
                      "but performance might be better while scrolling.  Alternatively you can\n"
                      "incrementally load the thumbnails as needed, and for larger folders with\n"
                      "thousands of images this is usually the better choice.";
            QStandardItem *thumbnailCacheStrategyCaption = new QStandardItem;
            thumbnailCacheStrategyCaption->setToolTip(tooltip);
            thumbnailCacheStrategyCaption->setText("Strategy");
            thumbnailCacheStrategyCaption->setEditable(false);
            QStandardItem *thumbnailCacheStrategyValue = new QStandardItem;
            thumbnailCacheStrategyValue->setToolTip(tooltip);
            thumbnailCacheStrategyValue->setData("Incremental", Qt::EditRole); // ** no MW variable yet
            thumbnailCacheStrategyValue->setData(DT_Combo, UR_DelegateType);
            thumbnailCacheStrategyValue->setData("thumbnailCacheStrategy", UR_Source);
            thumbnailCacheStrategyValue->setData("QString", UR_Type);
            QStringList list2 = {"All", "Incremental"};
            thumbnailCacheStrategyValue->setData(list1, UR_StringList);
            cacheThumbnailCatItem->setChild(thirdGenerationCount, 0, thumbnailCacheStrategyCaption);
            cacheThumbnailCatItem->setChild(thirdGenerationCount, 1, thumbnailCacheStrategyValue);
            idxVal = thumbnailCacheStrategyValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);

            thirdGenerationCount++;
            // Type = SPINBOX
            // name = metadataChunkSize
            // parent = cacheThumbnailCatItem
            tooltip = "Enter the number of thumbnails and metadata you want to cache.  If the\n"
                      "grid is displaying a larger number then the larger number will be used\n"
                      "to make sure they are all shown.  You can experiment to see what works\n"
                      "best.  250 is the default amount.";
            QStandardItem *metadataChunkSizeCaption = new QStandardItem;
            metadataChunkSizeCaption->setToolTip(tooltip);
            metadataChunkSizeCaption->setText("Chunk size");
            metadataChunkSizeCaption->setEditable(false);
            QStandardItem *metadataChunkSizeValue = new QStandardItem;
            metadataChunkSizeValue->setToolTip(tooltip);
            metadataChunkSizeValue->setData(mw->metadataCacheThread->metadataChunkSize, Qt::EditRole);
            metadataChunkSizeValue->setData(DT_Spinbox, UR_DelegateType);
            metadataChunkSizeValue->setData("metadataChunkSize", UR_Source);
            metadataChunkSizeValue->setData("int", UR_Type);
            metadataChunkSizeValue->setData(1, UR_Min);
            metadataChunkSizeValue->setData(3000, UR_Max);
            cacheThumbnailCatItem->setChild(thirdGenerationCount, 0, metadataChunkSizeCaption);
            cacheThumbnailCatItem->setChild(thirdGenerationCount, 1, metadataChunkSizeValue);
            idxVal = metadataChunkSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, idxVal);


}
