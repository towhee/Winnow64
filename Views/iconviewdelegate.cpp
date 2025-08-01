
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
    // if (event->type() == QEvent::ToolTip)
    // {
    //     // QString toolTipData = view->model()->data(index, Qt::ToolTipRole).toString();
    //     if (!tooltip.isEmpty())
    //     {
    //         QPoint pos = event->globalPos() - QPoint(20, 20);
    //         QToolTip::showText(pos, tooltip, view, QRect(), 5000);
    //         return true;
    //     }
    // }

    // return QStyledItemDelegate::helpEvent(event, view, option, index);

    if (event->type() != QEvent::ToolTip || !index.isValid())
        return QStyledItemDelegate::helpEvent(event, view, option, index);

    // Extract useful data from model
    QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel*>(view->model());
    QModelIndex sourceIndex = proxy ? proxy->mapToSource(index) : index;
    QVariant v = sourceIndex.model()->data(sourceIndex.siblingAtColumn(G::IconSymbolColumn));

    using SymbolRectMap = QHash<QString, QRect>;
    QHash<QString, QRect> rects;
    if (v.canConvert<SymbolRectMap>())
        rects = v.value<SymbolRectMap>();

    QPoint viewPos = view->viewport()->mapFromGlobal(event->globalPos());
    int row = sourceIndex.row();

    auto sf = proxy ? proxy->sourceModel() : view->model();

    bool isMissing = sf->index(row, G::MissingThumbColumn).data().toBool();
    bool isLock = !sf->index(row, G::ReadWriteColumn).data().toBool();
    bool isRating = sf->index(row, G::RatingColumn).data().toBool();
    bool isCombineRawJpg = sf->index(row, 0).data(G::DupIsJpgRole).toBool() && G::combineRawJpg;
    bool isCached = sf->index(row, G::IsCachedColumn).data().toBool();
    bool isVideo = sf->index(row, G::VideoColumn).data().toBool();

    QString tooltip;

    if (!rects.value("Thumb").contains(viewPos))
        tooltip = "Borders:\n  Yellow:\t Current image\n  White:\t Selected image\n  Green:\t Picked\n  Blue:\t Ingested\n  Red:\t Rejected";
    else if (isMissing && rects.value("MissingThumb").contains(viewPos))
        tooltip = "Image does not have an embedded thumbnail";
    else if (isLock && rects.value("Lock").contains(viewPos))
        tooltip = "Image file is locked";
    else if (isRating && rects.value("Rating").contains(viewPos))
        tooltip = "Rating";
    else if (isCombineRawJpg && rects.value("CombineRawJpg").contains(viewPos))
        tooltip = "Image is JPG version of a RAW+JPG pair";
    else if (!isCached && rects.value("Cache").contains(viewPos))
        tooltip = "This image is not cached";
    else if (isVideo && rects.value("Duration").contains(viewPos))
        tooltip = "Duration";
    else
        tooltip = sf->index(row, 0).data(G::PathRole).toString();

    if (!tooltip.isEmpty()) {
        QToolTip::showText(event->globalPos(), tooltip, view, option.rect, 10000);
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
    The thumbSize cell contains a number of cells or rectangles:

    Outer dimensions = cellRect or option.rect (QListView icon spacing is set to zero)
    frameRect        = cellRect - cBrdT - fPad
    thumbRect        = itemRect - thumbBorderGap - padding - thumbBorderThickness
    iconRect         = thumbRect - icon (icon has different aspect so either the
                       width or height will have to be centered inside the thumbRect
    textRect         = a rectangle below itemRect
*/
    // Ignore paint before icon has been loaded into DataModel
    if (index.data(Qt::DecorationRole).isNull()) return;

    painter->save();
    /* debug
    qDebug() << "IconViewDelegate::paint  "
             << "row =" << index.row()
             << "index =" << index
             << "option.state =" << option.state
             //<< "option.state just enabled =" << enabled
             //<< "option.type =" << option.type
             //<< "option.features =" << option.features
             //<< "option.index =" << option.index
                ;
             //*/

    // show all symbols to document for help
    bool showAllSymbols = false;

    // make default border relative to background
    int l40 = G::backgroundShade + 40;
    QPen border;
    border.setWidth(1);
    border.setColor(QColor(l40,l40,l40));

    // get data from model
    int row = index.row();

    // first/last visible (not being used at present)
    if (row < firstVisible) firstVisible = row;
    if (row > lastVisible) lastVisible = row;
    midVisible = firstVisible + ((lastVisible - firstVisible) / 2);

    QString labelText;
    if (labelChoice == "Title") {
        labelText = index.model()->index(row, G::TitleColumn).data(Qt::DisplayRole).toString();
    }
    else {
        labelText = index.model()->index(row, G::NameColumn).data(Qt::DisplayRole).toString();
    }
    QString colorClass = index.model()->index(row, G::LabelColumn).data(Qt::EditRole).toString();
    int ratingNumber = index.model()->index(row, G::RatingColumn).data(Qt::EditRole).toInt();
    QString rating = index.model()->index(row, G::RatingColumn).data(Qt::EditRole).toString();
    QString duration = index.model()->index(row, G::DurationColumn).data(Qt::DisplayRole).toString();
    if (duration.isNull()) duration = "XXX";
    bool isSelected = dm->isSelected(row);
    bool isCurrentIndex = row == dm->currentSfRow;
    QString pickStatus = index.model()->index(row, G::PickColumn).data(Qt::EditRole).toString();
    bool isPicked = pickStatus == "Picked";
    bool isRejected = pickStatus == "Rejected";
    bool isIngested = index.model()->index(row, G::IngestedColumn).data(Qt::EditRole).toBool();
    bool isCached = index.model()->index(row, G::IsCachedColumn).data(Qt::EditRole).toBool();
    bool isMissingThumb = index.model()->index(row, G::MissingThumbColumn).data().toBool();
    bool metaLoaded = index.model()->index(row, G::MetadataLoadedColumn).data().toBool();
    bool isVideo = index.model()->index(row, G::VideoColumn).data().toBool();
    bool isReadWrite = index.model()->index(row, G::ReadWriteColumn).data().toBool();
    bool isCombineRawJpg = index.model()->index(row, 0).data(G::DupIsJpgRole).toBool() && G::combineRawJpg;

    // Cell structure (see IconViewDelegate Anatomy at top of file).
    QRect cellRect(option.rect);
    QRect frameRect(cellRect.topLeft() + fPadOffset, frameSize);
    QRect thumbRect(frameRect.topLeft() + tPadOffset, thumbSize);

    // save info row symbol rects in datamodel so can show tooltips later
    QHash<QString, QRect>iconSymbolRects;

    iconSymbolRects["Thumb"] = QRect(thumbRect.topLeft() + QPoint(1,1),
                                     thumbRect.bottomRight() - QPoint(1,1));

    // get icon (thumbnail) from the datamodel and scale
    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    // get the icon dimensions to fit into G::maxIconSize.  Must start with largest size
    // to prevent scaling glitches.
    QSize maxSize = icon.actualSize(QSize(G::maxIconSize,G::maxIconSize));  // 256,256
    // convert to a QPixmap
    QPixmap pm = icon.pixmap(maxSize);
    if (!pm.isNull()) {
        // if the pm is nearly square scale slightly smaller so border large enough to show color class
        int pmMargin = 8;
        bool isSquare = (qAbs(pm.width() - pm.height()) < pmMargin);
        // scale the pixmap to fit in the thumbRect
        if (isSquare) pm = pm.scaled(thumbSize - QSize(pmMargin,pmMargin), Qt::KeepAspectRatio);
        else pm = pm.scaled(thumbSize, Qt::KeepAspectRatio);
    }
    // define iconSize for reporting
    QSize iconSize = QSize(pm.width(), pm.height());
    // center the iconRect in the thumbRect
    int alignVertPad = (thumbRect.height() - pm.height()) / 2;
    int alignHorPad = (thumbRect.width() - pm.width()) / 2;
    QRect iconRect(thumbRect.left() + alignHorPad, thumbRect.top() + alignVertPad,
                   pm.width(), pm.height());

    // INFO ROW containment rects overlap bottom of thumbRect and
    // It is 1/6 or 0.17 of the thumbRect height

    int infoHt = thumbRect.height() / 6;

    /* debug
    if (row == 0)
        qDebug() << "IconViewDelegate::paint "
                 << "row =" << row
                 << "currentRow =" << currentRow
                 << "selected =" << isSelected
                 << "cellRect =" << cellRect
                 << "frameRect =" << frameRect
                 << "thumbRect =" << thumbRect
                 << "iconRect =" << iconRect
                 << "thumbSize =" << thumbSize
                 << "infoHt =" << infoHt
                 << "fontHt =" << fontHt
                 << "textHeight =" << textHeight
            ;
    //             */

    // cached rect located bottom right as containment for circle
    int dotDiam = 6;
    int dotOffset = 3;
    QPoint cacheTopLeft(thumbRect.right() - dotDiam - dotOffset,
                        thumbRect.bottom() - dotDiam - dotOffset);
    QPoint cacheBottomRight(thumbRect.right() - dotOffset, thumbRect.bottom() - dotOffset);
    QRect cacheRect(cacheTopLeft, cacheBottomRight);
    iconSymbolRects["Cache"] = cacheRect;

    // combine raw/jpg rect to left of cached rect
    {
        int pxSize = 14;
        int h = pxSize;
        int w = h * 1.2;
        int x = cacheRect.left() - w - 2;
        int y = thumbRect.bottom() - h + 3;
        combineRawJpgRect.setRect(x, y, w, h);
        iconSymbolRects["CombineRawJpg"] = combineRawJpgRect;
    }

    // missing thumb rect located bottom left as containment for circle
    const QPoint missingThumbTopLeft(thumbRect.left() + dotDiam - dotOffset,
                        thumbRect.bottom() - dotDiam - dotOffset);
    const QPoint missingThumbBottomRight(thumbRect.left() + dotDiam + dotOffset,
                                         thumbRect.bottom() - dotOffset);
    QRect missingThumbRect(missingThumbTopLeft, missingThumbBottomRight);
    iconSymbolRects["MissingThumb"] = missingThumbRect;
    int missingThumbX = fPad + tPad + dotDiam - dotOffset;
    int missingThumbY = fPad + tPad + thumbRect.height() - dotDiam - dotOffset;
    missingIconRect.setRect(missingThumbX, missingThumbY, dotDiam, dotDiam);

    // locked file rect
    {
        int pxSize = 10;
        int x = missingThumbRect.right() + 2;
        int y = thumbRect.bottom() - pxSize;
        int w = pxSize + 2;
        int h = w;
        lockRect.setRect(x, y, w, h);
        iconSymbolRects["Lock"] = lockRect;
    }

    QPainterPath iconPath;
    iconPath.addRoundedRect(iconRect, 6, 6);

    QRect textRect(frameRect.bottomLeft() - textHtOffset, frameRect.bottomRight());
    QPainterPath textPath;
    textPath.addRoundedRect(textRect, 8, 8);

    QColor labelColorToUse;
    if (G::labelColors.contains(colorClass) && isRatingBadgeVisible) {
        if (colorClass == "Red") labelColorToUse = G::labelRedColor;
        if (colorClass == "Yellow") labelColorToUse = G::labelYellowColor;
        if (colorClass == "Green") labelColorToUse = G::labelGreenColor;
        if (colorClass == "Blue") labelColorToUse = G::labelBlueColor;
        if (colorClass == "Purple") labelColorToUse = G::labelPurpleColor;
    }
    else labelColorToUse = G::backgroundColor;

    // START PAINTING (painters algorithm - last over first)

    // paint the background label color and border
    painter->setBrush(labelColorToUse);
    painter->setPen(border);
    painter->drawRoundedRect(frameRect, 8, 8);

    // label (file name or title)
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    if (delegateShowThumbLabels) {
        painter->setPen(labelTextColor);
        painter->setFont(font);
        painter->drawText(textRect, Qt::AlignHCenter, labelText);
    }

    // icon/thumbnail image
    painter->setClipping(true);
    painter->setClipPath(iconPath);
    painter->drawPixmap(iconRect, pm);
    painter->setClipping(false);

    /*
    qDebug() << "IconViewDelegate::paint"
             << "row =" << row
             << "currentRow =" << currentRow
             << "selected item =" << option.state.testFlag(QStyle::State_Selected)
             << "isVideo =" << isVideo
             << "labeltext =" << labelText
        ;
            // */

    int videoDurationHt = 0;
    if (isVideo || showAllSymbols) {
        QFont videoFont = painter->font();
        videoFont.setPixelSize(G::fontSize);
        painter->setFont(videoFont);
        QRect bRect;
        painter->setPen(G::backgroundColor);
        painter->drawText(thumbRect, Qt::AlignBottom | Qt::AlignHCenter, "03:45:00", &bRect);
        painter->setBrush(G::backgroundColor);
        painter->drawRect(bRect);
        painter->setPen(videoTextColor);
        if (G::renderVideoThumb)
            painter->drawText(bRect, Qt::AlignBottom | Qt::AlignHCenter, duration);
        videoDurationHt = bRect.height();
        iconSymbolRects["Duration"] = bRect;
    }

    // rating badge (color filled circle with rating number in center)
    isRatingBadgeVisible = true;
    if (isRatingBadgeVisible || showAllSymbols) {
        // label/rating rect located top-right as containment for circle
        if (G::ratings.contains(rating)) {
            QColor textColor(Qt::white);
            QPen ratingTextPen(textColor);
            painter->setBrush(ratingBackgoundColor);
            QFont starFont = painter->font();
            QString stars;
            int pxSize;
            if (infoHt > 14) {
                stars.fill('*', ratingNumber);
                pxSize = 20;
                starFont.setPixelSize(pxSize);
                painter->setFont(starFont);
                QFontMetrics fm(starFont);
                QRect bRect = fm.boundingRect("*******");
                int w = bRect.width();
                int b = (thumbRect.width() - w) / 2;
                int h = pxSize * 0.8;
                int t = h / 5;      // translate to center * in ratingRect
                QPoint ratingTopLeft(thumbRect.left() + b, thumbRect.bottom() - videoDurationHt - h);
                QPoint ratingBottomRight(thumbRect.right() - b, thumbRect.bottom() - videoDurationHt);
                QRect ratingRect(ratingTopLeft, ratingBottomRight);
                iconSymbolRects["Rating"] = ratingRect;
                ratingTextPen.setWidth(1);
                painter->setPen(ratingTextPen);
                painter->drawText(ratingRect.adjusted(0,t,0,t), Qt::AlignHCenter | Qt::AlignTop, stars);
            }
            else {
                stars = QString::number(ratingNumber);
                pxSize = infoHt;
                starFont.setPixelSize(pxSize);
                painter->setFont(starFont);
                painter->setPen(ratingTextPen);
                int w = pxSize;
                int h = w;
                int x = thumbRect.center().x() - w / 2;
                int y = thumbRect.bottom() - videoDurationHt - h + 3;
                QRect ratingRect(x, y, w, h);
                painter->drawText(ratingRect, Qt::AlignBottom, stars);
            }
        }
    }

    // show if combine raw/jpg for this image
    if (isCombineRawJpg) {
        painter->drawImage(combineRawJpgRect, combineRawJpgSymbol);
    }

    // show lock if file does not have read/write permissions
    if (!isReadWrite || showAllSymbols) {
        QFont lockFont = painter->font();
        int pxSize = 10;
        lockFont.setPixelSize(pxSize);
        painter->setFont(lockFont);
        painter->setPen(G::textColor);  // any pen
        painter->drawText(lockRect, Qt::AlignBottom, "🔒");
    }

    // draw the cache circle
    if ((!isCached && !isVideo && metaLoaded && !G::isSlideShow) || showAllSymbols) {
        painter->setPen(cacheBorderColor);
        painter->setBrush(cacheColor);
        painter->drawEllipse(cacheRect);
    }

    // draw the missing thumb circle
    // qDebug() << "IconviewDeledate::paint" << index.row() << "isMissingThumb =" << isMissingThumb;
    if ((G::useMissingThumbs && isMissingThumb) || showAllSymbols /*&& !G::isSlideShow*/) {
        painter->setPen(cacheBorderColor);
        painter->setBrush(missingThumbColor);
        painter->drawEllipse(missingThumbRect);
    }

    painter->setPen(border);
    painter->setBrush(Qt::transparent);

    // pick status
    if (isPicked) {
        painter->setPen(pickedPen);
        painter->drawPath(iconPath);
    }
    if (isIngested) {
        painter->setPen(ingestedPen);
        painter->drawPath(iconPath);
    }
    if (isRejected) {
        painter->setPen(rejectedPen);
        painter->drawPath(iconPath);
    }

    // draw icon number
    if (isIconNumberVisible || showAllSymbols) {
        painter->setBrush(labelColorToUse);
        QFont numberFont = painter->font();
        int pxSize = iconNumberSize;
        if (pxSize < 6) pxSize = 6;
        numberFont.setPixelSize(pxSize);
        numberFont.setBold(true);
        painter->setFont(numberFont);
        QFontMetrics fm(numberFont);
        QString labelNumber = QString::number(row + 1);
        int numberWidth = fm.boundingRect(labelNumber).width() + 4;
        QPoint numberTopLeft(frameRect.left(), frameRect.top());
        QPoint numberBottomRight(frameRect.left() + numberWidth + 4, frameRect.top() + iconNumberSize);
        QRect numberRect(numberTopLeft, numberBottomRight);
        painter->setPen(Qt::transparent);
        painter->drawRoundedRect(numberRect, 8, 8);
        QPen numberPen(numberTextColor);
        painter->setPen(numberPen);
        painter->drawText(numberRect, Qt::AlignCenter, labelNumber);
    }

    painter->setBrush(Qt::transparent);

    // selected item
    if (isSelected) {
        painter->setPen(selectedPen);
        painter->drawRoundedRect(frameRect, 8, 8);
    }

    // current index item
    if (isCurrentIndex) {
        //if (row == currentRow) {
        QRect currRect(cellRect.topLeft() + currOffset, cellRect.bottomRight() - currOffset);
        painter->setPen(currentPen);
        painter->drawRoundedRect(currRect, 8, 8);
    }

    // imageView viewport rect when zoomed
    if (isCurrentIndex && vpRectIsVisible) {
        // imA is original image aspect (in case thumbnail has black borders)
        // qDebug() << "IconViewDelegate::paint  imA =" << imA;
        qreal wIcon, hIcon;
        int wBlack = 0;     // black border
        int hBlack = 0;     // black border
        if (imA > 1) {
            wIcon = iconRect.width();
            hIcon = static_cast<int>(wIcon / imA);
            hBlack = (iconRect.height() - hIcon) / 2;
        }
        else {
            hIcon = iconRect.height();
            wIcon = static_cast<int>(hIcon * imA);
            wBlack = (iconRect.width() - hIcon) / 2;
        }
        int xIconTL = iconRect.x() + wBlack;
        int yIconTL = iconRect.y() + hBlack;
        int x1Vp = static_cast<int>(vpRect.topLeft().x() * wIcon);
        int y1Vp = static_cast<int>(vpRect.topLeft().y() * hIcon);
        int x2Vp = static_cast<int>(vpRect.bottomRight().x() * wIcon);
        int y2Vp = static_cast<int>(vpRect.bottomRight().y() * hIcon);
        int x1 = x1Vp + xIconTL;
        int y1 = y1Vp + yIconTL;
        int x2 = x2Vp + xIconTL;
        int y2 = y2Vp + yIconTL;
        painter->setPen(vp1Pen);
        painter->drawRect(QRect(QPoint(x1,y1), QPoint(x2,y2)));
        painter->setPen(vp2Pen);
        painter->drawRect(QRect(QPoint(x1+1,y1+1), QPoint(x2-1,y2-1)));
    }

    /* provide rect data to calc thumb mouse click position that is then sent to imageView to
    zoom to the same spot */
    emit update(index, iconRect);

    // save the locations of the symbols so can show tooltips
    dm->setData(dm->index(row, G::IconSymbolColumn), QVariant::fromValue(iconSymbolRects));

    /*
    qDebug() << "IconViewDelegate::paint"
             << "row =" << index.row()
             << "CombineRawJpg =" << iconSymbolRects["CombineRawJpg"]
        ; */

    painter->restore();
}
