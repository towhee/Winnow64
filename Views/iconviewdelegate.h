#ifndef THUMBVIEWDELEGATE_H
#define THUMBVIEWDELEGATE_H

#include <QtGui>
#include <QtWidgets>
#include <QStyleOptionViewItem>
#include <QFont>
#include "Main/global.h"

class IconViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    IconViewDelegate(QObject *parent, bool &isRatingBadgeVisible);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index ) const;

    void setThumbDimensions(int thumbWidth, int thumbHeight,
          int labelFontSize, bool showThumbLabels, int badgeSize);

    QSize getCellSize();
    QSize getCellSize(QSize icon);
    int getCellHeightFromAvailHeight(int availHeight);
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
    QColor ingestedColor;
    QColor cacheColor;
    QColor cacheBorderColor;

    QPen border;
    QPen pick;
    QPen ingested;
    QPen notPick;
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
