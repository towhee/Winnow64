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

    void setThumbDimensions(int thumbWidth, int thumbHeight,
          int thumbPadding, int labelFontSize, bool showThumbLabels);

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
    QSize thumbSize;
    QSize thumbSpace;
    int itemPadding;
    int itemBorderThickness;
    int thumbBorderThickness;
    int thumbBorderPadding;
    int fontHt;
};

#endif // THUMBVIEWDELEGATE_H
