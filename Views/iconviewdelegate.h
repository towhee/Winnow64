#ifndef THUMBVIEWDELEGATE_H
#define THUMBVIEWDELEGATE_H

#include <QtGui>
#include <QtWidgets>
#include <QStyleOptionViewItem>
#include <QFont>
#include "Cache/cachedata.h"
#include "Main/global.h"

class IconViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    IconViewDelegate(QObject *parent, bool &isRatingBadgeVisible, ImageCacheData *icd);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &/*index*/ ) const override;

    void setThumbDimensions(int thumbWidth, int thumbHeight,
          int labelFontSize, bool showThumbLabels, int badgeSize);

    QSize getCellSize();
    QSize getCellSize(QSize icon);
    int getThumbHeightFromAvailHeight(int availHeight);
    int getThumbWidthFromCellWidth(int cellWidth);
    int getCellWidthFromThumbWidth(int width);
    int getCellHeightFromThumbHeight(int height);

    QString diagnostics();

    QModelIndex currentIndex;
    int currentRow;

    const QRect r;

    int thumbSpacing;

    int fPad;
    int tPad;
    int pad;
    int pad2;
    int fPad2;
    int tPad2;

signals:
    void update(const QModelIndex index, QRect iconRect) const;

public slots:
    void setCurrentIndex(QModelIndex current);
    void setCurrentRow(int row);

private:
    QObject parent;
    ImageCacheData *icd;
    bool &isRatingBadgeVisible;
    bool delegateShowThumbLabels;
    QFont font;

    // define colors
    QColor defaultBorderColor;
    QColor currentItemColor;
    QColor selectedColor;
    QColor pickColor;
    QColor rejectColor;
    QColor ingestedColor;
    QColor cacheColor;
    QColor cacheBorderColor;
    QColor videoTextcolor;

    QPen pickPen;
    QPen rejectPen;
    QPen ingestedPen;
    QPen notPickPen;
    QPen backPen;
    QPen currentPen;
    QPen selectedPen;

    int fontHt;
    int textHeadroom;
    int textHeight;
    int badgeSize;
    int cacheDiam = 6;
    int cacheOffset = 3;
    int ratingDiam;
    int ratingTextSize;
    int alignVertPad;
    int alignHorPad;

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

    QSize thumbSize;
    QSize frameSize;
    QSize selectedSize;
    QSize cellSize;
    QSize cellSpace;
};

#endif // THUMBVIEWDELEGATE_H
