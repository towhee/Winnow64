
#include "iconviewdelegate.h"
#include "Main/global.h"

/*
IconViewDelegate Anatomy:


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
|     |     4....................................|  t  |     |
|     |     |     .                        .     |  :  |     |
|     |     |     .                        .     |  :  |     |
|     |     |     .        *****           .     |  :  |     |   info row at bottom of thumbRect
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
4 = info row (rating etc) = 1/6 thumbRect height

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
                                   QItemSelectionModel *selectionModel
                                   )
        : isRatingBadgeVisible(isRatingBadgeVisible),
          isIconNumberVisible(isIconNumberVisible)
{
    parent->isWidgetType();         // suppress compiler warning
    this->dm = dm;
    fPad = 4;
    tPad = 4;         // allow small gap between thumb and outer border

    int currentWidth = 3;
    int selectedWidth = 2;
    int pickWidth = 2;

    // define colors
    int b = G::backgroundShade;
    int l20 = G::backgroundShade + 20;
    int l40 = G::backgroundShade + 40;
    int l60 = G::backgroundShade + 60;
    defaultBorderColor = QColor(l20,l20,l20);
    currentItemColor = QColor(Qt::yellow);
    selectedColor = QColorConstants::Svg::white;
    pickColor = QColor(Qt::green);
    rejectColor = QColor(Qt::red);
    ingestedColor = QColor(Qt::blue);
    cacheColor = QColor(200,0,0);
    cacheBorderColor = QColor(l20,l20,l20);
    missingThumbColor = QColor(Qt::yellow);
    ratingBackgoundColor = QColor(Qt::yellow);
    // ratingBackgoundColor = QColor(b,b,b,50);
    labelTextColor = G::textColor;
    videoTextColor = G::textColor;
    numberTextColor = QColor(l60,l60,l60);
    vp1Color = QColor(Qt::white);
    vp2Color = QColor(Qt::black);

    // define pens
    currentPen.setColor(currentItemColor);
    currentPen.setWidth(currentWidth);
    selectedPen.setColor(selectedColor);
    selectedPen.setWidth(selectedWidth);
    pickedPen.setColor(pickColor);
    pickedPen.setWidth(pickWidth);
    notPickPen.setColor(defaultBorderColor);
    notPickPen.setWidth(pickWidth);
    rejectedPen.setColor(rejectColor);
    rejectedPen.setWidth(pickWidth);
    ingestedPen.setColor(ingestedColor);
    ingestedPen.setWidth(pickWidth);
    vp1Pen.setColor(vp1Color);
    vp1Pen.setWidth(1);
    vp2Pen.setColor(vp2Color);
    vp2Pen.setWidth(1);

    // keep currentPen inside the cell
    int currentOffsetWidth = currentWidth / 2;
    currentOffsetWidth += currentWidth % 2;
    currOffset.setX(currentOffsetWidth);
    currOffset.setY(currentOffsetWidth);

    combineRawJpgSymbol.load(":/images/icon16/link.png");
    lockRenderer = new QSvgRenderer(QString(":/images/lock.svg"), this);
}

void IconViewDelegate::setThumbDimensions(int thumbWidth,
                                          int thumbHeight,
                                          int labelFontSize,
                                          bool showThumbLabels,
                                          QString labelChoice,
                                          int badgeSize,
                                          int iconNumberSize)

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
             << "thumbWidth =" << thumbWidth
             << "thumbHeight =" << thumbHeight
             << "labelFontSize =" << labelFontSize
                ;
             //*/
    delegateShowThumbLabels = showThumbLabels;
    //fPad = thumbPadding;
    font = QApplication::font();
    if (labelFontSize < 6) labelFontSize = 6;
    this->labelFontSize = labelFontSize;
    font.setPixelSize(labelFontSize);
    QFontMetrics fm(font);
    fontHt = fm.height();
    textHeadroom = 6;
    textHeight = 0;
    if (delegateShowThumbLabels) textHeight = fontHt + textHeadroom;
    this->labelChoice = labelChoice;
    this->badgeSize = badgeSize;
    this->iconNumberSize = iconNumberSize;
    if (thumbWidth < ICON_MIN) thumbWidth = ICON_MIN;
    if (thumbHeight < ICON_MIN) thumbHeight = ICON_MIN;
    starsWidth = fm.boundingRect("*******").width();

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

    /* debug
    qDebug() << "IconViewDelegate::setThumbDimensions"
             << "cellSize =" << cellSize
             << "frameSize =" << frameSize
             << "thumbSize =" << thumbSize
                ;
                //*/

    // Pre-calculate symbol rectangles relative to a zero-based thumbRect
    int dotDiam = 6;
    int dotOffset = 1; //3;

    // Cache Circle (Bottom Right)
    cacheRect.setRect(thumbSize.width() - dotDiam - dotOffset,
                      thumbSize.height() - dotDiam - dotOffset /*+ 2*/,
                      dotDiam, dotDiam);

    // Missing Thumb Circle (Bottom Left)
    missingThumbRect.setRect(dotDiam - dotOffset,
                             thumbSize.height() - dotDiam - dotOffset /*+ 2*/,
                             dotDiam, dotDiam);

    // Lock Icon (To the right of Missing Thumb)
    int lockSize = 12; // Adjusted for SVG
    lockRect.setRect(missingThumbRect.right() + 2,
                     thumbSize.height() - lockSize,
                     lockSize, lockSize);

    // Combine RAW+JPG (To the left of Cache)
    int linkW = 14;
    int linkH = 14;
    combineRawJpgRect.setRect(cacheRect.left() - linkW - 2,
                              thumbSize.height() - 0.8 * linkH,
                              linkW, linkH);
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
   /*
   qDebug() << "IconViewDelegate::getCellHeightFromThumbHeight"
            << "height =" << height
            << "pad2 =" << pad2
            << "textHeight =" << textHeight
            << "cellHeight =" << height + pad2 + textHeight + 2
               ; //*/
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
    return thumbHeight;
//    int maxHeight = G::maxIconSize - pad2 - textHeight - 2;
//    return thumbHeight <= G::maxIconSize ? thumbHeight : maxHeight;
}

void IconViewDelegate::setVpRectVisibility(bool isVisible)
{
    vpRectIsVisible = isVisible;
}

void IconViewDelegate::setVpRect(QRectF vp, qreal imA)
{
    vpRect = vp;
    this->imA = imA;

    // Pre-calculate the draw coordinates relative to a zero-origin icon
    qreal wIcon, hIcon;
    int wBlack = 0;
    int hBlack = 0;

    // Calculate icon dimensions based on image aspect ratio
    if (imA > 1) {
        wIcon = thumbSize.width();
        hIcon = wIcon / imA;
        hBlack = (thumbSize.height() - hIcon) / 2;
    } else {
        hIcon = thumbSize.height();
        wIcon = hIcon * imA;
        wBlack = (thumbSize.width() - wIcon) / 2;
    }

    // Map the normalized vpRect (0.0 - 1.0) to actual pixel coordinates
    int x1 = static_cast<int>(vpRect.topLeft().x() * wIcon) + wBlack;
    int y1 = static_cast<int>(vpRect.topLeft().y() * hIcon) + hBlack;
    int x2 = static_cast<int>(vpRect.bottomRight().x() * wIcon) + wBlack;
    int y2 = static_cast<int>(vpRect.bottomRight().y() * hIcon) + hBlack;

    targetVpRect = QRect(QPoint(x1, y1), QPoint(x2, y2));
}

QRect IconViewDelegate::getSymbolRect(const QString &symbol, const QRect &optionRect, const QModelIndex &index) const
{
    // Common geometry bases
    QRect frameRect(optionRect.topLeft() + fPadOffset, frameSize);
    QRect thumbRect(frameRect.topLeft() + tPadOffset, thumbSize);
    QPoint origin = thumbRect.topLeft();

    // 1. Static Symbols (Pre-calculated in setThumbDimensions)
    if (symbol == "Thumb") return thumbRect;
    if (symbol == "MissingThumb") return missingThumbRect.translated(origin);
    if (symbol == "Lock") return lockRect.translated(origin);
    if (symbol == "CombineRawJpg") return combineRawJpgRect.translated(origin);
    if (symbol == "Cache") return cacheRect.translated(origin);

    // 2. Dynamic Symbols (Calculated based on Row Data)
    if (symbol == "Duration" || symbol == "Rating") {
        bool isVideo = index.model()->index(index.row(), G::VideoColumn).data().toBool();
        int videoDurationHt = 0;

        // Calculate Duration Rect first as it offsets the Rating
        if (isVideo) {
            // Replicate the bRect logic from paint()
            QFont videoFont = QApplication::font();
            videoFont.setPixelSize(G::fontSize);
            QFontMetrics fm(videoFont);
            QRect bRect = fm.boundingRect("00:00:00"); // Standard duration width
            // Center at bottom of thumbRect
            int x = thumbRect.left() + (thumbRect.width() - bRect.width()) / 2;
            int y = thumbRect.bottom() - bRect.height();
            QRect durationRect(x, y, bRect.width(), bRect.height());

            if (symbol == "Duration") return durationRect;
            videoDurationHt = bRect.height();
        }

        if (symbol == "Rating") {
            int infoHt = thumbRect.height() / 6;
            if (infoHt > 14) {
                // Star-based rating rect
                int h = fontHt * 0.8;
                int b = (thumbRect.width() - starsWidth) / 2;
                return QRect(thumbRect.left() + b, thumbRect.bottom() - videoDurationHt - h, starsWidth, h);
            } else {
                // Single number rating rect
                int h = infoHt;
                return QRect(thumbRect.center().x() - h/2, thumbRect.bottom() - videoDurationHt - h + 3, h, h);
            }
        }
    }
    return QRect();
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

void IconViewDelegate::resetFirstLastVisible()
{
    firstVisible = 99999999;
    lastVisible = 0;
    midVisible = 0;
}

bool IconViewDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view,
                                 const QStyleOptionViewItem &option,
                                 const QModelIndex &index)
{
    if (event->type() != QEvent::ToolTip || !index.isValid())
        return QStyledItemDelegate::helpEvent(event, view, option, index);

    // Extract useful data from model
    QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel*>(view->model());
    QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
    int row = sourceIndex.row();
    auto sf = proxy ? proxy->sourceModel() : view->model();
    QPoint viewPos = view->viewport()->mapFromGlobal(event->globalPos());

    if (getSymbolRect("MissingThumb", option.rect, index).contains(viewPos))
        tooltip = "Image does not have an embedded thumbnail";
    else if (getSymbolRect("Lock", option.rect, index).contains(viewPos))
        tooltip = "Image file is locked";
    else if (getSymbolRect("CombineRawJpg", option.rect, index).contains(viewPos))
        tooltip = "Image is JPG version of a RAW+JPG pair";
    else if (getSymbolRect("Cache", option.rect, index).contains(viewPos))
        tooltip = "Image file is not cached";
    else if (getSymbolRect("Rating", option.rect, index).contains(viewPos))
        tooltip = "Rating";
    else if (getSymbolRect("Duration", option.rect, index).contains(viewPos))
        tooltip = "Video Duration";
    else if (getSymbolRect("Thumb", option.rect, index).contains(viewPos))
        tooltip = sf->index(row, 0).data(G::PathRole).toString();
    else
        tooltip = "Borders:\n  Yellow:\t Current \n  White:\t Selected \n  Green:\t Picked\n  Blue:\t Ingested\n  Red:\t Rejected";

    if (!tooltip.isEmpty()) {
        QToolTip::showText(event->globalPos(), tooltip, view);
        return true;
    }
    return false;
}

QSize IconViewDelegate::sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    QFont font = QApplication::font();
    QFontMetrics fm(font);
    /*
    qDebug() << "IconViewDelegate::sizeHint  "
             << "row =" << index.row()
             << "cellSize =" << cellSize
                ; //*/
    return cellSize;
}

void IconViewDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    // Quick exit if the icon has not been loaded into the DataModel
    if (index.data(Qt::DecorationRole).isNull()) return;

    painter->save();

    // Pull model data once to avoid repeated indexing during the paint loop
    int sfRow = index.row();
    bool isSelected = dm->isSelected(sfRow);
    bool isCurrentIndex = (sfRow == dm->currentSfRow);

    // Determine label text based on user preference
    QString labelText = (labelChoice == "Title")
                            ? index.model()->index(sfRow, G::TitleColumn).data(Qt::DisplayRole).toString()
                            : index.model()->index(sfRow, G::NameColumn).data(Qt::DisplayRole).toString();

    QString colorClass = index.model()->index(sfRow, G::LabelColumn).data(Qt::EditRole).toString();
    int ratingNumber = index.model()->index(sfRow, G::RatingColumn).data(Qt::EditRole).toInt();
    QString rating = index.model()->index(sfRow, G::RatingColumn).data(Qt::EditRole).toString();
    QString duration = index.model()->index(sfRow, G::DurationColumn).data(Qt::DisplayRole).toString();

    // Status flags and metadata states
    QString pickStatus = index.model()->index(sfRow, G::PickColumn).data(Qt::EditRole).toString();
    bool isPicked = (pickStatus == "Picked");
    bool isRejected = (pickStatus == "Rejected");
    bool isIngested = index.model()->index(sfRow, G::IngestedColumn).data(Qt::EditRole).toBool();
    bool isCached = index.model()->index(sfRow, G::IsCachedColumn).data(Qt::EditRole).toBool();
    bool isMissingThumb = index.model()->index(sfRow, G::MissingThumbColumn).data().toBool();
    bool metaLoaded = index.model()->index(sfRow, G::MetadataLoadedColumn).data().toBool();
    bool isVideo = index.model()->index(sfRow, G::VideoColumn).data().toBool();
    bool isReadWrite = index.model()->index(sfRow, G::ReadWriteColumn).data().toBool();
    bool isCombineRawJpg = index.model()->index(sfRow, 0).data(G::DupIsJpgRole).toBool() && G::combineRawJpg;

    // Calculate the standard Winnow cell geometry
    QRect cellRect(option.rect);
    QRect frameRect(cellRect.topLeft() + fPadOffset, frameSize);
    QRect thumbRect(frameRect.topLeft() + tPadOffset, thumbSize);
    QPoint origin = thumbRect.topLeft();

    // Retrieve and scale the icon pixmap to fit the thumbRect
    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    QSize maxSize = icon.actualSize(QSize(G::maxIconSize, G::maxIconSize));
    QPixmap pm = icon.pixmap(maxSize);

    QRect iconRect;
    if (!pm.isNull()) {
        // Apply square-ish scaling adjustment for color class border visibility
        int pmMargin = 8;
        bool isSquare = (qAbs(pm.width() - pm.height()) < pmMargin);

        if (isSquare) {
            pm = pm.scaled(thumbSize - QSize(pmMargin, pmMargin), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } else {
            pm = pm.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        // Center the scaled pixmap within the thumbRect
        int alignVert = (thumbRect.height() - pm.height()) / 2;
        int alignHor = (thumbRect.width() - pm.width()) / 2;
        iconRect = QRect(thumbRect.left() + alignHor, thumbRect.top() + alignVert, pm.width(), pm.height());
    }

    // Determine frame background color based on rating labels
    QColor labelColorToUse = G::backgroundColor;
    if (isRatingBadgeVisible && G::labelColors.contains(colorClass)) {
        if (colorClass == "Red") labelColorToUse = G::labelRedColor;
        else if (colorClass == "Yellow") labelColorToUse = G::labelYellowColor;
        else if (colorClass == "Green") labelColorToUse = G::labelGreenColor;
        else if (colorClass == "Blue") labelColorToUse = G::labelBlueColor;
        else if (colorClass == "Purple") labelColorToUse = G::labelPurpleColor;
    }

    // Paint the frame background and main border
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(labelColorToUse);
    painter->setPen(QPen(QColor(G::backgroundShade + 40, G::backgroundShade + 40, G::backgroundShade + 40), 1));
    painter->drawRoundedRect(frameRect, 8, 8);

    // Render filename or title label if enabled
    if (delegateShowThumbLabels) {
        QRect textRect(frameRect.bottomLeft() - textHtOffset, frameRect.bottomRight());
        painter->setPen(labelTextColor);
        painter->setFont(font);
        painter->drawText(textRect, Qt::AlignHCenter, labelText);
    }

    // Draw the thumbnail image with appropriate clipping
    if (!pm.isNull()) {
        painter->setClipping(true);
        painter->setClipRect(iconRect);
        painter->drawPixmap(iconRect, pm);
        painter->setClipping(false);
    }

    // Render video duration overlay if applicable
    int videoDurationHt = 0;
    if (isVideo && G::renderVideoThumb) {
        QFont videoFont = QApplication::font();
        videoFont.setPixelSize(G::fontSize);
        painter->setFont(videoFont);
        QRect bRect;
        painter->setPen(videoTextColor);
        painter->drawText(thumbRect, Qt::AlignBottom | Qt::AlignHCenter, duration, &bRect);
        videoDurationHt = bRect.height();
    }

    // Render rating stars or numerical badges
    int infoHt = thumbRect.height() / 6;
    if (isRatingBadgeVisible && G::ratings.contains(rating)) {
        painter->setPen(Qt::white);
        if (infoHt > 14) {
            QString stars(ratingNumber, '*');
            int b = (thumbRect.width() - starsWidth) / 2;
            int h = fontHt * 0.8;
            painter->drawText(QRect(thumbRect.left() + b, thumbRect.bottom() - videoDurationHt - h, starsWidth, h),
                              Qt::AlignHCenter | Qt::AlignTop, stars);
        } else {
            painter->drawText(QRect(thumbRect.center().x() - infoHt/2, thumbRect.bottom() - videoDurationHt - infoHt + 3, infoHt, infoHt),
                              Qt::AlignBottom, QString::number(ratingNumber));
        }
    }

    // Render file status symbols (RAW+JPG link and file lock)
    if (isCombineRawJpg) painter->drawImage(combineRawJpgRect.translated(origin), combineRawJpgSymbol);
    if (!isReadWrite) lockRenderer->render(painter, lockRect.translated(origin));

    // Draw cache and missing thumbnail status circles
    if (!isCached && !isVideo && metaLoaded && !G::isSlideShow) {
        painter->setPen(cacheBorderColor);
        painter->setBrush(cacheColor);
        painter->drawEllipse(cacheRect.translated(origin));
    }
    if (G::useMissingThumbs && isMissingThumb) {
        painter->setPen(cacheBorderColor);
        painter->setBrush(missingThumbColor);
        painter->drawEllipse(missingThumbRect.translated(origin));
    }

    // Render icon number overlay
    if (isIconNumberVisible) {
        painter->setBrush(labelColorToUse);
        QFont numberFont = painter->font();
        int pxSize = qMax(6, iconNumberSize);
        numberFont.setPixelSize(pxSize);
        numberFont.setBold(true);
        painter->setFont(numberFont);

        QFontMetrics fm(numberFont);
        QString labelNumber = QString::number(sfRow + 1);
        int numberWidth = fm.boundingRect(labelNumber).width() + 4;
        QRect numberRect(frameRect.left(), frameRect.top(), numberWidth + 4, iconNumberSize);

        painter->setPen(Qt::transparent);
        painter->drawRoundedRect(numberRect, 8, 8);
        painter->setPen(numberTextColor);
        painter->drawText(numberRect, Qt::AlignCenter, labelNumber);
    }

    // Draw status borders for Picked, Ingested, or Rejected states
    painter->setBrush(Qt::transparent);
    if (isPicked) painter->setPen(pickedPen);
    else if (isIngested) painter->setPen(ingestedPen);
    else if (isRejected) painter->setPen(rejectedPen);

    if ((isPicked || isIngested || isRejected) && !pm.isNull()) {
        painter->drawRoundedRect(iconRect, 6, 6);
    }

    // Draw selection and current index highlight borders
    if (isSelected) {
        painter->setPen(selectedPen);
        painter->drawRoundedRect(frameRect, 8, 8);
    }
    if (isCurrentIndex) {
        painter->setPen(currentPen);
        painter->drawRoundedRect(QRect(cellRect.topLeft() + currOffset, cellRect.bottomRight() - currOffset), 8, 8);
    }

    // Render the loupe viewport rectangle for focus stacking
    if (isCurrentIndex && vpRectIsVisible) {
        QRect drawRect = targetVpRect.translated(iconRect.topLeft());
        painter->setPen(vp1Pen);
        painter->drawRect(drawRect);
        painter->setPen(vp2Pen);
        painter->drawRect(drawRect.adjusted(1, 1, -1, -1));
    }

    // Notify the view of the updated iconRect for coordinate mapping
    emit update(index, iconRect);

    painter->restore();
}

// void IconViewDelegate::paint(QPainter *painter,
//                              const QStyleOptionViewItem &option,
//                              const QModelIndex &index) const
// {
// /*
//     The delegate cell size is defined in setThumbDimensions and assigned in sizeHint.
//     The thumbSize cell contains a number of cells or rectangles:

//     Outer dimensions = cellRect or option.rect (QListView icon spacing is set to zero)
//     frameRect        = cellRect - cBrdT - fPad
//     thumbRect        = itemRect - thumbBorderGap - padding - thumbBorderThickness
//     iconRect         = thumbRect - icon (icon has different aspect so either the
//                        width or height will have to be centered inside the thumbRect
//     textRect         = a rectangle below itemRect
// */
//     // Ignore paint before icon has been loaded into DataModel
//     if (index.data(Qt::DecorationRole).isNull()) return;

//     painter->save();
//     /* debug
//     qDebug() << "IconViewDelegate::paint  "
//              << "row =" << index.row()
//              << "index =" << index
//              << "option.state =" << option.state
//              //<< "option.state just enabled =" << enabled
//              //<< "option.type =" << option.type
//              //<< "option.features =" << option.features
//              //<< "option.index =" << option.index
//                 ;
//              //*/

//     // show all symbols to document for help
//     bool showAllSymbols = false;

//     // make default border relative to background
//     int l40 = G::backgroundShade + 40;
//     QPen border;
//     border.setWidth(1);
//     border.setColor(QColor(l40,l40,l40));

//     // get data from model
//     int sfRow = index.row();

//     // first/last visible (not being used at present)
//     if (sfRow < firstVisible) firstVisible = sfRow;
//     if (sfRow > lastVisible) lastVisible = sfRow;
//     midVisible = firstVisible + ((lastVisible - firstVisible) / 2);

//     QString labelText;
//     if (labelChoice == "Title") {
//         labelText = index.model()->index(sfRow, G::TitleColumn).data(Qt::DisplayRole).toString();
//     }
//     else {
//         labelText = index.model()->index(sfRow, G::NameColumn).data(Qt::DisplayRole).toString();
//     }
//     QString colorClass = index.model()->index(sfRow, G::LabelColumn).data(Qt::EditRole).toString();
//     int ratingNumber = index.model()->index(sfRow, G::RatingColumn).data(Qt::EditRole).toInt();
//     QString rating = index.model()->index(sfRow, G::RatingColumn).data(Qt::EditRole).toString();
//     QString duration = index.model()->index(sfRow, G::DurationColumn).data(Qt::DisplayRole).toString();
//     if (duration.isNull()) duration = "XXX";
//     bool isSelected = dm->isSelected(sfRow);
//     bool isCurrentIndex = sfRow == dm->currentSfRow;
//     QString pickStatus = index.model()->index(sfRow, G::PickColumn).data(Qt::EditRole).toString();
//     bool isPicked = pickStatus == "Picked";
//     bool isRejected = pickStatus == "Rejected";
//     bool isIngested = index.model()->index(sfRow, G::IngestedColumn).data(Qt::EditRole).toBool();
//     bool isCached = index.model()->index(sfRow, G::IsCachedColumn).data(Qt::EditRole).toBool();
//     bool isMissingThumb = index.model()->index(sfRow, G::MissingThumbColumn).data().toBool();
//     bool metaLoaded = index.model()->index(sfRow, G::MetadataLoadedColumn).data().toBool();
//     bool isVideo = index.model()->index(sfRow, G::VideoColumn).data().toBool();
//     bool isReadWrite = index.model()->index(sfRow, G::ReadWriteColumn).data().toBool();
//     bool isCombineRawJpg = index.model()->index(sfRow, 0).data(G::DupIsJpgRole).toBool() && G::combineRawJpg;

//     // Cell structure (see IconViewDelegate Anatomy at top of file).
//     QRect cellRect(option.rect);
//     QRect frameRect(cellRect.topLeft() + fPadOffset, frameSize);
//     QRect thumbRect(frameRect.topLeft() + tPadOffset, thumbSize);

//     // Cell origin
//     QPoint origin = thumbRect.topLeft();

//     // get icon (thumbnail) from the datamodel and scale
//     QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
//     // get the icon dimensions to fit into G::maxIconSize.  Must start with largest size
//     // to prevent scaling glitches.
//     QSize maxSize = icon.actualSize(QSize(G::maxIconSize,G::maxIconSize));  // 256,256
//     // convert to a QPixmap
//     QPixmap pm = icon.pixmap(maxSize);
//     if (!pm.isNull()) {
//         // if the pm is nearly square, scale slightly smaller so border large enough
//         // to show color class
//         int pmMargin = 8;
//         bool isSquare = (qAbs(pm.width() - pm.height()) < pmMargin);
//         // scale the pixmap to fit in the thumbRect
//         if (isSquare) pm = pm.scaled(thumbSize - QSize(pmMargin,pmMargin), Qt::KeepAspectRatio);
//         else pm = pm.scaled(thumbSize, Qt::KeepAspectRatio);
//     }
//     // define iconSize for reporting
//     QSize iconSize = QSize(pm.width(), pm.height());
//     // center the iconRect in the thumbRect
//     int alignVertPad = (thumbRect.height() - pm.height()) / 2;
//     int alignHorPad = (thumbRect.width() - pm.width()) / 2;
//     QRect iconRect(thumbRect.left() + alignHorPad, thumbRect.top() + alignVertPad,
//                    pm.width(), pm.height());

//     // INFO ROW containment rects overlap bottom of thumbRect and
//     // It is 1/6 or 0.17 of the thumbRect height

//     int infoHt = thumbRect.height() / 6;

//     /* debug
//     if (row == 0)
//         qDebug() << "IconViewDelegate::paint "
//                  << "row =" << row
//                  << "currentRow =" << currentRow
//                  << "selected =" << isSelected
//                  << "cellRect =" << cellRect
//                  << "frameRect =" << frameRect
//                  << "thumbRect =" << thumbRect
//                  << "iconRect =" << iconRect
//                  << "thumbSize =" << thumbSize
//                  << "infoHt =" << infoHt
//                  << "fontHt =" << fontHt
//                  << "textHeight =" << textHeight
//             ;
//     //             */

//     painter->drawRoundedRect(iconRect, 6, 6);

//     QRect textRect(frameRect.bottomLeft() - textHtOffset, frameRect.bottomRight());
//     QPainterPath textPath;
//     textPath.addRoundedRect(textRect, 8, 8);

//     QColor labelColorToUse;
//     if (G::labelColors.contains(colorClass) && isRatingBadgeVisible) {
//         if (colorClass == "Red") labelColorToUse = G::labelRedColor;
//         if (colorClass == "Yellow") labelColorToUse = G::labelYellowColor;
//         if (colorClass == "Green") labelColorToUse = G::labelGreenColor;
//         if (colorClass == "Blue") labelColorToUse = G::labelBlueColor;
//         if (colorClass == "Purple") labelColorToUse = G::labelPurpleColor;
//     }
//     else labelColorToUse = G::backgroundColor;

//     // START PAINTING (painters algorithm - last over first)

//     // paint the background label color and border
//     painter->setBrush(labelColorToUse);
//     painter->setPen(border);
//     painter->drawRoundedRect(frameRect, 8, 8);

//     // label (file name or title)
//     painter->setRenderHint(QPainter::Antialiasing, true);
//     painter->setRenderHint(QPainter::TextAntialiasing, true);

//     if (delegateShowThumbLabels) {
//         painter->setPen(labelTextColor);
//         painter->setFont(font);
//         painter->drawText(textRect, Qt::AlignHCenter, labelText);
//     }

//     // icon/thumbnail image
//     painter->setClipping(true);
//     painter->setClipRect(iconRect);
//     // painter->setClipPath(iconPath);
//     painter->drawPixmap(iconRect, pm);
//     painter->setClipping(false);

//     /*
//     qDebug() << "IconViewDelegate::paint"
//              << "row =" << row
//              << "currentRow =" << currentRow
//              << "selected item =" << option.state.testFlag(QStyle::State_Selected)
//              << "isVideo =" << isVideo
//              << "labeltext =" << labelText
//         ;
//             // */

//     // duration
//     int videoDurationHt = 0;
//     if (isVideo || showAllSymbols) {
//         QFont videoFont = painter->font();
//         videoFont.setPixelSize(G::fontSize);
//         painter->setFont(videoFont);
//         QRect bRect;
//         painter->setPen(G::backgroundColor);
//         painter->drawText(thumbRect, Qt::AlignBottom | Qt::AlignHCenter, "03:45:00", &bRect);
//         painter->setBrush(G::backgroundColor);
//         painter->drawRect(bRect);
//         painter->setPen(videoTextColor);
//         if (G::renderVideoThumb)
//             painter->drawText(bRect, Qt::AlignBottom | Qt::AlignHCenter, duration);
//         videoDurationHt = bRect.height();
//     }

//     // rating badge (color filled circle with rating number in center)
//     isRatingBadgeVisible = true;
//     if (isRatingBadgeVisible || showAllSymbols) {
//         // label/rating rect located top-right as containment for circle
//         if (G::ratings.contains(rating)) {
//             QColor textColor(Qt::white);
//             QPen ratingTextPen(textColor);
//             painter->setBrush(ratingBackgoundColor);
//             QFont starFont = painter->font();
//             QString stars;
//             int pxSize;

//             if (infoHt > 14) {
//                 stars.fill('*', ratingNumber);
//                 // Use the cached starsWidth and fontHt instead of fm.boundingRect()
//                 int b = (thumbRect.width() - starsWidth) / 2;
//                 int h = fontHt * 0.8;

//                 QPoint ratingTopLeft(thumbRect.left() + b, thumbRect.bottom() - videoDurationHt - h);
//                 QPoint ratingBottomRight(thumbRect.right() - b, thumbRect.bottom() - videoDurationHt);
//                 QRect ratingRect(ratingTopLeft, ratingBottomRight);

//                 painter->setPen(ratingTextPen);
//                 painter->drawText(ratingRect, Qt::AlignHCenter | Qt::AlignTop, stars);
//             }
//             else {
//                 stars = QString::number(ratingNumber);
//                 pxSize = infoHt;
//                 starFont.setPixelSize(pxSize);
//                 painter->setFont(starFont);
//                 painter->setPen(ratingTextPen);
//                 int w = pxSize;
//                 int h = w;
//                 int x = thumbRect.center().x() - w / 2;
//                 int y = thumbRect.bottom() - videoDurationHt - h + 3;
//                 QRect ratingRect(x, y, w, h);
//                 painter->drawText(ratingRect, Qt::AlignBottom, stars);
//             }
//         }
//     }

//     // show if combine raw/jpg for this image
//     if (isCombineRawJpg) {
//         painter->drawImage(combineRawJpgRect.translated(origin), combineRawJpgSymbol);
//     }

//     // show lock if file does not have read/write permissions
//     if (!isReadWrite || showAllSymbols) {
//         lockRenderer->render(painter, lockRect.translated(origin));
//     }

//     // draw the cache circle
//     if ((!isCached && !isVideo && metaLoaded && !G::isSlideShow) || showAllSymbols) {
//         painter->setPen(cacheBorderColor);
//         painter->setBrush(cacheColor);
//         painter->drawEllipse(cacheRect.translated(origin));
//     }

//     // draw the missing thumb circle
//     // qDebug() << "IconviewDeledate::paint" << index.row() << "isMissingThumb =" << isMissingThumb;
//     if ((G::useMissingThumbs && isMissingThumb) || showAllSymbols /*&& !G::isSlideShow*/) {
//         painter->setPen(cacheBorderColor);
//         painter->setBrush(missingThumbColor);
//         painter->drawEllipse(missingThumbRect.translated(origin));
//     }

//     painter->setPen(border);
//     painter->setBrush(Qt::transparent);

//     // pick status
//     if (isPicked) {
//         painter->setPen(pickedPen);
//         // painter->drawPath(iconPath);
//         painter->drawRoundedRect(iconRect, 6, 6);
//     }
//     if (isIngested) {
//         painter->setPen(ingestedPen);
//         // painter->drawPath(iconPath);
//         painter->drawRoundedRect(iconRect, 6, 6);
//     }
//     if (isRejected) {
//         painter->setPen(rejectedPen);
//         // painter->drawPath(iconPath);
//         painter->drawRoundedRect(iconRect, 6, 6);
//     }

//     // draw icon number
//     if (isIconNumberVisible || showAllSymbols) {
//         painter->setBrush(labelColorToUse);
//         QFont numberFont = painter->font();
//         int pxSize = iconNumberSize;
//         if (pxSize < 6) pxSize = 6;
//         numberFont.setPixelSize(pxSize);
//         numberFont.setBold(true);
//         painter->setFont(numberFont);
//         QFontMetrics fm(numberFont);
//         QString labelNumber = QString::number(sfRow + 1);
//         int numberWidth = fm.boundingRect(labelNumber).width() + 4;
//         QPoint numberTopLeft(frameRect.left(), frameRect.top());
//         QPoint numberBottomRight(frameRect.left() + numberWidth + 4, frameRect.top() + iconNumberSize);
//         QRect numberRect(numberTopLeft, numberBottomRight);
//         painter->setPen(Qt::transparent);
//         painter->drawRoundedRect(numberRect, 8, 8);
//         QPen numberPen(numberTextColor);
//         painter->setPen(numberPen);
//         painter->drawText(numberRect, Qt::AlignCenter, labelNumber);
//     }

//     painter->setBrush(Qt::transparent);

//     // selected item
//     if (isSelected) {
//         painter->setPen(selectedPen);
//         painter->drawRoundedRect(frameRect, 8, 8);
//     }

//     // current index item
//     if (isCurrentIndex) {
//         //if (row == currentRow) {
//         QRect currRect(cellRect.topLeft() + currOffset, cellRect.bottomRight() - currOffset);
//         painter->setPen(currentPen);
//         painter->drawRoundedRect(currRect, 8, 8);
//     }

//     // imageView viewport rect when zoomed
//     if (isCurrentIndex && vpRectIsVisible) {
//         if (isCurrentIndex && vpRectIsVisible) {
//             // Translate the pre-calculated rect to the current icon's screen position
//             QRect drawRect = targetVpRect.translated(iconRect.topLeft());

//             painter->setPen(vp1Pen);
//             painter->drawRect(drawRect);

//             painter->setPen(vp2Pen);
//             painter->drawRect(drawRect.adjusted(1, 1, -1, -1));
//         }


//         // // imA is original image aspect (in case thumbnail has black borders)
//         // // qDebug() << "IconViewDelegate::paint  imA =" << imA;
//         // qreal wIcon, hIcon;
//         // int wBlack = 0;     // black border
//         // int hBlack = 0;     // black border
//         // if (imA > 1) {
//         //     wIcon = iconRect.width();
//         //     hIcon = static_cast<int>(wIcon / imA);
//         //     hBlack = (iconRect.height() - hIcon) / 2;
//         // }
//         // else {
//         //     hIcon = iconRect.height();
//         //     wIcon = static_cast<int>(hIcon * imA);
//         //     wBlack = (iconRect.width() - wIcon) / 2;
//         // }
//         // int xIconTL = iconRect.x() + wBlack;
//         // int yIconTL = iconRect.y() + hBlack;
//         // int x1Vp = static_cast<int>(vpRect.topLeft().x() * wIcon);
//         // int y1Vp = static_cast<int>(vpRect.topLeft().y() * hIcon);
//         // int x2Vp = static_cast<int>(vpRect.bottomRight().x() * wIcon);
//         // int y2Vp = static_cast<int>(vpRect.bottomRight().y() * hIcon);
//         // int x1 = x1Vp + xIconTL;
//         // int y1 = y1Vp + yIconTL;
//         // int x2 = x2Vp + xIconTL;
//         // int y2 = y2Vp + yIconTL;
//         // /*
//         // qDebug() << "IconViewDelegate::paint"
//         //          << "cellRect =" << cellRect
//         //          << "thumbRect =" << thumbRect
//         //          << "iconRect =" << iconRect
//         //          << "vpRect =" << vpRect
//         //          << QRect(QPoint(x1,y1), QPoint(x2,y2))
//         //             ; // */
//         // painter->setPen(vp1Pen);
//         // painter->drawRect(QRect(QPoint(x1,y1), QPoint(x2,y2)));
//         // painter->setPen(vp2Pen);
//         // painter->drawRect(QRect(QPoint(x1+1,y1+1), QPoint(x2-1,y2-1)));
//     }

//     /* provide rect data to calc thumb mouse click position that is then sent to imageView to
//     zoom to the same spot */
//     emit update(index, iconRect);
//     /*
//     qDebug() << "IconViewDelegate::paint"
//              << "row =" << index.row()
//              << "CombineRawJpg =" << iconSymbolRects["CombineRawJpg"]
//         ; */

//     painter->restore();
// }
