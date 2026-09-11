#ifndef THUMBVIEWDELEGATE_H
#define THUMBVIEWDELEGATE_H

#include <QtGui>
#include <QtWidgets>
#include <QStyleOptionViewItem>
#include <QSvgRenderer>
#include <QFont>
#include "Datamodel/datamodel.h"
#include "Cache/cachedata.h"
#include "Utilities/utilities.h"

class IconViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    IconViewDelegate(QObject *parent,
                     bool &isRatingBadgeVisible,
                     bool &isIconNumberVisible,
                     DataModel *dm,
                     QItemSelectionModel *selectionModel
                     );

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index ) const override;

    void setThumbDimensions(int thumbWidth, int thumbHeight, int labelFontSize,
                            bool showThumbLabels, QString labelChoice,
                            int badgeSize, int iconNumberSize);

    QSize getCellSize();
    QSize getCellSize(QSize icon);
//    QSize getCellSizeFromAvailHeight(int availHeight);
    int getThumbHeightFromAvailHeight(int availHeight);
    int getThumbWidthFromCellWidth(int cellWidth);
    int getCellWidthFromThumbWidth(int width);
    int getCellHeightFromThumbHeight(int height);
    QPoint blackBorderOffset(const QModelIndex &sfIdx) const;
    void resetFirstLastVisible();
    void clearCacheItem(int sfRow) { iconCache.remove(sfRow); }
    void clearAllCache() { iconCache.clear(); }
    QString diagnostics();

    // The key is the model proxy row
    mutable QCache<int, QPixmap> iconCache;     // item pixmap (source icon or thumb)
    mutable QCache<int, QRect> thumbRectCache;  // thumb rect (w/o black border)
    int maxCacheSize = 10000;

    QModelIndex currentIndex;
    int currentRow = 0;
    /*  The same values resetFirstLastVisible() primes them with -- firstVisible is a
        MINIMUM being reduced, so its starting point has to be large, not zero. */
    mutable int firstVisible = 99999999;
    mutable int lastVisible = 0;
    mutable int midVisible = 0;
    mutable QRect missingIconRect;          // cell coordinates
    mutable QRect lockRect;                 // cell coordinates
    mutable QRect combineRawJpgRect;        // cell coordinates

    const QRect r;

    // int itemSpacing;
    // QSize itemSize;

    int fPad = 0;
    int tPad = 0;
    int pad = 0;
    int pad2 = 0;
    int fPad2 = 0;
    int tPad2 = 0;

    QString tooltip;
    QString objName;

signals:

protected:
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
                   const QStyleOptionViewItem &option, const QModelIndex &index);

public slots:
    void setNormVpRect(QSizeF vpSizeN, qreal vpA, QPointF vpCntr);
    void setVpRectVisibility(bool isVisible);

private:
    QObject parent;
    DataModel *dm;
    QItemSelectionModel *selectionModel;

    QRect getSymbolRect(const QString &symbol, const QRect &optionRect,
                        const QModelIndex &index) const;
    // QPoint blackBorderOffset(const QModelIndex &sfIdx) const;

    bool &isRatingBadgeVisible;
    bool &isIconNumberVisible;
    bool delegateShowThumbLabels;
    QString labelChoice;
    QFont font;
    QFont starFont;
    QString badFile = "🚫";
    QSvgRenderer *lockRenderer;

    // define colors
    QColor defaultBorderColor;
    QColor currentItemColor;
    QColor selectedColor;
    QColor pickColor;
    QColor rejectColor;
    QColor ingestedColor;
    QColor cacheColor;
    QColor sidecarColor;
    QColor developColor;
    QColor offlineColor;
    QColor missingColor;
    QColor cacheBorderColor;
    QColor ratingBackgoundColor;
    QColor labelTextColor;
    QColor videoTextColor;
    QColor numberTextColor;
    QColor vp1Color;
    QColor vp2Color;

    QPen pickedPen;
    QPen rejectedPen;
    QPen ingestedPen;
    QPen notPickPen;
    QPen backPen;
    QPen currentPen;
    QPen selectedPen;
    QPen vp1Pen;
    QPen vp2Pen;

    /*  ZERO-INITIALISED, BECAUSE paint() CAN RUN BEFORE setThumbDimensions DOES.

        These are set in exactly one place (IconViewDelegate::setThumbDimensions) and read
        by paint(). They are non-static members, so until that first call they hold
        whatever was on the heap -- and starsWidth and fontHt go straight into
        QRect(x, y, w, h) for the rating badge. Qt 6.9 made QRect's constructor compute
        x2 = x + w - 1 with CHECKED arithmetic, so a garbage width is no longer a wrong
        rectangle, it is an assert: qt_assert_x -> QCheckedInt operator+ -> QRect ->
        IconViewDelegate::paint, which is the abort captured on 2026-09-06.

        Zero turns that back into an empty rect -- nothing drawn, no crash -- which is the
        right answer for "we do not know the geometry yet". The file already carries one
        scar from this class of bug: see the setPixelSize note in setThumbDimensions. */
    int labelFontSize = 0;
    int fontHt = 0;
    int textHeadroom = 0;
    int textHeight = 0;
    int badgeSize = 0;
    int iconNumberSize = 0;
    int cacheDiam = 6;
    int cacheOffset = 3;
    int ratingDiam = 0;
    int ratingTextSize = 0;
    int alignVertPad = 0;
    int alignHorPad = 0;
    int starsWidth = 0;

    QPoint fPadOffset;
    QPoint tPadOffset;
    QPoint currOffset;
    QPoint textHtOffset;

    QPoint ratingTopLeft;
    QPoint ratingBottomRight;
    QRect ratingRect;
    QPoint ratingTextTopLeft;
    QPoint ratingTextBottomRight;
    QPoint cacheTopLeft;
    QPoint cacheBottomRight;

    QSize cellSize;
    QSize cellSpace;
    QSize frameSize;
    QSize selectedSize;
    QSize itemSize;
    int itemSpacing = 0;

    QRect cacheRect;
    QRect sidecarRect;
    QRect developRect;
    QRect availabilityRect;

    QPointF vpCntrN;
    QSizeF vpSizeN;
    qreal vpA;

    bool vpRectIsVisible;

    QImage combineRawJpgSymbol;
};

#endif // THUMBVIEWDELEGATE_H
