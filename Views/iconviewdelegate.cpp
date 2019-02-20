
#include "iconviewdelegate.h"

/*
ThumbViewDelegate Anatomy:


0-----cellRect-----------------------------------------------|
|                                                            |
|           framePadding (fPad)                              |
|                                                            |
|     1-----frameRect----------------------------------+     |
|     |                                                |     |
|     |          thumbPadding (tPad)                   |     |
|     |                                                |     |
|     |     2----thumbRect-----------------------+  ^  |     |
|     |     |     .                        .     |  :  |     |
|     |     |     .                        .     |  :  |     |
|     |     |     .                        .     |  :  |     |
|     |     |....................................|  t  |     |
|     |     |     .                        .     |  h  |     |
|     |     |     .                        .     |  u  |     |
|     |     |     .                        .     |  m  |     |
|     |     |     .                        .     |  b  |     |
|     |     |     .                        .     |  H  |     |
|     |     |     .         icon           .     |  e  |     |
|     |     |     .                        .     |  i  |     |
|     |     |     .                        .     |  g  |     |
|     |     |     .                        .     |  h  |     |
|     |     |....................................|  t  |     |
|     |     |     .                        .     |  :  |     |
|     |     |     .                        .     |  :  |     |
|     |     |     .                        .     |  :  |     |
|     3----textRect -----------------------------+--˅--|     |
|     |                                                |     |
|     |     <---------- thumbWidth -------------->     |     |
|     |                                                |     |
|     +------------------------------------------------+     |
|                                                            |
|                                                            |
|                                                            |
+------------------------------------------------------------+

0 = option.rect.topLeft()
1 = fPadOffset
2 = fPadOffset + tPadOffset
3 = frameRect.bottomLeft() - textHtOffset

*/

IconViewDelegate::IconViewDelegate(QObject *parent, bool &isRatingBadgeVisible)
        : isRatingBadgeVisible(isRatingBadgeVisible)
{
    parent->isWidgetType();         // suppress compiler warning
    fPad = 3;
    tPad = 2;         // allow small gap between thumb and outer border

    int currentWidth = 2;
    int selectedWidth = 2;
    int pickWidth = 2;

    // define colors
    defaultBorderColor = QColor(95, 95, 95, 255);
    currentItemColor = QColor(Qt::yellow);
    selectedColor = QColor(Qt::lightGray);
    pickColor = QColor(Qt::green);
    cacheColor = QColor(Qt::red);
    cacheBorderColor = QColor(Qt::lightGray);

    // define pens
    border.setColor(defaultBorderColor);
    border.setWidth(1);
    currentPen.setColor(currentItemColor);
    currentPen.setWidth(currentWidth);
    selectedPen.setColor(selectedColor);
    selectedPen.setWidth(selectedWidth);
    pick.setColor(pickColor);
    pick.setWidth(pickWidth);
    notPick.setColor(defaultBorderColor);

    // keep currentPen inside the cell
    int currentOffsetWidth = currentWidth / 2;
    currentOffsetWidth += currentWidth % 2;
    currOffset.setX(currentOffsetWidth);
    currOffset.setY(currentOffsetWidth);
}

void IconViewDelegate::setThumbDimensions(int thumbWidth, int thumbHeight,
         int thumbSpacing, int thumbPadding, int labelFontSize, bool showThumbLabels,
          int badgeSize)
{
    {
    #ifdef ISDEBUG
//  G::track(__FUNCTION__);
    #endif
    }
    /* thumbSpacing not being used at present.  It was initially meant to control the
    QListView->setSpacing(), which is outside of the delegate.  Using it makes it more
    difficult to mange the gridView justification.  If it is decided another layer would be
    useful it would be better to include the cellBorder and thumbPadding suggested in the
    diagram comments above.  */

    delegateShowThumbLabels = showThumbLabels;
//    fPad = thumbPadding;
    font = QApplication::font();
    font.setPixelSize(labelFontSize);
    QFontMetrics fm(font);
    fontHt = fm.height();
    textHeadroom = 6;
    textHeight = 0;
    if (delegateShowThumbLabels) textHeight = fontHt + textHeadroom;
    this->badgeSize = badgeSize;
    if (thumbWidth < ICON_MIN) thumbWidth = ICON_MIN;
    if (thumbHeight < ICON_MIN) thumbHeight = ICON_MIN;

    // define everything we can here so not doing every time delegate is painted

    tPad2 = tPad * 2;
    fPad2 = fPad * 2;
    pad = fPad + tPad;
    pad2 = tPad2 + fPad2;

    thumbSize.setWidth(thumbWidth);
    thumbSize.setHeight(thumbHeight);

    frameSize.setWidth(thumbWidth + tPad2);
    frameSize.setHeight(thumbHeight + tPad2 + textHeight);

    cellSize.setWidth(thumbWidth + pad2);
    cellSize.setHeight(thumbHeight + pad2 + textHeight);

    cellSpace = cellSize + QSize(thumbSpacing, thumbSpacing);

    // define some offsets
    fPadOffset = QPoint(fPad, fPad);
    tPadOffset = QPoint(tPad, tPad);

    textHtOffset.setX(0);
    textHtOffset.setY(fPad + textHeight - textHeadroom);

    reportThumbAttributes();
}

void IconViewDelegate::reportThumbAttributes()
{
   qDebug() << "\nthumbSize     =" << thumbSize
            << "\nframeSize     =" << frameSize
            << "\ncellSize      =" << cellSize
            << "\nframePadding  =" << fPad
            << "\nthumbPadding  =" << tPad
            << "\nText height   =" << textHeight
            << "\nText headroom =" << textHeadroom
            << "\ndelegateShowThumbLabels =" << delegateShowThumbLabels;
}

QSize IconViewDelegate::getCellSize()
{
//    reportThumbAttributes();
    return cellSize;
}

int IconViewDelegate::getThumbWidthFromCellWidth(int cellWidth)
{
    return cellWidth - pad2;
}

int IconViewDelegate::getCellWidthFromThumbWidth(int width)
{
    return width + pad2;
}

int IconViewDelegate::getCellHeightFromAvailHeight(int availHeight)
{
    int thumbHeight = availHeight - fPad - tPad;
    if(delegateShowThumbLabels) thumbHeight -= fontHt;
    return thumbHeight <= ICON_MAX ? thumbHeight : ICON_MAX;
}

void IconViewDelegate::onCurrentChanged(QModelIndex current, QModelIndex /*previous*/)
{
    currentRow = current.row();     // this slot not being used
}

QSize IconViewDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                              const QModelIndex& /*index*/) const
{
    {
    #ifdef ISDEBUG
//  G::track(__FUNCTION__);
    #endif
    }
    QFont font = QApplication::font();
    QFontMetrics fm(font);
    return cellSize;
}

void IconViewDelegate::paint(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
/* The delegate cell size is defined in setThumbDimensions and assigned in sizeHint.
The thumbSize cell contains a number of cells or rectangles:

Outer dimensions = cellRect or option.rect (QListView icon spacing is set to zero)
frameRect        = cellRect - cBrdT - fPad
thumbRect        = itemRect - thumbBorderGap - padding - thumbBorderThickness
iconRect         = thumbRect - icon (icon has different aspect so either the
                   width or height will have to be centered inside the thumbRect
textRect         = a rectangle below itemRect
*/
    {
    #ifdef ISDEBUG
//  G::track(__FUNCTION__);
    #endif
    }
    painter->save();

//qDebug() << "option =" << option.rect;/

    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    QSize iconsize = icon.actualSize(thumbSize);

    // get data from model
    int row = index.row();
    QString fName = index.model()->index(row, G::NameColumn).data(Qt::DisplayRole).toString();
//    QString fPath = index.model()->index(row, G::PathColumn).data(G::PathRole).toString();
    QString colorClass = index.model()->index(row, G::LabelColumn).data(Qt::EditRole).toString();
    QString rating = index.model()->index(row, G::RatingColumn).data(Qt::EditRole).toString();
    bool isPicked = index.model()->index(row, G::PickColumn).data(Qt::EditRole).toBool();
    bool isCached = index.model()->index(row, G::PathColumn).data(G::CachedRole).toBool();

    // Make the item border rect smaller to accommodate the border.
    QRect cellRect(option.rect);
    QRect frameRect(cellRect.topLeft() + fPadOffset, frameSize);
    QRect thumbRect(frameRect.topLeft() + tPadOffset, thumbSize);

     // the icon rect is aligned within the thumb rect
    int alignVertPad = (thumbRect.height() - iconsize.height()) / 2;
    int alignHorPad = (thumbRect.width() - iconsize.width()) / 2;
    QRect iconRect(thumbRect.left() + alignHorPad, thumbRect.top() + alignVertPad,
                   iconsize.width(), iconsize.height());

    // label/rating rect located top-right as containment for circle
    QPoint ratingTopLeft(option.rect.right() - badgeSize, option.rect.top());
    QPoint ratingBottomRight(option.rect.right(), option.rect.top() + badgeSize);
    QRect ratingRect(ratingTopLeft, ratingBottomRight);

    QPoint ratingTextTopLeft(ratingRect.left(), ratingRect.top() - 1);
    QPoint ratingTextBottomRight(ratingRect.right(), ratingRect.bottom() - 1);
    QRect ratingTextRect(ratingTextTopLeft, ratingTextBottomRight);

    // cached rect located bottom right as containment for circle
    int cacheDiam = 6;
    int cacheOffset = 3;
    QPoint cacheTopLeft(thumbRect.right() - cacheDiam - cacheOffset,
                        thumbRect.bottom() - cacheDiam - cacheOffset);
    QPoint cacheBottomRight(thumbRect.right() - cacheOffset, thumbRect.bottom() - cacheOffset);
    QRect cacheRect(cacheTopLeft, cacheBottomRight);

    QPainterPath iconPath;
    iconPath.addRoundRect(iconRect, 12, 12);

    QRect textRect(frameRect.bottomLeft() - textHtOffset, frameRect.bottomRight());
    QPainterPath textPath;
    textPath.addRoundedRect(textRect, 8, 8);

    // start painting
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    if (delegateShowThumbLabels) {
        painter->setFont(font);
        painter->drawText(textRect, Qt::AlignHCenter, fName);
    }

    painter->setClipping(true);
    painter->setClipPath(iconPath);
    painter->drawPixmap(iconRect, icon.pixmap(iconsize.width(),iconsize.height()));
    painter->setClipping(false);

    // default thumb border
    painter->setPen(border);
    painter->drawRoundedRect(frameRect, 8, 8);

    // current index item
    if (row == currentRow) {
        QRect currRect(cellRect.topLeft() + currOffset, cellRect.bottomRight() - currOffset);
        painter->setPen(currentPen);
        painter->drawRoundedRect(currRect, 8, 8);
    }

    // selected item
    if (option.state.testFlag(QStyle::State_Selected)) {
        painter->setPen(selectedPen);
        painter->drawRoundedRect(frameRect, 8, 8);
    }

    // picked item
    if (isPicked) {
        painter->setPen(pick);
        painter->drawPath(iconPath);
    }

    if (isRatingBadgeVisible) {
        QColor labelColorToUse;
        QColor textColor(Qt::white);
        if (G::labelColors.contains(colorClass) || G::ratings.contains(rating)) {
            if (G::labelColors.contains(colorClass)) {
                if (colorClass == "Red") labelColorToUse = G::labelRedColor;
                if (colorClass == "Yellow") labelColorToUse = G::labelYellowColor;
//                if (colorClass == "Yellow") textColor = Qt::black;
                if (colorClass == "Green") labelColorToUse = G::labelGreenColor;
                if (colorClass == "Blue") labelColorToUse = G::labelBlueColor;
                if (colorClass == "Purple") labelColorToUse = G::labelPurpleColor;
            }
            else labelColorToUse = G::labelNoneColor;
            painter->setBrush(labelColorToUse);
            QPen ratingPen(labelColorToUse);
            ratingPen.setWidth(0);
            painter->setPen(ratingPen);
            painter->drawEllipse(ratingRect);
            QPen ratingTextPen(textColor);
            ratingTextPen.setWidth(2);
            painter->setPen(ratingTextPen);
            QFont font = painter->font();
            font.setPixelSize(badgeSize);
            font.setBold(true);
            painter->setFont(font);
            painter->drawText(ratingTextRect, Qt::AlignCenter, rating);
        }
    }

    // draw the cache circle
    if (!isCached) {
        painter->setPen(cacheBorderColor);
        painter->setBrush(cacheColor);
        painter->drawEllipse(cacheRect);
    }

    emit update(index, iconRect);

    painter->restore();
}
