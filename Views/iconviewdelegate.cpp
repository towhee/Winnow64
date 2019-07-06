
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
    fPad = 5;
    tPad = 4;         // allow small gap between thumb and outer border

    int currentWidth = 3;
    int selectedWidth = 2;
    int pickWidth = 2;

    // define colors
    defaultBorderColor = QColor(108,108,108);
//    defaultBorderColor = QColor(118,118,118);
//    defaultBorderColor = QColor(128,128,128);
//    defaultBorderColor = QColor(Qt::gray);
    currentItemColor = QColor(Qt::yellow);
    selectedColor = QColor(Qt::white);
//    selectedColor = QColor(Qt::lightGray);
    pickColor = QColor(Qt::green);
    ingestedColor = QColor(Qt::blue);
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
    ingested.setColor(ingestedColor);
    ingested.setWidth(pickWidth);
    notPick.setColor(defaultBorderColor);
    notPick.setWidth(pickWidth);

    // keep currentPen inside the cell
    int currentOffsetWidth = currentWidth / 2;
    currentOffsetWidth += currentWidth % 2;
    currOffset.setX(currentOffsetWidth);
    currOffset.setY(currentOffsetWidth);
}

void IconViewDelegate::setThumbDimensions(int thumbWidth, int thumbHeight, int labelFontSize, bool showThumbLabels, int badgeSize)

{
    {
    #ifdef ISDEBUG
//  G::track(__FUNCTION__);
    #endif
    }
    /* thumbSpacing not being used at present.  It was initially meant to control the
    QListView->setSpacing(), which is outside of the delegate.  Using it makes it more
    difficult to manage the gridView justification.  If it is decided another layer would be
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

//    cellSpace = cellSize + QSize(thumbSpacing, thumbSpacing);

    // define some offsets
    fPadOffset = QPoint(fPad, fPad);
    tPadOffset = QPoint(tPad, tPad);

    textHtOffset.setX(0);
    textHtOffset.setY(fPad + textHeight - textHeadroom);

//    reportThumbAttributes();
}

QString IconViewDelegate::diagnostics()
{
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << "iconViewDelegate Diagnostics:\n";
    rpt << "\n" << "currentRow = " << G::s(currentRow);
    rpt << "\n" << "thumbSize = " << G::s(thumbSize.width()) << "," << G::s(thumbSize.height());
    rpt << "\n" << "frameSize = " << G::s(frameSize.width()) << "," << G::s(frameSize.height());
    rpt << "\n" << "selectedSize = " << G::s(selectedSize.width()) << "," << G::s(selectedSize.height());
    rpt << "\n" << "cellSize = " << G::s(cellSize.width()) << "," << G::s(cellSize.height());
    rpt << "\n" << "thumbSpacing = " << G::s(thumbSpacing);
    rpt << "\n" << "textHeight = " << G::s(textHeight);
    rpt << "\n" << "textHeadroom = " << G::s(textHeadroom);
    rpt << "\n" << "fontHt = " << G::s(fontHt);
    rpt << "\n" << "badgeSize = " << G::s(badgeSize);
    rpt << "\n" << "cacheDiam = " << G::s(cacheDiam);
    rpt << "\n" << "cacheOffset = " << G::s(cacheOffset);
    rpt << "\n" << "ratingDiam = " << G::s(ratingDiam);
    rpt << "\n" << "ratingTextSize = " << G::s(ratingTextSize);
    rpt << "\n" << "alignVertPad = " << G::s(alignVertPad);
    rpt << "\n" << "alignHorPad = " << G::s(alignHorPad);
    rpt << "\n" << "delegateShowThumbLabels =" << G::s(delegateShowThumbLabels);
    rpt << "\n" << "frame padding fPad = " << G::s(fPad);
    rpt << "\n" << "thumb padding tPad = " << G::s(tPad);
    rpt << "\n" << "pad = " << G::s(pad);
    rpt << "\n" << "pad2 = " << G::s(pad2);
    rpt << "\n" << "fPad2 = " << G::s(fPad2);
    rpt << "\n" << "tPad2 = " << G::s(tPad2);
    rpt << "\n" << "isRatingBadgeVisible = " << G::s(isRatingBadgeVisible);
    rpt << "\n" << "delegateShowThumbLabels = " << G::s(delegateShowThumbLabels);
    rpt << "\n" << "font = " << G::s(font);
    rpt << "\n" << "Colors:";
    rpt << "\n  " << "defaultBorderColor = " << G::s(defaultBorderColor);
    rpt << "\n  " << "currentItemColor = " << G::s(currentItemColor);
    rpt << "\n  " << "selectedColor = " << G::s(selectedColor);
    rpt << "\n  " << "pickColor = " << G::s(pickColor);
    rpt << "\n  " << "ingestedColor = " << G::s(ingestedColor);
    rpt << "\n  " << "cacheColor = " << G::s(cacheColor);
    rpt << "\n  " << "cacheBorderColor = " << G::s(cacheBorderColor);
    return reportString;
}

QSize IconViewDelegate::getCellSize()
{
//    reportThumbAttributes();
    return cellSize;
}

QSize IconViewDelegate::getCellSize(QSize icon)
{
    return (icon + QSize(pad2, pad2 + textHeight));
}

int IconViewDelegate::getThumbWidthFromCellWidth(int cellWidth)
{
    return cellWidth - pad2;
}

int IconViewDelegate::getCellWidthFromThumbWidth(int width)
{
    return width + pad2;
}

int IconViewDelegate::getCellHeightFromThumbHeight(int height)
{
    return height + pad2 + textHeight;
}

int IconViewDelegate::getCellHeightFromAvailHeight(int availHeight)
{
    int thumbHeight = availHeight - pad2 - textHeight;
    return thumbHeight <= G::maxIconSize ? thumbHeight : G::maxIconSize;
}

void IconViewDelegate::setCurrentIndex(QModelIndex current)
{
    currentRow = current.row();     // this slot not being used
    qDebug() << "IconViewDelegate::onCurrentChanged" << currentRow;
}

QSize IconViewDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                              const QModelIndex& index) const
{
    {
    #ifdef ISDEBUG
//  G::track(__FUNCTION__);
    #endif
    }
//    qDebug() << "IconViewDelegate::sizeHint  "
//             << "row =" << index.row();
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

//qDebug() << "IconViewDelegate::paint  "
//         << "option =" << option.rect
//         << "row =" << index.row();

    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    QSize iconsize = icon.actualSize(thumbSize);

    // get data from model
    int row = index.row();
    QString fName = index.model()->index(row, G::NameColumn).data(Qt::DisplayRole).toString();
//    QString fPath = index.model()->index(row, G::PathColumn).data(G::PathRole).toString();
    QString colorClass = index.model()->index(row, G::LabelColumn).data(Qt::EditRole).toString();
    QString rating = index.model()->index(row, G::RatingColumn).data(Qt::EditRole).toString();
    bool isPicked = index.model()->index(row, G::PickColumn).data(Qt::EditRole).toBool();
    bool isIngested = index.model()->index(row, G::IngestedColumn).data(Qt::EditRole).toBool();
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

    // ingested item
    if (isIngested) {
        painter->setPen(ingested);
        painter->drawPath(iconPath);
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

    /* provide rect data to calc thumb mouse click position that is then sent to imageView to
    zoom to the same spot */
    emit update(index, iconRect);

    painter->restore();
}
