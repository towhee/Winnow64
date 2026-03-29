
#include "iconviewdelegate.h"
#include "Main/global.h"

/*

IconViewDelegate Anatomy:

    cell:  the entire icon shown be the delegate
    frame: the cell border
    item:  the "thumbnail" provided for the image
    thumb: the item without any black borders

0---- cellRect = option.rect --------------------------------|
|                                                            |
|           framePadding (fPad)                              |
|                                                            |
|     1-----frameRect----------------------------------+     |
|     |                                                |     |
|     |          thumbPadding (tPad)                   |     |
|     |                                                |     |
|     |     2----itemRect------------------------+  ^  |     |
|     |     |                                    |  :  |     |
|     |     5----outerItem-----------------------|  :  |     |
|     |     |    horizontal black border         |  :  |     |
|     |     6----thumbRect-----------------------|  t  |     |
|     |     |                                    |  h  |     |
|     |     |                                    |  u  |     |
|     |     |                                    |  m  |     |
|     |     |                                    |  b  |     |
|     |     |                                    |     |     |
|     |     |                                    |  H  |     |
|     |     |                                    |  e  |     |
|     |     |                                    |  i  |     |
|     |     |----------------------------------- |  g  |     |
|     |     |     horizontal black border        |  h  |     |
|     |     4 -----------------------------------|  t  |     |
|     |     |                                    |  :  |     |
|     |     |                                    |  :  |     |
|     |     |               *****                |  :  |     |
|     3----textRect -----------------------------+--˅--|     |
|     |                                                |     |
|     |     <---------- thumbWidth -------------->     |     |
|     |                                                |     |
|     +------------------------------------------------+     |
|                                                            |
|                                                            |
|                                                            |
+------------------------------------------------------------+

Point coordinates:
0 = option.rect.topLeft()
1 = fPadOffset
2 = fPadOffset + tPadOffset
3 = frameRect.bottomLeft() - textHtOffset
4 = info row (rating etc) = 1/6 thumbRect height
5 = itemRect.topleft() + alignOff (based on aspect ratio)
6 = outerThumbRect.topleft() + bboOff (black border offset)

IconView thumbDock Anatomy:

+-----------------------------------------------------------------------^-------->
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

    itemSize.setWidth(thumbWidth);
    itemSize.setHeight(thumbHeight);

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
    cacheRect.setRect(itemSize.width() - dotDiam - dotOffset,
                      itemSize.height() - dotDiam - dotOffset /*+ 2*/,
                      dotDiam, dotDiam);

    // Missing Thumb Circle (Bottom Left)
    missingThumbRect.setRect(dotDiam - dotOffset,
                             itemSize.height() - dotDiam - dotOffset /*+ 2*/,
                             dotDiam, dotDiam);

    // Lock Icon (To the right of Missing Thumb)
    int lockSize = 12; // Adjusted for SVG
    lockRect.setRect(missingThumbRect.right() + 2,
                     itemSize.height() - lockSize,
                     lockSize, lockSize);

    // Combine RAW+JPG (To the left of Cache)
    int linkW = 14;
    int linkH = 14;
    combineRawJpgRect.setRect(cacheRect.left() - linkW - 2,
                              itemSize.height() - 0.8 * linkH,
                              linkW, linkH);

    // When dimensions change, all cached pixmaps are the wrong size
    iconCache.clear();
}

QString IconViewDelegate::diagnostics()
{
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << "iconViewDelegate Diagnostics:\n";
    rpt << "\n" << "currentRow = " << G::s(currentRow);
    rpt << "\n" << "thumbSize = " << G::s(itemSize.width()) << "," << G::s(itemSize.height());
    rpt << "\n" << "frameSize = " << G::s(frameSize.width()) << "," << G::s(frameSize.height());
    rpt << "\n" << "selectedSize = " << G::s(selectedSize.width()) << "," << G::s(selectedSize.height());
    rpt << "\n" << "cellSize = " << G::s(cellSize.width()) << "," << G::s(cellSize.height());
    rpt << "\n" << "thumbSpacing = " << G::s(itemSpacing);
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
    // used to fit item (thumb with black borders) into the available viewport height
    return availHeight - pad2 - textHeight - 2;
}

void IconViewDelegate::setVpRectVisibility(bool isVisible)
{
    vpRectIsVisible = isVisible;
}

void IconViewDelegate::setNormVpRect(QSizeF vpSizeN, qreal vpA, QPointF vpCntrN)
{
    this->vpSizeN = vpSizeN;
    this->vpCntrN = vpCntrN;
    this->vpA = vpA;
}

QPoint IconViewDelegate::blackBorderOffset(const QModelIndex &sfIdx) const
{
    /* Some brands create thumbnails with black borders, which are not part of the
    image, and should be excluded. The long side (ie width if landscape, height if
    portrait, will not have a black border. Using that side and the aspect of the
    original image can give the correct length for the other side of the thumbnail.
    Returns offset in normalized coordinates.
    */
    QRect itemRect = sfIdx.data(G::IconRectRole).toRect();
    int itemW = itemRect.width();
    int itemH = itemRect.height();
    qreal imA = dm->sf->index(sfIdx.row(), G::AspectRatioColumn).data().toReal();
    qreal itemA = static_cast<qreal>(itemRect.width()) / itemRect.height();
    qreal xOff = 0;
    qreal yOff = 0;
    if (!qFuzzyCompare(imA, itemA)) {
        if (imA > 1) {
            // landscape, top/bottom black border
            yOff = (itemH - itemW / imA) / 2;
        }
        else {
            // portrait, left/right black border
            xOff = (itemW - itemH * imA) / 2;
        }
    }
    /*
    QString srcFun = "IconViewDelegate::blackBorderOffset";
    qDebug().noquote()
             << srcFun.leftJustified(40)
             << "sfRow =" << sfIdx.row()
             << "imA =" << imA
             << "iconA =" << iconA
             << "iconW =" << iconW
             << "iconH =" << iconH
             << "xOff =" << xOff
             << "yOff =" << yOff
                ; //*/
    return QPoint(xOff, yOff);
}

QRect IconViewDelegate::getSymbolRect(const QString &symbol, const QRect &optionRect, const QModelIndex &index) const
{
    // Common geometry bases
    QRect frameRect(optionRect.topLeft() + fPadOffset, frameSize);
    QRect thumbRect(frameRect.topLeft() + tPadOffset, itemSize);
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
/*
The delegate cell size is defined in setThumbDimensions and assigned in sizeHint.
The cell anatomy:

Outer dimensions = cellRect or option.rect (QListView icon spacing is set to zero)
frameRect        = cellRect - cBrdT - fPad
itemRect         = cellRect - thumbBorderGap - padding - thumbBorderThickness
thumbRect        = itemRect - item (item has different aspect so either the
                   width or height will have to be centered inside the thumbRect
textRect         = a rectangle below itemRect
*/
    // Quick exit if the icon has not been loaded into the DataModel
    if (index.data(Qt::DecorationRole).isNull()) return;

    QString srcFun = "IconViewDelegate::Paint";
    // qDebug().noquote()
    //     << srcFun.leftJustified(40)
    //     << "sfRow =" << index.row()
    //     << "option.state =" << option.state;


    painter->save();

    // --- DATA ---

    // Pull model data once to avoid repeated indexing during the paint loop
    int sfRow = index.row();
    bool isSelected = dm->isSelected(sfRow);
    bool isCurrentIndex = (sfRow == dm->currentSfRow);

    /*
    if (isCurrentIndex)
    qDebug() << "IconViewDelegate::paint"
             << "sfRow =" << sfRow
             << "dm->currentSfRow =" << dm->currentSfRow
             << "isCurrentIndex =" << isCurrentIndex
             << "vpRectIsVisible =" << vpRectIsVisible
             << "option.state =" << option.state
        ;//*/

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

    // --- THUMBNAIL ---

    // Check if we already have the scaled pixmap for this row
    QPixmap *cachedPm = iconCache.object(sfRow);
    QPixmap pm;

    if (cachedPm) {
        pm = *cachedPm;
    } else {
        // Expensive retrieve and scale the datamodel icon pixmap to fit in thumbRect
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QSize maxSize = icon.actualSize(QSize(G::maxIconSize, G::maxIconSize));
        pm = icon.pixmap(maxSize);

        if (!pm.isNull()) {
            // Apply square-ish scaling adjustment for color class border visibility
            int pmMargin = 8;
            bool isSquare = (qAbs(pm.width() - pm.height()) < pmMargin);

            if (isSquare) {
                pm = pm.scaled(itemSize - QSize(pmMargin, pmMargin), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            } else {
                pm = pm.scaled(itemSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }

            // Insert into cache so we don't do this again for this row
            iconCache.insert(sfRow, new QPixmap(pm));
        }
    }

    // --- CELL GEOMETRY ---

    // Calculate the standard Winnow cell geometry
    QRect cellRect(option.rect);
    QRect frameRect(cellRect.topLeft() + fPadOffset, frameSize);
    QRect itemRect(frameRect.topLeft() + tPadOffset, itemSize);
    QRect outerThumbRect;    // includes possible black border
    QRect thumbRect;         // inside black border
    QPoint origin = itemRect.topLeft();

    // Calculate iconRect based on the (now potentially cached) pixmap dimensions
    int alignVert = (itemRect.height() - pm.height()) / 2;
    int alignHor = (itemRect.width() - pm.width()) / 2;
    QPoint alignOffset(alignHor, alignVert);
    QSize outerIconSize(pm.width(), pm.height());
    outerThumbRect = QRect(itemRect.topLeft() + alignOffset, outerIconSize);
    // outerIconRect = QRect(thumbRect.left() + alignHor, thumbRect.top() + alignVert, pm.width(), pm.height());

    // bbo = Black Border Offset (N normalized)
    QPointF bboN = blackBorderOffset(index);
    int bboX = bboN.x();
    int bboY = bboN.y();
    QPoint bboOffset(bboX, bboY);

    // thumb = itemRect excluding black borders
    int thumbW = outerThumbRect.width() - bboX * 2;
    int thumbH = outerThumbRect.height() - bboY * 2;
    QSize thumbSize(thumbW, thumbH);

    // iconRect (used in IconView::mouseReleaseEvent to emit thumbClick)
    thumbRect = QRect(outerThumbRect.topLeft() + bboOffset, thumbSize);
    thumbRectCache.insert(sfRow, new QRect(thumbRect));

    // --- PAINTING ---

    // Determine frame background color based on colorClass labels
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
        painter->setClipRect(outerThumbRect);
        painter->drawPixmap(outerThumbRect, pm);
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
        painter->drawText(itemRect, Qt::AlignBottom | Qt::AlignHCenter, duration, &bRect);
        videoDurationHt = bRect.height();
    }

    // Render rating stars or numerical badges
    int infoHt = itemRect.height() / 6;
    if (isRatingBadgeVisible && G::ratings.contains(rating)) {
        painter->setPen(Qt::white);
        if (infoHt > 14) {
            QString stars(ratingNumber, '*');
            int b = (itemRect.width() - starsWidth) / 2;
            int h = fontHt * 0.8;
            painter->drawText(QRect(itemRect.left() + b, itemRect.bottom() - videoDurationHt - h, starsWidth, h),
                              Qt::AlignHCenter | Qt::AlignTop, stars);
        } else {
            QFont starFont = painter->font();
            starFont.setPixelSize(infoHt);
            painter->setFont(starFont);
            painter->drawText(QRect(itemRect.center().x() - infoHt/2, itemRect.bottom() - videoDurationHt - infoHt + 3, infoHt, infoHt),
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

    // Draw status borders for Picked / Ingested / Rejected states
    painter->setBrush(Qt::transparent);
    if (isPicked) painter->setPen(pickedPen);
    else if (isIngested) painter->setPen(ingestedPen);
    else if (isRejected) painter->setPen(rejectedPen);

    if ((isPicked || isIngested || isRejected) && !pm.isNull()) {
        painter->drawRoundedRect(outerThumbRect, 6, 6);
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

    // Render the loupe viewport rectangle
    if (isCurrentIndex && vpRectIsVisible) {

        // vp = relative ImageView viewport
        int vpW = vpSizeN.width() * thumbW;      // convert normalized values
        int vpH = vpSizeN.height() * thumbW;     // convert normalized values

        // apply aspect ratio
        if (vpA > 1.0) vpH = vpW / vpA;
        else vpW = vpH * vpA;

        // center
        int vpCntrX = vpCntrN.x() * thumbW;      // convert normalized values
        int vpCntrY = vpCntrN.y() * thumbH;      // convert normalized values

        // top left
        int vpX = thumbRect.x() + vpCntrX - vpW/2;
        int vpY = thumbRect.y() + vpCntrY - vpH/2;

        /*
        QString srcFun = "IconViewDelegate::Paint";
        qDebug().noquote()
                 << srcFun.leftJustified(40)
                 << "sfRow =" << index.row()
                 // << "objName =" << objName
                 // << "bboX =" << bboX
                 // << "bboY =" << bboY
                 // << "iconW =" << iconW
                 // << "iconH =" << iconH
                 // << "nIconW =" << iconW
                 // << "nIconH =" << iconH
                 // << "vpCntrN =" << vpCntrN
                 // << "vpCntrX =" << vpCntrX
                 // << "vpCntrY =" << vpCntrY
                 // << "vpX =" << vpX
                 // << "vpY =" << vpY
                 << "vpW =" << vpW
                 << "vpH =" << vpH
                 << "vpA =" << vpA
                 // << "option.state =" << option.state
            ;  //*/

        // viewport rectange with room for border
        QRect vpRect(vpX-=2, vpY-=2, vpW+=4, vpH+=4);

        painter->save();
        painter->setClipping(true);
        painter->setClipRect(outerThumbRect);
        painter->setPen(vp1Pen);
        painter->drawRect(vpRect);
        painter->setPen(vp2Pen);
        painter->drawRect(vpRect.adjusted(1, 1, -1, -1));
        painter->restore();
    }

    painter->restore();
}
