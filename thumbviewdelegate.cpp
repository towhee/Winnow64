#include "thumbviewdelegate.h"

ThumbViewDelegate::ThumbViewDelegate(QObject *parent)
{
//    iconPadding = GData::thumbPadding;           //pixels
    penWidth = 2;
}

void ThumbViewDelegate::setThumbDimensions(int thumbWidth, int thumbHeight,
         int thumbPadding, int labelFontSize, bool showThumbLabels)
{
    {
    #ifdef ISDEBUG
    qDebug() << "ThumbViewDelegate::setThumbDimensions";
    #endif
    }
    iconPadding = thumbPadding;
    font = QApplication::font();
    font.setPixelSize(labelFontSize);
    QFontMetrics fm(font);
    fontHt = fm.height();
    thumbSize.setWidth(thumbWidth);
    thumbSize.setHeight(thumbHeight);
    thumbSpace.setWidth(thumbSize.width() + iconPadding*2 + penWidth*4);
    thumbSpace.setHeight(thumbSize.height() + iconPadding*2 + penWidth*4);
    if (showThumbLabels)
        thumbSpace.setHeight(thumbSize.height() + iconPadding*2
            + penWidth*2 + fontHt + iconPadding);
//    else thumbSpace.setHeight(thumbSize.height() + iconPadding*2
//            + penWidth*2);
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
    bool isPicked = qvariant_cast<bool>(index.data(Qt::UserRole+4));

    // Make the item border rect smaller to accommodate the border.
    QPoint strokeOffset(penWidth, penWidth);
    QRect itemRect(option.rect.topLeft() + strokeOffset,
                   option.rect.bottomRight() - strokeOffset);

    // The thumb rect is padded inside the item rect
    QPoint iconPadOffset(iconPadding, iconPadding);
    QRect thumbRect(itemRect.left() + iconPadding,
                    itemRect.top() + iconPadding,
                    thumbSize.width(),
                    thumbSize.height());

    int alignVertPad = (thumbRect.height()-iconsize.height())/2+2;
    int alignHorPad = (thumbRect.width()-iconsize.width())/2+2;
    QRect iconRect(thumbRect.left()+alignHorPad, thumbRect.top()+alignVertPad,
                   iconsize.width(), iconsize.height());

    QPainterPath iconPath;
    iconPath.addRoundRect(iconRect, 12, 12);

    QRect textRect = itemRect;
    textRect.setTop(itemRect.bottom()-fontHt-iconPadding);
    QPainterPath textPath;
    textPath.addRoundedRect(textRect, 8, 8);

//    painter->drawRect(textRect);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    if (G::showThumbLabels) {
        painter->setFont(font);
        painter->drawText(textRect, Qt::AlignHCenter, fName);
    }

    QColor borderGray(95, 95, 95, 255);
//    painter->setPen(borderGray);
    QPen notPick(borderGray);
    painter->setClipping(true);
    painter->setClipPath(iconPath);
    painter->drawPixmap(iconRect, icon.pixmap(iconsize.width(),iconsize.height()));
    painter->setClipping(false);

    if (isPicked) {
        QPen pick(Qt::green);
        pick.setWidth(2);
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
        selectedPen.setWidth(2);
        painter->setPen(selectedPen);
    } else {
        QPen activePen(borderGray);
        activePen.setWidth(1);
        painter->setPen(activePen);
    }
    painter->drawRoundedRect(itemRect, 8, 8);

    emit update(index, iconRect);

    painter->restore();
}

