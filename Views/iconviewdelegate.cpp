
#include "iconviewdelegate.h"

/*
ThumbViewDelegate Anatomy:


0-----cellRect = option.rect --------------------------------|
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

IconView thumbDock Anatomy


+-----------------------------------------------------------------------^-------->   ^
|   |+------------------------------------------------------------+     |
|   ||                                                            |
| H ||                                                            |
| e ||                                                            |
| a ||                                                            |
| d ||                                                            |
| e ||                                                            |
| r ||                                                            |
|   ||                                                            |
|   ||                                                            |
|   ||                            Cell                            |
|   ||                                                            |
|   ||                                                            |
|   ||                                                            |
|   ||                                                            |
|   ||                                                            |
|   ||                                                            |
|   ||                                                            |
|   ||                                                            |
|   ||                                                            |
|   ||                                                            |
|   ||                                                            |
|   |+------------------------------------------------------------+
|   |---------------------------------------------------------------------------->
|   |
|   |                          Horizontal Scrollbar
|   |
+-------------------------------------------------------------------------------->

*/

IconViewDelegate::IconViewDelegate(QObject *parent,
                                   bool &isRatingBadgeVisible,
                                   bool &isIconNumberVisible,
                                   DataModel *dm,
                                   ImageCacheData *icd,
                                   QItemSelectionModel *selectionModel
                                   )
        : isRatingBadgeVisible(isRatingBadgeVisible),
          isIconNumberVisible(isIconNumberVisible)
{
    parent->isWidgetType();         // suppress compiler warning
    this->dm = dm;
    this->icd = icd;
    fPad = 4;
    tPad = 4;         // allow small gap between thumb and outer border

    int currentWidth = 3;
    int selectedWidth = 2;
    int pickWidth = 2;

    // define colors
    int b = G::backgroundShade;
    int l20 = G::backgroundShade + 20;
    int l40 = G::backgroundShade + 40;
    defaultBorderColor = QColor(l20,l20,l20);
    currentItemColor = QColor(Qt::yellow);
    selectedColor = QColorConstants::Svg::white;
    pickColor = QColor(Qt::green);
    rejectColor = QColor(Qt::red);
    ingestedColor = QColor(Qt::blue);
    cacheColor = QColor(Qt::red);
    cacheBorderColor = QColor(Qt::lightGray);
    videoTextcolor = G::textColor;
    numberTextColor = QColor(l40,l40,l40);

    // define pens
    currentPen.setColor(currentItemColor);
    currentPen.setWidth(currentWidth);
    selectedPen.setColor(selectedColor);
    selectedPen.setWidth(selectedWidth);
    pickPen.setColor(pickColor);
    pickPen.setWidth(pickWidth);
    notPickPen.setColor(defaultBorderColor);
    notPickPen.setWidth(pickWidth);
    rejectPen.setColor(rejectColor);
    rejectPen.setWidth(pickWidth);
    ingestedPen.setColor(ingestedColor);
    ingestedPen.setWidth(pickWidth);

    // keep currentPen inside the cell
    int currentOffsetWidth = currentWidth / 2;
    currentOffsetWidth += currentWidth % 2;
    currOffset.setX(currentOffsetWidth);
    currOffset.setY(currentOffsetWidth);
}

void IconViewDelegate::setThumbDimensions(int thumbWidth,
                                          int thumbHeight,
                                          int labelFontSize,
                                          bool showThumbLabels,
                                          QString labelChoice,
                                          int badgeSize)

{
/*
    thumbSpacing not being used at present.  It was initially meant to control the
    QListView->setSpacing(), which is outside of the delegate.  Using it makes it more
    difficult to manage the gridView justification.  If it is decided another layer would be
    useful it would be better to include the cellBorder and thumbPadding suggested in the
    diagram comments above.

    This function has triggered the setPixelSize error if labelFontSize = 0
*/
    /*
    qDebug() << "IconViewDelegate::setThumbDimensions"
             << "row =" << currentRow
             << "labelFontSize =" << labelFontSize
                ;
             //*/
    delegateShowThumbLabels = showThumbLabels;
//    fPad = thumbPadding;
    font = QApplication::font();
    if (labelFontSize < 6) labelFontSize = 6;
    font.setPixelSize(labelFontSize);
    QFontMetrics fm(font);
    fontHt = fm.height();
    textHeadroom = 6;
    textHeight = 0;
    if (delegateShowThumbLabels) textHeight = fontHt + textHeadroom;
    this->labelChoice = labelChoice;
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

int IconViewDelegate::getCellWidthFromThumbWidth(int width)
{
    return width + pad2;
}

int IconViewDelegate::getCellHeightFromThumbHeight(int height)
{
    return height + pad2 + textHeight + 2;
}

int IconViewDelegate::getThumbWidthFromCellWidth(int cellWidth)
{
    return cellWidth - pad2;
}

int IconViewDelegate::getThumbHeightFromAvailHeight(int availHeight)
{
    // used to fit thumb (icon) into the available viewport height
    int thumbHeight = availHeight - pad2 - textHeight - 2;
    int maxHeight = G::maxIconSize - pad2 - textHeight - 2;
    return thumbHeight <= G::maxIconSize ? thumbHeight : maxHeight;
}

void IconViewDelegate::setCurrentIndex(QModelIndex current)
{
//    qDebug() << "IconViewDelegate::setCurrentIndex" << current << current.row();
    currentRow = current.row();     // this slot not being used
//    qDebug() << "IconViewDelegate::onCurrentChanged" << currentRow;
}

void IconViewDelegate::setCurrentRow(int row)
{
    currentRow = row;               // not used
}

QSize IconViewDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                              const QModelIndex& /*index*/) const
{
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
/*
    The delegate cell size is defined in setThumbDimensions and assigned in sizeHint.
    The thumbSize cell contains a number of cells or rectangles:

    Outer dimensions = cellRect or option.rect (QListView icon spacing is set to zero)
    frameRect        = cellRect - cBrdT - fPad
    thumbRect        = itemRect - thumbBorderGap - padding - thumbBorderThickness
    iconRect         = thumbRect - icon (icon has different aspect so either the
                       width or height will have to be centered inside the thumbRect
    textRect         = a rectangle below itemRect
*/
    painter->save();
    /*
    qDebug() << "IconViewDelegate::paint  "
             << "row =" << index.row()
             << painter
             << "option.state =" << option.state
             << "option.type =" << option.type
                ;
            //*/

    // make default border relative to background
    int l40 = G::backgroundShade + 40;
    QPen border;
    border.setWidth(1);
    border.setColor(QColor(l40,l40,l40));

    // get data from model
    int row = index.row();
    QModelIndex sfIdx0 = index.model()->index(row, 0);
//    QString fName = index.model()->index(row, G::NameColumn).data(Qt::DisplayRole).toString();
//    QString title = index.model()->index(row, G::TitleColumn).data(Qt::DisplayRole).toString();
    QString labelText;
    if (labelChoice == "Title") {
        labelText = index.model()->index(row, G::TitleColumn).data(Qt::DisplayRole).toString();
    }
    else {
        labelText = index.model()->index(row, G::NameColumn).data(Qt::DisplayRole).toString();
    }
    QString colorClass = index.model()->index(row, G::LabelColumn).data(Qt::EditRole).toString();
    QString rating = index.model()->index(row, G::RatingColumn).data(Qt::EditRole).toString();
    QString pickStatus = index.model()->index(row, G::PickColumn).data(Qt::EditRole).toString();
    QString duration = index.model()->index(row, G::DurationColumn).data(Qt::DisplayRole).toString();
    bool isSelected = dm->isSelected(row);
    bool isIngested = index.model()->index(row, G::IngestedColumn).data(Qt::EditRole).toBool();
    bool isCached = index.model()->index(row, G::PathColumn).data(G::CachedRole).toBool();
    bool metaLoaded = index.model()->index(row, G::MetadataLoadedColumn).data().toBool();
    bool isVideo = index.model()->index(row, G::VideoColumn).data().toBool();
    bool isReadWrite = index.model()->index(row, G::ReadWriteColumn).data().toBool();

    // icon size
    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    QSize iconSize = icon.actualSize(thumbSize);
    double aspectRatio = index.model()->index(row, G::AspectRatioColumn).data().toDouble();
    if (aspectRatio > 0) {
        if (aspectRatio > 1) {
            iconSize.setHeight(thumbSize.width() * 1.0 / aspectRatio);
            iconSize.setWidth(thumbSize.width());
        }
        else {
            iconSize.setWidth(thumbSize.width() * aspectRatio);
            iconSize.setHeight(thumbSize.height());
        }
    }

    // Make the item border rect smaller to accommodate the border.
    QRect cellRect(option.rect);
    QRect frameRect(cellRect.topLeft() + fPadOffset, frameSize);
    QRect thumbRect(frameRect.topLeft() + tPadOffset, thumbSize);

     // the icon rect is aligned within the thumb rect
    int alignVertPad = (thumbRect.height() - iconSize.height()) / 2;
    int alignHorPad = (thumbRect.width() - iconSize.width()) / 2;
    QRect iconRect(thumbRect.left() + alignHorPad, thumbRect.top() + alignVertPad,
                   iconSize.width(), iconSize.height());

//    /*
    qDebug() << "IconViewDelegate::paint "
             << "row =" << row
             << "currentRow =" << currentRow
//             << "selected =" << isSelected
             << "cellRect =" << cellRect
//             << "frameRect =" << frameRect
             << "thumbSize =" << thumbSize
             << "iconsize =" << iconSize
             << "thumbRect =" << thumbRect
             << "iconRect =" << iconRect
                ;
//             */

    // cached rect located bottom right as containment for circle
    int cacheDiam = 6;
    int cacheOffset = 3;
    QPoint cacheTopLeft(thumbRect.right() - cacheDiam - cacheOffset,
                        thumbRect.bottom() - cacheDiam - cacheOffset);
    QPoint cacheBottomRight(thumbRect.right() - cacheOffset, thumbRect.bottom() - cacheOffset);
    QRect cacheRect(cacheTopLeft, cacheBottomRight);

    QPainterPath iconPath;
    iconPath.addRoundedRect(iconRect, 6, 6);

    QRect textRect(frameRect.bottomLeft() - textHtOffset, frameRect.bottomRight());
    QPainterPath textPath;
    textPath.addRoundedRect(textRect, 8, 8);

    // start painting
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    if (delegateShowThumbLabels) {
        painter->setFont(font);
        painter->drawText(textRect, Qt::AlignHCenter, labelText);
    }

    painter->setClipping(true);
    painter->setClipPath(iconPath);
    painter->drawPixmap(iconRect, icon.pixmap(iconSize.width(),iconSize.height()));
    painter->setClipping(false);

    // default thumb border
    painter->setPen(border);
    painter->drawRoundedRect(frameRect, 8, 8);
    /*
    qDebug() << "IconViewDelegate::paint"
             << "row =" << row
             << "currentRow =" << currentRow
             << "selected item =" << option.state.testFlag(QStyle::State_Selected);
//             */

    // current index item
    if (row == dm->currentSfRow) {
        QRect currRect(cellRect.topLeft() + currOffset, cellRect.bottomRight() - currOffset);
        painter->setPen(currentPen);
        painter->drawRoundedRect(currRect, 8, 8);
    }

    // selected item
    if (isSelected) {
        painter->setPen(selectedPen);
        painter->drawRoundedRect(frameRect, 8, 8);
    }

    // ingested item
    if (isIngested) {
        painter->setPen(ingestedPen);
        painter->drawPath(iconPath);
    }

    // picked item
    if (pickStatus != "false") {
        if (pickStatus == "picked") painter->setPen(pickPen);
        else painter->setPen(rejectPen);
        painter->drawPath(iconPath);
    }

    // draw icon number
    if (isIconNumberVisible) {
        QPen numberPen(numberTextColor);
        numberPen.setWidth(2);
        painter->setBrush(G::backgroundColor);
        QFont numberFont = painter->font();
        int pxSize = badgeSize;
        if (pxSize < 6) pxSize = 6;
        numberFont.setPixelSize(pxSize);
        numberFont.setBold(true);
        painter->setFont(numberFont);
        QFontMetrics fm(numberFont);
        QString labelNumber = QString::number(row + 1);
        int numberWidth = fm.boundingRect(labelNumber).width() + 4;
        QPoint numberTopLeft(option.rect.left(), option.rect.top());
        QPoint numberBottomRight(option.rect.left() + numberWidth, option.rect.top() + badgeSize - 1);
        QRect numberRect(numberTopLeft, numberBottomRight);
        painter->setPen(G::backgroundColor);
        painter->drawRoundedRect(numberRect, 2, 2);
        painter->setPen(numberPen);
        painter->drawText(numberRect, Qt::AlignCenter, labelNumber);
    }

    // rating badge (color filled circle with rating number in center)
    if (isRatingBadgeVisible) {
        // label/rating rect located top-right as containment for circle
        QPoint ratingTopLeft(option.rect.right() - badgeSize, option.rect.top());
        QPoint ratingBottomRight(option.rect.right(), option.rect.top() + badgeSize);
        QRect ratingRect(ratingTopLeft, ratingBottomRight);

        QPoint ratingTextTopLeft(ratingRect.left(), ratingRect.top() - 1);
        QPoint ratingTextBottomRight(ratingRect.right(), ratingRect.bottom() - 1);
        QRect ratingTextRect(ratingTextTopLeft, ratingTextBottomRight);

        QColor labelColorToUse;
        QColor textColor(Qt::white);
        if (G::labelColors.contains(colorClass) || G::ratings.contains(rating)) {
            if (G::labelColors.contains(colorClass)) {
                if (colorClass == "Red") labelColorToUse = G::labelRedColor;
                if (colorClass == "Yellow") labelColorToUse = G::labelYellowColor;
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
            int pxSize = badgeSize;
            if (pxSize < 6) pxSize = 6;
            font.setPixelSize(pxSize);
            font.setBold(true);
            painter->setFont(font);
            painter->drawText(ratingTextRect, Qt::AlignCenter, rating);
        }
    }

    if (isVideo) {
        QFont videoFont = painter->font();
        videoFont.setPixelSize(12);
        painter->setFont(videoFont);
        QRectF bRect;
        painter->setPen(G::backgroundColor);
        painter->drawText(thumbRect, Qt::AlignBottom | Qt::AlignHCenter, "03:45:00", &bRect);
        painter->setBrush(G::backgroundColor);
        painter->drawRect(bRect);
        painter->setPen(videoTextcolor);
        QString vText;
        G::renderVideoThumb ? vText = duration : vText = "Video";
        painter->drawText(bRect, Qt::AlignBottom | Qt::AlignHCenter, vText);
    }

    // show lock if file does not have read/write permissions
    if (!isReadWrite) {
        int lockSize = thumbRect.height() / 6;
        if (lockSize < 11) lockSize = 11;
        if (lockSize > 20) lockSize = 20;
        QFont lockFont = painter->font();
        lockFont.setPixelSize(lockSize);
        painter->setFont(lockFont);
        QRectF bRect;
        painter->drawText(thumbRect, Qt::AlignBottom | Qt::AlignLeft, "🔒", &bRect);
        painter->drawText(bRect, "🔒");
        /*
        qDebug() << "IconViewDelegate::paint "
                 << "font =" << painter->font().pixelSize()
                 << "lockSize =" << lockSize
                 << "cellRect =" << cellRect
                 << "thumbRect =" << thumbRect
                 << "thumbSize =" << thumbSize
                 << "iconsize =" << iconSize
                 << "iconRect =" << iconRect
                    ;
                    //*/
    }

    // draw the cache circle
    if (!isCached && !isVideo && metaLoaded && !G::isSlideShow) {
        painter->setPen(cacheBorderColor);
        painter->setBrush(cacheColor);
        painter->drawEllipse(cacheRect);
    }

    /* provide rect data to calc thumb mouse click position that is then sent to imageView to
    zoom to the same spot */
    emit update(index, iconRect);

    painter->restore();
}
