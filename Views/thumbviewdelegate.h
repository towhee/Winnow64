#ifndef THUMBVIEWDELEGATE_H
#define THUMBVIEWDELEGATE_H

#include <QtGui>
#include <QtWidgets>
#include <QStyleOptionViewItem>
#include <QFont>
#include "Main/global.h"

class ThumbViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    ThumbViewDelegate(QObject *parent, bool &isRatingBadgeVisible);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index ) const;

    void setThumbDimensions(int thumbWidth, int thumbHeight, int thumbPadding,
          int labelFontSize, bool showThumbLabels, int badgeSize);

    QSize getThumbCell();
    int getThumbHeightFromAvailHeight(int availHeight);
    void reportThumbAttributes();

    QModelIndex currentIndex;
    int currentRow;

signals:
    void update(const QModelIndex index, QRect iconRect) const;

public slots:
    void onCurrentChanged(QModelIndex current, QModelIndex previous);

private:
    QObject parent;
    bool &isRatingBadgeVisible;
    bool delegateShowThumbLabels;
    QFont font;

    // define colors
    QColor defaultBorderColor;
    QColor currentItemColor;
    QColor selectedColor;
    QColor pickColor;
    QColor cacheColor;
    QColor cacheBorderColor;

    QPen border;
    QPen pick;
    QPen notPick;
    QPen backPen;
    QPen currentPen;
    QPen selectedPen;

    int itemPadding;
    int itemBorderThickness;
    int thumbBorderThickness;
    int thumbBorderPadding;
    int fontHt;
    int badgeSize;
    int cacheDiam = 6;
    int cacheOffset = 3;
    int ratingDiam;
    int ratingTextSize;
    int alignVertPad;
    int alignHorPad;

    QPoint itemBorderOffset;
    QPoint thumbPaddingOffset;
    QPoint thumbBorderOffset;
    QPoint paddingOffset;
    QPoint ratingTopLeft;
    QPoint ratingBottomRight;
    QRect ratingRect;
    QPoint ratingTextTopLeft;
    QPoint ratingTextBottomRight;
    QPoint cacheTopLeft;
    QPoint cacheBottomRight;

    QSize thumbSize;
    QSize selectedSize;
    QSize thumbSpace;

    QRect thumbRect;
    QRect itemRect;
    QRect selectedRect;
    QRect iconRect;
    QRect ratingTextRect;
    QRect cacheRect;
    QRect textRect;
};

#endif // THUMBVIEWDELEGATE_H
