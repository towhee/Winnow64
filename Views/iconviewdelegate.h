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
    int currentRow;
    mutable int firstVisible;
    mutable int lastVisible;
    mutable int midVisible;
    mutable QRect missingIconRect;          // cell coordinates
    mutable QRect lockRect;                 // cell coordinates
    mutable QRect combineRawJpgRect;        // cell coordinates

    const QRect r;

    // int itemSpacing;
    // QSize itemSize;

    int fPad;
    int tPad;
    int pad;
    int pad2;
    int fPad2;
    int tPad2;

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
    QColor missingThumbColor;
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

    int labelFontSize;
    int fontHt;
    int textHeadroom;
    int textHeight;
    int badgeSize;
    int iconNumberSize;
    int cacheDiam = 6;
    int cacheOffset = 3;
    int ratingDiam;
    int ratingTextSize;
    int alignVertPad;
    int alignHorPad;
    int starsWidth;

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
    int itemSpacing;

    QRect cacheRect;
    QRect missingThumbRect;

    QPointF vpCntrN;
    QSizeF vpSizeN;
    qreal vpA;

    bool vpRectIsVisible;

    QImage combineRawJpgSymbol;
};

#endif // THUMBVIEWDELEGATE_H
