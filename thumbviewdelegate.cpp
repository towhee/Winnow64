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
//    qDebug() << "ThumbViewDelegate::setThumbDimensions";
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
//    qDebug() << "ThumbViewDelegate::sizeHint" << index.data(Qt::DisplayRole);
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
//    qDebug() << "ThumbViewDelegate::paint" << index.data(Qt::DisplayRole);
    #endif
    }
    painter->save();

    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    QSize iconsize = icon.actualSize(thumbSize);

//    QString fName = qvariant_cast<QString>(index.data(Qt::EditRole));
    QStandardItemModel *model = new QStandardItemModel;
//    model = index.model();
    int row = index.row();
    QString fName = index.model()->index(row, G::PathColumn).data(G::FileNameRole).toString();
    QString labelColor = index.model()->index(row, G::LabelColumn).data(Qt::EditRole).toString();
    QString rating = index.model()->index(row, G::RatingColumn).data(Qt::EditRole).toString();


    // UserRoles defined in ThumbView header
    bool isPicked = qvariant_cast<bool>(index.data(G::PickedRole));
//    QString rating = qvariant_cast<QString>(index.data(Qt::UserRole + 12));
    qDebug() << "Rating =" << rating << "LabelColor =" << labelColor << "for" << fName;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

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

    // label/rating rect
    int ratingDiam = 18;
    QPoint ratingTopLeft(option.rect.right() - ratingDiam, option.rect.top());
    QPoint ratingBottomRight(option.rect.right(), option.rect.top() + ratingDiam);
    QRect ratingRect(ratingTopLeft, ratingBottomRight);

    QPainterPath iconPath;
    iconPath.addRoundRect(iconRect, 12, 12);

    QRect textRect = itemRect;
    textRect.setTop(itemRect.bottom()-fontHt-iconPadding);
    QPainterPath textPath;
    textPath.addRoundedRect(textRect, 8, 8);

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

    // draw rect around icon (thumb) and make it green if picked
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

    // draw the item rect around the icon and label (if shown)
    // and make it white if selected or gray if not selected
    if (option.state.testFlag(QStyle::State_Selected)) {
        QPen selectedPen(Qt::white);
        selectedPen.setWidth(itemBorderThickness);
        painter->setPen(selectedPen);
    } else {
        QPen activePen(borderGray);
        activePen.setWidth(itemBorderThickness / 2);
        painter->setPen(activePen);
    }
    painter->drawRoundedRect(itemRect, 8, 8);

    QColor labelColorToUse;
    if(rating != "" || labelColor != "") {
        // ratings/label color

        if (labelColor == "") labelColorToUse = QColor(65,65,65,255);
        if (labelColor == "Red") labelColorToUse = QColor(Qt::red);
        if (labelColor == "Yellow") labelColorToUse = QColor(Qt::darkYellow);
        if (labelColor == "Green") labelColorToUse = QColor(Qt::darkGreen);
        if (labelColor == "Blue") labelColorToUse = QColor(Qt::blue);
        if (labelColor == "Purple") labelColorToUse = QColor(Qt::darkMagenta);
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

