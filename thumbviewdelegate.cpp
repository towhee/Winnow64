#include "thumbviewdelegate.h"

ThumbViewDelegate::ThumbViewDelegate(QObject *parent)
{
    itemBorderThickness = 2;
    thumbBorderThickness = 2;
    thumbBorderGap = 1;         // allow small gap between thumb and outer border
}

void ThumbViewDelegate::setThumbDimensions(int thumbWidth, int thumbHeight,
         int thumbPadding, int labelFontSize, bool showThumbLabels)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbViewDelegate::setThumbDimensions";
    #endif
    }
    delegateShowThumbLabels = showThumbLabels;
    iconPadding = thumbPadding;
    font = QApplication::font();
    font.setPixelSize(labelFontSize);
    QFontMetrics fm(font);
    fontHt = fm.height();
    if (thumbWidth < 40) thumbWidth = 100;
    if (thumbHeight < 40) thumbHeight = 100;

    thumbSize.setWidth(thumbWidth);
    thumbSize.setHeight(thumbHeight);
    thumbSpace.setWidth(thumbWidth + thumbBorderThickness*2 + iconPadding*2 + thumbBorderGap*2 + itemBorderThickness*2);
    thumbSpace.setHeight(thumbHeight + thumbBorderThickness*2 + iconPadding*2 + thumbBorderGap*2 + itemBorderThickness*2);
    if (showThumbLabels)
        thumbSpace.setHeight(thumbSize.height() + iconPadding*2
            + itemBorderThickness*2 + thumbBorderGap*2 + thumbBorderThickness*2 + fontHt + iconPadding);
//    reportThumbAttributes();
}

void ThumbViewDelegate::reportThumbAttributes()
{
   qDebug() << "thumbSize    =" << thumbSize
            << "thumbSpace   =" << thumbSpace
            << "thumbPadding =" << iconPadding
            << "Font height =" << fontHt;
}

QSize ThumbViewDelegate::getThumbCell()
{
    return thumbSpace;
}

QSize ThumbViewDelegate::sizeHint(const QStyleOptionViewItem &option ,
                              const QModelIndex &index) const
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbViewDelegate::sizeHint";
    #endif
    }
    QFont font = QApplication::font();
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
    qDebug() << "ThumbViewDelegate::paint" << index;
    #endif
    }
    painter->save();

    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    QSize iconsize = icon.actualSize(thumbSize);

    QString fName = qvariant_cast<QString>(index.data(Qt::EditRole));

    // UserRoles defined in ThumbView header
    bool isPicked = qvariant_cast<bool>(index.data(Qt::UserRole + 4));

    // define some offsets
    QPoint itemBorderOffset(itemBorderThickness, itemBorderThickness);
    QPoint thumbGapOffset(thumbBorderGap, thumbBorderGap);
    QPoint thumbBorderOffset(thumbBorderThickness, thumbBorderThickness);
    QPoint paddingOffset(iconPadding, iconPadding);

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

    QPainterPath iconPath;
    iconPath.addRoundRect(iconRect, 12, 12);

    QRect textRect = itemRect;
    textRect.setTop(itemRect.bottom()-fontHt-iconPadding);
    QPainterPath textPath;
    textPath.addRoundedRect(textRect, 8, 8);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    if (delegateShowThumbLabels) {
        painter->setFont(font);
        painter->drawText(textRect, Qt::AlignHCenter, fName);
    }

    QColor borderGray(95, 95, 95, 255);
    QPen notPick(borderGray);
    painter->setClipping(true);
    painter->setClipPath(iconPath);
    painter->drawPixmap(iconRect, icon.pixmap(iconsize.width(),iconsize.height()));
    painter->setClipping(false);

    if (isPicked) {
        QPen pick(Qt::green);
        pick.setWidth(thumbBorderThickness);
        painter->setPen(pick);
    }
    else painter->setPen(notPick);
    painter->drawPath(iconPath);

    QPen backPen(borderGray);
    backPen.setWidth(3);
    painter->setPen(backPen);
    painter->drawRoundedRect(itemRect, 8, 8);

    if (option.state.testFlag(QStyle::State_Selected)) {
        QPen selectedPen(Qt::white);
        selectedPen.setWidth(itemBorderThickness);
        painter->setPen(selectedPen);
//        if (index == currentIndex) {
//            QPen selectedPen(Qt::white);
//            selectedPen.setWidth(2);
//            painter->setPen(selectedPen);
//        } else {
//            QPen selectedPen(Qt::darkGray);
//            selectedPen.setWidth(2);
//            painter->setPen(selectedPen);
//        }
    } else {
        QPen activePen(borderGray);
        activePen.setWidth(itemBorderThickness / 2);
        painter->setPen(activePen);
    }
    painter->drawRoundedRect(itemRect, 8, 8);

    emit update(index, iconRect);

    painter->restore();
}

