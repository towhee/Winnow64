#include "thumbviewdelegate.h"

/*
ThumbViewDelegate Anatomy:


|-----------------------thumbSpace-----------------------|
|       itemPadding                                      |
|  ||===================itemBorder===================||  |
|  ||   thumbPadding                                 ||  |
|  ||  ||==============thumbBorder===============||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||            thumb/icon                  ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||                                        ||  ||  |
|  ||  ||==============thumbBorder===============||  ||  |
|  ||   thumbPadding                                 ||  |
|  ||------------------------------------------------||  |
|  ||                     label*                     ||  |
|  ||===================itemBorder===================||  |
|       itemPadding                                      |
|-----------------------thumbSpace-----------------------|

* label border has 0 thickness

thumbSpaceWidth = itemPadding * 2 + itemBorderThickness * 2 + thumbPadding * 2
                  + thumbBorderThickness * 2 + thumbWidth

thumbSpaceHeight = itemPadding * 2 + itemBorderThickness * 2 + thumbPadding * 2
                  + thumbBorderThickness * 2 + thumbHeight + labelHeight

thumbMargin = thumbSpace - thumb (all the space between the thumb and thumbSpace

*/

ThumbViewDelegate::ThumbViewDelegate(QObject *parent)
{
    parent->isWidgetType();     // suppress compiler warning
    itemBorderThickness = 2;
    thumbBorderThickness = 2;
    thumbBorderPadding = 1;         // allow small gap between thumb and outer border
}

void ThumbViewDelegate::setThumbDimensions(int thumbWidth, int thumbHeight,
         int thumbPadding, int labelFontSize, bool showThumbLabels)
{
    {
    #ifdef ISDEBUG
//    qDebug() << "ThumbViewDelegate::setThumbDimensions";
    #endif
    }
    delegateShowThumbLabels = showThumbLabels;
    itemPadding = thumbPadding;
    font = QApplication::font();
    font.setPixelSize(labelFontSize);
    QFontMetrics fm(font);
    fontHt = fm.height();
    if (thumbWidth < 40) thumbWidth = 40;
    if (thumbHeight < 40) thumbHeight = 40;

    thumbSize.setWidth(thumbWidth);
    thumbSize.setHeight(thumbHeight);
    thumbSpace.setWidth(thumbWidth
                        + thumbBorderThickness*2
                        + thumbBorderPadding*2
                        + itemBorderThickness*2
                        + itemPadding*2);
    thumbSpace.setHeight(thumbHeight
                         + thumbBorderThickness*2
                         + thumbBorderPadding*2
                         + itemBorderThickness*2
                         + itemPadding*2);
    if (delegateShowThumbLabels)
        thumbSpace.setHeight(thumbSize.height()
                             + thumbBorderThickness*2
                             + thumbBorderPadding*2
                             + fontHt
                             + itemBorderThickness*2
                             + itemPadding*2);
//    reportThumbAttributes();
}

void ThumbViewDelegate::reportThumbAttributes()
{
   qDebug() << "thumbSize    =" << thumbSize
            << "thumbSpace   =" << thumbSpace
            << "thumbPadding =" << itemPadding
            << "Font height =" << fontHt;
}

QSize ThumbViewDelegate::getThumbCell()
{
    return thumbSpace;
}

int ThumbViewDelegate::getThumbHeightFromAvailHeight(int availHeight)
{
    int thumbHeight = availHeight - itemPadding*2 - itemBorderThickness*2
                  - thumbBorderPadding*2 - thumbBorderThickness*2;
    if(delegateShowThumbLabels) thumbHeight -= fontHt;
    return thumbHeight <= 160 ? thumbHeight : 160;
}

void ThumbViewDelegate::onCurrentChanged(QModelIndex current, QModelIndex /*previous*/)
{
    currentRow = current.row();     // this slot not being used
    qDebug() << "TestTestTest";
//    currentIndex = current;
}

QSize ThumbViewDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                              const QModelIndex& /*index*/) const
{
    {
    #ifdef ISDEBUG
//    qDebug() << "ThumbViewDelegate::sizeHint" << index.data(Qt::DisplayRole);
    #endif
    }
//    option.HasCheckIndicator;           // suppress compiler warning
//    index.isValid();                    // suppress compiler warning
    QFont font = QApplication::font();
//    font.setPixelSize(9);
    QFontMetrics fm(font);
    return thumbSpace;
}

void ThumbViewDelegate::paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const
{
/* The delegate cell size is defined in setThumbDimensions and assigned in sizeHint.
The thumbSize cell contains a number of cells or rectangles:

Outer dimensions = thumbSpace or option.rect
itemRect         = thumbSpace - itemBorderThickness
thumbRect        = itemRect - thumbBorderGap - padding - thumbBorderThickness
iconRect         = thumbRect - icon (icon has different aspect so either the
                   width or height will have to be centered inside the thumbRect
textRect         = a rectangle below itemRect
*/
    {
    #ifdef ISDEBUG
//    qDebug() << "ThumbViewDelegate::paint" << index.data(Qt::DisplayRole);
    #endif
    }
//    qDebug() << "ThumbViewDelegate::paint" << index.data(Qt::DisplayRole);
    painter->save();

    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    QSize iconsize = icon.actualSize(thumbSize);

//    int currentRow = currentIndex.row();

    // get data from model
    int row = index.row();
    QString fName = index.model()->index(row, G::NameColumn).data(Qt::DisplayRole).toString();
//    QString fName = index.model()->index(row, G::PathColumn).data(G::FilePathRole).toString();
    QString labelColor = index.model()->index(row, G::LabelColumn).data(Qt::EditRole).toString();
    QString rating = index.model()->index(row, G::RatingColumn).data(Qt::EditRole).toString();
    bool isPicked = index.model()->index(row, G::PickColumn).data(Qt::EditRole).toBool();

//    qDebug() << "ThumbViewDelegate::paint" << fName
//             << "current index" << currentIndex
//             << "index" << index;

    // define some offsets
    QPoint itemBorderOffset(itemBorderThickness, itemBorderThickness);
    QPoint thumbGapOffset(thumbBorderPadding, thumbBorderPadding);
    QPoint thumbBorderOffset(thumbBorderThickness, thumbBorderThickness);
    QPoint paddingOffset(itemPadding, itemPadding);

    // Make the item border rect smaller to accommodate the border.
    QRect itemRect(option.rect.topLeft() + itemBorderOffset,
                   option.rect.bottomRight() - itemBorderOffset);

    // The thumb rect is padded inside the item rect
    QRect thumbRect(itemRect.topLeft() + thumbBorderOffset +
                    paddingOffset + thumbGapOffset, thumbSize);

    // the icon rect is aligned within the thumb rect
    int alignVertPad = (thumbRect.height() - iconsize.height()) / 2;
    int alignHorPad = (thumbRect.width() - iconsize.width()) / 2;
    QRect iconRect(thumbRect.left() + alignHorPad, thumbRect.top() + alignVertPad,
                   iconsize.width(), iconsize.height());

    // label/rating rect located top-right as containment for circle
    int ratingDiam = 18;
    QPoint ratingTopLeft(option.rect.right() - ratingDiam, option.rect.top());
    QPoint ratingBottomRight(option.rect.right(), option.rect.top() + ratingDiam);
    QRect ratingRect(ratingTopLeft, ratingBottomRight);

    QPainterPath iconPath;
    iconPath.addRoundRect(iconRect, 12, 12);

    QRect textRect = itemRect;
    textRect.setTop(itemRect.bottom()-fontHt-itemPadding);
    QPainterPath textPath;
    textPath.addRoundedRect(textRect, 8, 8);

    // start painting

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    if (delegateShowThumbLabels) {
//        QFont thumbFont = font;
//        thumbFont.setPixelSize(9);
        painter->setFont(font);
        painter->drawText(textRect, Qt::AlignHCenter, fName);
    }

    painter->setClipping(true);
    painter->setClipPath(iconPath);
    painter->drawPixmap(iconRect, icon.pixmap(iconsize.width(),iconsize.height()));
    painter->setClipping(false);

    // define colors
    QColor defaultBorderColor(95, 95, 95, 255);
    QColor currentItemColor(Qt::yellow);
    QColor selectedNotCurrentItemColor(Qt::lightGray);
    QColor pickColor(Qt::green);
    QPen notPick(defaultBorderColor);

    // draw rect around icon (thumb) and make it green if picked
    if (isPicked) {
        QPen pick(pickColor);
        pick.setWidth(thumbBorderThickness);
        painter->setPen(pick);
    }
    else painter->setPen(notPick);
    painter->drawPath(iconPath);

    QPen backPen(defaultBorderColor);
    backPen.setWidth(3);
    painter->setPen(backPen);
    painter->drawRoundedRect(itemRect, 8, 8);

    /* draw the item rect around the icon and label (if shown) and make it
    white if current item, light gray if selected but not the current item, or
    gray if not selected
    */
    QColor selectedColor  = selectedNotCurrentItemColor;
//    qDebug() << "thumbViewDelegate   row =" << row
//             << "currentRow =" <<  currentRow
//             << "option.state.testFlag(QStyle::State_Selected)"
//             << option.state.testFlag(QStyle::State_Selected);

    // currentRow is set in MW::fileSelectionChange
    if (row == currentRow) selectedColor = currentItemColor;
    if (option.state.testFlag(QStyle::State_Selected)) {
        QPen selectedPen(selectedColor);
        selectedPen.setWidth(itemBorderThickness);
        painter->setPen(selectedPen);
    } else {
        QPen activePen(defaultBorderColor);
        activePen.setWidth(itemBorderThickness / 2);
        painter->setPen(activePen);
    }
    painter->drawRoundedRect(itemRect, 8, 8);

    QColor labelColorToUse;
    if((rating != "" && rating != "0") || labelColor != "") {
        // ratings/label color
        if (labelColor == "") labelColorToUse = G::labelNoneColor;
        if (labelColor == "Red") labelColorToUse = G::labelRedColor;
        if (labelColor == "Yellow") labelColorToUse = G::labelYellowColor;
        if (labelColor == "Green") labelColorToUse = G::labelGreenColor;
        if (labelColor == "Blue") labelColorToUse = G::labelBlueColor;
        if (labelColor == "Purple") labelColorToUse = G::labelPurpleColor;
        painter->setBrush(labelColorToUse);
        QPen ratingPen(labelColorToUse);
        ratingPen.setWidth(0);
        painter->setPen(ratingPen);
        painter->drawEllipse(ratingRect);
        QPen ratingTextPen(Qt::white);
        ratingTextPen.setWidth(2);
        painter->setPen(ratingTextPen);
        QFont font = painter->font();
        font.setPixelSize(12);
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(ratingRect, Qt::AlignCenter, rating);
    }

    emit update(index, iconRect);

    painter->restore();
}
